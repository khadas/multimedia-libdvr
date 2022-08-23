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

#include "dvr_playback.h"

static void display_usage(void)
{
    fprintf(stderr, "dvr_play_test v=vpid:vfmt a=apid:afmt segid=segmentid location=dir\n");
    fprintf(stderr, "==================\n");
    fprintf(stderr, "*startpause\n");
    fprintf(stderr, "*play\n");
    fprintf(stderr, "*pause\n");
    fprintf(stderr, "*resume\n");
    fprintf(stderr, "*ff speed(1=1X,2=4X,3=6X)\n");
    fprintf(stderr, "*fb speed(1=1X,2=4X,3=6X)\n");
    fprintf(stderr, "*seek time_in_ms\n");
    fprintf(stderr, "*quit\n");
    fprintf(stderr, "==================\n");
}

int start_playback_test(DVR_PlaybackHandle_t handle)
{
    DVR_Bool_t  go = DVR_TRUE;
    char buf[256];
    display_usage();

    while (go) {
        if (fgets(buf, sizeof(buf), stdin)) {
            if (!strncmp(buf, "quit", 4u)) {
                go = DVR_FALSE;
                continue;
            }
            else if (!strncmp(buf, "startpause", 10u)) {
              int flags = 0;
              flags = flags | DVR_PLAYBACK_STARTED_PAUSEDLIVE;
              dvr_playback_start(handle, flags);
            }
            else if (!strncmp(buf, "play", 4u)) {
              dvr_playback_start(handle, 0);
            }
            else if (!strncmp(buf, "pause", 5u)) {
                dvr_playback_pause(handle, 1);
            }
            else if (!strncmp(buf, "resume", 6u)) {
                dvr_playback_resume(handle);
            }
            else if (!strncmp(buf, "ff", 2u)) {
               int speed;
                sscanf(buf + 2, "%d", &speed);
                printf("fast forward speed is %d\n", speed);
                DVR_PlaybackSpeed_t speeds;
                speeds.mode = DVR_PLAYBACK_FAST_FORWARD;
                speeds.speed.speed = speed;
                dvr_playback_set_speed(handle, speeds);
            }
            else if (!strncmp(buf, "fb", 2u)) {
                int speed;
                sscanf(buf + 2, "%d", &speed);
                printf("fast forward speed is %d\n",speed);
                DVR_PlaybackSpeed_t speeds;
                speeds.mode = DVR_PLAYBACK_FAST_BACKWARD;
                speeds.speed.speed = speed;
                dvr_playback_set_speed(handle, speeds);
            }
            else if (!strncmp(buf, "seek", 4u)) {
                int time;
                sscanf(buf + 4, "%d", &time);
                dvr_playback_seek(handle, 0,time);
            } else {
                fprintf(stderr, "Unknown command: %s\n", buf);
                display_usage();
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
  am_tsplayer_handle device_handle;
  am_tsplayer_init_params dev_params;

  int vpid = 2064, apid = 2068, vfmt = DVR_VIDEO_FORMAT_MPEG1, afmt = DVR_AUDIO_FORMAT_MPEG;
  int bsize = 256 * 1024;
  int pause = 0;
  int segid = 0;
  int dmx = 0;
  int i;
  display_usage();
  if (argc < 4) {
    printf("input param is error");
    return 0;
  }
  //add chunk info
  DVR_PlaybackSegmentInfo_t info;
  memset(&info, 0, sizeof(DVR_PlaybackSegmentInfo_t));


  for (i = 1; i < argc; i++) {
      if (!strncmp(argv[i], "v", 1))
          sscanf(argv[i], "v=%i:%i", &vpid, &vfmt);
      else if (!strncmp(argv[i], "a", 1))
          sscanf(argv[i], "a=%i:%i", &apid, &afmt);
      else if (!strncmp(argv[i], "bsize", 4))
          sscanf(argv[i], "bsize=%i", &bsize);
      else if (!strncmp(argv[i], "dmx", 3))
          sscanf(argv[i], "dmx=%i", &dmx);
      else if (!strncmp(argv[i], "pause", 5))
          sscanf(argv[i], "pause=%i", &pause);
      else if (!strncmp(argv[i], "segid", 5))
          sscanf(argv[i], "segid=%i", &segid);
      else if (!strncmp(argv[i], "location", 8))
          sscanf(argv[i], "location=%s", info.location);
      else if (!strncmp(argv[i], "help", 4)) {
          printf("Usage: %s  [dmx=id] [segid=id] [v=pid:fmt] [a=pid:fmt] [bsize=size] [pause=n]\n", argv[0]);
          exit(0);
      }
  }

  printf("video:%d:%d(pid/fmt) audio:%d:%d(pid/fmt)\n", vpid, vfmt, apid, afmt);
  printf("segid:%d bsize:%d dmx:%d\n", segid, bsize, dmx);
  printf("pause:%d\n", pause);
  printf("info.location:%s\n", info.location);
  memset(&dev_params, 0, sizeof(dev_params));
  dev_params.dmx_dev_id = dmx;
  dev_params.source = TS_MEMORY;

  int ret = AmTsPlayer_create(dev_params, &device_handle);
  AmTsPlayer_setWorkMode(device_handle, TS_PLAYER_MODE_NORMAL);
  AmTsPlayer_setSyncMode(device_handle, TS_SYNC_AMASTER);

  DVR_PlaybackHandle_t handle = 0;
  DVR_PlaybackOpenParams_t params;
  params.dmx_dev_id = dmx;
  params.block_size = bsize;
  params.player_handle = device_handle;

  printf("open dvr playback device\r\n");
  dvr_playback_open(&handle, &params);

  info.segment_id = segid;
  info.flags = 0;
  info.flags = info.flags | DVR_PLAYBACK_SEGMENT_DISPLAYABLE;
  info.flags = info.flags | DVR_PLAYBACK_SEGMENT_CONTINUOUS;
  info.pids.video.pid = vpid;
  info.pids.video.format = vfmt;
  info.pids.video.type = DVR_STREAM_TYPE_VIDEO;
  info.pids.audio.pid = apid;
  info.pids.audio.format = afmt;
  info.pids.audio.type = DVR_STREAM_TYPE_AUDIO;
  dvr_playback_add_segment(handle, &info);
  dvr_playback_seek(handle, info.segment_id, 0);
  start_playback_test(handle);
  dvr_playback_stop(handle, 0);
  dvr_playback_close(handle);
  AmTsPlayer_release(device_handle);
  return ret;
}

