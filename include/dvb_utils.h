/**
 * \file
 * \brief DVB utility functions
 */

#ifndef DVB_UTILS_H_
#define DVB_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**Demux input source.*/
typedef enum {
	DVB_DEMUX_SOURCE_TS0,  /**< Hardware TS input port 0.*/
	DVB_DEMUX_SOURCE_TS1,  /**< Hardware TS input port 1.*/
	DVB_DEMUX_SOURCE_TS2,  /**< Hardware TS input port 2.*/
	DVB_DEMUX_SOURCE_TS3,  /**< Hardware TS input port 3.*/
	DVB_DEMUX_SOURCE_TS4,  /**< Hardware TS input port 4.*/
	DVB_DEMUX_SOURCE_TS5,  /**< Hardware TS input port 5.*/
	DVB_DEMUX_SOURCE_TS6,  /**< Hardware TS input port 6.*/
	DVB_DEMUX_SOURCE_TS7,  /**< Hardware TS input port 7.*/
	DVB_DEMUX_SOURCE_DMA0,  /**< DMA input port 0.*/
    DVB_DEMUX_SOURCE_DMA1,  /**< DMA input port 1.*/
    DVB_DEMUX_SOURCE_DMA2,  /**< DMA input port 2.*/
    DVB_DEMUX_SOURCE_DMA3,  /**< DMA input port 3.*/
    DVB_DEMUX_SOURCE_DMA4,  /**< DMA input port 4.*/
    DVB_DEMUX_SOURCE_DMA5,  /**< DMA input port 5.*/
    DVB_DEMUX_SOURCE_DMA6,  /**< DMA input port 6.*/
    DVB_DEMUX_SOURCE_DMA7  /**< DMA input port 7.*/
} DVB_DemuxSource_t;

/**
 * Set the demux's input source.
 * \param dmx_idx Demux device's index.
 * \param src The demux's input source.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int dvb_set_demux_source (int dmx_idx, DVB_DemuxSource_t src);

/**
 * Get the demux's input source.
 * \param dmx_idx Demux device's index.
 * \param point src that demux's input source.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int dvb_get_demux_source (int dmx_idx, DVB_DemuxSource_t *src);


#ifdef __cplusplus
}
#endif

#endif /*DVB_UTILS_H_*/

