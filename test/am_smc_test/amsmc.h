/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description: */

#ifndef _AMSMC_H
#define _AMSMC_H

#include <asm/types.h>

#define AMSMC_MAX_ATR_LEN    33

enum {
    AMSMC_CARDOUT = 0,
    AMSMC_CARDIN = 1
};

struct am_smc_atr {
    char    atr[AMSMC_MAX_ATR_LEN];
    int     atr_len;
};

struct am_smc_param {
    int     f;
    int     d;
    int     n;
    int     bwi;
    int     cwi;
    int     bgt;
    int     freq;
    int     recv_invert;
    int     recv_lsb_msb;
    int     recv_no_parity;
    int     xmit_invert;
    int     xmit_lsb_msb;
    int     xmit_retries;
    int     xmit_repeat_dis;
};

#define AMSMC_IOC_MAGIC  'C'

#define AMSMC_IOC_RESET        _IOR(AMSMC_IOC_MAGIC, 0x00, struct am_smc_atr)
#define AMSMC_IOC_GET_STATUS   _IOR(AMSMC_IOC_MAGIC, 0x01, int)
#define AMSMC_IOC_ACTIVE       _IO(AMSMC_IOC_MAGIC, 0x02)
#define AMSMC_IOC_DEACTIVE     _IO(AMSMC_IOC_MAGIC, 0x03)
#define AMSMC_IOC_GET_PARAM    _IOR(AMSMC_IOC_MAGIC, 0x04, struct am_smc_param)
#define AMSMC_IOC_SET_PARAM    _IOW(AMSMC_IOC_MAGIC, 0x05, struct am_smc_param)

#endif

