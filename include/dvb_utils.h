/**
 * \file
 * \brief DVB utility functions
 */

#ifndef DVB_UTILS_H_
#define DVB_UTILS_H_

#include <android/log.h>
#include <stdio.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum
  {
    DVB_FAILURE = -1,
    DVB_SUCCESS = 0
  } DVB_RESULT;

/**Logcat TAG of dvb*/
#define DVB_LOG_TAG "dvb_debug"
/**Default debug level*/
#define DVB_DEBUG_LEVEL 1

/**Log output*/
#define dvb_log_print(...) __android_log_print(ANDROID_LOG_INFO, DVB_LOG_TAG, __VA_ARGS__)

#ifndef __ANDROID_API__
typedef unsigned int uint_t;
#endif

/**Output debug message.*/
#define DVB_DEBUG(_level, _fmt...) \
  do                               \
  {                                \
    if (_level <= DVB_DEBUG_LEVEL) \
      dvb_log_print(_fmt);         \
  } while (0)

  /**Demux input source.*/
  typedef enum
  {
    DVB_DEMUX_SOURCE_TS0,  /**< Hardware TS input port 0.*/
    DVB_DEMUX_SOURCE_TS1,  /**< Hardware TS input port 1.*/
    DVB_DEMUX_SOURCE_TS2,  /**< Hardware TS input port 2.*/
    DVB_DEMUX_SOURCE_TS3,  /**< Hardware TS input port 3.*/
    DVB_DEMUX_SOURCE_TS4,  /**< Hardware TS input port 4.*/
    DVB_DEMUX_SOURCE_TS5,  /**< Hardware TS input port 5.*/
    DVB_DEMUX_SOURCE_TS6,  /**< Hardware TS input port 6.*/
    DVB_DEMUX_SOURCE_TS7,  /**< Hardware TS input port 7.*/
    DVB_DEMUX_SOURCE_DMA0, /**< DMA input port 0.*/
    DVB_DEMUX_SOURCE_DMA1, /**< DMA input port 1.*/
    DVB_DEMUX_SOURCE_DMA2, /**< DMA input port 2.*/
    DVB_DEMUX_SOURCE_DMA3, /**< DMA input port 3.*/
    DVB_DEMUX_SOURCE_DMA4, /**< DMA input port 4.*/
    DVB_DEMUX_SOURCE_DMA5, /**< DMA input port 5.*/
    DVB_DEMUX_SOURCE_DMA6, /**< DMA input port 6.*/
    DVB_DEMUX_SOURCE_DMA7,  /**< DMA input port 7.*/
    DVB_DEMUX_SECSOURCE_DMA0, /**< DMA secure port 0.*/
    DVB_DEMUX_SECSOURCE_DMA1, /**< DMA secure port 1.*/
    DVB_DEMUX_SECSOURCE_DMA2, /**< DMA secure port 2.*/
    DVB_DEMUX_SECSOURCE_DMA3, /**< DMA secure port 3.*/
    DVB_DEMUX_SECSOURCE_DMA4, /**< DMA secure port 4.*/
    DVB_DEMUX_SECSOURCE_DMA5, /**< DMA secure port 5.*/
    DVB_DEMUX_SECSOURCE_DMA6, /**< DMA secure port 6.*/
    DVB_DEMUX_SECSOURCE_DMA7  /**< DMA secure port 7.*/
  } DVB_DemuxSource_t;

  /**
 * Set the demux's input source.
 * \param dmx_idx Demux device's index.
 * \param src The demux's input source.
 * \retval 0 On success.
 * \retval -1 On error.
 */
  int dvb_set_demux_source(int dmx_idx, DVB_DemuxSource_t src);

  /**
 * Get the demux's input source.
 * \param dmx_idx Demux device's index.
 * \param point src that demux's input source.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int dvb_get_demux_source (int dmx_idx, DVB_DemuxSource_t *src);

/**
 * check the demux's is new driver.
 * \retval 0 On old.
 * \retval 1 On new.
 */
int dvr_check_dmx_isNew(void);


#ifdef __cplusplus
}
#endif

#endif /*DVB_UTILS_H_*/

