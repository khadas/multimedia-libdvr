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
#include "dvr_playback.h"

int main(int argc, char **argv)
{
  int ret=0;
  DVR_PlaybackHandle_t handle = 0;
  DVR_PlaybackOpenParams_t params;
  params.dmx_dev_id = 1;
  DVR_PlaybackSegmentInfo_t info;
  info.segment_id = 0;
  info.flags = 1;
  memcpy(info.location, "/data/data/", 12);
  info.pids.video.pid = 0x38;
  info.pids.video.format = 0x38;
  info.pids.audio.pid = 0x39;
  info.pids.audio.format = 0x39;
  info.pids.pcr.pid = 0x30;
  info.pids.pcr.format = 0x30;
  info.pids.ad.pid = 0x31;
  info.pids.ad.format = 0x31;

  printf("open dvr playback device\r\n");
  dvr_playback_open(&handle, &params);
  //add chunk info
  dvr_playback_add_segment(handle, &info);
  dvr_dump_segmentinfo(handle, -1);
  info.segment_id = 1;
  info.flags = 2;
  memcpy(info.location, "/data/data/", 12);
  info.pids.video.pid = 0x48;
  info.pids.video.format = 0x48;
  info.pids.audio.pid = 0x49;
  info.pids.audio.format = 0x49;
  info.pids.pcr.pid = 0x40;
  info.pids.pcr.format = 0x40;
  info.pids.ad.pid = 0x41;
  info.pids.ad.format = 0x41;
  dvr_playback_add_segment(handle, &info);
  dvr_dump_segmentinfo(handle, -1);
  info.segment_id = 2;
  info.flags = 3;
  memcpy(info.location, "/data/data/", 12);
  info.pids.video.pid = 0x58;
  info.pids.video.format = 0x58;
  info.pids.audio.pid = 0x59;
  info.pids.audio.format = 0x59;
  info.pids.pcr.pid = 0x50;
  info.pids.pcr.format = 0x50;
  info.pids.ad.pid = 0x51;
  info.pids.ad.format = 0x51;
  dvr_playback_add_segment(handle, &info);
  dvr_dump_segmentinfo(handle, -1);
  //updata segment info
  dvr_playback_update_segment_flags(handle, 0, 7);
  //printf segment info
  dvr_dump_segmentinfo(handle, -1);
  DVR_PlaybackPids_t pids;
  pids.video.pid = 0x1fff;
  pids.video.format = 0x78;
  pids.audio.pid = 0x79;
  pids.audio.format = 0x79;
  pids.pcr.pid = 0x70;
  pids.ad.pid = 0x1fff;
  pids.pcr.format = 0x70;
  pids.ad.format = 0x71;
  dvr_playback_update_segment_pids(handle, 1, &pids);
  dvr_dump_segmentinfo(handle, -1);
  //remove chunk
  dvr_playback_remove_segment(handle, 2);
  //printf chunk info
  dvr_dump_segmentinfo(handle, -1);
  return ret;
}

