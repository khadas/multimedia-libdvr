#include <stdio.h>
#include <pthread.h>
#include "dvr_types.h"
#include "dvr_record.h"
#include "dvr_crypto.h"
#include "record_device.h"
#include "segment.h"
#include "segment_file.h"

#define MAX_DVR_RECORD_SESSION_COUNT 2
typedef struct {
  pthread_t                       thread;
  Record_DeviceHandle_t           dev_handle;
  Segment_Handle_t                segment_handle;
  DVR_RecordState_t               state;
  char                            location[DVR_MAX_LOCATION_SIZE];
  DVR_RecordSegmentStartParams_t  segment_params;
} DVR_RecordContext_t;

static DVR_RecordContext_t record_ctx[MAX_DVR_RECORD_SESSION_COUNT] = {
  {
    .state = DVR_RECORD_STATE_STOPPED
  },
  {
    .state = DVR_RECORD_STATE_STOPPED
  }
};

void *record_thread(void *arg)
{
  DVR_RecordContext_t *p_ctx = (DVR_RecordContext_t *)arg;
  ssize_t len;
  uint8_t *buf;
  int block_size = 32*1024;
  off_t pos = 0;
  uint64_t pts = 0;
  int ret;

  buf = malloc(block_size);
  if (!buf) {
    DVR_DEBUG(1, "%s, malloc failed", __func__);
    return NULL;
  }

  while (p_ctx->state == DVR_RECORD_STATE_STARTED) {
    len = record_device_read(p_ctx->dev_handle, buf, sizeof(buf), 1000);
#if 0
    ret = sw_dmx_extract_pcr(buf, len, &pts, &pos);
    if (ret == DVR_FAILURE) {
      get_local_pts(&pts);
    }
#endif
    pos += segment_tell(p_ctx->segment_handle);
    ret = segment_update_pts(p_ctx->segment_handle, pts, pos);
    ret = segment_write(p_ctx->segment_handle, buf, len);
  }

  free(buf);
  return NULL;
}

int dvr_record_open(DVR_RecordHandle_t *p_handle, DVR_RecordOpenParams_t *params)
{
  DVR_RecordContext_t *p_ctx;
  Record_DeviceOpenParams_t dev_open_params;
  int ret;
  int i;

  DVR_ASSERT(p_handle);
  DVR_ASSERT(params);

  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (record_ctx[i].state == DVR_RECORD_STATE_STOPPED) {
      break;
    }
  }
  DVR_ASSERT(record_ctx[i].state == DVR_RECORD_STATE_STOPPED);
  p_ctx = &record_ctx[i];

  /*Process params, todo*/
  {
  }

  memset(&dev_open_params, 0, sizeof(dev_open_params));
  /*Process dev_open_params, todo*/
  {
  }
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
  int i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_ASSERT(p_ctx == &record_ctx[i]);

  DVR_ASSERT(p_ctx->state != DVR_RECORD_STATE_STOPPED);

  ret = record_device_close(p_ctx->dev_handle);
  if (ret != DVR_SUCCESS) {
    DVR_DEBUG(1, "%s, failed", __func__);
  }

  p_ctx->state = DVR_RECORD_STATE_STOPPED;
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
  int i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_ASSERT(p_ctx == &record_ctx[i]);

  DVR_ASSERT(p_ctx->state != DVR_RECORD_STATE_STARTED);
  DVR_ASSERT(params);

  DVR_ASSERT(strlen(params->location) < DVR_MAX_LOCATION_SIZE);
  memset(&open_params, 0, sizeof(open_params));
  memcpy(open_params.location, params->location, strlen(params->location));
  open_params.segment_id = params->segment.segment_id;
  open_params.mode = SEGMENT_MODE_WRITE;

  ret = segment_open(&open_params, &p_ctx->segment_handle);
  DVR_ASSERT(ret == DVR_SUCCESS);

  /*process params, todo*/
  {
    memcpy(p_ctx->location, params->location, strlen(params->location));
    memcpy(&p_ctx->segment_params, &params->segment, sizeof(params->segment));
  }

  for (i = 0; i < params->segment.nb_pids; i++) {
    ret = record_device_add_pid(p_ctx->dev_handle, params->segment.pids[i].pid);
    DVR_ASSERT(ret);
  }

  ret = record_device_start(p_ctx->dev_handle);
  DVR_ASSERT(ret);

  pthread_create(&p_ctx->thread, NULL, record_thread, p_ctx);
  p_ctx->state = DVR_RECORD_STATE_STARTED;
  return DVR_SUCCESS;
}

int dvr_record_next_segment(DVR_RecordHandle_t handle, DVR_RecordStartParams_t *params, DVR_RecordSegmentInfo_t *p_info)
{
  DVR_RecordContext_t *p_ctx;
  int ret;
  int i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_ASSERT(p_ctx == &record_ctx[i]);

  DVR_ASSERT(p_ctx->state != DVR_RECORD_STATE_STARTED);
  DVR_ASSERT(params);
  DVR_ASSERT(p_info);

  /*Stop the on going record segment*/
  ret = record_device_stop(p_ctx->dev_handle);
  DVR_ASSERT(ret == DVR_SUCCESS);

  p_ctx->state = DVR_RECORD_STATE_STOPPED;
  pthread_join(p_ctx->thread, NULL);

  ret = segment_file_store(p_ctx->location, p_ctx->segment_params.segment_id, p_info);
  DVR_ASSERT(ret == DVR_SUCCESS);

  /*Update segment info, todo*/
  {
  }

  /*Start the new record segment*/
  /*process params, todo*/
  {
    memcpy(&p_ctx->segment_params, &params->segment, sizeof(params->segment));
  }

  for (i = 0; i < params->segment.nb_pids; i++) {
    ret = record_device_add_pid(p_ctx->dev_handle, params->segment.pids[i].pid);
    DVR_ASSERT(ret);
  }

  ret = record_device_start(p_ctx->dev_handle);
  DVR_ASSERT(ret);

  pthread_create(&p_ctx->thread, NULL, record_thread, p_ctx);
  p_ctx->state = DVR_RECORD_STATE_STARTED;
  return DVR_SUCCESS;
}

int dvr_record_stop_segment(DVR_RecordHandle_t handle, DVR_RecordSegmentInfo_t *p_info)
{
  DVR_RecordContext_t *p_ctx;
  int ret;
  int i;

  p_ctx = (DVR_RecordContext_t *)handle;
  for (i = 0; i < MAX_DVR_RECORD_SESSION_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_ASSERT(p_ctx == &record_ctx[i]);

  DVR_ASSERT(p_ctx->state != DVR_RECORD_STATE_STOPPED);
  DVR_ASSERT(p_info);

  ret = record_device_stop(p_ctx->dev_handle);
  DVR_ASSERT(ret == DVR_SUCCESS);

  p_ctx->state = DVR_RECORD_STATE_STOPPED;
  pthread_join(p_ctx->thread, NULL);

  ret = segment_close(p_ctx->segment_handle);
  DVR_ASSERT(ret == DVR_SUCCESS);

  /*Update segment info, todo*/
  {
  }
  ret = segment_file_store(p_ctx->location, p_ctx->segment_params.segment_id, p_info);
  DVR_ASSERT(ret == DVR_SUCCESS);

  return DVR_SUCCESS;
}
