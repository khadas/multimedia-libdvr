#ifndef _PLAYBACK_DEVICE_H_
#define _PLAYBACK_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Playback_DeviceHandle;

typedef struct Playback_DevideOpenParams_s {
} Playback_DeviceOpenParams;

typedef struct Playback_Params_s {
} Playback_Params;

typedef struct Playback_Speed_s {
} Playback_Speed;

typedef struct Playback_AudioParams_s {
} Playback_AudioParams;

typedef struct Playback_VideoParams_s {
} Playback_VideoParams;

int playback_device_open(Playback_DeviceHandle *p_handle, Playback_DeviceOpenParams *params);

int playback_device_close(Playback_DeviceHandle handle);

int playback_device_audio_start(Playback_DeviceHandle handle, Playback_AudioParams *param);

int playback_device_audio_stop(Playback_DeviceHandle handle);

int playback_device_video_start(Playback_DeviceHandle handle, Playback_VideoParams *param);

int playback_device_video_stop(Playback_DeviceHandle handle);

int playback_device_pause(Playback_DeviceHandle handle);

int playback_device_resume(Playback_DeviceHandle handle);

int playback_device_seek(Playback_DeviceHandle handle, time_t time);

int playback_device_set_speed(Playback_DeviceHandle handle, Playback_Speed speed);

ssize_t playback_device_write(Playback_DeviceHandle handle, void *buf, size_t len, int timeout);

#ifdef __cplusplus
}
#endif

#endif /*END _PLAYBACK_DEVICE_H_*/
