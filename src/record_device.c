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
#include <dlfcn.h>

#include <dmx.h>
/*add for config define for linux dvb *.h*/
#include "record_device.h"
#include "dvr_types.h"
#include "dvr_utils.h"
#include "dvb_utils.h"

#define MAX_RECORD_DEVICE_COUNT 8
#define MAX_DEMUX_DEVICE_COUNT 8
#define MAX_FEND_DEVICE_COUNT 2

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
  int                           fend_dev_id;                           /**< Frontend device id*/
  uint32_t                      dmx_dev_id;                            /**< Record source*/
  size_t                        dvr_buf;
  size_t                        output_handle;                         /**< Secure demux output*/
  pthread_mutex_t               lock;                                  /**< Record device lock*/
  int                           evtfd;                                 /**< eventfd for poll's exit*/
} Record_DeviceContext_t;

/*  each sid need one mutex */
static pthread_mutex_t secdmx_lock[MAX_FEND_DEVICE_COUNT] = PTHREAD_MUTEX_INITIALIZER;

static Record_DeviceContext_t record_ctx[MAX_RECORD_DEVICE_COUNT] = {
  {
    .state = RECORD_DEVICE_STATE_CLOSED,
    .fend_dev_id = -1,
    .lock = PTHREAD_MUTEX_INITIALIZER
  },
  {
    .state = RECORD_DEVICE_STATE_CLOSED,
    .fend_dev_id = -1,
    .lock = PTHREAD_MUTEX_INITIALIZER
  }
};
/*define sec dmx function api ptr*/
static int SECDMX_API_INIT = 0;
int (*SECDMX_Init_Ptr)(void);
int (*SECDMX_Deinit_Ptr)(void);
int (*SECDMX_AllocateDVRBuffer_Ptr)(int sid, size_t size, size_t *addr);
int (*SECDMX_FreeDVRBuffer_Ptr)(int sid);
int (*SECDMX_AddOutputBuffer_Ptr)(int sid, size_t addr, size_t size, size_t *handle);
int (*SECDMX_AddDVRPids_Ptr)(size_t handle, uint16_t *pids, int pid_num);
int (*SECDMX_RemoveOutputBuffer_Ptr)(size_t handle);
int (*SECDMX_GetOutputBufferStatus_Ptr)(size_t handle, size_t *start_addr, size_t *len);
int (*SECDMX_ProcessData_Ptr)(int sid, size_t wp);

int load_secdmx_api(void)
{
  if (SECDMX_API_INIT == 1) {
    return 0;
  }
  SECDMX_API_INIT = 1;
  void* handle = NULL;
  handle = dlopen("libdmx_client_sys.so", RTLD_NOW);//RTLD_NOW  RTLD_LAZY

  if (handle == NULL) {
    DVR_DEBUG(0, "load_secdmx_api load libdmx_client_sys error[%s] no:%d", strerror(errno), errno);
    handle = dlopen("libdmx_client.so", RTLD_NOW);//RTLD_NOW  RTLD_LAZY
  }

  if (handle == NULL) {
    DVR_DEBUG(0, "load_secdmx_api load libdmx_client error[%s] no:%d", strerror(errno), errno);
    return 0;
  }

  SECDMX_Init_Ptr = dlsym(handle, "SECDMX_Init");
  SECDMX_Deinit_Ptr = dlsym(handle, "SECDMX_Deinit");
  SECDMX_AllocateDVRBuffer_Ptr = dlsym(handle, "SECDMX_AllocateDVRBuffer");
  SECDMX_FreeDVRBuffer_Ptr = dlsym(handle, "SECDMX_FreeDVRBuffer");
  SECDMX_AddOutputBuffer_Ptr = dlsym(handle, "SECDMX_AddOutputBuffer");
  SECDMX_AddDVRPids_Ptr = dlsym(handle, "SECDMX_AddDVRPids");
  SECDMX_RemoveOutputBuffer_Ptr = dlsym(handle, "SECDMX_RemoveOutputBuffer");
  SECDMX_GetOutputBufferStatus_Ptr = dlsym(handle, "SECDMX_GetOutputBufferStatus");
  SECDMX_ProcessData_Ptr = dlsym(handle, "SECDMX_ProcessData");

  return 0;
}

int add_dvr_pids(Record_DeviceContext_t *p_ctx)
{
  int i;
  int result = DVR_SUCCESS;
  int cnt = 0;
  uint16_t pids[DVR_MAX_RECORD_PIDS_COUNT];

  if (dvr_check_dmx_isNew() == 1) {
    DVR_RETURN_IF_FALSE(p_ctx->dvr_buf);
    DVR_RETURN_IF_FALSE(p_ctx->output_handle);

    memset(pids, 0, sizeof(pids));
    for (i = 0; i < DVR_MAX_RECORD_PIDS_COUNT; i++) {
      if (p_ctx->streams[i].pid != DVR_INVALID_PID) {
        pids[cnt++] = p_ctx->streams[i].pid;
        DVR_DEBUG(0, "dvr pid:%#x, cnt:%#x", pids[cnt-1], cnt);
      }
    }
    if (SECDMX_AddDVRPids_Ptr != NULL)
      result = SECDMX_AddDVRPids_Ptr(p_ctx->output_handle, pids, cnt);
    DVR_RETURN_IF_FALSE(result == DVR_SUCCESS);
  }

  return DVR_SUCCESS;
}

int record_device_open(Record_DeviceHandle_t *p_handle, Record_DeviceOpenParams_t *params)
{
  int i;
  int dev_no;
  char dev_name[32];
  int ret;
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
  load_secdmx_api();
  /*Configure flush size*/
  if (dvr_check_dmx_isNew() == 1) {
    /* initialize secure demux client */
    if (SECDMX_Init_Ptr != NULL) {
      ret = SECDMX_Init_Ptr();
      if (ret != DVR_SUCCESS) {
        DVR_DEBUG(1, "%s secure demux init failed:%d", __func__, ret);
      }
    }

    //set buf size
    int buf_size = params->buf_size;
    ret = ioctl(p_ctx->fd, DMX_SET_BUFFER_SIZE, buf_size);
    if (ret == -1) {
      DVR_DEBUG(1, "%s set dvr buf size failed\"%s\" (%s) buf_size:%d", __func__, dev_name, strerror(errno), buf_size);
    } else {
      DVR_DEBUG(1, "%s set dvr buf size success \"%s\" buf_size:%d", __func__, dev_name, buf_size);
    }
  } else {
      //set del buf size is 10 * 188 *1024
      int buf_size = params->ringbuf_size;
      if (buf_size > 0) {
          ret = ioctl(p_ctx->fd, DMX_SET_BUFFER_SIZE, buf_size);
          if (ret == -1) {
            DVR_DEBUG(1, "%s set dvr ringbuf size failed\"%s\" (%s) buf_size:%d", __func__, dev_name, strerror(errno), buf_size);
          } else {
            DVR_DEBUG(1, "%s set dvr ringbuf size success \"%s\" buf_size:%d", __func__, dev_name, buf_size);
          }
      }
  }
  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "/sys/class/stb/asyncfifo%d_flush_size", dev_no);
  memset(cmd, 0, sizeof(cmd));
  snprintf(cmd, sizeof(cmd), "%d", 64*1024);
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

  if (params->fend_dev_id > MAX_FEND_DEVICE_COUNT -1) {
    DVR_DEBUG(0, "invalid frontend devicie id:%d, will use default.\n",
	      params->fend_dev_id);
    p_ctx->fend_dev_id = 0;
  } else {
    p_ctx->fend_dev_id = params->fend_dev_id;
  }
  p_ctx->output_handle = (size_t)NULL;
  p_ctx->dvr_buf = (size_t)NULL;
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
  if (dvr_check_dmx_isNew()) {
    if (p_ctx->output_handle) {
      if (SECDMX_RemoveOutputBuffer_Ptr != NULL)
        SECDMX_RemoveOutputBuffer_Ptr(p_ctx->output_handle);
      p_ctx->output_handle = (size_t)NULL;
    }
    if (p_ctx->dvr_buf) {
      if (SECDMX_FreeDVRBuffer_Ptr != NULL) {
	for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
	  if (&record_ctx[i] != p_ctx &&
	      record_ctx[i].fend_dev_id == p_ctx->fend_dev_id &&
	      record_ctx[i].dvr_buf == p_ctx->dvr_buf) {
		break;
	  }
	}
	if (i >= MAX_RECORD_DEVICE_COUNT) {
          SECDMX_FreeDVRBuffer_Ptr(p_ctx->fend_dev_id);
	}
	p_ctx->dvr_buf = (size_t)NULL;
      }
    }
  }
  p_ctx->fend_dev_id = -1;
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
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(i < DVR_MAX_RECORD_PIDS_COUNT, &p_ctx->lock);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(p_ctx->streams[i].pid == DVR_INVALID_PID, &p_ctx->lock);

  p_ctx->streams[i].pid = pid;
  DVR_DEBUG(1, "%s add pid:%#x", __func__, pid);
	snprintf(dev_name, sizeof(dev_name), "/dev/dvb0.demux%d", p_ctx->dmx_dev_id);
  fd = open(dev_name, O_RDWR);
  if (fd == -1) {
    DVR_DEBUG(1, "%s cannot open \"%s\" (%s)", __func__, dev_name, strerror(errno));
    pthread_mutex_unlock(&p_ctx->lock);
    return DVR_FAILURE;
  }
  p_ctx->streams[i].fid = fd;

  //DVR_RETURN_IF_FALSE_WITH_UNLOCK(DVR_SUCCESS == add_dvr_pids(p_ctx), &p_ctx->lock);
  add_dvr_pids(p_ctx);
  if (p_ctx->state == RECORD_DEVICE_STATE_STARTED) {
    //need start pid filter
    fcntl(fd, F_SETFL, O_NONBLOCK);
    memset(&params, 0, sizeof(params));
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
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(i < DVR_MAX_RECORD_PIDS_COUNT, &p_ctx->lock);
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
  //DVR_RETURN_IF_FALSE_WITH_UNLOCK(DVR_SUCCESS == add_dvr_pids(p_ctx), &p_ctx->lock);
  add_dvr_pids(p_ctx);
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

  //DVR_RETURN_IF_FALSE_WITH_UNLOCK(DVR_SUCCESS == add_dvr_pids(p_ctx), &p_ctx->lock);
  add_dvr_pids(p_ctx);

  for (i = 0; i < DVR_MAX_RECORD_PIDS_COUNT; i++) {
    if (p_ctx->streams[i].fid != -1 &&
        p_ctx->streams[i].pid != DVR_INVALID_PID &&
        p_ctx->streams[i].is_start == DVR_FALSE) {
      //need start pid filter
      fd = p_ctx->streams[i].fid;
      fcntl(fd, F_SETFL, O_NONBLOCK);
      memset(&params, 0, sizeof(params));
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

ssize_t record_device_read_ext(Record_DeviceHandle_t handle, size_t *buf, size_t *len)
{
  Record_DeviceContext_t *p_ctx;
  int result;
  int sid;
  struct dvr_mem_info info;

  p_ctx = (Record_DeviceContext_t *)handle;
  DVR_RETURN_IF_FALSE(p_ctx);
  DVR_RETURN_IF_FALSE(buf);
  DVR_RETURN_IF_FALSE(len);
  DVR_RETURN_IF_FALSE(p_ctx->dvr_buf);
  DVR_RETURN_IF_FALSE(p_ctx->output_handle);

  sid = p_ctx->fend_dev_id;
  pthread_mutex_lock(&p_ctx->lock);

  /* wp_offset is hw write pointer shared by multiple recordings under one sid,
   * must use mutex for thread safe
   */
  pthread_mutex_lock(&secdmx_lock[sid]);
  memset(&info, 0, sizeof(info));
  result = ioctl(p_ctx->fd, DMX_GET_DVR_MEM, &info);
  //DVR_DEBUG(1, "sid[%d] fd[%d] wp:%#x\n", sid, p_ctx->fd, info.wp_offset);
  if (result == DVR_SUCCESS) {
    if (SECDMX_ProcessData_Ptr != NULL)
      result = SECDMX_ProcessData_Ptr(sid, info.wp_offset);
  }
  if (result) {
    DVR_DEBUG(1, "result:%#x\n", result);
  }
  pthread_mutex_unlock(&secdmx_lock[sid]);

  DVR_RETURN_IF_FALSE_WITH_UNLOCK(result == DVR_SUCCESS, &p_ctx->lock);
  if (SECDMX_GetOutputBufferStatus_Ptr != NULL)
    result = SECDMX_GetOutputBufferStatus_Ptr(p_ctx->output_handle, buf, len);
  //DVR_DEBUG(1, "addr:%#x, len:%#x\n", *buf, *len);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(result == DVR_SUCCESS, &p_ctx->lock);

  pthread_mutex_unlock(&p_ctx->lock);
  return *len;
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

  if (dvr_check_dmx_isNew() == 1) {
    //new dmx drive,used io to set sec buf.
    int result;
    size_t dvr_buf;
    size_t op_handle;
    int sid = p_ctx->fend_dev_id;
    int fd;
    char node[32] = {0};
    memset(node, 0, sizeof(node));
    snprintf(node, sizeof(node), "/dev/dvb0.demux%d", p_ctx->dmx_dev_id);
    fd = open(node, O_RDONLY);
    if (SECDMX_AllocateDVRBuffer_Ptr != NULL) {
	for (i = 0; i < MAX_RECORD_DEVICE_COUNT; i++) {
	  if (record_ctx[i].state != RECORD_DEVICE_STATE_CLOSED &&
	      &record_ctx[i] != p_ctx &&
	      record_ctx[i].fend_dev_id == p_ctx->fend_dev_id &&
	      record_ctx[i].dvr_buf != 0) {
		break;
	  }
	}
	if (i >= MAX_RECORD_DEVICE_COUNT) {
	  result = SECDMX_AllocateDVRBuffer_Ptr(sid, len, &dvr_buf);
	  DVR_RETURN_IF_FALSE_WITH_UNLOCK(result == DVR_SUCCESS, &p_ctx->lock);
	} else {
	  dvr_buf = record_ctx[i].dvr_buf;
	}

	p_ctx->dvr_buf = dvr_buf;
    } else {
	p_ctx->dvr_buf = (size_t)sec_buf;
    }

    struct dmx_sec_mem sec_mem;
    sec_mem.buff = p_ctx->dvr_buf;
    sec_mem.size = len;
    if (ioctl(fd, DMX_SET_SEC_MEM, &sec_mem) == -1) {
      DVR_DEBUG(1, "record_device_set_secure_buffer ioctl DMX_SET_SEC_MEM error:%d", errno);
    }
    else
    {
      DVR_DEBUG(1, "record_device_set_secure_buffer ioctl sucesss DMX_SET_SEC_MEM: fd:%d, buf:%#x\n", fd, p_ctx->dvr_buf);
    }
    if (SECDMX_AddOutputBuffer_Ptr != NULL)
      result = SECDMX_AddOutputBuffer_Ptr(sid, (size_t)sec_buf, len, &op_handle);
    DVR_RETURN_IF_FALSE_WITH_UNLOCK(result == DVR_SUCCESS, &p_ctx->lock);
    p_ctx->output_handle = op_handle;

    pthread_mutex_unlock(&p_ctx->lock);
    return DVR_SUCCESS;
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

