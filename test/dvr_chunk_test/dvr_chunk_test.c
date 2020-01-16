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
  DVR_Playback_Handle_t handle = 0;
  DVR_PlayBack_OpenParams_t params;
  params.dmx = 1;
  DVR_Playback_Chunk_Info_t info;
  info.chunk_id = 0;
  info.flags = 1;
  memcpy(info.location, "/data/data/", 12);
  info.pids.vpid.pid = 0x38;
  info.pids.vpid.fmt = 0x38;
  info.pids.apid.pid = 0x39;
  info.pids.apid.fmt = 0x39;
  info.pids.pcrpid.pid = 0x30;
  info.pids.pcrpid.fmt = 0x30;
  info.pids.sub_apid.pid = 0x31;
  info.pids.sub_apid.fmt = 0x31;

  printf("open dvr playback device\r\n");
  dvr_playback_open(&handle, &params);
  //add chunk info
  dvr_playback_add_chunk(handle, &info);
  dvr_dump_chunkinfo(handle, -1);
  info.chunk_id = 1;
  info.flags = 2;
  memcpy(info.location, "/data/data/", 12);
  info.pids.vpid.pid = 0x48;
  info.pids.vpid.fmt = 0x48;
  info.pids.apid.pid = 0x49;
  info.pids.apid.fmt = 0x49;
  info.pids.pcrpid.pid = 0x40;
  info.pids.pcrpid.fmt = 0x40;
  info.pids.sub_apid.pid = 0x41;
  info.pids.sub_apid.fmt = 0x41;
  dvr_playback_add_chunk(handle, &info);
  dvr_dump_chunkinfo(handle, -1);
  info.chunk_id = 2;
  info.flags = 3;
  memcpy(info.location, "/data/data/", 12);
  info.pids.vpid.pid = 0x58;
  info.pids.vpid.fmt = 0x58;
  info.pids.apid.pid = 0x59;
  info.pids.apid.fmt = 0x59;
  info.pids.pcrpid.pid = 0x50;
  info.pids.pcrpid.fmt = 0x50;
  info.pids.sub_apid.pid = 0x51;
  info.pids.sub_apid.fmt = 0x51;
  dvr_playback_add_chunk(handle, &info);
  dvr_dump_chunkinfo(handle, -1);
  //updata chunk info
  dvr_playback_Update_Chunk_Flags(handle, 0, 7);
  //printf chunk info
  dvr_dump_chunkinfo(handle, -1);
  DVR_Playback_Pids_t pids;
  pids.vpid.pid = 0x1fff;
  pids.vpid.fmt = 0x78;
  pids.apid.pid = 0x79;
  pids.apid.fmt = 0x79;
  pids.pcrpid.pid = 0x70;
  pids.sub_apid.pid = 0x1fff;
  pids.pcrpid.fmt = 0x70;
  pids.sub_apid.fmt = 0x71;
  dvr_playback_Update_Chunk_Pids(handle, 1, &pids);
  dvr_dump_chunkinfo(handle, -1);
  //remove chunk
  dvr_playback_remove_chunk(handle, 2);
  //printf chunk info
  dvr_dump_chunkinfo(handle, -1);
  return ret;
}

