#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>


#include <amports/amstream.h>

#include "dvr_common.h"

#include "playback_device.h"

#define DVB_STB_DEMUXSOURCE_FILE  "/sys/class/stb/demux%d_source"
#define STREAM_TS_FILE                          "/dev/amstream_mpts"
#define STREAM_TS_SCHED_FILE             "/dev/amstream_mpts_sched"
#define DVB_STB_SOURCE_FILE               "/sys/class/stb/source"
#define AMVIDEO_FILE                               "/dev/amvideo"
#define TRICKMODE_NONE      0x00
#define TRICKMODE_I         0x01
#define TRICKMODE_FFFB      0x02
#define TRICK_STAT_DONE     0x01
#define TRICK_STAT_WAIT     0x00



#define PLAYBACK_DEV_COUNT 1
static PlayBack_Device_t playback_devices[PLAYBACK_DEV_COUNT] =
{
    {
        .isopen= 0
    }
};

/**\brief write cmd to file
 * \param[in] name file name
 * \param[in] cmd writed cmd
 * \return
 *   - DVR_SUCCESS
  */
int _DVR_FileEcho(const char *name, const char *cmd)
{
	int fd, len, ret;

	fd = open(name, O_WRONLY);
	if (fd == -1)
	{
		DVR_DEBUG(1, "cannot open file \"%s\"", name);
		return DVR_FAILURE;
	}

	len = strlen(cmd);

	ret = write(fd, cmd, len);
	if (ret != len)
	{
		DVR_DEBUG(1, "write failed file:\"%s\" cmd:\"%s\" error:\"%s\"", name, cmd, strerror(errno));
		close(fd);
		return DVR_FAILURE;
	}

	close(fd);

	return DVR_SUCCESS;
}

/**\brief read from file
 * \param[in] name file name
 * \param[out] buf store buf
 * \param len buf len
 * \return
 *   - DVR_SUCCESS
 */
int _DVR_FileRead(const char *name, char *buf, int len)
{
	FILE *fp;
	char *ret;

	fp = fopen(name, "r");
	if (!fp)
	{
		DVR_DEBUG(1, "cannot open file \"%s\"", name);
		return DVR_FAILURE;
	}

	ret = fgets(buf, len, fp);
	if (!ret)
	{
		DVR_DEBUG(1, "read the file:\"%s\" error:\"%s\" failed", name, strerror(errno));
	}

	fclose(fp);

	return ret ? DVR_SUCCESS : DVR_FAILURE;
}


static DVR_Bool_t _check_vfmt_support_sched(PlayBack_DeviceVFormat_t vfmt)
{

  if (vfmt == VFORMAT_MPEG12 ||
      vfmt == VFORMAT_H264 ||
      vfmt == VFORMAT_HEVC ||
      vfmt == VFORMAT_MJPEG ||
      vfmt == VFORMAT_MPEG4 ||
      vfmt == VFORMAT_VP9 ||
      vfmt == VFORMAT_AVS)
    return DVR_TRUE;
  else
    return DVR_FALSE;
}

/**\brief Open an playback device
 * \param[out] p_handle playback device addr
 * \param[in] params Device open parameters
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_open(PlayBack_DeviceHandle_t *p_handle, PlayBack_DeviceOpenParams_t *params) {

  PlayBack_Device_t *dev;
  int i =0;
  int finddev = 0;
  char buf[64] = {0};
  //find one palyback dev to playback stream
  for (i = 0; i < PLAYBACK_DEV_COUNT; i++) {
      dev = &playback_devices[i];
      if (dev->isopen) {
         continue;
      } else {
         DVR_DEBUG(1, "%s, open playback dev", __func__);
         finddev = 1;
         break;
      }
  }
  if (finddev == 0) {
      DVR_DEBUG(1, "%s,not find free playback dev", __func__);
      return DVR_FAILURE;
  }
  dev->isopen = 1;
  dev->fd = open(STREAM_TS_FILE, O_RDWR);
  //dev->params
  memcpy(&dev->params, params, sizeof(PlayBack_DeviceOpenParams_t));
  //change and store dmx source
  _DVR_FileRead(DVB_STB_SOURCE_FILE, dev->last_stb_src, 16);
  snprintf(buf, sizeof(buf), "dmx%d", dev->params.dmx);
  _DVR_FileEcho(DVB_STB_SOURCE_FILE, buf);
  //change and store stb source
  snprintf(buf, sizeof(buf), DVB_STB_DEMUXSOURCE_FILE, dev->params.dmx);
  _DVR_FileRead(buf, dev->last_dmx_src, 16);
  _DVR_FileEcho(buf, "hiu");
  *p_handle = &playback_devices[i];
  return DVR_SUCCESS;
}

/**\brief Close an playback device
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_close(PlayBack_DeviceHandle_t handle) {

  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;
  char buf[128];
  dev->isopen = 0;
  if (dev->fd != -1)
  {
      DVR_DEBUG(1, "Closing mpts");
      close(dev->fd);
      dev->fd = -1;
  }
  _DVR_FileEcho(DVB_STB_SOURCE_FILE, dev->last_stb_src);
  snprintf(buf, sizeof(buf), DVB_STB_DEMUXSOURCE_FILE, dev->params.dmx);
  _DVR_FileEcho(buf, dev->last_dmx_src);
  return DVR_SUCCESS;
}


/**\brief Start play audio
 * \param[in] p_handle playback device
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_audio_start(PlayBack_DeviceHandle_t handle, PlayBack_DeviceAudioParams_t *param) {

  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;
  int val = 0;

  val = param->fmt;
  if (ioctl(dev->fd, AMSTREAM_IOC_AFORMAT, val) == -1)
  {
      DVR_DEBUG(1, "set audio format failed");
      return DVR_PLAYBACK_ERR_SYS;
  }

  val = param->pid;
  if (ioctl(dev->fd, AMSTREAM_IOC_AID, val) == -1)
  {
      DVR_DEBUG(1, "set audio PID failed");
      return DVR_PLAYBACK_ERR_SYS;
  }
  //start play
  if (ioctl(dev->fd, AMSTREAM_IOC_PORT_INIT, 0) == -1)
  {
      DVR_DEBUG(1, "amport init failed");
      return DVR_PLAYBACK_ERR_SYS;
  }
  DVR_DEBUG(1, "set ts skipbyte to 0");
  if (ioctl(dev->fd, AMSTREAM_IOC_TS_SKIPBYTE, 0) == -1)
  {
      DVR_DEBUG(1, "set ts skipbyte failed");
      return DVR_PLAYBACK_ERR_SYS;
  }
  //audio codec satrt
  return DVR_SUCCESS;
}

/**\brief Stop play audio
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_audio_stop(PlayBack_DeviceHandle_t handle) {

  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;

  if (dev->fd != -1)
      close(dev->fd);
  if (dev->vid_fd != -1)
      close(dev->vid_fd);
  //adec_stop_decode
  dev->adec_start = 0;
  dev->has_audio = 0;
  return DVR_SUCCESS;
}

/**\brief Start play video
 * \param[in] p_handle playback device
 * \param[in] params video playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_video_start(PlayBack_DeviceHandle_t handle, PlayBack_DeviceVideoParams_t *param) {

  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;
  int val = 0;

  if (dev->fd < 0 || dev->isopen == 0) {
      DVR_DEBUG(1, "play back dev not open");
      return 0;
  }

  DVR_DEBUG(1, "Openning video");
  //used to pause seek cmd
  dev->vid_fd = open(AMVIDEO_FILE, O_RDWR);
  if (dev->vid_fd == -1)
  {
      DVR_DEBUG(1, "cannot create data source \"/dev/amvideo\"");
      return DVR_PLAYBACK_ERR_SYS;
  }

  val = param->fmt;
  if (ioctl(dev->fd, AMSTREAM_IOC_VFORMAT, val) == -1)
  {
      DVR_DEBUG(1, "set video format failed");
      return DVR_PLAYBACK_ERR_SYS;
  }

  val = param->pid;
  if (ioctl(dev->fd, AMSTREAM_IOC_VID, val) == -1)
  {
      DVR_DEBUG(1, "set video PID failed");
      return DVR_PLAYBACK_ERR_SYS;
  }
  return DVR_SUCCESS;
}

/**\brief Stop play video
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_video_stop(PlayBack_DeviceHandle_t handle) {

  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;

  if (dev->vid_fd == -1) {
     return 0;
  }
  DVR_DEBUG(1, "Closing video");
  if (dev->vid_fd != -1)
      close(dev->vid_fd);
  dev->vid_fd = -1;
  return DVR_SUCCESS;
}

/**\brief Pause play
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_pause(PlayBack_DeviceHandle_t handle) {

  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;
  if (dev->has_audio && dev->adec_start) {
      //adec_pause_decode
  }
  if (dev->vid_fd > 0) {
      ioctl(dev->vid_fd, AMSTREAM_IOC_VPAUSE, 1);
  }
  return DVR_SUCCESS;
}

/**\brief Resume play
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_resume(PlayBack_DeviceHandle_t handle) {
    PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;
    if (dev->has_audio && dev->adec_start) {
        //adec_resume_decode
    }
    if (dev->vid_fd > 0) {
        ioctl(dev->vid_fd, AMSTREAM_IOC_VPAUSE, 0);
    }
    return DVR_SUCCESS;
}

/**\brief Set play speed
 * \param[in] p_handle playback device
 * \param[in] speed playback speed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_set_speed(PlayBack_DeviceHandle_t handle, PlayBack_DeviceSpeeds_t speed) {
  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;

  return DVR_SUCCESS;
}

/**\brief write ts data to playback device
 * \param[in] p_handle playback device
 * \param[in] buf inject ts data.
 * \param[in] len inject buf ts data len.
 * \param[in] timeout inject timeout.
 * \retval had writed data len
 * \return writed len
 */
ssize_t playback_device_write(PlayBack_DeviceHandle_t handle, PlayBack_DeviceWBufs_t *bufs) {
  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;
  int ret;
  int real_written = 0;
  int fd = dev->fd;
  uint8_t *data = bufs->buf;
  int size = bufs->len;
  int timeout = bufs->timeout;

  if (timeout >= 0)
  {
      struct pollfd pfd;

      pfd.fd = fd;
      pfd.events = POLLOUT;

      ret = poll(&pfd, 1, timeout);
      if (ret != 1)
      {
          DVR_DEBUG(1, "timeshift poll timeout");
          goto inject_end;
      }
  }

  if (size && fd > 0)
  {
      ret = write(fd, data, size);
      if ((ret == -1) && (errno != EAGAIN))
      {
          DVR_DEBUG(1, "inject data failed errno:%d msg:%s", errno, strerror(errno));
          goto inject_end;
      }
      else if ((ret == -1) && (errno == EAGAIN))
      {
          DVR_DEBUG(1, "ret=%d,inject data failed errno:%d msg:%s",ret, errno, strerror(errno));
          real_written = 0;
      }
      else if (ret >= 0)
      {
          real_written = ret;
      }
  }
inject_end:
  return real_written;
}

/**Miute/unmute the audio output.*/
int playback_device_mute_audio (PlayBack_DeviceHandle_t handle, int mute) {

  return 0;
}

/**Miute/unmute the video output.*/
int playback_device_mute_video (PlayBack_DeviceHandle_t handle, int mute) {

  return 0;
}

int playback_device_trick_mode (PlayBack_DeviceHandle_t handle, int set) {
  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;
  if (set == 0) {
      //clear
      if (dev->vid_fd > 0) {
          ioctl(dev->vid_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
      }
  } else {
      //set trick mode
      if (dev->vid_fd > 0) {
          ioctl(dev->vid_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_FFFB);
      }
  }
  return 0;
}

int playback_device_get_trick_stat(PlayBack_DeviceHandle_t handle)
{
  int state;
  PlayBack_Device_t *dev = (PlayBack_Device_t *) handle;

  if (dev->vid_fd == -1)
      return -1;

  ioctl(dev->vid_fd, AMSTREAM_IOC_TRICK_STAT, (unsigned long)&state);

  return state;
}


