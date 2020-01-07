#include <stdio.h>
#include "playback_device.h"

int playback_device_open(Playback_DeviceHandle *p_handle, Playback_DeviceOpenParams *params)
{
  return 0;
}

int playback_device_close(Playback_DeviceHandle handle)
{
  return 0;
}

int playback_device_audio_start(Playback_DeviceHandle handle, Playback_AudioParams *param)
{
  return 0;
}

int playback_device_audio_stop(Playback_DeviceHandle handle)
{
  return 0;
}

int playback_device_video_start(Playback_DeviceHandle handle, Playback_VideoParams *param)
{
  return 0;
}

int playback_device_video_stop(Playback_DeviceHandle handle)
{
  return 0;
}

int playback_device_pause(Playback_DeviceHandle handle)
{
  return 0;
}

int playback_device_resume(Playback_DeviceHandle handle)
{
  return 0;
}

int playback_device_seek(Playback_DeviceHandle handle, time_t time)
{
  return 0;
}

int playback_device_set_speed (Playback_DeviceHandle handle, Playback_Speed speed)
{
  return 0;
}

ssize_t playback_device_write (Playback_DeviceHandle handle, void *buf, size_t len, int timeout)
{
  return 0;
}
