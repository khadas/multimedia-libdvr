#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>


#include "dmx.h"
/*add for config define for linux dvb *.h*/
#include "record_device.h"
#include "dvr_types.h"
#include "dvr_utils.h"

#define MAX_RECORD_DEVICE_COUNT 2
#define MAX_DEMUX_DEVICE_COUNT 3

/**\brief DVR record device state*/
typedef enum {
  RECORD_DEVICE_STATE_OPENED,                                         /**< Record open state*/
  RECORD_DEVICE_STATE_STARTED,                                        /**< Record start state*/
  RECORD_DEVICE_STATE_STOPPED,                                        /**< Record stop state*/
  RECORD_DEVICE_STATE_CLOSED,                                         /**< Record close state*/
} Record_DeviceState_t;

/**\brief Record stream information*/
typedef struct {
  int                           fid;                                   /**< DMX Filter ID*/
  uint16_t                      pid;                                   /**< Stream PID*/
  DVR_Bool_t                    is_start;                              /**< Flag indicate the stream is start or not*/
} Record_Stream_t;

/**\brief Record device context information*/
typedef struct {
  int                           fd;                                    /**< DVR device file descriptor*/
  int                           stream_cnt;                            /**< Stream counts*/
  Record_Stream_t               streams[DVR_MAX_RECORD_PIDS_COUNT];    /**< Record stream list*/
  Record_DeviceState_t          state;                                 /**< Record device state*/
  uint32_t                      dmx_dev_id;                            /**< Record source*/
  pthread_mutex_t               lock;                                  /**< Record device lock*/
  int                           evtfd;                                 /**< eventfd for poll's exit*/
} Record_DeviceContext_t;

static Record_DeviceContext_t record_ctx[MAX_RECORD_DEVICE_COUNT] = {
  {
    .state = RECORD_DEVICE_STATE_CLOSED,
    .lock = PTHREAD_MUTEX_INITIALIZER
  },
  {
    .state = RECORD_DEVICE_STATE_CLOSED,
    .lock = PTHREAD_MUTEX_INITIALIZER
  }
};

int record_device_open(Record_DeviceHandle_t *p_handle, Record_DeviceOpenParams_t *params)
{
  int i;
  int dev_no;
  char dev_name[32];
  //int ret;
  char buf[64];
  char cmd[32];
  Record_DeviceContext_t *p_ctx;

  DVR_RETURN_IF_FALSE(p_handle);
  DVR_RETURN_IF_FALSE(params);
  DVR_RETURN_IF_FALSE(params->dmx_dev_id < MAX_DEMUX_DEVICE_COUNT);

  for (dev_no = 0; dev_no <  MAX_RECORD_DEVICE_COUNT; dev_no++) {
    if (record_ctx[dev_no].state == RECORD_DEVICE_STATE_CLOSED)
      break;
  }
  DVR_RETURN_IF_FALSE(dev_no < MAX_RECORD_DEVICE_COUNT);
  DVR_RETURN_IF_FALSE(record_ctx[dev_no].state == RECORD_DEVICE_STATE_CLOSED);
  p_ctx = &record_ctx[dev_no];

  pthread_mutex_lock(&p_ctx->lock);
  for (i = 0; i < DVR_MAX_RECORD_PIDS_COUNT; i++) {
    p_ctx->streams[i].is_start = DVR_FALSE;
    p_ctx->streams[i].pid = DVR_INVALID_PID;
    p_ctx->streams[i].fid = -1;
  }
  /*Open dvr device*/
  memset(dev_name, 0, sizeof(dev_name));
  snprintf(dev_name, sizeof(dev_name), "/dev/dvb0.dvr%d", params->dmx_dev_id);
  p_ctx->fd = open(dev_name, O_RDONLY);
  if (p_ctx->fd == -1)
  {
    DVR_DEBUG(1, "%s cannot open \"%s\" (%s)", __func__, dev_name, strerror(errno));
    pthread_mutex_unlock(&p_ctx->lock);
    return DVR_FAILURE;
  }
  fcntl(p_ctx->fd, F_SETFL, fcntl(p_ctx->fd, F_GETFL, 0) | O_NONBLOCK, 0);

  p_ctx->evtfd = eventfd(0, 0);
  DVR_DEBUG(1, "%s, %d fd: %d %p %d %p", __func__, __LINE__, p_ctx->fd, &(p_ctx->fd), p_ctx->evtfd, &(p_ctx->evtfd));

  /*Configure flush size*/
  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_flush_size", dev_no);
  memset(cmd, 0, sizeof(cmd));
  snprintf(cmd, sizeof(cmd), "%d", params->buf_size);
  dvr_file_echo(buf, cmd);

  /*Configure source*/
  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_source", dev_no);
  memset(cmd, 0, sizeof(cmd));
  snprintf(cmd, sizeof(cmd), "dmx%d", params->dmx_dev_id);
  p_ctx->dmx_dev_id = params->dmx_dev_id;
  dvr_file_echo(buf, cmd);

  /*Configure Non secure mode*/
  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_secure_enable", dev_no);
  dvr_file_echo(buf, "0");

  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_secure_addr", dev_no);
  dvr_file_echo(buf, "0");

  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_secure_addr_size", dev_no);
  dvr_file_echo(buf, "0");

  p_ctx->state = RECORD_DEVICE_STATE_OPENED;
  *p_handle = p_ctx;
  pthread_mutex_unlock(&p_ctx->lock);
  return DVR_SUCCESS;
}

int record_device_close(Record_DeviceHandle_t handle)
{
  Record_DeviceContext_t *p_ctx;
  int i;

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(i < MAX_RECORD_DEVICE_COUNT);
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  pthread_mutex_lock(&p_ctx->lock);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(p_ctx->state != RECORD_DEVICE_STATE_CLOSED, &p_ctx->lock);
  close(p_ctx->fd);
  close(p_ctx->evtfd);
  p_ctx->state = RECORD_DEVICE_STATE_CLOSED;
  pthread_mutex_unlock(&p_ctx->lock);
  return DVR_SUCCESS;
}

int record_device_add_pid(Record_DeviceHandle_t handle, int pid)
{
  int i;
  int fd;
  int ret;
  Record_DeviceContext_t *p_ctx;
  char dev_name[32];
  struct dmx_pes_filter_params params;

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  DVR_RETURN_IF_FALSE(pid != DVR_INVALID_PID);
  for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(i < MAX_RECORD_DEVICE_COUNT);
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  pthread_mutex_lock(&p_ctx->lock);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(p_ctx->state != RECORD_DEVICE_STATE_CLOSED, &p_ctx->lock);
  for (i = 0; i < DVR_MAX_RECORD_PIDS_COUNT; i++) {
    if (p_ctx->streams[i].pid == DVR_INVALID_PID)
      break;
  }
  DVR_RETURN_IF_FALSE(i < DVR_MAX_RECORD_PIDS_COUNT);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(p_ctx->streams[i].pid == DVR_INVALID_PID, &p_ctx->lock);

  p_ctx->streams[i].pid = pid;
	snprintf(dev_name, sizeof(dev_name), "/dev/dvb0.demux%d", p_ctx->dmx_dev_id);
  fd = open(dev_name, O_RDWR);
  if (fd == -1) {
    DVR_DEBUG(1, "%s cannot open \"%s\" (%s)", __func__, dev_name, strerror(errno));
    pthread_mutex_unlock(&p_ctx->lock);
    return DVR_FAILURE;
  }
  p_ctx->streams[i].fid = fd;

  if (p_ctx->state == RECORD_DEVICE_STATE_STARTED) {
    //need start pid filter
    fcntl(fd, F_SETFL, O_NONBLOCK);
    params.pid = p_ctx->streams[i].pid;
    params.input = DMX_IN_FRONTEND;
    params.output = DMX_OUT_TS_TAP;
    params.pes_type = DMX_PES_OTHER;
    ret = ioctl(fd, DMX_SET_PES_FILTER, &params);
    if (ret == -1) {
      DVR_DEBUG(1, "%s set pes filter failed\"%s\" (%s)", __func__, dev_name, strerror(errno));
      pthread_mutex_unlock(&p_ctx->lock);
      return DVR_FAILURE;
    }
    ret = ioctl(fd, DMX_START, 0);
    if (ret == -1) {
      DVR_DEBUG(1, "%s start pes filter failed:\"%s\" (%s)", __func__, dev_name, strerror(errno));
      pthread_mutex_unlock(&p_ctx->lock);
      return DVR_FAILURE;
    }
    p_ctx->streams[i].is_start = DVR_TRUE;
  }
  pthread_mutex_unlock(&p_ctx->lock);
  return DVR_SUCCESS;
}

int record_device_remove_pid(Record_DeviceHandle_t handle, int pid)
{
  Record_DeviceContext_t *p_ctx;
  int fd;
  int ret;
  int i;

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  DVR_RETURN_IF_FALSE(pid != DVR_INVALID_PID);
  for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(i < MAX_RECORD_DEVICE_COUNT);
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  pthread_mutex_lock(&p_ctx->lock);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(p_ctx->state != RECORD_DEVICE_STATE_CLOSED, &p_ctx->lock);
  for (i = 0; i < DVR_MAX_RECORD_PIDS_COUNT; i++) {
    if (p_ctx->streams[i].pid == pid)
      break;
  }
  DVR_RETURN_IF_FALSE(i < DVR_MAX_RECORD_PIDS_COUNT);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(p_ctx->streams[i].pid == pid, &p_ctx->lock);

  fd = p_ctx->streams[i].fid;
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(fd != -1, &p_ctx->lock);

  if (p_ctx->streams[i].is_start == DVR_TRUE) {
    ret = ioctl(fd, DMX_STOP, 0);
    if (ret == -1) {
      DVR_DEBUG(1, "%s stop pes filter failed (%s)", __func__, strerror(errno));
      pthread_mutex_unlock(&p_ctx->lock);
      return DVR_FAILURE;
    }
  }

  p_ctx->streams[i].pid = DVR_INVALID_PID;
  close(fd);
  p_ctx->streams[i].fid = -1;
  p_ctx->streams[i].is_start = DVR_FALSE;
  pthread_mutex_unlock(&p_ctx->lock);
  return DVR_SUCCESS;
}

int record_device_start(Record_DeviceHandle_t handle)
{
  Record_DeviceContext_t *p_ctx;
  int fd;
  int ret;
  int i;
  struct dmx_pes_filter_params params;

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(i < MAX_RECORD_DEVICE_COUNT);
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  pthread_mutex_lock(&p_ctx->lock);
  if (p_ctx->state != RECORD_DEVICE_STATE_OPENED &&
      p_ctx->state != RECORD_DEVICE_STATE_STOPPED) {
    pthread_mutex_unlock(&p_ctx->lock);
    DVR_DEBUG(1, "%s, %d, wrong state:%d", __func__, __LINE__,p_ctx->state);
    return DVR_FAILURE;
  }

  for (i = 0; i < DVR_MAX_RECORD_PIDS_COUNT; i++) {
    if (p_ctx->streams[i].fid != -1 &&
        p_ctx->streams[i].pid != DVR_INVALID_PID &&
        p_ctx->streams[i].is_start == DVR_FALSE) {
      //need start pid filter
      fd = p_ctx->streams[i].fid;
      fcntl(fd, F_SETFL, O_NONBLOCK);
      params.pid = p_ctx->streams[i].pid;
      params.input = DMX_IN_FRONTEND;
      params.output = DMX_OUT_TS_TAP;
      params.pes_type = DMX_PES_OTHER;
      ret = ioctl(fd, DMX_SET_PES_FILTER, &params);
      if (ret == -1) {
        DVR_DEBUG(1, "%s set pes filter failed (%s)", __func__, strerror(errno));
        pthread_mutex_unlock(&p_ctx->lock);
        return DVR_FAILURE;
      }
      ret = ioctl(fd, DMX_START, 0);
      if (ret == -1) {
        DVR_DEBUG(1, "%s start pes filter failed (%s)", __func__, strerror(errno));
        pthread_mutex_unlock(&p_ctx->lock);
        return DVR_FAILURE;
      }
      p_ctx->streams[i].is_start = DVR_TRUE;
    }
  }
  p_ctx->state = RECORD_DEVICE_STATE_STARTED;
  pthread_mutex_unlock(&p_ctx->lock);
  return DVR_SUCCESS;
}

int record_device_stop(Record_DeviceHandle_t handle)
{
  Record_DeviceContext_t *p_ctx;
  int fd;
  int ret;
  int i;

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(i < MAX_RECORD_DEVICE_COUNT);
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  pthread_mutex_lock(&p_ctx->lock);
  if (p_ctx->state != RECORD_DEVICE_STATE_STARTED) {
    DVR_DEBUG(1, "%s, %d, wrong state:%d", __func__, __LINE__,p_ctx->state);
    pthread_mutex_unlock(&p_ctx->lock);
    return DVR_FAILURE;
  }

  for (i = 0; i < DVR_MAX_RECORD_PIDS_COUNT; i++) {
    if (p_ctx->streams[i].fid != -1 &&
        p_ctx->streams[i].pid != DVR_INVALID_PID &&
        p_ctx->streams[i].is_start == DVR_TRUE) {
      /*Stop the filter*/
      fd = p_ctx->streams[i].fid;
      ret = ioctl(fd, DMX_STOP, 0);
      if (ret == -1) {
        DVR_DEBUG(1, "%s stop pes filter failed (%s)", __func__, strerror(errno));
        pthread_mutex_unlock(&p_ctx->lock);
        return DVR_FAILURE;
      }
      /*Close the filter*/
      p_ctx->streams[i].pid = DVR_INVALID_PID;
      close(fd);
      p_ctx->streams[i].fid = -1;
      p_ctx->streams[i].is_start = DVR_FALSE;
    }
  }
  p_ctx->state = RECORD_DEVICE_STATE_STOPPED;
  {
    /*wakeup the poll*/
    int64_t pad = 1;
    write(p_ctx->evtfd, &pad, sizeof(pad));
  }
  pthread_mutex_unlock(&p_ctx->lock);
  return DVR_SUCCESS;
}

ssize_t record_device_read(Record_DeviceHandle_t handle, void *buf, size_t len, int timeout)
{
  Record_DeviceContext_t *p_ctx;
  struct pollfd fds[2];
  int ret;

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  DVR_RETURN_IF_FALSE(p_ctx->fd != -1);
  DVR_RETURN_IF_FALSE(buf);
  DVR_RETURN_IF_FALSE(len);

  memset(fds, 0, sizeof(fds));

  pthread_mutex_lock(&p_ctx->lock);
  fds[0].fd = p_ctx->fd;
  fds[1].fd = p_ctx->evtfd;
  pthread_mutex_unlock(&p_ctx->lock);

  fds[0].events = fds[1].events = POLLIN | POLLERR;
  ret = poll(fds, 2, timeout);
  if (ret <= 0) {
    if (ret < 0)
      DVR_DEBUG(1, "%s, %d failed: %s fd %d evfd %d", __func__, __LINE__, strerror(errno), p_ctx->fd, p_ctx->evtfd);
    return DVR_FAILURE;
  }

  if (!(fds[0].revents & POLLIN))
    return DVR_FAILURE;

  pthread_mutex_lock(&p_ctx->lock);
  if (p_ctx->state == RECORD_DEVICE_STATE_STARTED) {
    ret = read(fds[0].fd, buf, len);
    if (ret <= 0) {
      DVR_DEBUG(1, "%s, %d failed: %s", __func__, __LINE__, strerror(errno));
      pthread_mutex_unlock(&p_ctx->lock);
      return DVR_FAILURE;
    }
  } else {
      ret = DVR_FAILURE;
  }
  pthread_mutex_unlock(&p_ctx->lock);
  return ret;
}

int record_device_set_secure_buffer(Record_DeviceHandle_t handle, uint8_t *sec_buf, uint32_t len)
{
  Record_DeviceContext_t *p_ctx;
  int i;
  char buf[64];
  char cmd[32];

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  DVR_RETURN_IF_FALSE(sec_buf);
  DVR_RETURN_IF_FALSE(len);

  for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
    if (p_ctx == &record_ctx[i])
      break;
  }
  DVR_RETURN_IF_FALSE(i < MAX_RECORD_DEVICE_COUNT);
  DVR_RETURN_IF_FALSE(p_ctx == &record_ctx[i]);

  pthread_mutex_lock(&p_ctx->lock);
  if (p_ctx->state != RECORD_DEVICE_STATE_OPENED &&
      p_ctx->state != RECORD_DEVICE_STATE_STOPPED) {
    pthread_mutex_unlock(&p_ctx->lock);
    DVR_DEBUG(1, "%s, %d, wrong state:%d", __func__, __LINE__,p_ctx->state);
    return DVR_FAILURE;
  }

  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_secure_enable", i);
  dvr_file_echo(buf, "1");

  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_secure_addr", i);
  snprintf(cmd, sizeof(cmd), "%llu", (uint64_t)sec_buf);
  dvr_file_echo(buf, cmd);

  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_secure_addr_size", i);
  snprintf(cmd, sizeof(cmd), "%d", len);
  dvr_file_echo(buf, cmd);

  pthread_mutex_unlock(&p_ctx->lock);
  return DVR_SUCCESS;
}
