 /**
  * \page aml_fend_test
  * \section Introduction
  * test code with am_fend_xxxx APIs.
  * It supports:
  * \li lock DVB-C
  * \li lock DVB-T
  * \li lock DVB-T2
  * \li lock DVB-S
  * \li lock DVB-S2
  *
  * \section Usage
  *
  * Help msg will be shown if the test runs without parameters.\n
  * There are some general concepts for the parameters:
  * \li fe: fontend_idx
  * \li mode: tune mode
  * \li freq: frequency MHz
  * \li sym_rate: symbol_rate
  * \li modul: modulation
  * \li bw: bandwidth
  * \li tran_mode: transmission_mode
  * \li plp: plp_id, only for dvb-t2
  * \li lo: local_oscillator MHz
  * \li timer: check tune status time, second
  *
  * \section FormatCode Format Code
  *
  * mode
  * \li 1: DVB-C
  * \li 2: DVB-T
  * \li 3: DVB-T2
  * \li 4: DVB-S
  * \li 5: DVB-S2
  *
  * modulation
  * \li 0x0000: DMD_MOD_NONE
  * \li 0x0001: DMD_MOD_QPSK
  * \li 0x0002: DMD_MOD_8PSK
  * \li 0x0004: DMD_MOD_QAM
  * \li 0x0008: DMD_MOD_4QAM
  * \li 0x0010: DMD_MOD_16QAM
  * \li 0x0020: DMD_MOD_32QAM
  * \li 0x0040: DMD_MOD_64QAM
  * \li 0x0080: DMD_MOD_128QAM
  * \li 0x0100: DMD_MOD_256QAM
  *
  * transmission_mode
  * \li 0x01: DMD_TRANSMISSION_2K
  * \li 0x02: DMD_TRANSMISSION_8K
  * \li 0x03: DMD_TRANSMISSION_4K
  * \li 0x04: DMD_TRANSMISSION_1K
  * \li 0x05: DMD_TRANSMISSION_16K
  * \li 0x06: DMD_TRANSMISSION_32K
  *
  *
  * Lock DVB-C:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=1] [freq=frequency] [sym_rate=symbol_rate] [modul=modulation] [timer=check_tune_time]
  * \endcode
  * Lock DVB-T:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=2] [freq=frequency] [bw=bandwidth] [tran_mode=transmission_mode]	[timer=check_tune_time]
  * \endcode
  * Lock DVB-T2:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=3] [freq=frequency] [bw=bandwidth] [tran_mode=transmission_mode] [plp=plp_id] [timer=check_tune_time]
  * \endcode
  * Lock DVB-S:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=4] [freq=frequency] [sym_rate=symbol_rate] [modul=modulation] [lo=local_oscillator] [timer=check_tune_time]
  * \endcode
  * Lock DVB-S2:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=5] [freq=frequency] [sym_rate=symbol_rate] [modul=modulation] [lo=local_oscillator] [timer=check_tune_time]
  * \endcode
  *
  * \endsection
  */

 /***************************************************************************
  * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
  *
  * This source code is subject to the terms and conditions defined in the
  * file 'LICENSE' which is part of this source code package.
  *
  * Description:
  */
 /**\file
  * \brief DVB前端测试
  *
  * \author chuanzhi wang <chuanzhi.wang@amlogic.com>
  * \date 2020-04-21: create the document
  ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/dvb/frontend.h>

#define TUNE_MODE_UNKNOW  0
#define TUNE_MODE_DVB_C   1
#define TUNE_MODE_DVB_T   2
#define TUNE_MODE_DVB_T2  3
#define TUNE_MODE_DVB_S   4
#define TUNE_MODE_DVB_S2  5
#define INVALID_FD        -1

typedef enum
{
  DMD_DOWNLINK_INJECTION,
  DMD_DOWNLINK_CUSTOM,
  DMD_DOWNLINK_Ku_LOW,
  DMD_DOWNLINK_Ku_HIGH,
  DMD_DOWNLINK_C
} dmd_downlink_t;

typedef enum
{
  DMD_LNB_TONE_DEFAULT,
  DMD_LNB_TONE_OFF,
  DMD_LNB_TONE_22KHZ
} dmd_lnb_tone_state_t;

typedef struct
{
  uint_t				   band_start;
  uint_t				   band_end;
  uint_t				   lo;
  dmd_downlink_t		   downlink;
} dmd_satellite_band_t;

typedef enum
{
  DMD_DISEQC_DEFAULT,
  DMD_DISEQC_PORTA,
  DMD_DISEQC_PORTB,
  DMD_DISEQC_PORTC,
  DMD_DISEQC_PORTD,
  DMD_DISEQC_ALL
} dmd_diseqc_port_t;

typedef enum
{
  DMD_LNB_VOLTAGE_OFF = 0,
  DMD_LNB_VOLTAGE_14V = 1,
  DMD_LNB_VOLTAGE_18V = 2
} dmd_lnb_voltage_t;

typedef enum
{
  DMD_PLR_HORIZONTAL      = 0x01,
  DMD_PLR_VERTICAL        = 0x02,
  DMD_PLR_NONE            = 0x03,
  DMD_PLR_CIRCULAR_LEFT   = 0x04,
  DMD_PLR_CIRCULAR_RIGHT  = 0x05
} dmd_polarization_t;

typedef enum
{
  DMD_ROLLOFF_035 = 0x00,
  DMD_ROLLOFF_025 = 0x01,
  DMD_ROLLOFF_020 = 0x02
} dmd_rolloff_t;

typedef enum
{
  DMD_MODSYS_DVBS  = 0x00,
  DMD_MODSYS_DVBS2 = 0x01
} dmd_modulation_system_t;

typedef enum
{
  DMD_FEC_NONE    = 0x0000,
  DMD_FEC_1_2     = 0x0001,
  DMD_FEC_2_3     = 0x0002,
  DMD_FEC_3_4     = 0x0004,
  DMD_FEC_4_5     = 0x0008,
  DMD_FEC_5_6     = 0x0010,
  DMD_FEC_6_7     = 0x0020,
  DMD_FEC_7_8     = 0x0040,
  DMD_FEC_8_9     = 0x0080,
  DMD_FEC_3_5     = 0x0100,
  DMD_FEC_9_10    = 0x0200,
  DMD_FEC_ALL     = 0xFFFF
} dmd_fec_rate_t;

typedef enum
{
  DMD_FEC_OUTER_NONE        = 0x00,
  DMD_FEC_OUTER_NOT_DEFINED = 0x01,
  DMD_FEC_OUTER_RS          = 0x02
} dmd_fec_rate_outer_t;

typedef enum
{
  DMD_MOD_NONE    = 0x0000,
  DMD_MOD_QPSK    = 0x0001,
  DMD_MOD_8PSK    = 0x0002,
  DMD_MOD_QAM     = 0x0004,
  DMD_MOD_4QAM    = 0x0008,
  DMD_MOD_16QAM   = 0x0010,
  DMD_MOD_32QAM   = 0x0020,
  DMD_MOD_64QAM   = 0x0040,
  DMD_MOD_128QAM  = 0x0080,
  DMD_MOD_256QAM  = 0x0100,
  DMD_MOD_BPSK    = 0x0200,
  DMD_MOD_ALL     = 0xFFFF
} dmd_modulation_t;

typedef enum
{
  DMD_CONSTELLATION_NONE    = 0x0000,
  DMD_CONSTELLATION_QPSK    = 0x0001,
  DMD_CONSTELLATION_16QAM   = 0x0002,
  DMD_CONSTELLATION_64QAM   = 0x0004,
  DMD_CONSTELLATION_32QAM   = 0x0008,
  DMD_CONSTELLATION_128QAM  = 0x0010,
  DMD_CONSTELLATION_256QAM  = 0x0020,
  DMD_CONSTELLATION_1024QAM = 0x0040,
  DMD_CONSTELLATION_ALL     = 0xFFFF
} dmd_constellation_t;

typedef enum
{
  DMD_TRANSMISSION_2K   = 0x01,
  DMD_TRANSMISSION_8K   = 0x02,
  DMD_TRANSMISSION_4K   = 0x03,
  DMD_TRANSMISSION_1K   = 0x04,
  DMD_TRANSMISSION_16K  = 0x05,
  DMD_TRANSMISSION_32K  = 0x06
} dmd_transmission_mode_t;

typedef enum
{
  DMD_BANDWIDTH_8M    = 0x01,
  DMD_BANDWIDTH_7M    = 0x02,
  DMD_BANDWIDTH_6M    = 0x03,
  DMD_BANDWIDTH_5M    = 0x04,
  DMD_BANDWIDTH_10M   = 0x05,
  DMD_BANDWIDTH_17M   = 0x06
} dmd_bandwidth_t;

typedef enum
{
  DMD_GUARD_INTERVAL_1_32    = 0x01,
  DMD_GUARD_INTERVAL_1_16    = 0x02,
  DMD_GUARD_INTERVAL_1_8     = 0x03,
  DMD_GUARD_INTERVAL_1_4     = 0x04,
  DMD_GUARD_INTERVAL_1_128   = 0x05,
  DMD_GUARD_INTERVAL_19_128  = 0x06,
  DMD_GUARD_INTERVAL_19_256  = 0x07
} dmd_guard_interval_t;

typedef enum
{
  DMD_HIERARCHY_NONE = 0x01,
  DMD_HIERARCHY_1    = 0x02,
  DMD_HIERARCHY_2    = 0x03,
  DMD_HIERARCHY_4    = 0x04
} dmd_hierarchy_t;

typedef enum
{
  DMD_HIERARCHY_HP = 0x00,
  DMD_HIERARCHY_LP = 0x01
} dmd_hierarchy_hplp_t;

typedef enum
{
  DMD_PLP_COMMON,
  DMD_PLP_DATA1,
  DMD_PLP_DATA2
} dmd_plp_type_t;

typedef enum
{
  DMD_DVBTYPE_DVBT,
  DMD_DVBTYPE_DVBT2
} dmd_terrestrial_dvbtype_t;

typedef enum
{
   DMD_SATELLITE   = 0x0100,
   DMD_CABLE 	   = 0x0200,
   DMD_TERRESTRIAL = 0x0300,
} dmd_device_type_t;

typedef enum
{
   TUNER_STATE_LOCKED,
   TUNER_STATE_TIMEOUT,
   TUNER_STATE_UNKNOW
} dmd_tuner_event_t;


typedef struct
{
  uint_t                  frequency;
  uint_t                  symbol_rate;
  dmd_modulation_system_t modulation_system;
  dmd_polarization_t      polarization;
  dmd_modulation_t        modulation;
  dmd_fec_rate_t          fec_rate;
  dmd_rolloff_t           roll_off;
  dmd_lnb_tone_state_t    lnb_tone_state;
  dmd_diseqc_port_t       diseqc_port;
  dmd_satellite_band_t    band;
} dmd_satellite_desc_t;

typedef struct
{
  uint_t               frequency;         /* frequency in KHz */
  dmd_fec_rate_t       fec_rate;          /* select FEC rate(s) */
  dmd_fec_rate_outer_t fec_rate_outer;    /* select FEC outer */
  dmd_modulation_t     modulation;        /* select modulation scheme */
  uint_t               symbol_rate;       /* symbol rate in KBAUD */
} dmd_cable_desc_t;

typedef struct
{
  uint_t                   frequency;
  dmd_constellation_t      constellation;
  dmd_hierarchy_t          hierarchy;
  dmd_hierarchy_hplp_t     hp_lp;
  dmd_fec_rate_t           HP_code_rate;
  dmd_fec_rate_t           LP_code_rate;
  dmd_guard_interval_t     guard_interval;
  dmd_transmission_mode_t  transmission_mode;
  dmd_bandwidth_t          bandwidth;
} dmd_terrestrial_dvbt_desc_t;

typedef struct
{
  uint_t                   frequency;
  uint_t                   plp_id;
  dmd_guard_interval_t     guard_interval;
  dmd_transmission_mode_t  transmission_mode;
  uint_t                   T2_system_id;
  dmd_bandwidth_t          bandwidth;
} dmd_terrestrial_dvbt2_desc_t;

typedef struct
{
  dmd_terrestrial_dvbtype_t dvb_type;
  union
  {
    dmd_terrestrial_dvbt_desc_t  dvbt;
    dmd_terrestrial_dvbt2_desc_t dvbt2;
  } desc;
} dmd_terrestrial_desc_t;


typedef struct
{
   dmd_device_type_t	device_type;
   union
   {
      dmd_satellite_desc_t	satellite;
      dmd_cable_desc_t		cable;
      dmd_terrestrial_desc_t terrestrial;
   } delivery;
} dmd_delivery_t;

static int dmd_set_prop(int fe_fd, const struct dtv_properties *prop)
{
  if (ioctl(fe_fd, FE_SET_PROPERTY, prop) == -1) {
     printf("set prop failed>>>>>>>>>>>>.\n");
     return -1;
  }
  return 0;
}

static int dmd_lock_t(int fe_fd, const dmd_delivery_t * pDelivery)
{
   int tmp = 0;
   int cmd_num = 0;
   struct dtv_properties props;
   struct dtv_property p[DTV_IOCTL_MAX_MSGS];

   p[cmd_num].cmd = DTV_DELIVERY_SYSTEM;
   if (pDelivery->delivery.terrestrial.dvb_type == DMD_DVBTYPE_DVBT)
	 p[cmd_num].u.data = SYS_DVBT;
   else
	 p[cmd_num].u.data = SYS_DVBT2;
   cmd_num++;

   if (pDelivery->delivery.terrestrial.dvb_type == DMD_DVBTYPE_DVBT)
	 p[cmd_num].u.data = pDelivery->delivery.terrestrial.desc.dvbt.frequency * 1000;
   else
	 p[cmd_num].u.data = pDelivery->delivery.terrestrial.desc.dvbt2.frequency * 1000;

   p[cmd_num].cmd = DTV_FREQUENCY;
   cmd_num++;

   if (pDelivery->delivery.terrestrial.dvb_type == DMD_DVBTYPE_DVBT)
	 tmp = pDelivery->delivery.terrestrial.desc.dvbt.bandwidth;
   else
	 tmp = pDelivery->delivery.terrestrial.desc.dvbt2.bandwidth;
   p[cmd_num].cmd = DTV_BANDWIDTH_HZ;
   switch (tmp) {
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

   p[cmd_num].cmd = DTV_CODE_RATE_HP;
   p[cmd_num].u.data = FEC_AUTO;
   cmd_num++;

   p[cmd_num].cmd = DTV_CODE_RATE_LP;
   p[cmd_num].u.data = FEC_AUTO;
   cmd_num++;

   if (pDelivery->delivery.terrestrial.dvb_type == DMD_DVBTYPE_DVBT)
	 tmp = pDelivery->delivery.terrestrial.desc.dvbt.transmission_mode;
   else
	 tmp = pDelivery->delivery.terrestrial.desc.dvbt2.transmission_mode;
   if (tmp <= DMD_TRANSMISSION_8K)
	 tmp += -1;
   p[cmd_num].cmd = DTV_TRANSMISSION_MODE;
   p[cmd_num].u.data = tmp;
   cmd_num++;

   if (pDelivery->delivery.terrestrial.dvb_type == DMD_DVBTYPE_DVBT)
	 tmp = pDelivery->delivery.terrestrial.desc.dvbt.guard_interval;
   else
	 tmp = pDelivery->delivery.terrestrial.desc.dvbt2.guard_interval;
   if (tmp <= DMD_GUARD_INTERVAL_1_4)
	 tmp += -1;
   p[cmd_num].cmd = DTV_GUARD_INTERVAL;
   p[cmd_num].u.data = tmp;
   cmd_num++;

   if (pDelivery->delivery.terrestrial.dvb_type == DMD_DVBTYPE_DVBT) {
	 p[cmd_num].cmd = DTV_HIERARCHY;
	 p[cmd_num].u.data = HIERARCHY_AUTO;
	 cmd_num++;
   }
   if (pDelivery->delivery.terrestrial.dvb_type == DMD_DVBTYPE_DVBT2) {
	 p[cmd_num].cmd = DTV_DVBT2_PLP_ID_LEGACY;
	 p[cmd_num].u.data = pDelivery->delivery.terrestrial.desc.dvbt2.plp_id;
	 cmd_num++;
   }

   p[cmd_num].cmd = DTV_TUNE;
   cmd_num++;
   props.num = cmd_num;
   props.props = (struct dtv_property *)&p;

   return dmd_set_prop(fe_fd, &props);
}

static int dmd_lock_c(int fe_fd, const dmd_delivery_t * pDelivery)
{
   int tmp = 0;
   int cmd_num = 0;
   struct dtv_properties props;
   struct dtv_property p[DTV_IOCTL_MAX_MSGS];

   p[cmd_num].cmd = DTV_DELIVERY_SYSTEM;
   p[cmd_num].u.data = SYS_DVBC_ANNEX_A;
   cmd_num++;
   p[cmd_num].cmd = DTV_FREQUENCY;
   p[cmd_num].u.data = pDelivery->delivery.cable.frequency * 1000;
   cmd_num++;

   p[cmd_num].cmd = DTV_SYMBOL_RATE;
   p[cmd_num].u.data = pDelivery->delivery.cable.symbol_rate * 1000;
   cmd_num++;

   tmp = pDelivery->delivery.cable.modulation;
   switch (tmp) {
   case DMD_MOD_NONE:
	 tmp = QAM_AUTO;
	 break;
   case DMD_MOD_QPSK:
	 tmp = QPSK;
	 break;
   case DMD_MOD_8PSK:
	 tmp = PSK_8;
	 break;
   case DMD_MOD_QAM:
	 tmp = QAM_AUTO;
	 break;
   case DMD_MOD_4QAM:
	 tmp = QAM_AUTO;
	 break;
   case DMD_MOD_16QAM:
	 tmp = QAM_16;
	 break;
   case DMD_MOD_32QAM:
	 tmp = QAM_32;
	 break;
   case DMD_MOD_64QAM:
	 tmp = QAM_64;
	 break;
   case DMD_MOD_128QAM:
	 tmp = QAM_128;
	 break;
   case DMD_MOD_256QAM:
	 tmp = QAM_256;
	 break;
   case DMD_MOD_BPSK:
   case DMD_MOD_ALL:
	 tmp = QAM_AUTO;
	 break;
   }
   p[cmd_num].cmd = DTV_MODULATION;
   p[cmd_num].u.data = tmp;
   cmd_num++;

   p[cmd_num].cmd = DTV_TUNE;
   cmd_num++;
   props.num = cmd_num;
   props.props = (struct dtv_property *)&p;

   return dmd_set_prop(fe_fd, &props);
}

static int dmd_lock_s(int fe_fd, const dmd_delivery_t * pDelivery, dmd_lnb_tone_state_t tone_state, dmd_lnb_voltage_t vol)
{
   int cmd_num = 0;
   int code_rate = 0;
   fe_sec_tone_mode_t tone;
   fe_sec_voltage_t voltage;
   struct dtv_properties props;
   struct dvb_diseqc_master_cmd cmd;
   struct dtv_property p[DTV_IOCTL_MAX_MSGS];

   /*printf("lock S, freq:%d, symbol rate:%d, band start:%d Khz, end:%d Khz, LO:%d Khz, dowlink:%d\n",
	   pDelivery->delivery.satellite.frequency, pDelivery->delivery.satellite.symbol_rate,
	   pDelivery->delivery.satellite.band.band_start, pDelivery->delivery.satellite.band.band_end,
	   pDelivery->delivery.satellite.band.lo, pDelivery->delivery.satellite.band.downlink);*/

   switch (vol)
   {
      case DMD_LNB_VOLTAGE_14V:
        voltage = SEC_VOLTAGE_13;
        break;
      case DMD_LNB_VOLTAGE_18V:
        voltage = SEC_VOLTAGE_18;
        break;
      case DMD_LNB_VOLTAGE_OFF:
      default:
        voltage = SEC_VOLTAGE_OFF;
        break;
   }
   if (ioctl(fe_fd, FE_SET_VOLTAGE, voltage) == -1)
   {
       printf("ioctl FE_SET_VOLTAGE failed, fd:%d error:%d", fe_fd, errno);
   }

   /*Diseqc start*/
   printf("Diseqc, LNB tone:%d, port:%d\n",
	   pDelivery->delivery.satellite.lnb_tone_state,
	   pDelivery->delivery.satellite.diseqc_port);
   /*Diseqc end*/

   /*LNB TONE*/
   switch (pDelivery->delivery.satellite.lnb_tone_state) {
	 case DMD_LNB_TONE_DEFAULT:
	   tone = (tone_state == DMD_LNB_TONE_22KHZ) ? SEC_TONE_ON : SEC_TONE_OFF;
	   break;
	 case DMD_LNB_TONE_OFF:
	   tone = SEC_TONE_OFF;
	   break;
	 case DMD_LNB_TONE_22KHZ:
	   tone = SEC_TONE_ON;
	   break;
	 default:
	   tone = SEC_TONE_OFF;
	   break;
   }
   if (ioctl(fe_fd, FE_SET_TONE, tone) == -1) {
	  printf("set TONE failed, %d\n", tone);
   }

   p[cmd_num].cmd = DTV_DELIVERY_SYSTEM;
   p[cmd_num].u.data = pDelivery->delivery.satellite.modulation_system == DMD_MODSYS_DVBS2 ? SYS_DVBS2 : SYS_DVBS;
   cmd_num++;
   p[cmd_num].cmd = DTV_FREQUENCY;
   p[cmd_num].u.data = abs(pDelivery->delivery.satellite.frequency - pDelivery->delivery.satellite.band.lo);
   printf("tune to %d\n", p[cmd_num].u.data);
   cmd_num++;

   p[cmd_num].cmd = DTV_SYMBOL_RATE;
   p[cmd_num].u.data = pDelivery->delivery.satellite.symbol_rate * 1000;
   cmd_num++;

   code_rate = pDelivery->delivery.satellite.fec_rate;
   switch (code_rate) {
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

   return dmd_set_prop(fe_fd, &props);
}


static int open_fend(int fe_idx, int *fe_id)
{
   char fe_name[24];
   struct stat file_status;

   snprintf(fe_name, sizeof(fe_name), "/dev/dvb0.frontend%u", fe_idx);
   if (stat(fe_name, &file_status) == 0)
   {
       printf("Found FE[%s]\n", fe_name);
   }
   else
   {
       printf("No FE found [%s]!", fe_name);
	   return -1;
   }

   if ((*fe_id = open(fe_name, O_RDWR | O_NONBLOCK)) < 0)
   {
      printf("Failed to open [%s], errno %d\n", fe_name, errno);
      return -2;
   }
   else
   {
      printf("Open %s frontend_fd:%d \n", fe_name, *fe_id);
   }

   return 0;
}

static void close_fend(int fe_id)
{
	if (fe_id != INVALID_FD)
	{
	   printf("close frontend_fd:%d \n", fe_id);
	   close(fe_id);
	}
}

static dmd_tuner_event_t get_dmd_lock_status(int frontend_fd)
{
    struct dvb_frontend_event fe_event;
    dmd_tuner_event_t tune_event = TUNER_STATE_UNKNOW;
    if (ioctl(frontend_fd, FE_READ_STATUS, &fe_event.status) >= 0)
    {
       printf("current tuner status=0x%02x \n", fe_event.status);
       if ((fe_event.status & FE_HAS_LOCK) != 0)
       {
           tune_event = TUNER_STATE_LOCKED;
		   printf("current tuner status [locked]\n");
       }
       else if ((fe_event.status & FE_TIMEDOUT) != 0)
       {
           tune_event = TUNER_STATE_TIMEOUT;
		   printf("current tuner status [unlocked]\n");
       }
    }
	else
    {
        printf("frontend_fd:%d FE_READ_STATUS errno:%d \n" ,frontend_fd, errno);
    }
    return tune_event;
}


static char *help_mode =
"\n 1: DVB-C"
"\n 2: DVB-T"
"\n 3: DVB-T2"
"\n 4: DVB-S"
"\n 5: DVB-S2";
static char *help_modulation =
"\n 0x0000: DMD_MOD_NONE"
"\n 0x0001: DMD_MOD_QPSK"
"\n 0x0002: DMD_MOD_8PSK"
"\n 0x0004: DMD_MOD_QAM"
"\n 0x0008: DMD_MOD_4QAM"
"\n 0x0010: DMD_MOD_16QAM"
"\n 0x0020: DMD_MOD_32QAM"
"\n 0x0040: DMD_MOD_64QAM"
"\n 0x0080: DMD_MOD_128QAM"
"\n 0x0100: DMD_MOD_256QAM";
static char *help_bandwidth =
"\n 0x01: DMD_BANDWIDTH_8M"
"\n 0x02: DMD_BANDWIDTH_7M"
"\n 0x03: DMD_BANDWIDTH_6M"
"\n 0x04: DMD_BANDWIDTH_5M"
"\n 0x05: DMD_BANDWIDTH_10M"
"\n 0x06: DMD_BANDWIDTH_17M";
static char *help_transmission_mode =
"\n 0x01: DMD_TRANSMISSION_2K-C"
"\n 0x02: DMD_TRANSMISSION_8K"
"\n 0x03: DMD_TRANSMISSION_4K"
"\n 0x04: DMD_TRANSMISSION_1K"
"\n 0x05: DMD_TRANSMISSION_16K"
"\n 0x06: DMD_TRANSMISSION_32K";

static void usage(int argc, char *argv[])
{
  printf( "[Lock DVB-C:] \n%s fe=fontend_idx mode=1 freq=frequency sym_rate=symbol_rate modul=modulation timer=check_tune_time\n", argv[0]);
  printf( "[Lock DVB-T:] \n%s fe=fontend_idx mode=2 freq=frequency bw=bandwidth tran_mode=transmission_mode timer=check_tune_time\n", argv[0]);
  printf( "[Lock DVB-T2:]\n%s fe=fontend_idx mode=3 freq=frequency bw=bandwidth  tran_mode=transmission_mode  plp=plp_id timer=check_tune_time\n", argv[0]);
  printf( "[Lock DVB-S:] \n%s fe=fontend_idx mode=4 freq=frequency sym_rate=symbol_rate modul=modulation lo=local_oscillator timer=check_tune_time\n", argv[0]);
  printf( "[Lock DVB-S2:]\n%s fe=fontend_idx mode=5 freq=frequency sym_rate=symbol_rate modul=modulation lo=local_oscillator timer=check_tune_time\n", argv[0]);

  printf( "\nfreq(MHZ) lo(MHz) timer(second)\n");
  printf( "mode:%s\n", help_mode);
  printf( "modulation:%s\n", help_modulation);
  printf( "bw:%s\n", help_bandwidth);
  printf( "tran_mode:%s\n", help_transmission_mode);
}

int main(int argc, char **argv)
{
    int i;
	int ret = -1;
	int check_tune_time;
	int mode = TUNE_MODE_UNKNOW;
	int fontend_id = INVALID_FD;
	int fontend_idx = 0;
	int frequency;   //MHz
	//DVB-C
	int symbol_rate;
	int modulation;
	//DVB-T
	int bandwidth;
	int transmission_mode;
	int guard_interval;
	int plp_id = 0;
	//DVB-S
	int local_oscillator;  //MHz
	dmd_delivery_t delivery;

    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "fe", 2))
            sscanf(argv[i], "fe=%i", &fontend_idx);
        else if (!strncmp(argv[i], "mode", 4))
            sscanf(argv[i], "mode=%i", &mode);
        else if (!strncmp(argv[i], "freq", 4))
            sscanf(argv[i], "freq=%i", &frequency);
        else if (!strncmp(argv[i], "sym_rate", 8))
            sscanf(argv[i], "sym_rate=%i", &symbol_rate);
        else if (!strncmp(argv[i], "modul", 5))
            sscanf(argv[i], "modul=%i", &modulation);
        else if (!strncmp(argv[i], "bw", 2))
            sscanf(argv[i], "bw=%i", &bandwidth);
        else if (!strncmp(argv[i], "tran_mode", 9))
            sscanf(argv[i], "tran_mode=%i", &transmission_mode);
        else if (!strncmp(argv[i], "plp", 3))
            sscanf(argv[i], "plp=%i", &plp_id);
        else if (!strncmp(argv[i], "lo", 2))
            sscanf(argv[i], "lo=%i", &local_oscillator);
        else if (!strncmp(argv[i], "timer", 5))
            sscanf(argv[i], "timer=%i", &check_tune_time);
        else if (!strncmp(argv[i], "help", 4)) {
            usage(argc, argv);
            exit(0);
        }
    }

    if (argc == 1) {
        usage(argc, argv);
        exit(0);
    }

    if (open_fend(fontend_idx, &fontend_id) != 0)
		return 0;

    switch (mode)
	{
        case TUNE_MODE_DVB_C:
			delivery.device_type = DMD_CABLE;
		    delivery.delivery.cable.frequency = frequency * 1000;
			delivery.delivery.cable.modulation = modulation;
			delivery.delivery.cable.symbol_rate = symbol_rate;
			ret = dmd_lock_c(fontend_id, &delivery);
			printf("DVB-C: lock to freq:%d, modulation:%d symbol_rate:%d ret:%d \n", frequency, modulation, symbol_rate, ret);
            break;
        case TUNE_MODE_DVB_T:
			delivery.device_type = DMD_TERRESTRIAL;
		    delivery.delivery.terrestrial.dvb_type = DMD_DVBTYPE_DVBT;
			delivery.delivery.terrestrial.desc.dvbt.frequency = frequency * 1000;
			delivery.delivery.terrestrial.desc.dvbt.bandwidth = bandwidth;
			delivery.delivery.terrestrial.desc.dvbt.transmission_mode = transmission_mode;
			delivery.delivery.terrestrial.desc.dvbt.guard_interval = DMD_GUARD_INTERVAL_1_8;
			ret = dmd_lock_t(fontend_id, &delivery);
			printf("DVB-T: lock to freq:%d, bandwidth:%d transmission_mode:%d ret:%d \n", frequency, bandwidth, transmission_mode, ret);
            break;
        case TUNE_MODE_DVB_T2:
			delivery.device_type = DMD_TERRESTRIAL;
		    delivery.delivery.terrestrial.dvb_type = DMD_DVBTYPE_DVBT2;
			delivery.delivery.terrestrial.desc.dvbt2.frequency = frequency * 1000;
			delivery.delivery.terrestrial.desc.dvbt2.bandwidth = bandwidth;
			delivery.delivery.terrestrial.desc.dvbt2.transmission_mode = transmission_mode;
			delivery.delivery.terrestrial.desc.dvbt2.guard_interval = DMD_GUARD_INTERVAL_1_8;
			delivery.delivery.terrestrial.desc.dvbt2.plp_id = plp_id;
			ret = dmd_lock_t(fontend_id, &delivery);
			printf("DVB-T2: lock to freq:%d, bandwidth:%d transmission_mode:%d plp_id:%d ret:%d \n", frequency, bandwidth, transmission_mode, plp_id, ret);
			break;
        case TUNE_MODE_DVB_S:
		case TUNE_MODE_DVB_S2:
			delivery.device_type = DMD_SATELLITE;
			delivery.delivery.satellite.frequency = frequency * 1000;
		    delivery.delivery.satellite.modulation = modulation;
			delivery.delivery.satellite.modulation_system = (mode == TUNE_MODE_DVB_S2 ? DMD_MODSYS_DVBS2 : DMD_MODSYS_DVBS);
			delivery.delivery.satellite.symbol_rate = symbol_rate;
			delivery.delivery.satellite.lnb_tone_state = DMD_LNB_TONE_OFF;
			delivery.delivery.satellite.fec_rate = DMD_FEC_ALL;
			delivery.delivery.satellite.diseqc_port = DMD_DISEQC_ALL;
			delivery.delivery.satellite.polarization = DMD_PLR_HORIZONTAL;
			delivery.delivery.satellite.band.lo = local_oscillator * 1000;
			delivery.delivery.satellite.band.band_start = 0;
			delivery.delivery.satellite.band.band_end = 0;
			delivery.delivery.satellite.band.downlink = DMD_DOWNLINK_CUSTOM;
			ret = dmd_lock_s(fontend_id, &delivery, DMD_LNB_TONE_OFF, DMD_LNB_VOLTAGE_14V);
			printf("%s: lock to freq:%dMHz, modulation:%d symbol_rate:%d LO:%dMhz ret:%d\n", mode==TUNE_MODE_DVB_S2?"DVB-S2":"DVB-S", frequency, modulation, symbol_rate, local_oscillator, ret);
            break;
        case TUNE_MODE_UNKNOW:
        default:
            printf("tune mode unknow, mode:%d", mode);
            break;
    }

    if (ret)
    {
       printf("lock faild, ret:%d\n");
	   return -1;
	}

    int num = (check_tune_time > 0 ? check_tune_time : 60);
	printf("Will check tune status %ds\n", num);
	while(num--)
    {
       sleep(1);
       get_dmd_lock_status(fontend_id);
	}

    close_fend(fontend_id);
	return 0;
}


