#ifndef _PLAYBACK_DEVICE_H_
#define _PLAYBACK_DEVICE_H_
#include <amports/aformat.h>
#include <amports/vformat.h>
#include <amports/amstream.h>


#ifdef __cplusplus
extern "C" {
#endif

#define DVR_PLAYBACK_ERR_SYS 1

typedef void* Playback_DeviceHandle;
/**\brief playback device open information*/
typedef struct Playback_DevideOpenParams_s {
    int dmx;    /**< playback used dmxid*/
} Playback_DeviceOpenParams_t;

/**\brief playback speed*/
typedef enum
{
  PlayBack_Speed_S16,         /**<slow 1/16 speed*/
  PlayBack_Speed_S8,          /**<slow 1/8 speed*/
  PlayBack_Speed_S4,          /**<slow 1/4 speed*/
  PlayBack_Speed_S2,          /**<slow 1/2 speed*/
  PlayBack_Speed_X1,          /**< X 1 normal speed*/
  PlayBack_Speed_X2,          /**< X 2 speed*/
  PlayBack_Speed_X4,          /**< X 4 speed*/
  PlayBack_Speed_X8,          /**< X 8 speed*/
  PlayBack_Speed_X16,         /**< X 16 speed*/
  PlayBack_Speed_X32,         /**< X 32 speed*/
  PlayBack_Speed_MAX,
} PlayBack_Device_Speed_t;

/**\brief playback speed*/
typedef struct Playback_Device_Speeds_s {
  PlayBack_Device_Speed_t speed; /**< playback speed*/
} Playback_Device_Speeds_t;

/**
 * Audio format
 *
 * detail definition in "linux/amlogic/amports/aformat.h"
 */
typedef aformat_t Playback_Device_AFormat_t;

/**
 * Video format
 *
 * detail definition in "linux/amlogic/amports/vformat.h"
 */
typedef vformat_t Playback_Device_VFormat_t;

/**\brief start playback audio params*/
typedef struct Playback_AudioParams_s {
  Playback_Device_AFormat_t fmt; /**< audio fmt*/
  int                   pid; /**< audio pid*/
} Playback_Device_AudioParams_t;

/**\brief start playback video params*/
typedef struct Playback_VideoParams_s {
  Playback_Device_VFormat_t   fmt; /**< video fmt*/
  int                     pid; /**< video pid*/
} Playback_Device_VideoParams_t;

/**\brief start playback video params*/
typedef struct Playback_WBufs_s {
  void *buf;  /**< ts buf*/
  size_t len; /**< buf len*/
  int timeout; /**< write timeout*/
  int  flag;  /**< encreamble or not*/
} Playback_Device_WBufs_t;

typedef struct PlayBack_Devices {
  int    dev_no;         /**< device no*/
  Playback_DeviceOpenParams_t params; /**< device open params*/
  int    fd;             /**< amstream fd*/
  int    isopen;         /**< device is opend*/
  char   last_stb_src[16]; /**< device last stb source*/
  char   last_dmx_src[16]; /**< device last dmx source*/
  int    vid_fd;          /**< device video dev fd*/
  int    has_audio;       /**< device is has audio to play*/
  int    adec_start;      /**< device adec is start*/
} PlayBack_Device;


/**\brief Open an palyback device
 * \param[out] p_handle playback device addr
 * \param[in] params Device open parameters
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_open(Playback_DeviceHandle *p_handle, Playback_DeviceOpenParams_t *params);

/**\brief Close an palyback device
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_close(Playback_DeviceHandle handle);

/**\brief Start play audio
 * \param[in] p_handle playback device
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_audio_start(Playback_DeviceHandle handle, Playback_Device_AudioParams_t *param);

/**\brief Stop play audio
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_audio_stop(Playback_DeviceHandle handle);

/**\brief Start play video
 * \param[in] p_handle playback device
 * \param[in] params video playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_video_start(Playback_DeviceHandle handle, Playback_Device_VideoParams_t *param);

/**\brief Stop play video
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_video_stop(Playback_DeviceHandle handle);

/**\brief Pause play
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_pause(Playback_DeviceHandle handle);

/**\brief Resume play
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_resume(Playback_DeviceHandle handle);

/**\brief Set play speed
 * \param[in] p_handle playback device
 * \param[in] speed playback speed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_set_speed(Playback_DeviceHandle handle, Playback_Device_Speeds_t speed);

/**\brief write ts data to playback device
 * \param[in] p_handle playback device
 * \param[in] buf inject ts data.
 * \param[in] len inject buf ts data len.
 * \param[in] timeout inject timeout.
 * \retval had writed data len
 * \return writed len
 */
ssize_t playback_device_write(Playback_DeviceHandle handle, Playback_Device_WBufs_t *bufs);

/**\brief mute audio out put
 * \param[in] p_handle playback device
 * \param[in] mute mute or not
 * \return 0
 */
int playback_device_mute_audio (Playback_DeviceHandle handle, int mute);

/**\brief mute video out put
 * \param[in] p_handle playback device
 * \param[in] mute mute or not
 * \return 0
 */
int playback_device_mute_video (Playback_DeviceHandle handle, int mute);

/**\brief set video trick mode
 * \param[in] p_handle playback device
 * \param[in] set or clear mode
 * \return 0
 */
int playback_device_trick_mode (Playback_DeviceHandle handle, int set);

/**\brief set video trick mode
 * \param[in] p_handle playback device
 * \param[in] set or clear mode
 * \return 0
*/
int playback_device_get_trick_stat(Playback_DeviceHandle handle);


#ifdef __cplusplus
}
#endif

#endif /*END _PLAYBACK_DEVICE_H_*/
