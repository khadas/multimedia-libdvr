/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *
 * @brief   linux dvb frontend wrapper
 * @file    dvb_dmx_frontend.c
 *
 * \author Chuanzhi Wang <chuanzhi.wang@amlogic.com>
 * \date 2020-09-11: create the document
 ***************************************************************************/

#include <sys/ioctl.h>

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "frontend.h"
#include "dvb_frontend_wrapper.h"

static DVB_RESULT dmd_set_prop(int frontend_fd, const struct dtv_properties *prop);

/**\brief open frontend device
 * \param FE device path
 * \param FE device fd
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_Open(const char *name, int *frontend_fd)
{
    DVB_RESULT retval = DVB_SUCCESS;

    if ((*frontend_fd = open(name, O_RDWR | O_NONBLOCK)) < 0)
    {
        DVB_DEBUG(1, "Failed to open tune:%s, errno[%d]:%s", name, errno, strerror(errno));
        retval = DVB_FAILURE;
    }

    return retval;
}

/**\brief close frontend device
 * \param FE device fd
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_Colse(int frontend_fd)
{
    DVB_RESULT retval = DVB_SUCCESS;

    if (close(frontend_fd) < 0)
    {
        retval = DVB_FAILURE;
        DVB_DEBUG(1, "Failed to close frontend_fd:%d, errno[%d]:%s", frontend_fd, errno, strerror(errno));
    }

    return retval;
}

/**\brief get current tune status
 * \param FE device fd
 * \return whether it is locked
 */
dmd_tuner_event_t AML_FE_GetTuneStatus(int frontend_fd)
{
    struct dvb_frontend_event fe_event;
    dmd_tuner_event_t tune_event = TUNER_STATE_UNKNOW;

    if (ioctl(frontend_fd, FE_READ_STATUS, &fe_event.status) >= 0)
    {
        DVB_DEBUG(1, "current tuner status=0x%02x \n", fe_event.status);
        if ((fe_event.status & FE_HAS_LOCK) != 0)
        {
            tune_event = TUNER_STATE_LOCKED;
            DVB_DEBUG(1, "[ LOCKED ]\n");
        }
        else if ((fe_event.status & FE_TIMEDOUT) != 0)
        {
            tune_event = TUNER_STATE_TIMEOUT;
            DVB_DEBUG(1, "[ UNLOCKED ]\n");
        }
    }
    else
    {
        DVB_DEBUG(1, "frontend_fd:%d FE_READ_STATUS errno[%d]:%s", frontend_fd, errno, strerror(errno));
    }

    return tune_event;
}

/**\brief tune DVB-C
 * \param FE device fd
 * \param  dvb cable parameter
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_TuneDVB_C(int frontend_fd, const dmd_cable_desc_t *cable)
{
    int cmd_num = 0;
    int modulation = 0;
    struct dtv_property p[DTV_IOCTL_MAX_MSGS];
    struct dtv_properties props;

    p[cmd_num].cmd = DTV_DELIVERY_SYSTEM;
    p[cmd_num].u.data = SYS_DVBC_ANNEX_A;
    cmd_num++;
    p[cmd_num].cmd = DTV_FREQUENCY;
    p[cmd_num].u.data = cable->frequency * 1000;
    cmd_num++;

    p[cmd_num].cmd = DTV_SYMBOL_RATE;
    p[cmd_num].u.data = cable->symbol_rate * 1000;
    cmd_num++;

    switch (cable->modulation)
    {
    case DMD_MOD_NONE:
        modulation = QAM_AUTO;
        break;
    case DMD_MOD_QPSK:
        modulation = QPSK;
        break;
    case DMD_MOD_8PSK:
        modulation = PSK_8;
        break;
    case DMD_MOD_QAM:
        modulation = QAM_AUTO;
        break;
    case DMD_MOD_4QAM:
        modulation = QAM_AUTO;
        break;
    case DMD_MOD_16QAM:
        modulation = QAM_16;
        break;
    case DMD_MOD_32QAM:
        modulation = QAM_32;
        break;
    case DMD_MOD_64QAM:
        modulation = QAM_64;
        break;
    case DMD_MOD_128QAM:
        modulation = QAM_128;
        break;
    case DMD_MOD_256QAM:
        modulation = QAM_256;
        break;
    case DMD_MOD_BPSK:
    case DMD_MOD_ALL:
        modulation = QAM_AUTO;
        break;
    }
    p[cmd_num].cmd = DTV_MODULATION;
    p[cmd_num].u.data = modulation;
    cmd_num++;

    p[cmd_num].cmd = DTV_TUNE;
    cmd_num++;
    props.num = cmd_num;
    props.props = (struct dtv_property *)&p;

    return dmd_set_prop(frontend_fd, &props);
}

/**\brief tune DVB-T
 * \param FE device fd
 * \param  dvb terrestrial parameter
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_TuneDVB_T(int frontend_fd, const dmd_terrestrial_desc_t *terr)
{
    int tmp = 0;
    int cmd_num = 0;
    struct dtv_property p[DTV_IOCTL_MAX_MSGS];
    struct dtv_properties props;

    p[cmd_num].cmd = DTV_DELIVERY_SYSTEM;
    if (terr->dvb_type == DMD_DVBTYPE_DVBT)
        p[cmd_num].u.data = SYS_DVBT;
    else if (terr->dvb_type == DMD_DVBTYPE_DVBT2)
        p[cmd_num].u.data = SYS_DVBT2;
    else
        p[cmd_num].u.data = SYS_DTMB;
    cmd_num++;

    if (terr->dvb_type == DMD_DVBTYPE_DVBT)
        p[cmd_num].u.data = terr->desc.dvbt.frequency * 1000;
    else if (terr->dvb_type == DMD_DVBTYPE_DVBT2)
        p[cmd_num].u.data = terr->desc.dvbt2.frequency * 1000;
    else
        p[cmd_num].u.data = terr->desc.dtmb.frequency * 1000;
    DVB_DEBUG(1, "%s, type:%s, freq:%d", __func__,
             (terr->dvb_type == DMD_DVBTYPE_DVBT) ? "DVB-T" : ((terr->dvb_type == DMD_DVBTYPE_DVBT2) ? "DVB-T2" : "DTMB"),
             p[cmd_num].u.data);

    p[cmd_num].cmd = DTV_FREQUENCY;
    cmd_num++;

    if (terr->dvb_type == DMD_DVBTYPE_DVBT)
        tmp = terr->desc.dvbt.bandwidth;
    else if (terr->dvb_type == DMD_DVBTYPE_DVBT2)
        tmp = terr->desc.dvbt2.bandwidth;
    else
        tmp = terr->desc.dtmb.bandwidth;
    p[cmd_num].cmd = DTV_BANDWIDTH_HZ;
    switch (tmp)
    {
    case DMD_BANDWIDTH_10M:
        p[cmd_num].u.data = 10000000;
        break;
    case DMD_BANDWIDTH_8M:
        p[cmd_num].u.data = 8000000;
        break;
    case DMD_BANDWIDTH_7M:
        p[cmd_num].u.data = 7000000;
        break;
    case DMD_BANDWIDTH_6M:
        p[cmd_num].u.data = 6000000;
        break;
    case DMD_BANDWIDTH_5M:
        p[cmd_num].u.data = 5000000;
        break;
    case DMD_BANDWIDTH_17M:
        p[cmd_num].u.data = 1712000;
        break;
    }
    cmd_num++;

    if (terr->dvb_type != DMD_DVBTYPE_DTMB)
    {
    p[cmd_num].cmd = DTV_CODE_RATE_HP;
    p[cmd_num].u.data = FEC_AUTO;
    cmd_num++;

    p[cmd_num].cmd = DTV_CODE_RATE_LP;
    p[cmd_num].u.data = FEC_AUTO;
    cmd_num++;

    if (terr->dvb_type == DMD_DVBTYPE_DVBT)
        tmp = terr->desc.dvbt.transmission_mode;
    else
        tmp = terr->desc.dvbt2.transmission_mode;
    if (tmp <= DMD_TRANSMISSION_8K)
        tmp += -1;
    p[cmd_num].cmd = DTV_TRANSMISSION_MODE;
    p[cmd_num].u.data = tmp;
    cmd_num++;

    if (terr->dvb_type == DMD_DVBTYPE_DVBT)
        tmp = terr->desc.dvbt.guard_interval;
    else
        tmp = terr->desc.dvbt2.guard_interval;
    if (tmp <= DMD_GUARD_INTERVAL_1_4)
        tmp += -1;
    p[cmd_num].cmd = DTV_GUARD_INTERVAL;
    p[cmd_num].u.data = tmp;
    cmd_num++;

    if (terr->dvb_type == DMD_DVBTYPE_DVBT)
    {
        p[cmd_num].cmd = DTV_HIERARCHY;
        p[cmd_num].u.data = HIERARCHY_AUTO;
        cmd_num++;
    }
    if (terr->dvb_type == DMD_DVBTYPE_DVBT2)
    {
        p[cmd_num].cmd = DTV_DVBT2_PLP_ID_LEGACY;
        p[cmd_num].u.data = terr->desc.dvbt2.plp_id;
        cmd_num++;
    }
    }

    p[cmd_num].cmd = DTV_TUNE;
    cmd_num++;
    props.num = cmd_num;
    props.props = (struct dtv_property *)&p;

    return dmd_set_prop(frontend_fd, &props);
}

/**\brief tune DVB-S
 * \param FE device fd
 * \param  dvb satellite parameter
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_TuneDVB_S(int frontend_fd, const dmd_satellite_desc_t *sate)
{
    int cmd_num = 0;
    int code_rate = 0;
    struct dtv_properties props;
    struct dtv_property p[DTV_IOCTL_MAX_MSGS];

    DVB_DEBUG(1, "lock S, freq:%d, symbol rate:%d,  DVB-%s\n",
             sate->frequency, sate->symbol_rate,
             sate->modulation_system == DMD_MODSYS_DVBS2 ? "S2" : "S");

    p[cmd_num].cmd = DTV_DELIVERY_SYSTEM;
    p[cmd_num].u.data = sate->modulation_system == DMD_MODSYS_DVBS2 ? SYS_DVBS2 : SYS_DVBS;
    cmd_num++;
    p[cmd_num].cmd = DTV_FREQUENCY;
    p[cmd_num].u.data = sate->frequency;
    cmd_num++;

    p[cmd_num].cmd = DTV_SYMBOL_RATE;
    p[cmd_num].u.data = sate->symbol_rate * 1000;
    cmd_num++;

    code_rate = sate->fec_rate;
    switch (code_rate)
    {
    case DMD_FEC_NONE:
        code_rate = FEC_NONE;
        break;
    case DMD_FEC_1_2:
        code_rate = FEC_1_2;
        break;
    case DMD_FEC_2_3:
        code_rate = FEC_2_3;
        break;
    case DMD_FEC_3_4:
        code_rate = FEC_3_4;
        break;
    case DMD_FEC_4_5:
        code_rate = FEC_4_5;
        break;
    case DMD_FEC_5_6:
        code_rate = FEC_5_6;
        break;
    case DMD_FEC_6_7:
        code_rate = FEC_6_7;
        break;
    case DMD_FEC_7_8:
        code_rate = FEC_7_8;
        break;
    case DMD_FEC_8_9:
        code_rate = FEC_8_9;
        break;
    case DMD_FEC_3_5:
        code_rate = FEC_3_5;
        break;
    case DMD_FEC_9_10:
        code_rate = FEC_9_10;
        break;
    default:
        code_rate = FEC_AUTO;
        break;
    }
    p[cmd_num].cmd = DTV_INNER_FEC;
    p[cmd_num].u.data = code_rate;
    cmd_num++;

    p[cmd_num].cmd = DTV_TUNE;
    cmd_num++;
    props.num = cmd_num;
    props.props = (struct dtv_property *)&p;

    return dmd_set_prop(frontend_fd, &props);
}

/**\brief set lnb voltage
 * \param FE device fd
 * \param  voltage
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_LnbVoltage(int frontend_fd, dmd_lnb_voltage_t voltage)
{
    DVB_RESULT ret = DVB_SUCCESS;
    fe_sec_voltage_t volt;
    switch (voltage)
    {
    case DMD_LNB_VOLTAGE_14V:
        volt = SEC_VOLTAGE_13;
        break;
    case DMD_LNB_VOLTAGE_18V:
        volt = SEC_VOLTAGE_18;
        break;
    case DMD_LNB_VOLTAGE_OFF:
    default:
        volt = SEC_VOLTAGE_OFF;
        break;
    }

    if (ioctl(frontend_fd, FE_SET_VOLTAGE, volt) == -1)
    {
        ret = DVB_FAILURE;
        DVB_DEBUG(1, "FE_SET_VOLTAGE failed, frontend_fd:%d, voltage:%d errno[%d]:%s", frontend_fd, voltage, errno, strerror(errno));
    }
    return ret;
}

/**\brief set lnb tone
 * \param FE device fd
 * \param  tone, 22khz
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_LnbTone(int frontend_fd, dmd_lnb_tone_state_t tone_state)
{
    DVB_RESULT ret;
    fe_sec_tone_mode_t tone;

    switch (tone_state)
    {
    case DMD_LNB_TONE_OFF:
        tone = SEC_TONE_OFF;
        break;
    case DMD_LNB_TONE_22KHZ:
        tone = SEC_TONE_ON;
        break;
    case DMD_LNB_TONE_DEFAULT:
    default:
        tone = SEC_TONE_OFF;
        break;
    }

    if (ioctl(frontend_fd, FE_SET_TONE, tone) >= 0)
        ret = DVB_SUCCESS;
    else
        ret = DVB_FAILURE;
    return ret;
}

/**\brief send diseqc cmd
 * \param FE device fd
 * \param diseqc cmd data
 * \param diseqc cmd data length
 * \return DVB_SUCCESS On success, DVB_FAILURE on error.
 */
DVB_RESULT AML_FE_SendDISEQCMessage(int frontend_fd, uint8_t *data, uint8_t size)
{
    DVB_RESULT ret = DVB_SUCCESS;
    struct dvb_diseqc_master_cmd cmd;
    memset(&cmd, 0, sizeof(struct dvb_diseqc_master_cmd));

    for (int i = 0; i < size; i++)
    {
        cmd.msg[i] = data[i];
        DVB_DEBUG(1, "AML_FE_SendDISEQCMessage cmd:0x%x", data[i]);
    }
    cmd.msg_len = size;

    if (ioctl(frontend_fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1)
    {
        ret = DVB_FAILURE;
        DVB_DEBUG(1, "FE_DISEQC_SEND_MASTER_CMD failed, frontend_fd:%d, errno[%d]:%s", frontend_fd, errno, strerror(errno));
    }
    return ret;
}

static DVB_RESULT dmd_set_prop(int frontend_fd, const struct dtv_properties *prop)
{
    if (ioctl(frontend_fd, FE_SET_PROPERTY, prop) == -1)
    {
        DVB_DEBUG(1, "FE_SET_PROPERT failed, frontend_fd:%d, errno[%d]:%s", frontend_fd, errno, strerror(errno));
        return DVB_FAILURE;
    }
    return DVB_SUCCESS;
}
