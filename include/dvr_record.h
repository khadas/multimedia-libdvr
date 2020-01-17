#ifndef _DVR_RECORD_H_
#define _DVR_RECORD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_types.h"
#include "dvr_crypto.h"

typedef void* DVR_RecordHandle_t;

typedef enum {
  DVR_RECORD_STATE_OPENED,        /**< Record state is opened*/
  DVR_RECORD_STATE_STARTED,       /**< Record state is started*/
  DVR_RECORD_STATE_STOPPED,       /**< Record state is stopped*/
  DVR_RECORD_STATE_CLOSED,        /**< Record state is closed*/
} DVR_RecordState_t;

typedef enum
{
  DVR_RECORD_PID_CREATE,          /**< Create a new pid used to record*/
  DVR_RECORD_PID_KEEP,            /**< Indicate this pid keep last state*/
  DVR_RECORD_PID_CLOSE            /**< Close this pid record*/
} DVR_RecordPidAction_t;

typedef enum {
  DVR_RECORD_FLAG_SCRAMBLED = (1 << 0),
  DVR_RECORD_FLAG_ACCURATE  = (1 << 1),
} DVR_RecordFlag_t;

typedef enum {
  DVR_CRYPTO_PARITY_CLEAR,        /**< Current period is clear*/
  DVR_CRYPTO_PARITY_ODD,          /**< Current period is ODD*/
  DVR_CRYPTO_PARITY_EVEN,         /**< Current period is EVEN*/
} DVR_CryptoParity_t;

typedef enum {
  DVR_CRYPTO_FILTER_TYPE_AUDIO,   /**< Indicate current notification concerns audio packets*/
  DVR_CRYPTO_FILTER_TYPE_VIDEO,   /**< Indicate current notification concerns video packets*/
} DVR_CryptoFilterType_t;

typedef struct
{
  DVR_Bool_t              transition;     /**< DVR_TRUE is transition, DVR_FALSE is not transition. At the start of a recording this shall be set to DVR_TRUE*/
  DVR_CryptoParity_t      parity;         /**< The crypto parity at the ts_offset*/
  loff_t                  ts_offset;      /**< TS packet offset correspongding to this crypto period*/
  DVR_CryptoFilterType_t  filter_type;    /**< Indicate this notification concerns audio or video*/
} DVR_CryptoPeriodInfo_t;

#if 0
typedef void (*DVR_CryptoPeriodNotifyFn_t)(
                                        DVR_RecordHandle_t            handle,
                                        const DVR_CryptoPeriodInfo_t  *p_info);
#endif

typedef struct {
  //DVR_CryptoPeriodNotifyFn_t    notify_func;
  uint64_t                      interval_bytes;
  DVR_Bool_t                    notify_clear_periods;
} DVR_CryptoPeriod_t;;

typedef struct {
  int                  dmx_dev_id;
  DVR_RecordFlag_t     flags;
  DVR_CryptoPeriod_t   crypto_period;
  DVR_CryptoFunction_t crypto_fn;
  void                *crypto_data;
} DVR_RecordOpenParams_t;

typedef struct {
  uint64_t segment_id;
  uint32_t nb_pids;
  DVR_StreamPid_t pids[DVR_MAX_RECORD_PIDS_COUNT];
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
