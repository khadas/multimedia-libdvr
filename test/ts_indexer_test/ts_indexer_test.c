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

static void ts_indexer_event_cb(TS_Indexer_t *ts_indexer, TS_Indexer_Event_t *event)
{
  if (ts_indexer == NULL || event == NULL)
    return;

  switch (event->type) {
    case TS_INDEXER_EVENT_TYPE_VIDEO_I_FRAME:
      INF("pid: %#x ", event->pid);
      INF("TS_INDEXER_EVENT_TYPE_VIDEO_I_FRAME, offset: %lx, pts: %lx\n",
            event->offset, event->pts);
      break;

    case TS_INDEXER_EVENT_TYPE_VIDEO_PTS:
      #if 0
      INF("PID: %#x ", event->pid);
      INF("TS_INDEXER_EVENT_TYPE_VIDEO_PTS, Offset: %lx, Pts: %lx\n",
            event->offset, event->pts);
      #endif
      break;

    case TS_INDEXER_EVENT_TYPE_AUDIO_PTS:
      #if 0
      INF("PID: %#x ", event->pid);
      INF("TS_INDEXER_EVENT_TYPE_AUDIO_PTS, Offset: %lx, Pts: %lx\n",
            event->offset, event->pts);
      #endif
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
    (vpid == 0x1fff && apid == 0x1fff) ||
    (vpid != 0x1fff && vfmt == -1))
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
  while (1) {
    len = fread(data, 1, block_size, f);
    if (len <= 0)
      break;

    ts_indexer_parse(&ts_indexer, data, len);
  }

  return 0;
}
