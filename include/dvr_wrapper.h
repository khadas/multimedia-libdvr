/**
 * \mainpage Amlogic DVR library
 *
 * \section Introduction
 * "libdvr" is a library provides basic DVR functions used by Amlogic platform.
 * It supports:
 * \li Record
 * \li Playback
 * \li Index file generated
 * \li Segment split
 * \li Encrypt and decrypt
 * \endsection
 *
 * \file
 * \brief libdvr wrapper layer
 *
 * Wrapper layer is upper layer of libdvr.
 * It is on top of dvr_record and dvr_playback.
 * It supports:
 * \li Separate record segments automatically.
 * \li Load segments automatically.
 */

#ifndef DVR_WRAPPER_H_
#define DVR_WRAPPER_H_

#include "dvr_types.h"
#include "dvr_crypto.h"
#include "dvr_playback.h"
#include "dvr_record.h"

#ifdef __cplusplus
extern "C" {
#endif

/**Record wrapper handle.*/
typedef void* DVR_WrapperRecord_t;
/**Playback wrapper handle.*/
typedef void* DVR_WrapperPlayback_t;

/**Record wrapper open parameters.*/
typedef struct {
  int                   dmx_dev_id;                      /**< Demux device's index.*/
  char                  location[DVR_MAX_LOCATION_SIZE]; /**< Location of the record file.*/
  DVR_Bool_t            is_timeshift;                    /**< The record file is used by timeshift.*/
  loff_t                segment_size;                    /**< Segment file's size.*/
  size_t                max_size;                        /**< Maximum record file size in bytes.*/
  int                   max_time;                        /**< Maximum record time in seconds.*/
  DVR_RecordFlag_t      flags;                           /**< Flags.*/
  DVR_CryptoPeriod_t    crypto_period;                   /**< Crypto period.*/
  DVR_CryptoFunction_t  crypto_fn;                       /**< Crypto callback function.*/
  void                 *crypto_data;                     /**< User data of crypto function.*/
} DVR_WrapperRecordOpenParams_t;

/**Record start parameters.*/
typedef struct {
  uint32_t              nb_pids;                         /**< Number of PIDs.*/
  DVR_StreamPid_t       pids[DVR_MAX_RECORD_PIDS_COUNT]; /**< PIDs to be recorded.*/
} DVR_WrapperRecordStartParams_t;

/**Update PID parameters.*/
typedef struct {
  uint32_t              nb_pids;                               /**< Number of PID actions.*/
  DVR_StreamPid_t       pids[DVR_MAX_RECORD_PIDS_COUNT];       /**< PIDs.*/
  DVR_RecordPidAction_t pid_action[DVR_MAX_RECORD_PIDS_COUNT]; /**< Actions.*/
} DVR_WrapperUpdatePidsParams_t;

/**Playback wrapper open parameters.*/
typedef struct {
  int                     dmx_dev_id;                      /**< playback used dmx device index*/
  char                    location[DVR_MAX_LOCATION_SIZE]; /**< Location of the record file.*/
  int                     block_size;                      /**< playback inject block size*/
  DVR_Bool_t              is_timeshift;                    /**< 0:playback mode, 1 : is timeshift mode*/
  Playback_DeviceHandle_t playback_handle;                 /**< Playback device handle.*/
  DVR_CryptoFunction_t    crypto_fn;                       /**< Crypto function.*/
  void                   *crypto_data;                     /**< Crypto function's user data.*/
} DVR_WrapperPlaybackOpenParams_t;

/**
 * Open a new record wrapper.
 * \param[out] rec Return the new record handle.
 * \param params Record open parameters.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_open_record (DVR_WrapperRecord_t *rec, DVR_WrapperRecordOpenParams_t *params);

/**
 * Close an unused record wrapper.
 * \param rec The record handle.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_close_record (DVR_WrapperRecord_t rec);

/**
 * Start recording.
 * \param rec The record handle.
 * \param params Record start parameters.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_start_record (DVR_WrapperRecord_t rec, DVR_WrapperRecordStartParams_t *params);

/**
 * Stop recording..
 * \param rec The record handle.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_stop_record (DVR_WrapperRecord_t rec);

/**
 * Update the recording PIDs.
 * \param rec The record handle.
 * \param params The new PIDs.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_update_record_pids (DVR_WrapperRecord_t rec, DVR_WrapperUpdatePidsParams_t *params);

/**
 * Open a new playback wrapper handle.
 * \param[out] playback Return the new playback handle.
 * \param params Playback handle open parameters.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_open_playback (DVR_WrapperPlayback_t *playback, DVR_WrapperPlaybackOpenParams_t *params);

/**
 * Close a unused playback handle.
 * \param playback The playback handle to be closed.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_close_playback (DVR_WrapperPlayback_t playback);

/**
 * Start playback.
 * \param playback The playback handle.
 * \param flags Playback flags.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_start_playback (DVR_WrapperPlayback_t playback, DVR_PlaybackFlag_t flags);

/**
 * Stop playback.
 * \param playback The playback handle.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_stop_playback (DVR_WrapperPlayback_t playback);

/**
 * Pause the playback.
 * \param playback The playback handle.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_pause_playback (DVR_WrapperPlayback_t playback);

/**
 * Set the playback speed.
 * \param playback The playback handle.
 * \param speed The new speed.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_set_playback_speed (DVR_WrapperPlayback_t playback, Playback_DeviceSpeeds_t speed);

/**
 * Seek the current playback position.
 * \param playback The playback handle.
 * \param time_offset The current time in milliseconds.
 * \retval DVR_SUCCESS On success.
 * \return Error code.
 */
int dvr_wrapper_seek_playback (DVR_WrapperPlayback_t playback, int time_offset);

#ifdef __cplusplus
}
#endif

#endif /*DVR_WRAPPER_H_*/

