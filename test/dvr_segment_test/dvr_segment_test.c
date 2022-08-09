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
#include "segment.h"

int main(int argc, char **argv)
{
  int ret=0;
  Segment_Handle_t r_handle = NULL;
  int id = 1;
#define BUF_LEN 1024
  char buf[BUF_LEN];
  memset(buf, 0, BUF_LEN);

  Segment_OpenParams_t params;

  memset(params.location, 0, DVR_MAX_LOCATION_SIZE);
  strncpy(params.location, "/data/pvr/tsthal_rec1", strlen("/data/pvr/tsthal_rec1"));

  params.segment_id = (uint64_t)id;
  params.mode = SEGMENT_MODE_READ;
  printf("start open segment read mode\r\n");
  //dump file len write ptr pos
  segment_open(&params, &r_handle);
  if (r_handle == NULL) {
      printf("read error\n");
      return 0;
  }
  //dump pcr info
  segment_dump_pts(r_handle);
  int read_len = 0;
  segment_seek(r_handle, 15000);
  printf( "read offset[%d]\r\n", (int)segment_tell_position(r_handle));
  printf( "cur time[%d]\r\n",(int)segment_tell_current_time(r_handle));
  //dump file len read pts pos
  printf( "total time[%d]\r\n",(int)segment_tell_total_time(r_handle));
  segment_close(r_handle);
  return ret;
}

