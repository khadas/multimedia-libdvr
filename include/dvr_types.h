/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 * \brief Basic datatypes
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-19: create the document
 ***************************************************************************/

#ifndef _DVR_TYPES_H
#define _DVR_TYPES_H

#include <stdint.h>
#include "pthread.h"

#ifdef __cplusplus
extern "C"
{
#endif


/*****************************************************************************
* Global Definitions
*****************************************************************************/

/**\brief Boolean value*/
typedef uint8_t        DVR_Bool_t;


/**\brief Error code of the function result,
 * Low 24 bits store the error number.
 * High 8 bits store the module's index.
 */
typedef int            DVR_ErrorCode_t;

/**\brief The module's index */
enum DVR_MOD_ID
{
	DVR_MOD_EVT,    /**< Event module*/
	DVR_MOD_PLAY,    /**< Demux module*/
	DVR_MOD_REC,    /**< DVR module*/
	DVR_MOD_MAX
};

/**\brief Get the error code base of each module
 * \param _mod The module's index
 */
#define DVR_ERROR_BASE(_mod)    ((_mod)<<24)

#ifndef DVR_SUCCESS
/**\brief Function result: Success*/
#define DVR_SUCCESS     (0)
#endif

#ifndef DVR_FAILURE
/**\brief Function result: Unknown error*/
#define DVR_FAILURE     (-1)
#endif

#ifndef DVR_TRUE
/**\brief Boolean value: true*/
#define DVR_TRUE        (1)
#endif

#ifndef DVR_FALSE
/**\brief Boolean value: false*/
#define DVR_FALSE       (0)
#endif

#ifdef __cplusplus
}
#endif

#endif

