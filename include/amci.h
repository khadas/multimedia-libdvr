/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description: */

#ifndef _AMCI_H
#define _AMCI_H

/* #include <asm/types.h> */
#include <linux/types.h>


enum AM_CI_IO_MODE {
    AM_CI_IOR = 0,
    AM_CI_IOW,
    AM_CI_MEMR,
    AM_CI_MEMW
};

struct ci_rw_param {
    enum AM_CI_IO_MODE mode;
    int addr;
    u_int8_t value;
};


#define AMCI_IOC_MAGIC  'D'

#define AMCI_IOC_RESET       _IO(AMCI_IOC_MAGIC, 0x00)
#define AMCI_IOC_IO          _IOWR(AMCI_IOC_MAGIC, 0x01, struct ci_rw_param)
#define AMCI_IOC_GET_DETECT  _IOWR(AMCI_IOC_MAGIC, 0x02, int)
#define AMCI_IOC_SET_POWER   _IOW(AMCI_IOC_MAGIC, 0x03, int)


#endif
