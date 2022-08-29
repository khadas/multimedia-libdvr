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
  * \li lock DTMB
  *
  * \section Usage
  *
  * Help msg will be shown if the test runs without parameters.\n
  * There are some general concepts for the parameters:
  * \li fe: fontend_idx
  * \li mode: tune mode
  * \li freq: frequency MHz
  * \li sym_rate: symbol_rate
  * \li module: modulation
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
  * \li 6: DTMB
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
  *   am_fend_test [fe=fontend_idx] [mode=1] [freq=frequency] [sym_rate=symbol_rate] [module=modulation] [timer=check_tune_time]
  * \endcode
  * Lock DVB-T:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=2] [freq=frequency] [bw=bandwidth] [tran_mode=transmission_mode]    [timer=check_tune_time]
  * \endcode
  * Lock DVB-T2:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=3] [freq=frequency] [bw=bandwidth] [tran_mode=transmission_mode] [plp=plp_id] [timer=check_tune_time]
  * \endcode
  * Lock DVB-S:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=4] [freq=frequency] [sym_rate=symbol_rate] [module=modulation] [timer=check_tune_time]
  * \endcode
  * Lock DVB-S2:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=5] [freq=frequency] [sym_rate=symbol_rate] [module=modulation] [timer=check_tune_time]
  * \endcode
  * Lock DTMB:
  * \code
  *   am_fend_test [fe=fontend_idx] [mode=6] [freq=frequency] [bw=bandwidth] [timer=check_tune_time]
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
  * \brief fontend test code
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
#include <unistd.h>

#include "frontend.h"
#include "dvb_frontend_wrapper.h"

#define TUNE_MODE_UNKNOWN 0
#define TUNE_MODE_DVB_C 1
#define TUNE_MODE_DVB_T 2
#define TUNE_MODE_DVB_T2 3
#define TUNE_MODE_DVB_S 4
#define TUNE_MODE_DVB_S2 5
#define TUNE_MODE_DTMB 6
#define INVALID_FD -1




static char *help_mode =
    "\n 1: DVB-C"
    "\n 2: DVB-T"
    "\n 3: DVB-T2"
    "\n 4: DVB-S"
    "\n 5: DVB-S2"
    "\n 6: DTMB";
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
  printf("[Lock DVB-C:] \n%s fe=fontend_idx mode=1 freq=frequency sym_rate=symbol_rate module=modulation timer=check_tune_time\n", argv[0]);
  printf("[Lock DVB-T:] \n%s fe=fontend_idx mode=2 freq=frequency bw=bandwidth tran_mode=transmission_mode timer=check_tune_time\n", argv[0]);
  printf("[Lock DVB-T2:]\n%s fe=fontend_idx mode=3 freq=frequency bw=bandwidth  tran_mode=transmission_mode  plp=plp_id timer=check_tune_time\n", argv[0]);
  printf("[Lock DVB-S:] \n%s fe=fontend_idx mode=4 freq=frequency sym_rate=symbol_rate module=modulation timer=check_tune_time\n", argv[0]);
  printf("[Lock DVB-S2:]\n%s fe=fontend_idx mode=5 freq=frequency sym_rate=symbol_rate module=modulation timer=check_tune_time\n", argv[0]);
  printf("[Lock DTMB:]\n%s fe=fontend_idx mode=6 freq=frequency bw=bandwidth timer=check_tune_time\n", argv[0]);

  printf("\n""freq(MHZ) lo(MHz) timer(second)\n");
  printf("mode:%s\n", help_mode);
  printf("modulation:%s\n", help_modulation);
  printf("bw:%s\n", help_bandwidth);
  printf("tran_mode:%s\n", help_transmission_mode);
}

int main(int argc, char **argv)
{
  int i;
  int ret = -1;
  int check_tune_time;
  int mode = TUNE_MODE_UNKNOWN;
  int fontend_id = INVALID_FD;
  int fontend_idx = 0;
  int frequency; //MHz
  //DVB-C
  int symbol_rate;
  int modulation;
  //DVB-T
  int bandwidth;
  int transmission_mode;
  int guard_interval;
  int plp_id = 0;
  //DVB-S
  //int local_oscillator; //MHz
  //dmd_delivery_t delivery;
  dmd_satellite_desc_t satellite;
  dmd_cable_desc_t cable;
  dmd_terrestrial_desc_t terrestrial;

  for (i = 1; i < argc; i++)
  {
    if (!strncmp(argv[i], "fe", 2))
      sscanf(argv[i], "fe=%i", &fontend_idx);
    else if (!strncmp(argv[i], "mode", 4))
      sscanf(argv[i], "mode=%i", &mode);
    else if (!strncmp(argv[i], "freq", 4))
      sscanf(argv[i], "freq=%i", &frequency);
    else if (!strncmp(argv[i], "sym_rate", 8))
      sscanf(argv[i], "sym_rate=%i", &symbol_rate);
    else if (!strncmp(argv[i], "module", 5))
      sscanf(argv[i], "module=%i", &modulation);
    else if (!strncmp(argv[i], "bw", 2))
      sscanf(argv[i], "bw=%i", &bandwidth);
    else if (!strncmp(argv[i], "tran_mode", 9))
      sscanf(argv[i], "tran_mode=%i", &transmission_mode);
    else if (!strncmp(argv[i], "plp", 3))
      sscanf(argv[i], "plp=%i", &plp_id);
    //else if (!strncmp(argv[i], "lo", 2))
      //sscanf(argv[i], "lo=%i", &local_oscillator);
    else if (!strncmp(argv[i], "timer", 5))
      sscanf(argv[i], "timer=%i", &check_tune_time);
    else if (!strncmp(argv[i], "help", 4))
    {
      usage(argc, argv);
      exit(0);
    }
  }

  if (argc == 1)
  {
    usage(argc, argv);
    exit(0);
  }

  char fe_name[24];
  struct stat file_status;
  snprintf(fe_name, sizeof(fe_name), "/dev/dvb0.frontend%u", fontend_idx);
  if (stat(fe_name, &file_status) == 0)
  {
    printf("Found FE[%s]\n", fe_name);
  }
  else
  {
    printf("No FE found [%s]!", fe_name);
    return -1;
  }
  if (AML_FE_Open(fe_name, &fontend_id) != 0)
    return 0;

  switch (mode)
  {
  case TUNE_MODE_DVB_C:
    cable.frequency = frequency * 1000;
    cable.modulation = modulation;
    cable.symbol_rate = symbol_rate;
    ret = AML_FE_TuneDVB_C(fontend_id, &cable);
    printf("DVB-C: lock to freq:%d, modulation:%d symbol_rate:%d ret:%d \n", frequency, modulation, symbol_rate, ret);
    break;
  case TUNE_MODE_DVB_T:

    terrestrial.dvb_type = DMD_DVBTYPE_DVBT;
    terrestrial.desc.dvbt.frequency = frequency * 1000;
    terrestrial.desc.dvbt.bandwidth = bandwidth;
    terrestrial.desc.dvbt.transmission_mode = transmission_mode;
    terrestrial.desc.dvbt.guard_interval = DMD_GUARD_INTERVAL_1_8;
    ret = AML_FE_TuneDVB_T(fontend_id, &terrestrial);
    printf("DVB-T: lock to freq:%d, bandwidth:%d transmission_mode:%d ret:%d \n", frequency, bandwidth, transmission_mode, ret);
    break;
  case TUNE_MODE_DVB_T2:
    terrestrial.dvb_type = DMD_DVBTYPE_DVBT2;
    terrestrial.desc.dvbt2.frequency = frequency * 1000;
    terrestrial.desc.dvbt2.bandwidth = bandwidth;
    terrestrial.desc.dvbt2.transmission_mode = transmission_mode;
    terrestrial.desc.dvbt2.guard_interval = DMD_GUARD_INTERVAL_1_8;
    terrestrial.desc.dvbt2.plp_id = plp_id;
    ret = AML_FE_TuneDVB_T(fontend_id, &terrestrial);
    printf("DVB-T2: lock to freq:%d, bandwidth:%d transmission_mode:%d plp_id:%d ret:%d \n", frequency, bandwidth, transmission_mode, plp_id, ret);
    break;
  case TUNE_MODE_DVB_S:
  case TUNE_MODE_DVB_S2:
    satellite.frequency = frequency * 1000;
    satellite.modulation = modulation;
    satellite.modulation_system = (mode == TUNE_MODE_DVB_S2 ? DMD_MODSYS_DVBS2 : DMD_MODSYS_DVBS);
    satellite.symbol_rate = symbol_rate;
    satellite.fec_rate = DMD_FEC_ALL;
    AML_FE_LnbTone(fontend_id, DMD_LNB_TONE_OFF);
    AML_FE_LnbVoltage(fontend_id, DMD_LNB_VOLTAGE_OFF);
    ret = AML_FE_TuneDVB_S(fontend_id, &satellite);
    printf("%s: lock to freq:%dMHz, modulation:%d symbol_rate:%d ret:%d\n", mode == TUNE_MODE_DVB_S2 ? "DVB-S2" : "DVB-S", frequency, modulation, symbol_rate, ret);
    break;
  case TUNE_MODE_DTMB:
    terrestrial.dvb_type = DMD_DVBTYPE_DTMB;
    terrestrial.desc.dtmb.frequency = frequency * 1000;
    terrestrial.desc.dtmb.bandwidth = bandwidth;
    ret = AML_FE_TuneDVB_T(fontend_id, &terrestrial);
    printf("DTMB: lock to freq:%d, bandwidth:%d ret:%d \n", frequency, bandwidth, ret);
    break;
  case TUNE_MODE_UNKNOWN:
  default:
    printf("tune mode unknown, mode:%d", mode);
    break;
  }

  if (ret)
  {
    printf("lock faild, ret:%d\n", ret);
    return -1;
  }

  int num = (check_tune_time > 0 ? check_tune_time : 60);
  printf("Will check tune status %ds\n", num);
  dmd_tuner_event_t event = TUNER_STATE_UNKNOWN;
  while (num--)
  {
    sleep(1);
    event = AML_FE_GetTuneStatus(fontend_id);
    printf("####### [%s] #######\n", event == TUNER_STATE_LOCKED ? "LOCKED" : "UNLOCKED");
  }

  AML_FE_Close(fontend_id);
  return 0;
}
