#ifndef _PLAYBACK_DEVICE_H_
#define _PLAYBACK_DEVICE_H_
#include <amports/aformat.h>
#include <amports/vformat.h>
#include <amports/amstream.h>
#include "dvr_types.h"


#ifdef __cplusplus
extern "C" {
#endif

#define DVR_PLAYBACK_ERR_SYS 1

typedef void* Playback_DeviceHandle_t;
/**\brief playback device open information*/
typedef struct Playback_DevideOpenParams_s {
    int dmx;    /**< playback used dmxid*/
} Playback_DeviceOpenParams_t;

/**\brief playback speed*/
typedef enum
{
  PLAYBACK_SPEED_S16 = 6,         /**<slow 1/16 speed*/
  PLAYBACK_SPEED_S8 = 12,          /**<slow 1/8 speed*/
  PLAYBACK_SPEED_S4 = 26,          /**<slow 1/4 speed*/
  PLAYBACK_SPEED_S2 = 50,          /**<slow 1/2 speed*/
  PLAYBACK_SPEED_X1 = 100,          /**< X 1 normal speed*/
  PLAYBACK_SPEED_X2 = 200,          /**< X 2 speed*/
  PLAYBACK_SPEED_X4 = 400,          /**< X 4 speed*/
  PLAYBACK_SPEED_X8 = 800,          /**< X 8 speed*/
  PLAYBACK_SPEED_X16 = 1600,         /**< X 16 speed*/
  PLAYBACK_SPEED_X32    = 3200,         /**< X 32 speed*/
  PlayBack_Speed_MAX,
} Playback_DeviceSpeed_t;

/**\brief playback speed*/
typedef struct Playback_DeviceSpeeds_s {
  Playback_DeviceSpeed_t speed; /**< playback speed*/
} Playback_DeviceSpeeds_t;


/**\brief start playback audio params*/
typedef struct PlayBack_AudioParams_s {
  int      fmt; /**< audio fmt*/
  int      pid; /**< audio pid*/
} Playback_DeviceAudioParams_t;

/**\brief start playback video params*/
typedef struct PlayBack_VideoParams_s {
  int   fmt; /**< video fmt*/
  int   pid; /**< video pid*/
} Playback_DeviceVideoParams_t;

/**\brief start playback video params*/
typedef struct PlayBack_WBufs_s {
  void *buf;      /**< ts buf*/
  size_t len;     /**< buf len*/
  int   timeout;  /**< write timeout*/
  int  flag;      /**< encreamble or not*/
} Playback_DeviceWBufs_t;

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
} Playback_Device_t;


/**\brief Open an palyback device
 * \param[out] p_handle playback device addr
 * \param[in] params Device open parameters
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_open(Playback_DeviceHandle_t *p_handle, Playback_DeviceOpenParams_t *params);

/**\brief Close an palyback device
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_close(Playback_DeviceHandle_t handle);

/**\brief Start play audio
 * \param[in] p_handle playback device
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_audio_start(Playback_DeviceHandle_t handle, Playback_DeviceAudioParams_t *param);

/**\brief Stop play audio
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_audio_stop(Playback_DeviceHandle_t handle);

/**\brief Start play video
 * \param[in] p_handle playback device
 * \param[in] params video playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_video_start(Playback_DeviceHandle_t handle, Playback_DeviceVideoParams_t *param);

/**\brief Stop play video
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_video_stop(Playback_DeviceHandle_t handle);

/**\brief Pause play
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_pause(Playback_DeviceHandle_t handle);

/**\brief Resume play
 * \param[in] p_handle playback device
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_resume(Playback_DeviceHandle_t handle);

/**\brief Set play speed
 * \param[in] p_handle playback device
 * \param[in] speed playback speed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int playback_device_set_speed(Playback_DeviceHandle_t handle, Playback_DeviceSpeeds_t speed);

/**\brief write ts data to playback device
 * \param[in] p_handle playback device
 * \param[in] buf inject ts data.
 * \param[in] len inject buf ts data len.
 * \param[in] timeout inject timeout.
 * \retval had writed data len
 * \return writed len
 */
ssize_t playback_device_write(Playback_DeviceHandle_t handle, Playback_DeviceWBufs_t *bufs);

/**\brief mute audio out put
 * \param[in] p_handle playback device
 * \param[in] mute mute or not
 * \return 0
 */
int playback_device_mute_audio (Playback_DeviceHandle_t handle, int mute);

/**\brief mute video out put
 * \param[in] p_handle playback device
 * \param[in] mute mute or not
 * \return 0
 */
int playback_device_mute_video (Playback_DeviceHandle_t handle, int mute);

/**\brief set video trick mode
 * \param[in] p_handle playback device
 * \param[in] set or clear mode
 * \return 0
 */
int playback_device_trick_mode (Playback_DeviceHandle_t handle, int set);

/**\brief set video trick mode
 * \param[in] p_handle playback device
 * \param[in] set or clear mode
 * \return 0
*/
int playback_device_get_trick_stat(Playback_DeviceHandle_t handle);


#ifdef __cplusplus
}
#endif

#endif /*END _PLAYBACK_DEVICE_H_*/
