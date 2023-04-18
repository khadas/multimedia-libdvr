/**
 * \file
 * TS indexer.
 */

#ifndef _TS_INDEXER_H_
#define _TS_INDEXER_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**Video format*/
typedef enum {
  TS_INDEXER_VIDEO_FORMAT_MPEG2, /**< MPEG2*/
  TS_INDEXER_VIDEO_FORMAT_H264,  /**< H264*/
  TS_INDEXER_VIDEO_FORMAT_HEVC   /**< HEVC*/
} TS_Indexer_StreamFormat_t;

/**Event type.*/
typedef enum {
  TS_INDEXER_EVENT_TYPE_VIDEO_I_FRAME, /**< Video I frame PTS.*/
  TS_INDEXER_EVENT_TYPE_VIDEO_PTS,     /**< Video PTS.*/
  TS_INDEXER_EVENT_TYPE_AUDIO_PTS      /**< Audio PTS.*/
} TS_Indexer_EventType_t;

/**Stream Parser state.*/
typedef enum {
  TS_INDEXER_STATE_INIT,        /**< Init state.*/
  TS_INDEXER_STATE_TS_START,    /**< TS header state with start_indicator==1.*/
  TS_INDEXER_STATE_PES_HEADER,  /**< PES header state.*/
  TS_INDEXER_STATE_PES_PTS,     /**< PES pts state.*/
  TS_INDEXER_STATE_PES_I_FRAME  /**< PES I-frame state.*/
} TS_Indexer_State_t;

/**Event.*/
typedef struct {
  TS_Indexer_EventType_t type;   /**< Event type.*/
  int                    pid;    /**< The PID of the stream.*/
  uint64_t               offset; /**< The offset of the first TS packet of this frame.*/
  uint64_t               pts;    /**< The PTS of this frame.*/
} TS_Indexer_Event_t;

/**TS indexer.*/
typedef struct TS_Indexer_s TS_Indexer_t;

/**Event callback function.*/
typedef void (*TS_Indexer_EventCallback_t) (TS_Indexer_t *ts_indexer, TS_Indexer_Event_t *event);

/**PES parser.*/
typedef struct {
  uint64_t                      pts;            /**< The a/v PTS.*/
  uint64_t                      offset;         /**< The current offset.*/
  uint8_t                       data[184+16];   /**< The PES data.*/
  int                           len;            /**< The length of PES data`.*/
  TS_Indexer_State_t            state;          /**< The stream state.*/
} PESParser;

/**TS parser.*/
typedef struct {
  int                           pid;    /**< The a/v PID.*/
  TS_Indexer_StreamFormat_t     format; /**< The a/v format.*/
  PESParser                     PES;    /**< The PES parser.*/
  uint64_t                      offset; /**< The offset of packet with start indicator.*/
} TSParser;

/**TS indexer.*/
struct TS_Indexer_s {
  TSParser               video_parser;  /**< The video parser.*/
  TSParser               audio_parser;  /**< The audio parser.*/
  uint64_t                      offset; /**< The current offset.*/
  TS_Indexer_EventCallback_t callback;  /**< The event callback function.*/
};

/**
 * Initialize the TS indexer.
 * \param ts_indexer The TS indexer to be initialized.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ts_indexer_init (TS_Indexer_t *ts_indexer);

/**
 * Release the TS indexer.
 * \param ts_indexer The TS indexer to be released.
 */
void ts_indexer_destroy (TS_Indexer_t *ts_indexer);

/**
 * Set the video format.
 * \param ts_indexer The TS indexer.
 * \param format The video format.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ts_indexer_set_video_format (TS_Indexer_t *ts_indexer, TS_Indexer_StreamFormat_t format);

/**
 * Set the video PID.
 * \param ts_indexer The TS indexer.
 * \param pid The video PID.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ts_indexer_set_video_pid (TS_Indexer_t *ts_indexer, int pid);

/**
 * Set the audio PID.
 * \param ts_indexer The TS indexer.
 * \param pid The audio PID.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ts_indexer_set_audio_pid (TS_Indexer_t *ts_indexer, int pid);

/**
 * Set the event callback function.
 * \param ts_indexer The TS indexer.
 * \param callback The event callback function.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ts_indexer_set_event_callback (TS_Indexer_t *ts_indexer, TS_Indexer_EventCallback_t callback);

/**
 * Parse the TS stream and generate the index data.
 * \param ts_indexer The TS indexer.
 * \param data The TS data.
 * \param len The length of the TS data in bytes.
 * \return The left TS data length of bytes.
 */
int ts_indexer_parse (TS_Indexer_t *ts_indexer, uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /*_TS_INDEXER_H_*/

