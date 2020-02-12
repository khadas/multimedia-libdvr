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
} DVR_RecordContext_t;

static DVR_RecordContext_t record_ctx[MAX_DVR_RECORD_SESSION_COUNT] = {
  {
    .state = DVR_RECORD_STATE_CLOSED
  },
  {
    .state = DVR_RECORD_STATE_CLOSED
  }
};

void *record_thread(void *arg)
{
  DVR_RecordContext_t *p_ctx = (DVR_RecordContext_t *)arg;
  ssize_t len;
  uint8_t *buf;
  int block_size = 256*1024;
  off_t pos = 0;
  uint64_t pts = 0;
  int ret;
  struct timespec start_ts, end_ts;
  DVR_RecordStatus_t record_status;

  buf = (uint8_t *)malloc(block_size);
  if (!buf) {
    DVR_DEBUG(1, "%s, malloc failed", __func__);
    return NULL;
  }

  clock_gettime(CLOCK_MONOTONIC, &start_ts);
  while (p_ctx->state == DVR_RECORD_STATE_STARTED) {
    len = record_device_read(p_ctx->dev_handle, buf, block_size, 1000);
    if (len == DVR_FAILURE) {
      usleep(10*1000);
      continue;
    }
#if 0
    ret = sw_dmx_extract_pcr(buf, len, &pts, &pos);
    if (ret == DVR_FAILURE) {
      get_local_pts(&pts);
    }
#endif
    pos += segment_tell_position(p_ctx->segment_handle);
    ret = segment_update_pts(p_ctx->segment_handle, pts, pos);
    ret = segment_write(p_ctx->segment_handle, buf, len);
    p_ctx->segment_info.size += len;

    /*Event notification*/
    if (p_ctx->notification_size &&
        p_ctx->event_notify_fn &&
        !(p_ctx->segment_info.size % p_ctx->notification_size)) {
      memset(&record_status, 0, sizeof(record_status));
      clock_gettime(CLOCK_MONOTONIC, &end_ts);

      record_status.state = p_ctx->state;
      record_status.info.id = p_ctx->segment_info.id;
      record_status.info.duration = (end_ts.tv_sec*1000 + end_ts.tv_nsec/1000000) -
        (start_ts.tv_sec*1000 + start_ts.tv_nsec/1000000);
      record_status.info.size = p_ctx->segment_info.size;
      record_status.info.nb_packets = p_ctx->segment_info.size/188;
      p_ctx->event_notify_fn(DVR_RECORD_EVENT_STATUS, &record_status, p_ctx->event_userdata);
      DVR_DEBUG(1, "%s notify record status, state:%d, id:%lld, duration:%ld ms, size:%zu",
          __func__, record_status.state, record_status.info.id, record_status.info.duration, record_status.info.size);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  /*Duration need use pcr to calculate, todo...*/
  p_ctx->segment_info.duration = (end_ts.tv_sec*1000 + end_ts.tv_nsec/1000000) -
    (start_ts.tv_sec*1000 + start_ts.tv_nsec/1000000);
  p_ctx->segment_info.nb_packets = p_ctx->segment_info.size/188;
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
  dev_open_params.dmx_dev_id = params->dmx_dev_id;
  dev_open_params.buf_size = 256*1024;

  ret = record_device_open(&p_ctx->dev_handle, &dev_open_params);
  if (ret != DVR_SUCCESS) {
    DVR_DEBUG(1, "%s, open record devices failed", __func__);
    return DVR_FAILURE;
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

  ret = record_device_close(p_ctx->dev_handle);
  if (ret != DVR_SUCCESS) {
    DVR_DEBUG(1, "%s, failed", __func__);
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

  for (i = 0; i < params->segment.nb_pids; i++) {
    ret = record_device_add_pid(p_ctx->dev_handle, params->segment.pids[i].pid);
    DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  }

  ret = record_device_start(p_ctx->dev_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  p_ctx->state = DVR_RECORD_STATE_STARTED;
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

  ret = record_device_stop(p_ctx->dev_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  p_ctx->state = DVR_RECORD_STATE_STOPPED;
  pthread_join(p_ctx->thread, NULL);

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

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);
  DVR_RETURN_IF_FALSE(params);
  DVR_RETURN_IF_FALSE(p_resume_size);

  DVR_DEBUG(1, "%s , current state:%d", __func__, p_ctx->state);
  return DVR_SUCCESS;
}
