/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *
 * @brief   linux dvb demux wrapper
 * @file    dvb_dmx_wrapper.h
 *
 * \author chuanzhi wang <chaunzhi.wang@amlogic.com>
 * \date 2020-07-16: create the document
 ***************************************************************************/

#ifndef _AM_DMX_H
#define _AM_DMX_H

#include "dvb_utils.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*AML_DMX_DataCb)(int dev_no, int fd, const uint8_t *data, int len, void *user_data);

    /**\brief dmx device init, creat dmx thread
 * \param dmx device number
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_Open(int dev_no);

    /**\brief dmx device uninit, destroy dmx thread
 * \param dmx device number
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_Close(int dev_no);

    /**\brief allocate dmx filter
 * \param dmx device number
 * \param get dmx filter index
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_AllocateFilter(int dev_no, int *fhandle);

    /**\brief set demux section filter
 * \param dmx device number
 * \param dmx filter index
 * \param dmx section filter param
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_SetSecFilter(int dev_no, int fhandle, const struct dmx_sct_filter_params *params);

    /**\brief set demux pes filter
 * \param dmx device number
 * \param dmx filter index
 * \param dmx pes filter param
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_SetPesFilter(int dev_no, int fhandle, const struct dmx_pes_filter_params *params);

    /**\brief set demux filter buffer
 * \param dmx device number
 * \param dmx filter index
 * \param dmx filter buffer size
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_SetBufferSize(int dev_no, int fhandle, int size);

    /**\brief free demux filter
 * \param dmx device number
 * \param dmx filter index
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_FreeFilter(int dev_no, int fhandle);

    /**\brief start demux filter
 * \param dmx device number
 * \param dmx filter index
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_StartFilter(int dev_no, int fhandle);

    /**\brief stop demux filter
 * \param dmx device number
 * \param dmx filter index
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_StopFilter(int dev_no, int fhandle);

    /**\brief set demux callback
 * \param dmx device number
 * \param dmx filter index
 * \param dmx filter callback
 * \param dmx filter callback param
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
    DVB_RESULT AML_DMX_SetCallback(int dev_no, int fhandle, AML_DMX_DataCb cb, void *user_data);

#ifdef __cplusplus
}
#endif
#endif
