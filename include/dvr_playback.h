#ifndef DVR_PLAYBACK_H_
#define DVR_PLAYBACK_H_
#include <amports/aformat.h>
#include <amports/vformat.h>
#include "list.h"
#include "dvr_common.h"
#include "segment.h"
#include "playback_device.h"
#ifdef __cplusplus
extern "C" {
#endif
/**\brief dvr play chunk flag */
typedef enum
{
  DVR_PLAY_ENCRYPTED = (1 << 0), /**< encrypted stream */
  DVR_PLAY_DISPLAYABLE = (1 << 1), /**< displayable stream */
  DVR_PLAY_CONTINUOUS = (1 << 2)  /**< continuous stream with pre one */
} DVR_PlayBackChunkFlag_t;

/**\brief dvr pid type*/
typedef struct
{
  int   pid; /**< stream pid */
  int   fmt; /**< stream fmt */
} DVR_PlayBackStreamParams_t;

/**\brief dvr pid type*/
typedef enum
{
  DVR_PLAY_SYNC,    /**< sync play mode */
  DVR_PLAY_ASYNC    /**< out of sync play mode */
} DVR_PlayBackSyncMode_t;


/**\brief dvr play pids */
typedef struct
{
  DVR_PlayBackStreamParams_t vpid;       /**< video pid */
  DVR_PlayBackStreamParams_t apid;       /**< audio pid */
  DVR_PlayBackStreamParams_t pcrpid;     /**< pcr pid */
  DVR_PlayBackStreamParams_t sub_apid;   /**< sub audio pid */
} DVR_PlayBackPids_t;

/**\brief dvr chunk info */
typedef struct
{
  struct list_head head;                    /**< chunk list head */
  int chunk_id;                             /**< chunk id */
  uint8_t location[DVR_MAX_LOCATION_SIZE];  /**< chunk location */
  DVR_PlayBackPids_t  pids;                    /**< chunk cons pids */
  DVR_PlayBackChunkFlag_t flags;             /**< chunk flag */
  int key_data_id;                          /**< ??? */
} DVR_PlayBackChunkInfo_t;

/**\brief play flag, if set this flag, player need pause when decode first frame */
typedef enum
{
  DVR_PLAY_STARTED_PAUSEDLIVE = (1 << 0)      /**< dvr play stat,need change to pause state if set */
} DVR_PlayBackFlag_t;


/**\brief playback speed mode*/
typedef enum
{
  DVR_PLAY_FAST_FORWARD = 0,        /**< fast forward */
  DVR_PLAY_FAST_BACKWARD = 1,       /**< fast backward */
} DVR_PlayBackSpeedMode_t;


/**\brief playback play status*/
typedef struct
{
  PlayBack_DeviceSpeeds_t speed;  /**< playback speed */
  DVR_PlayBackSpeedMode_t mode;   /**< playback 0: fast forword or 1: fast backword*/
} DVR_PlayBackSpeed_t;

#define DVR_MAX_SUPPORTED_SPEEDS  32
/**\brief playback capability*/
typedef struct
{
  int nb_supported_speeds;       /**< support playback speed count*/
  int supported_speeds[DVR_MAX_SUPPORTED_SPEEDS]; /**< support playback speed*/
} DVR_PlayBackCapability_t;

typedef void* DVR_PlayBackHandle_t;

/**\brief playback play state*/
typedef enum
{
  DVR_PlayBack_State_Unint,       /**< uninit state */
  DVR_PlayBack_State_Init,        /**< init state, open device */
  DVR_PlayBack_State_Start,       /**< start play */
  DVR_PlayBack_State_Stop,        /**< stop */
  DVR_PlayBack_State_Pause,       /**< pause */
  DVR_PlayBack_State_FF,          /**< fast forward */
  DVR_PlayBack_State_FB,           /**< fast backword */
} DVR_PlayBackPlayState_t;

/**\brief playback play status*/
typedef struct
{
  DVR_PlayBackPlayState_t state;  /**< playback play state */
  int chunk_id;                     /**< playback ongoing chunkid */
  int time_cur;                     /**< playback cur time,0 <--> time_end Ms*/
  int time_end;                     /**< playback ongoing chunk dur,Ms */
  DVR_PlayBackPids_t pids;         /**< playback played pids */
  int speed;                        /**< playback played speed */
  DVR_PlayBackChunkFlag_t flags;  /**< playback played chunk flag */
} DVR_PlayBackStatus_t;

/**\brief playback play params*/
typedef struct
{
  int             vpid;       /**< video pid*/
  int             vfmt;       /**< video fmt*/
  int             apid;       /**< audio pid*/
  int             afmt;       /**< audio fmt*/
  int             sub_apid;   /**< sub audio pid*/
  int             sub_afmt;   /**< sub audio fmt*/
  int             pcr_pid;     /**< pcr pid*/
} DVR_PlayBackPlayParams_t;

/**\brief playback open params*/
typedef struct
{
  int             dmx; /**< playback used dmx device index*/
  int             blocksize; /**< playback inject block size*/
  int             is_timeshift;/**< 0:playback mode, 1 : is timeshift mode*/
} DVR_PlayBackOpenParams_t;

/**\brief playback play state*/
typedef enum
{
  DVR_PlayBack_Cmd_Start,        /**< start av */
  DVR_PlayBack_Cmd_Stop,        /**< stop av */
  DVR_PlayBack_Cmd_VStart,       /**< v start */
  DVR_PlayBack_Cmd_AStart,       /**< a start */
  DVR_PlayBack_Cmd_VStop,        /**< v stop */
  DVR_PlayBack_Cmd_AStop,        /**< a stop */
  DVR_PlayBack_Cmd_VReStart,       /**<v restart */
  DVR_PlayBack_Cmd_AReStart,       /**< a restart */
  DVR_PlayBack_Cmd_VStopAStart,        /**< v stop a start*/
  DVR_PlayBack_Cmd_AStopVStart,        /**< a stop vstart */
  DVR_PlayBack_Cmd_VStopAReStart,        /**<v stop a restart*/
  DVR_PlayBack_Cmd_AStopVReStart,        /**<a stop v restart*/
  DVR_PlayBack_Cmd_Pause,        /**< pause */
  DVR_PlayBack_Cmd_Resume,        /**< resume */
  DVR_PlayBack_Cmd_Seek,        /**< seek */
  DVR_PlayBack_Cmd_FF,           /**< fast forward */
  DVR_PlayBack_Cmd_FB,           /**< fast backword */
} DVR_PlayBackCmd_t;


/**\brief playback struct*/
typedef struct
{
  PlayBack_DeviceSpeeds_t   speed;     /**< play speed */
  DVR_PlayBackPlayState_t  state;     /**< plat state */
  DVR_PlayBackCmd_t         cur_cmd;    /**< cur send cmd */
  DVR_PlayBackCmd_t         last_cmd;   /**< last cmd */
  int                        pos;        /**< seek pos at cur chunk*/
} DVR_PlayBackCmdInfo_t;


/**\brief playback struct*/
typedef struct
{
  PlayBack_DeviceHandle_t     handle;             /**< device handle */
  int                       cur_chunkid;        /**< Current chunk id*/
  DVR_PlayBackChunkInfo_t  cur_chunk;          /**< Current playing chunk*/
  struct list_head          chunk_list;         /**< chunk list head*/
  pthread_t                 playback_thread;    /**< playback thread*/
  pthread_mutex_t           lock;               /**< playback lock*/
  pthread_cond_t            cond;               /**< playback cond*/
  void                      *user_data;         /**< playback userdata, used to send event*/
  DVR_PlayBackPlayParams_t   params;            /**< playback playparams,cont vpid vfm apid afmt...*/
  PlayBack_DeviceSpeeds_t    speed;           /**< playback speed*/
  DVR_PlayBackPlayState_t  state;           /**< playback state*/
  DVR_PlayBackFlag_t        play_flag;           /**< playback play flag*/
  int                        is_running;           /**< playback htread is runing*/
  DVR_PlayBackCmdInfo_t     cmd;           /**< playback cmd*/
  int                        offset;         /**< chunk read offset*/
  Segment_Handle_t           r_handle;           /**< playback current segment handle*/
  DVR_PlayBackOpenParams_t  openParams;           /**< playback openParams*/
  DVR_Bool_t                 has_video;    /**< has video playing*/
  DVR_Bool_t                 has_audio;    /**< has audio playing*/
} Dvr_PlayBack_t;

/**\brief Open an dvr palyback
 * \param[out] p_handle dvr playback addr
 * \param[in] params dvr playback open parameters
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_open(DVR_PlayBackHandle_t *p_handle, DVR_PlayBackOpenParams_t *params);

/**\brief Close an dvr palyback
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_close(DVR_PlayBackHandle_t handle);


/**\brief Start play audio and video, used start auido api and start video api
 * \param[in] handle playback handle
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_start(DVR_PlayBackHandle_t handle, DVR_PlayBackFlag_t flag);

/**\brief dvr play back add chunk info to chunk list
 * \param[in] handle playback handle
 * \param[in] info added chunk info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_add_chunk(DVR_PlayBackHandle_t handle, DVR_PlayBackChunkInfo_t *info);

/**\brief dvr play back remove chunk info by chunkid
 * \param[in] handle playback handle
 * \param[in] chunkid need removed chunk id
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_remove_chunk(DVR_PlayBackHandle_t handle, int chunkid);

/**\brief dvr play back add chunk info
 * \param[in] handle playback handle
 * \param[in] info added chunk info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_Update_Chunk_Flags(DVR_PlayBackHandle_t handle,
    int chunk_id,
    DVR_PlayBackChunkFlag_t flags);
/**\brief dvr play back up1date chunk pids
 * if updated chunk is ongoing chunk, we need start new
 * add pid stream and stop remove pid stream.
 * \param[in] handle playback handle
 * \param[in] chunk_id need updated pids chunk id
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_Update_Chunk_Pids(DVR_PlayBackHandle_t handle, int chunkid,
DVR_PlayBackPids_t *p_pids);

/**\brief Stop play, will stop video and audio
 * \param[in] handle playback handle
 * \param[in] clear is clear last frame
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_stop(DVR_PlayBackHandle_t handle, DVR_Bool_t clear);

/**\brief Start play audio
 * \param[in] handle playback handle
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_audio_start(DVR_PlayBackHandle_t handle, PlayBack_DeviceAudioParams_t *param);

/**\brief Stop play audio
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_audio_stop(DVR_PlayBackHandle_t handle);

/**\brief Start play video
 * \param[in] handle playback handle
 * \param[in] params video playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_start(DVR_PlayBackHandle_t handle, PlayBack_DeviceVideoParams_t *param);

/**\brief Stop play video
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_stop(DVR_PlayBackHandle_t handle);

/**\brief Pause play
 * \param[in] handle playback handle
 * \param[in] flush whether its internal buffers should be flushed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_pause(DVR_PlayBackHandle_t handle, DVR_Bool_t flush);


/**\brief seek
 * \param[in] handle playback handle
 * \param[in] time_offset time offset base cur chunk
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_seek(DVR_PlayBackHandle_t handle, int chunk_id, int time_offset);

/**\brief Set play speed
 * \param[in] handle playback handle
 * \param[in] speed playback speed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_set_speed(DVR_PlayBackHandle_t handle, PlayBack_DeviceSpeeds_t speed);

/**\brief Get playback status
 * \param[in] handle playback handle
 * \param[out] p_status playback status
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_get_status(DVR_PlayBackHandle_t handle, DVR_PlayBackStatus_t *p_status);

/**\brief Get playback capabilities
 * \param[out] p_capability playback capability
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_get_capabilities(DVR_PlayBackCapability_t *p_capability);

/**\brief dump chunkinfo throw print log
 * \param[int] handle playback
 * \param[int] chunk_id if chunk_id > 0, only dump this log. else dump all chunk info
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_dump_chunkinfo(DVR_PlayBackHandle_t handle, int chunk_id);

#ifdef __cplusplus
}
#endif

#endif /*END DVR_PLAYBACK_H_*/
