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
  Segment_Handle_t w_handle = NULL;
  Segment_Handle_t r_handle = NULL;
  int id = 1;
#define BUF_LEN 1024
  char buf[BUF_LEN];
  memset(buf, 0, BUF_LEN);
  Segment_OpenParams_t wparams;

  memset(wparams.location, 0, DVR_MAX_LOCATION_SIZE);
  strncpy(wparams.location, "/data/data/", strlen("/data/data/"));
  wparams.mode = SEGMENT_MODE_WRITE;
  wparams.segment_id = (uint64_t)id;
  Segment_OpenParams_t rparams;

  memset(rparams.location, 0, DVR_MAX_LOCATION_SIZE);
  strncpy(rparams.location, "/data/data/", strlen("/data/data/"));

  rparams.segment_id = (uint64_t)id;
  rparams.mode = SEGMENT_MODE_READ;

  segment_open(&wparams, &w_handle);
  //inject data and update pcr
  int i = 0;
  for (i = 0; i < 100; i++) {
      buf[i] = i & 0xff;
      segment_write(w_handle, buf, BUF_LEN);
      segment_update_pts(w_handle, i, (off_t)((i + 1) * BUF_LEN));
      printf( "write offset[%d] pts[%d] tell[%lld]\r\n", (i + 1) * BUF_LEN, i, segment_tell(w_handle));
  }
  printf("start open segment read mode\r\n");
  //dump file len write ptr pos
  segment_open(&rparams, &r_handle);
  if (r_handle == NULL) {
      printf("read error\n");
      return 0;
  }
  //dump pcr info
  segment_dump_pts(r_handle);
  int read_len = 0;
  //dump file len, read ptr pos
  for (i = 0; i < 100; i++) {
      memset(buf, 0 , BUF_LEN);
      read_len = segment_read(r_handle, buf, BUF_LEN);
      printf( "read buf[%d][%d][%d] read_len= %d BUF_LEN=%d offset=%d\r\n", i, buf[i], buf[i + 1], read_len, BUF_LEN, (int)segment_tell(r_handle));
  }
  printf( "read offset[%d]\r\n", (int)segment_tell(r_handle));
  //continus to write data
  segment_write(w_handle, buf, BUF_LEN);
  printf( "read offset[%d]\r\n",(int)segment_tell(r_handle));
  segment_write(w_handle, buf, BUF_LEN);
  //dump file len read pts pos
  printf( "read offset[%d]\r\n",(int)segment_tell(r_handle));
  segment_close(w_handle);
  segment_close(r_handle);
  return ret;
}

