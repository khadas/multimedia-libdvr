#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "dvr_types.h"
#include "dvr_record.h"
#include "dvr_crypto.h"
#include "record_device.h"
#include "segment.h"

#define MAX_DVR_RECORD_SESSION_COUNT 2

/**\brief DVR index file type*/
typedef enum {
  DVR_INDEX_TYPE_PCR,                                                   /**< DVR index file use pcr*/
  DVR_INDEX_TYPE_LOCAL_CLOCK,                                           /**< DVR index file use local clock*/
  DVR_INDEX_TYPE_INVALID                                                 /**< DVR index file type invalid type*/
} DVR_IndexType_t;

/**\brief DVR VOD context*/
typedef struct {
  pthread_mutex_t                 mutex;                                /**< VOD mutex lock*/
  pthread_cond_t                  cond;                                 /**< VOD condition*/
  void                            *buffer;                              /**< VOD buffer*/
  uint32_t                        buf_len;                              /**< VOD buffer len*/
} DVR_VodContext_t;

/**\brief DVR record context*/
typedef struct {
  pthread_t                       thread;                               /**< DVR thread handle*/
  Record_DeviceHandle_t           dev_handle;                           /**< DVR device handle*/
  Segment_Handle_t                segment_handle;                       /**< DVR segment handle*/
  DVR_RecordState_t               state;                                /**< DVR record state*/
  char                            location[DVR_MAX_LOCATION_SIZE];      /**< DVR record file location*/
  DVR_RecordSegmentStartParams_t  segment_params;                       /**< DVR record start parameters*/
  DVR_RecordSegmentInfo_t         segment_info;                         /**< DVR record current segment info*/
  size_t                          notification_size;                    /**< DVR record nogification size*/
  DVR_RecordEventFunction_t       event_notify_fn;                      /**< DVR record event notify function*/
  void                            *event_userdata;                      /**< DVR record event userdata*/
  //DVR_VodContext_t                vod;                                  /**< DVR record vod context*/
  int                             is_vod;                               /**< Indicate current mode is VOD record mode*/
} DVR_RecordContext_t;

static DVR_RecordContext_t record_ctx[MAX_DVR_RECORD_SESSION_COUNT] = {
  {
    .state = DVR_RECORD_STATE_CLOSED
  },
  {
    .state = DVR_RECORD_STATE_CLOSED
  }
};

//#define USE_TEST_DATA
#ifdef USE_TEST_DATA
static int test_data_read(uint8_t *buf, int len)
{
  int i;
  for (i = 0; i < len; i++) {
    buf[i] = i % 0xff;
  }
  usleep(50*1000);
  return len;
}
#endif

static int record_save_pcr(DVR_RecordContext_t *p_ctx, uint8_t *buf, loff_t pos)
{
  uint8_t *p = buf;
  int len;
  uint8_t afc;
  uint64_t pcr = 0;
  int has_pcr = 0;
  int pid;
  int adp_field_len;

  pid = ((p[1] & 0x1f) << 8) | p[2];
  if (pid == 0x1fff)
    return has_pcr;

  //scramble = p[3] >> 6;
  //cc = p[3] & 0x0f;
  afc = (p[3] >> 4) & 0x03;

  p += 4;
  len = 184;

  if (afc & 2) {
    adp_field_len = p[0];
    /* Skip adaptation len */
    p++;
    len--;
    /* Parse pcr field, see 13818 spec table I-2-6,adaptation_field */
    if (p[0] & 0x10 && len >= 6) {
    /* get pcr value,pcr is 33bit value */
    pcr = (((uint64_t)(p[1])) << 25)
        | (((uint64_t)p[2]) << 17)
        | (((uint64_t)(p[3])) << 9)
        | (((uint64_t)p[4]) << 1)
        | ((((uint64_t)p[5]) & 0x80) >> 7);
      has_pcr = 1;
    }

    p += adp_field_len;
    len -= adp_field_len;

    if (len < 0) {
      DVR_DEBUG(1, "parser pcr: illegal adaptation field length");
      return 0;
    }
  }

  if (has_pcr) {
    segment_update_pts(p_ctx->segment_handle, pcr/90, pos);
  }
  return has_pcr;
}

static int record_do_pcr_index(DVR_RecordContext_t *p_ctx, uint8_t *buf, int len)
{
  uint8_t *p = buf;
  int left = len;
  loff_t pos;
  int has_pcr = 0;

  pos = segment_tell_position(p_ctx->segment_handle);
  while (left >= 188) {
    if (*p == 0x47) {
      has_pcr |= record_save_pcr(p_ctx, p, pos);
      p += 188;
      left -= 188;
      pos += 188;
    } else {
      p++;
      left --;
      pos++;
    }
  }
  return has_pcr;
}

void *record_thread(void *arg)
{
  DVR_RecordContext_t *p_ctx = (DVR_RecordContext_t *)arg;
  ssize_t len;
  uint8_t *buf;
  int block_size = 256*1024;
  loff_t pos = 0;
  int ret;
  struct timespec start_ts, end_ts;
  DVR_RecordStatus_t record_status;
  int has_pcr;
  int index_type = DVR_INDEX_TYPE_INVALID;
  time_t pre_time = 0;
  #define DVR_STORE_INFO_TIME (1000)

  buf = (uint8_t *)malloc(block_size);
  if (!buf) {
    DVR_DEBUG(1, "%s, malloc failed", __func__);
    return NULL;
  }

  clock_gettime(CLOCK_MONOTONIC, &start_ts);
  while (p_ctx->state == DVR_RECORD_STATE_STARTED) {
    /* data from dmx, normal dvr case */
#ifdef USE_TEST_DATA
    len = test_data_read(buf, block_size);
#else
    len = record_device_read(p_ctx->dev_handle, buf, block_size, 1000);
#endif
    if (len == DVR_FAILURE) {
      usleep(10*1000);
      continue;
    }
    /* got data from device, record it */
    pos = segment_tell_position(p_ctx->segment_handle);
    has_pcr = record_do_pcr_index(p_ctx, buf, len);
    if (has_pcr == 0 && index_type == DVR_INDEX_TYPE_INVALID) {
      clock_gettime(CLOCK_MONOTONIC, &end_ts);
      if ((end_ts.tv_sec*1000 + end_ts.tv_nsec/1000000) -
          (start_ts.tv_sec*1000 + start_ts.tv_nsec/1000000) > 40) {
        /* PCR interval threshlod > 40 ms*/
        DVR_DEBUG(1, "%s use local clock time index", __func__);
        index_type = DVR_INDEX_TYPE_LOCAL_CLOCK;
      }
    } else if (has_pcr && index_type == DVR_INDEX_TYPE_INVALID){
      DVR_DEBUG(1, "%s use pcr time index", __func__);
      index_type = DVR_INDEX_TYPE_PCR;
    }
    ret = segment_write(p_ctx->segment_handle, buf, len);

    /* Update segment info */
    p_ctx->segment_info.size += len;
    /*Duration need use pcr to calculate, todo...*/
    if (index_type == DVR_INDEX_TYPE_PCR) {
      p_ctx->segment_info.duration = segment_tell_total_time(p_ctx->segment_handle);
      if (pre_time == 0)
       pre_time = p_ctx->segment_info.duration;
    } else if (index_type == DVR_INDEX_TYPE_LOCAL_CLOCK) {
      clock_gettime(CLOCK_MONOTONIC, &end_ts);
      p_ctx->segment_info.duration = (end_ts.tv_sec*1000 + end_ts.tv_nsec/1000000) -
        (start_ts.tv_sec*1000 + start_ts.tv_nsec/1000000);
            if (pre_time == 0)
       pre_time = p_ctx->segment_info.duration;
      segment_update_pts(p_ctx->segment_handle, p_ctx->segment_info.duration, pos);
    } else {
      DVR_DEBUG(1, "%s can NOT do time index", __func__);
    }
    p_ctx->segment_info.nb_packets = p_ctx->segment_info.size/188;

    if (p_ctx->segment_info.duration - pre_time > DVR_STORE_INFO_TIME) {
      pre_time = p_ctx->segment_info.duration + DVR_STORE_INFO_TIME;
      segment_store_info(p_ctx->segment_handle, &(p_ctx->segment_info));
    }
     /*Event notification*/
    if (p_ctx->notification_size &&
        p_ctx->event_notify_fn &&
        !(p_ctx->segment_info.size % p_ctx->notification_size) &&
        p_ctx->segment_info.duration > 0) {
      memset(&record_status, 0, sizeof(record_status));
      //clock_gettime(CLOCK_MONOTONIC, &end_ts);

      record_status.state = p_ctx->state;
      record_status.info.id = p_ctx->segment_info.id;
      record_status.info.duration = p_ctx->segment_info.duration;
      record_status.info.size = p_ctx->segment_info.size;
      record_status.info.nb_packets = p_ctx->segment_info.size/188;
      p_ctx->event_notify_fn(DVR_RECORD_EVENT_STATUS, &record_status, p_ctx->event_userdata);
      DVR_DEBUG(1, "%s notify record status, state:%d, id:%lld, duration:%ld ms, size:%zu",
          __func__, record_status.state, record_status.info.id, record_status.info.duration, record_status.info.size);
    }
  }

  free((void *)buf);
  DVR_DEBUG(1, "exit %s", __func__);
  return NULL;
}

int dvr_record_open(DVR_RecordHandle_t *p_handle, DVR_RecordOpenParams_t *params)
{
  DVR_RecordContext_t *p_ctx;
  Record_DeviceOpenParams_t dev_open_params;
  int ret;
  uint32_t i;

  DVR_RETURN_IF_FALSE(p_handle);
  DVR_RETURN_IF_FALSE(params);

  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (record_ctx[i].state == DVR_RECORD_STATE_CLOSED) {
      break;
    }
  }
  DVR_RETURN_IF_FALSE(record_ctx[i].state == DVR_RECORD_STATE_CLOSED);
  p_ctx = &record_ctx[i];
  DVR_DEBUG(1, "%s , current state:%d, dmx_id:%d, notification_size:%zu", __func__,
      p_ctx->state, params->dmx_dev_id, params->notification_size);

  /*Process event params*/
  p_ctx->notification_size = params->notification_size;
  p_ctx->event_notify_fn = params->event_fn;
  p_ctx->event_userdata = params->event_userdata;
  /*Process crypto params, todo*/
#if 0
  DVR_CryptoPeriod_t   crypto_period;     /**< DVR crypto period information*/
  DVR_CryptoFunction_t crypto_fn;         /**< DVR crypto callback function*/
  void                *crypto_data;       /**< DVR crypto userdata*/
#endif

  memset((void *)&dev_open_params, 0, sizeof(dev_open_params));
  if (params->data_from_memory) {
    /* data from memory, VOD case */
    p_ctx->is_vod = 1;
  } else {
    p_ctx->is_vod = 0;
    /* data from dmx, normal dvr case */
    dev_open_params.dmx_dev_id = params->dmx_dev_id;
    dev_open_params.buf_size = 256*1024;

    ret = record_device_open(&p_ctx->dev_handle, &dev_open_params);
    if (ret != DVR_SUCCESS) {
      DVR_DEBUG(1, "%s, open record devices failed", __func__);
      return DVR_FAILURE;
    }
  }

  p_ctx->state = DVR_RECORD_STATE_OPENED;

  *p_handle = p_ctx;
  return DVR_SUCCESS;
}

int dvr_record_close(DVR_RecordHandle_t handle)
{
  DVR_RecordContext_t *p_ctx;
  int ret;
  uint32_t i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  DVR_DEBUG(1, "%s , current state:%d", __func__, p_ctx->state);
  DVR_RETURN_IF_FALSE(p_ctx->state != DVR_RECORD_STATE_CLOSED);

  if (p_ctx->is_vod) {
    ret = DVR_SUCCESS;
  } else {
    ret = record_device_close(p_ctx->dev_handle);
    if (ret != DVR_SUCCESS) {
      DVR_DEBUG(1, "%s, failed", __func__);
    }
  }

  p_ctx->state = DVR_RECORD_STATE_CLOSED;
  return ret;
}

#if 0
int dvr_record_register_encryption(DVR_RecordHandle_t handle,
    DVR_CryptoFunction_t cb,
    DVR_CryptoParams_t params,
    void *userdata)
{
  return DVR_SUCCESS;
}
#endif

int dvr_record_start_segment(DVR_RecordHandle_t handle, DVR_RecordStartParams_t *params)
{
  DVR_RecordContext_t *p_ctx;
  Segment_OpenParams_t open_params;
  int ret;
  uint32_t i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  DVR_DEBUG(1, "%s , current state:%d", __func__, p_ctx->state);
  DVR_RETURN_IF_FALSE(p_ctx->state != DVR_RECORD_STATE_STARTED);
  DVR_RETURN_IF_FALSE(p_ctx->state != DVR_RECORD_STATE_CLOSED);
  DVR_RETURN_IF_FALSE(params);

  DVR_RETURN_IF_FALSE(strlen((const char *)params->location) < DVR_MAX_LOCATION_SIZE);
  memset(&open_params, 0, sizeof(open_params));
  memcpy(open_params.location, params->location, strlen(params->location));
  open_params.segment_id = params->segment.segment_id;
  open_params.mode = SEGMENT_MODE_WRITE;

  ret = segment_open(&open_params, &p_ctx->segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  /*process params*/
  {
    memcpy(p_ctx->location, params->location, strlen(params->location));
    //need all params??
    memcpy(&p_ctx->segment_params, &params->segment, sizeof(params->segment));
    /*save current segment info*/
    memset(&p_ctx->segment_info, 0, sizeof(p_ctx->segment_info));
    p_ctx->segment_info.id = params->segment.segment_id;
    p_ctx->segment_info.nb_pids = params->segment.nb_pids;
    memcpy(p_ctx->segment_info.pids, params->segment.pids, params->segment.nb_pids*sizeof(DVR_StreamPid_t));
  }

  if (!p_ctx->is_vod) {
    /* normal dvr case */
    for (i = 0; i < params->segment.nb_pids; i++) {
      ret = record_device_add_pid(p_ctx->dev_handle, params->segment.pids[i].pid);
      DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
    }
    ret = record_device_start(p_ctx->dev_handle);
    DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  }

  p_ctx->state = DVR_RECORD_STATE_STARTED;
  if (!p_ctx->is_vod)
    pthread_create(&p_ctx->thread, NULL, record_thread, p_ctx);

  return DVR_SUCCESS;
}

int dvr_record_next_segment(DVR_RecordHandle_t handle, DVR_RecordStartParams_t *params, DVR_RecordSegmentInfo_t *p_info)
{
  DVR_RecordContext_t *p_ctx;
  Segment_OpenParams_t open_params;
  int ret;
  uint32_t i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  DVR_DEBUG(1, "%s , current state:%d", __func__, p_ctx->state);
  DVR_RETURN_IF_FALSE(p_ctx->state == DVR_RECORD_STATE_STARTED);
  //DVR_RETURN_IF_FALSE(p_ctx->state != DVR_RECORD_STATE_CLOSED);
  DVR_RETURN_IF_FALSE(params);
  DVR_RETURN_IF_FALSE(p_info);
  DVR_RETURN_IF_FALSE(!p_ctx->is_vod);

  /*Stop the on going record segment*/
  //ret = record_device_stop(p_ctx->dev_handle);
  //DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  p_ctx->state = DVR_RECORD_STATE_STOPPED;
  pthread_join(p_ctx->thread, NULL);

  /*Update segment info*/
  memcpy(p_info, &p_ctx->segment_info, sizeof(p_ctx->segment_info));

  ret = segment_store_info(p_ctx->segment_handle, p_info);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  DVR_DEBUG(1, "%s dump segment info, id:%lld, nb_pids:%d, duration:%ld ms, size:%zu, nb_packets:%d",
      __func__, p_info->id, p_info->nb_pids, p_info->duration, p_info->size, p_info->nb_packets);

  /*Close current segment*/
  ret = segment_close(p_ctx->segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  /*Open the new record segment*/
  memset(&open_params, 0, sizeof(open_params));
  memcpy(open_params.location, p_ctx->location, strlen(p_ctx->location));
  open_params.segment_id = params->segment.segment_id;
  open_params.mode = SEGMENT_MODE_WRITE;

  ret = segment_open(&open_params, &p_ctx->segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  /*process params*/
  {
    //need all params??
    memcpy(&p_ctx->segment_params, &params->segment, sizeof(params->segment));
    /*save current segment info*/
    memset(&p_ctx->segment_info, 0, sizeof(p_ctx->segment_info));
    p_ctx->segment_info.id = params->segment.segment_id;
    memcpy(p_ctx->segment_info.pids, params->segment.pids, params->segment.nb_pids*sizeof(DVR_StreamPid_t));
  }

  p_ctx->segment_info.nb_pids = 0;
  for (i = 0; i < params->segment.nb_pids; i++) {
    switch (params->segment.pid_action[i]) {
      case DVR_RECORD_PID_CREATE:
        DVR_DEBUG(1, "%s create pid:%d", __func__, params->segment.pids[i].pid);
        ret = record_device_add_pid(p_ctx->dev_handle, params->segment.pids[i].pid);
        p_ctx->segment_info.nb_pids++;
        DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
        break;
      case DVR_RECORD_PID_KEEP:
        DVR_DEBUG(1, "%s keep pid:%d", __func__, params->segment.pids[i].pid);
        p_ctx->segment_info.nb_pids++;
        break;
      case DVR_RECORD_PID_CLOSE:
        DVR_DEBUG(1, "%s close pid:%d", __func__, params->segment.pids[i].pid);
        ret = record_device_remove_pid(p_ctx->dev_handle, params->segment.pids[i].pid);
        DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
        break;
      default:
        DVR_DEBUG(1, "%s wrong action pid:%d", __func__, params->segment.pids[i].pid);
        return DVR_FAILURE;
    }
  }

  //ret = record_device_start(p_ctx->dev_handle);
  //DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  pthread_create(&p_ctx->thread, NULL, record_thread, p_ctx);
  p_ctx->state = DVR_RECORD_STATE_STARTED;
  return DVR_SUCCESS;
}

int dvr_record_stop_segment(DVR_RecordHandle_t handle, DVR_RecordSegmentInfo_t *p_info)
{
  DVR_RecordContext_t *p_ctx;
  int ret;
  uint32_t i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  DVR_DEBUG(1, "%s , current state:%d", __func__, p_ctx->state);
  DVR_RETURN_IF_FALSE(p_ctx->state != DVR_RECORD_STATE_STOPPED);
  DVR_RETURN_IF_FALSE(p_ctx->state != DVR_RECORD_STATE_CLOSED);
  DVR_RETURN_IF_FALSE(p_info);

  p_ctx->state = DVR_RECORD_STATE_STOPPED;
  if (p_ctx->is_vod) {
    p_ctx->segment_info.duration = segment_tell_total_time(p_ctx->segment_handle);
    p_ctx->segment_info.duration = 10*1000; //debug, should delete it
  } else {
    ret = record_device_stop(p_ctx->dev_handle);
    DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
    //p_ctx->state = DVR_RECORD_STATE_STOPPED;
    pthread_join(p_ctx->thread, NULL);
  }


  /*Update segment info*/
  memcpy(p_info, &p_ctx->segment_info, sizeof(p_ctx->segment_info));

  ret = segment_store_info(p_ctx->segment_handle, p_info);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  DVR_DEBUG(1, "%s dump segment info, id:%lld, nb_pids:%d, duration:%ld ms, size:%zu, nb_packets:%d",
      __func__, p_info->id, p_info->nb_pids, p_info->duration, p_info->size, p_info->nb_packets);

  ret = segment_close(p_ctx->segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  return DVR_SUCCESS;
}

int dvr_record_resume_segment(DVR_RecordHandle_t handle, DVR_RecordStartParams_t *params, uint64_t *p_resume_size)
{
  DVR_RecordContext_t *p_ctx;
  uint32_t i;
  int ret;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);
  DVR_RETURN_IF_FALSE(params);
  DVR_RETURN_IF_FALSE(p_resume_size);

  DVR_DEBUG(1, "%s , current state:%d, resume size:%lld", __func__, p_ctx->state, *p_resume_size);
  ret = dvr_record_start_segment(handle, params);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  p_ctx->segment_info.size = *p_resume_size;

  return DVR_SUCCESS;
}

int dvr_record_get_status(DVR_RecordHandle_t handle, DVR_RecordStatus_t *p_status)
{
  DVR_RecordContext_t *p_ctx;
  int i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);
  DVR_RETURN_IF_FALSE(p_status);

  //lock
  p_status->state = p_ctx->state;
  p_status->info.id = p_ctx->segment_info.id;
  p_status->info.duration = p_ctx->segment_info.duration;
  p_status->info.size = p_ctx->segment_info.size;
  p_status->info.nb_packets = p_ctx->segment_info.size/188;

  return DVR_SUCCESS;
}

int dvr_record_write(DVR_RecordHandle_t handle, void *buffer, uint32_t len)
{
  DVR_RecordContext_t *p_ctx;
  uint32_t i;
  off_t pos = 0;
  int ret;
  int has_pcr;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);
  DVR_RETURN_IF_FALSE(buffer);
  DVR_RETURN_IF_FALSE(len);

  pos = segment_tell_position(p_ctx->segment_handle);
  has_pcr = record_do_pcr_index(p_ctx, buffer, len);
  if (has_pcr == 0) {
    /* Pull VOD record shoud use PCR time index */
    DVR_DEBUG(1, "%s has no pcr, can NOT do time index", __func__);
  }
  ret = segment_write(p_ctx->segment_handle, buffer, len);
  p_ctx->segment_info.size += len;
  p_ctx->segment_info.nb_packets = p_ctx->segment_info.size/188;

  return DVR_SUCCESS;
}
