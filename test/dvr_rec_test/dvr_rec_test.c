#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 * \brief
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-06-07: create the document
 ***************************************************************************/
#include "stdio.h"
#include <string.h>
#include <stdlib.h>
#include "dvr_record.h"

static DVR_Result_t wrapper_record_event_handler(DVR_RecordEvent_t event, void *params, void *userdata)
{
  return 0;
}

int main(int argc, char **argv)
{
  int error = 0;
  DVR_RecordHandle_t recorder;
  DVR_RecordOpenParams_t open_param;
  DVR_RecordStartParams_t start_param;
  int dmx = 0;
  int vpid = 611;
  int vfmt = 0;
  int apid = 612;
  int afmt = 0;
  int  i = 0;
  for (i = 1; i < argc; i++) {
      if (!strncmp(argv[i], "v", 1))
          sscanf(argv[i], "v=%i:%i", &vpid, &vfmt);
      else if (!strncmp(argv[i], "a", 1))
          sscanf(argv[i], "a=%i:%i", &apid, &afmt);
      else if (!strncmp(argv[i], "dmx", 3))
          sscanf(argv[i], "dmx=%i", &dmx);
      else if (!strncmp(argv[i], "help", 4)) {
          printf("Usage: %s  [dmx=id] [v=pid:fmt] [a=pid:fmt] \n", argv[0]);
          exit(0);
      }
  }

  memset(&open_param, 0, sizeof(open_param));
  open_param.dmx_dev_id = dmx;
  printf("dmx:%d\n", dmx);
  open_param.data_from_memory = 0;
  open_param.flags = 0;
  open_param.notification_size = (1000*1024 + 2);

  open_param.event_fn = wrapper_record_event_handler;
  open_param.event_userdata = NULL;
  printf("rec---open \r\n");
  error = dvr_record_open(&recorder, &open_param);
  printf("rec---open end\r\n");
  memset(&start_param, 0, sizeof(start_param));

  strncpy(start_param.location, "/data/data/0000-", strlen("/data/data/0000-"));
  start_param.segment.segment_id = 0;
  start_param.segment.nb_pids = 2;
  start_param.segment.pids[0].pid = vpid;
  start_param.segment.pids[0].type = DVR_STREAM_TYPE_VIDEO;
  start_param.segment.pid_action[0] = DVR_RECORD_PID_CREATE;
  start_param.segment.pids[1].pid = apid;
  start_param.segment.pids[1].type = DVR_STREAM_TYPE_VIDEO;
  start_param.segment.pid_action[1] = DVR_RECORD_PID_CREATE;
  printf("rec---start \r\n");
  error = dvr_record_start_segment(recorder, &start_param);
  printf("rec---start end \r\n");
  sleep(5*60);
  error = dvr_record_stop_segment(recorder, NULL);
  error = dvr_record_close(recorder);
  return error;
}

