#ifndef _DVR_RECORD_H_
#define _DVR_RECORD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_common.h"
#include "dvr_types.h"

typedef uint32_t DVR_RecordHandle_t;

typedef enum {
  DVR_RECORD_STATE_OPENED,
  DVR_RECORD_STATE_STARTED,
  DVR_RECORD_STATE_STOPPED,
  DVR_RECORD_STATE_CLOSED,
} DVR_RecordState;

typedef enum
{
  DVR_RECORD_PID_CREATE,
  DVR_PID_KEEP,
  DVR_PID_CLOSE
} DVR_RecordPidAction_t;

#if 0
struct HAL_PVR_Param_s {
  PVR_Record_Open_Params_t open_params;
  PVR_Record_Notify_Fn_t   func;
  PVR_Size_v2_t            notification_size;
  char                     root_location[PVR_MAX_LOCATION_SIZE];
};

struct HAL_PVR_Chunk_s {
  PVR_Chunk_Info_t  info;
  PVR_Chunk_ID_t    chunk_id;
  u32_t             nb_pids;
  PVR_PID_t         pids[PVR_MAX_UPDATED_RECORD_PIDS];
  PVR_PID_Action_t  update_info[PVR_MAX_UPDATED_RECORD_PIDS];
  //int               state;
  char              fname[HAL_PVR_MAX_LOCATION_SIZE];
  struct list_head  list;
};

struct HAL_PVR_Device_s {
  int dev_no;
  int state;
  struct HAL_PVR_Param_s param;
  struct list_head head;
  pthread_mutex_t lock;
  pthread_t thread;
  int current_chunk_id;
};
#endif

typedef enum {
  DVR_RECORD_SOURCE_MEMORY,
  DVR_RECORD_SOURCE_DEMUX_0,
  DVR_RECORD_SOURCE_DEMUX_1,
  DVR_RECORD_SOURCE_DEMUX_2
} DVR_RecordSource_t;

typedef enum {
  DVR_RECORD_FLAG_SCRAMBLED = (1 << 0),
  DVR_RECORD_FLAG_ACCURATE  = (1 << 1),
} DVR_RecordFlag_t;

typedef enum {
  DVR_CRYPTO_PARITY_CLEAR,
  DVR_CRYPTO_PARITY_ODD,
  DVR_CRYPTO_PARITY_EVEN,
} DVR_CryptoParity_t;

typedef enum {
  DVR_CRYPTO_FILTER_TYPE_AUDIO,
  DVR_CRYPTO_FILTER_TYPE_VIDEO,
} DVR_CryptoFilterType_t;

typedef struct
{
  DVR_Bool_t              transition;
  DVR_CryptoParity_t      parity;
  uint32_t                ts_offset;
  DVR_CryptoFilterType_t  filter_type;
} DVR_CryptoPeriodInfo_t;

typedef void (*DVR_CryptoPeriodNotifyFn_t)(
                                        DVR_RecordHandle_t            handle,
                                        const DVR_CryptoPeriodInfo_t  *p_info);

typedef struct {
  DVR_CryptoPeriodNotifyFn_t    notify_func;
  uint64_t                      interval_bytes;
  DVR_Bool_t                    notify_clear_periods;
} DVR_CryptoPeriod_t;;

typedef struct {
  DVR_RecordSource_t src_type;
  DVR_RecordFlag_t   flags;
  DVR_CryptoPeriod_t crypto_period;
} DVR_RecordOpenParams_t;

typedef struct {
  uint64_t segment_id;
  uint32_t nb_pids;
  DVR_PidInfo_t pids[DVR_MAX_RECORD_PIDS_COUNT];
  DVR_RecordPidAction_t pid_action[DVR_MAX_RECORD_PIDS_COUNT];
} DVR_RecordSegmentStartParams_t;

typedef struct {
  char location[DVR_MAX_LOCATION_SIZE];
  DVR_RecordSegmentStartParams_t segment;
} DVR_RecordStartParams_t;

typedef struct {
} DVR_RecordSegmentInfo_t;

/**\brief Open a recording session for a target giving some open parameters
 * \param[out] p_handle Return the handle of the newly created dvr session
 * \param[in] params Open parameters
 * \return DVR_SUCCESS on success
 * \return error code
 */
int dvr_record_open(DVR_RecordHandle_t *p_handle, DVR_RecordOpenParams_t *params);

/**\brief Close a recording session
 * \param[in] handle Dvr recording session handle
 * \return DVR_SUCCESS on success
 * \return error code
 */
int dvr_record_close(DVR_RecordHandle_t handle);

int dvr_record_set_encrypt();
/**\brief Start recording on a segment
 * \param[in] handle Dvr recording session handle
 * \param[in] params Dvr start parameters
 * \return DVR_SUCCESS on success
 * \return error code
 */
int dvr_record_start_segment(DVR_RecordHandle_t handle, DVR_RecordStartParams_t *params);

/**\brief Stop the ongoing segment and start recording a new segment
 * \param[in] handle Dvr recording session handle
 * \param[in] params Dvr start parameters
 * \param[out] p_info
 * \return DVR_SUCCESS on success
 * \return error code
 */
int dvr_record_next_segment(DVR_RecordHandle_t handle, DVR_RecordStartParams_t *params, DVR_RecordSegmentInfo_t *p_info);

/**\brief Stop the ongoing segment
 * \param[in] handle Dvr recording session handle
 * \param[out] p_info
 * \return DVR_SUCCESS on success
 * \return error code
 */
int dvr_record_stop_segment(DVR_RecordHandle_t handle, DVR_RecordSegmentInfo_t *p_info);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_RECORD_H_*/
