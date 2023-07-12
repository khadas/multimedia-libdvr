/**
 * \page ts_indexer_test
 * \section Introduction
 * test code with ts_indexer_xxxxx APIs.
 * It supports:
 * \li parse pts/i-frame from MPEG2/H264/HEVC transport stream
 *
 * \section Usage
 *
 * \li ts: bitstream file path
 * \li vpid: the pid of video bitstream with pts/i-frame
 * \li vfmt: the video format
 * \li apid: the pid of audio bitstream with pts
 *
 *
 * parse pts/i-frame:
 * \code
 *    ts_indexer_test [ts=] [vpid=] [vfmt=] [apid=]
 * \endcode
 *
 * \endsection
 */

#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ts_indexer.h"

#define INF(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
#define ERR(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

typedef struct
{
  uint8_t *ptr;
  uint64_t last_pts;
  uint64_t last_offset;
  FILE *dump_file;
} ts_indexer_ctl;

static ts_indexer_ctl gControl;

static void ts_indexer_event_cb(TS_Indexer_t *ts_indexer, TS_Indexer_Event_t *event)
{
  int write_len;

  if (ts_indexer == NULL || event == NULL)
    return;

  switch (event->type) {
    case TS_INDEXER_EVENT_TYPE_MPEG2_I_FRAME:
      #if 1
      INF("pid: %#x ", event->pid);
      INF("TS_INDEXER_EVENT_TYPE_I_FRAME, offset: %lx, pts: %lx\n",
            event->offset, event->pts);
      #endif
      break;

    case TS_INDEXER_EVENT_TYPE_VIDEO_PTS:
      #if 0
      INF("PID: %#x ", event->pid);
      INF("TS_INDEXER_EVENT_TYPE_VIDEO_PTS, Offset: %lx, Lastoffset: %lx, Pts: %lx\n",
            event->offset, gControl.last_offset, event->pts);
      write_len = ts_indexer->offset - gControl.last_offset;
      if (gControl.dump_file) {
        INF("ptr: %p, write_len: %#x\n", gControl.ptr, write_len);
        fwrite(gControl.ptr, 1, write_len, gControl.dump_file);
        gControl.ptr += write_len;
        INF("ptr: %p\n", gControl.ptr);
      }
      gControl.last_offset = ts_indexer->offset;
      #endif
      break;

    case TS_INDEXER_EVENT_TYPE_AUDIO_PTS:
      #if 0
      INF("PID: %#x ", event->pid);
      INF("TS_INDEXER_EVENT_TYPE_AUDIO_PTS, Offset: %lx, Pts: %lx\n",
            event->offset, event->pts);
      #endif
      break;

    case TS_INDEXER_EVENT_TYPE_START_INDICATOR:
    case TS_INDEXER_EVENT_TYPE_DISCONTINUITY_INDICATOR:
    case TS_INDEXER_EVENT_TYPE_MPEG2_P_FRAME:
    case TS_INDEXER_EVENT_TYPE_MPEG2_B_FRAME:
    case TS_INDEXER_EVENT_TYPE_MPEG2_SEQUENCE:
    case TS_INDEXER_EVENT_TYPE_AVC_I_SLICE:
    case TS_INDEXER_EVENT_TYPE_AVC_P_SLICE:
    case TS_INDEXER_EVENT_TYPE_AVC_B_SLICE:
    case TS_INDEXER_EVENT_TYPE_AVC_SI_SLICE:
    case TS_INDEXER_EVENT_TYPE_AVC_SP_SLICE:
    case TS_INDEXER_EVENT_TYPE_HEVC_SPS:
    case TS_INDEXER_EVENT_TYPE_HEVC_AUD:
    case TS_INDEXER_EVENT_TYPE_HEVC_BLA_W_LP:
    case TS_INDEXER_EVENT_TYPE_HEVC_BLA_W_RADL:
    case TS_INDEXER_EVENT_TYPE_HEVC_IDR_W_RADL:
    case TS_INDEXER_EVENT_TYPE_HEVC_IDR_N_LP:
    case TS_INDEXER_EVENT_TYPE_HEVC_TRAIL_CRA:
      INF("type: %d, offset: %lx\n", event->type, event->offset);
      break;

    default:
      break;
  }

  return;
}

static char *help_vfmt =
  "\n\t0:TS_INDEXER_VIDEO_FORMAT_MPEG2" /**< MPEG2 video.*/
  "\n\t1:TS_INDEXER_VIDEO_FORMAT_H264" /**< H264.*/
  "\n\t2:TS_INDEXER_VIDEO_FORMAT_HEVC"; /**<HEVC.*/

static void usage(int argc, char *argv[])
{
  INF("Usage: %s [ts=] [vpid=] [vfmt=] [apid=]\n", argv[0]);
  INF("Usage: %s\n", help_vfmt);
}

int main(int argc, char **argv)
{
  int i;
  char file[512];
  int vpid = 0x1fff;
  int apid = 0x1fff;
  int vfmt = -1;
  TS_Indexer_t ts_indexer;
  int block_size = 188*1024;

  memset(&file[0], 0, sizeof(file));
  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "ts=", 3))
      sscanf(argv[i], "ts=%s", &file[0]);
    else if (!strncmp(argv[i], "vpid=", 5))
      sscanf(argv[i], "vpid=%i", &vpid);
    else if (!strncmp(argv[i], "vfmt=", 5))
      sscanf(argv[i], "vfmt=%i", &vfmt);
    else if (!strncmp(argv[i], "apid=", 5))
      sscanf(argv[i], "apid=%i", &apid);
    else if (!strncmp(argv[i], "help", 4)) {
      usage(argc, argv);
      exit(0);
    }
  }

  if (argc == 1 ||
    (vpid == 0x1fff && apid == 0x1fff))
  {
    usage(argc, argv);
    exit(0);
  }

  memset(&ts_indexer, 0, sizeof(TS_Indexer_t));
  ts_indexer_init(&ts_indexer);
  ts_indexer_set_video_pid(&ts_indexer, vpid);
  ts_indexer_set_audio_pid(&ts_indexer, apid);
  ts_indexer_set_video_format(&ts_indexer, vfmt);
  ts_indexer_set_event_callback(&ts_indexer, ts_indexer_event_cb);

  FILE *f = fopen(file, "rb");
  if (f == NULL) {
    ERR("open %s failed!\n", file);
    return -1;
  }

  uint8_t *data = malloc(block_size);
  if (data == NULL) {
    ERR("no heap memory!\n");
    return -1;
  }

  size_t len = 0;
  INF("vpid: %#x, vfmt: %d, apid:%#x\n", vpid, vfmt, apid);
  memset(&gControl, 0, sizeof(ts_indexer_ctl));
  gControl.last_pts = -1;
  gControl.last_offset = 0;
  gControl.dump_file = fopen("./dump.ts", "wb+");
  if (gControl.dump_file == NULL) {
    ERR("open dump file failed\n");
    return -1;
  }

  INF("ptr: %p ~ %p\n", &data[0], &data[0] + block_size);
  while (1) {
    len = fread(data, 1, block_size, f);
    if (len <= 0)
      break;

    gControl.ptr = &data[0];
    ts_indexer_parse(&ts_indexer, data, len);
    if (gControl.ptr - &data[0] < len) {
      int left = len - (gControl.ptr - &data[0]);
      fwrite(gControl.ptr, 1, left, gControl.dump_file);
      gControl.last_offset += left;
    }
  }
  fflush(gControl.dump_file);
  fclose(gControl.dump_file);

  return 0;
}
