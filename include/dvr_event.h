/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 * \brief DVR event
 ***************************************************************************/

#ifndef _DVR_EVENT_H
#define _DVR_EVENT_H

#include "dvr_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**Event manager's handler.*/
typedef void* DVR_EventManagerHandle_t;

/**Event type.*/
typedef enum {
} DVR_EventType_t;

/**Event.*/
typedef struct {
  DVR_EventType_t type; /**< Event type.*/
  union {
  } p;                  /**< Event's parameters.*/
} DVR_Event_t;

/**\brief Event callback function*/
typedef void (*DVR_EventCallback_t)(DVR_EventManagerHandle_t handle, DVR_Event *evt, void *data);

/**Event manager.*/
typedef struct {
  DVR_EventCallback_t  callback; /**< Callback function.*/
  void                *userdata; /**< Userdata used as the callback's parameter.*/
} DVR_EventManager_t;

#ifdef __cplusplus
}
#endif

#endif

