#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "dvr_common.h"
#include "segment.h"

#define MAX_SEGMENT_FD_COUNT (128)
#define MAX_SEGMENT_PATH_SIZE (DVR_MAX_LOCATION_SIZE + 32)
#define MAX_PTS_THRESHOLD (10*1000)
typedef struct {
  //char location[DVR_MAX_LOCATION_SIZE];
  //uint64_t segment_id;
  FILE            *ts_fp;                    /**< segment ts file fd*/
  FILE            *index_fp;                 /**< time index file fd*/
  uint64_t        first_pts;                 /**< first pts value, use for write mode*/
  uint64_t        last_pts;                  /**< last pts value, use for write mode*/
} Segment_Context_t;

typedef enum {
  SEGMENT_FILE_TYPE_TS,
  SEGMENT_FILE_TYPE_INDEX,
} Segment_FileType_t;

static void segment_get_fname(char fname[MAX_SEGMENT_PATH_SIZE],
    char location[DVR_MAX_LOCATION_SIZE],
    uint64_t segment_id,
    Segment_FileType_t type)
{
  int offset;

  memset(fname, 0, MAX_SEGMENT_PATH_SIZE);
  strncpy(fname, location, strlen(location));
  offset = strlen(location);
  strncpy(fname + offset, "-", 1);
  offset += 1;
  sprintf(fname + offset, "%04llu", segment_id);
  offset += 4;
  if (type == SEGMENT_FILE_TYPE_TS)
    strncpy(fname + offset, ".ts", 3);
  else if (type == SEGMENT_FILE_TYPE_INDEX)
    strncpy(fname + offset, ".idx", 4);
}

int segment_open(Segment_OpenParams_t *params, Segment_Handle_t *p_handle)
{
  Segment_Context_t *p_ctx;
  char ts_fname[MAX_SEGMENT_PATH_SIZE];
  char index_fname[MAX_SEGMENT_PATH_SIZE];

  DVR_ASSERT(params);
  DVR_ASSERT(p_handle);

  DVR_DEBUG(1, "%s, location:%s, id:%llu", __func__, params->location, params->segment_id);

  p_ctx = (void*)malloc(sizeof(Segment_Context_t));
  DVR_ASSERT(p_ctx);

  memset(ts_fname, 0, sizeof(ts_fname));
  segment_get_fname(ts_fname, params->location, params->segment_id, SEGMENT_FILE_TYPE_TS);

  memset(index_fname, 0, sizeof(index_fname));
  segment_get_fname(index_fname, params->location, params->segment_id, SEGMENT_FILE_TYPE_INDEX);

  if (params->mode == SEGMENT_MODE_READ) {
    p_ctx->ts_fp = fopen(ts_fname, "r");
    p_ctx->index_fp = fopen(index_fname, "r");
  } else if (params->mode == SEGMENT_MODE_WRITE) {
    p_ctx->ts_fp = fopen(ts_fname, "w+");
    p_ctx->index_fp = fopen(index_fname, "w+");
    p_ctx->first_pts = ULLONG_MAX;
    p_ctx->last_pts = ULLONG_MAX;
  } else {
    DVR_DEBUG(1, "%s, unknow mode use default", __func__);
    p_ctx->ts_fp = fopen(ts_fname, "r");
    p_ctx->index_fp = fopen(index_fname, "r");
  }

  if (!p_ctx->ts_fp || !p_ctx->index_fp) {
    DVR_DEBUG(1, "%s open file failed [%p, %p], reason:%s", __func__,
        p_ctx->ts_fp, p_ctx->index_fp, strerror(errno));
    free(p_ctx);
    *p_handle = NULL;
    return DVR_FAILURE;
  }
  DVR_DEBUG(1, "%s, open file sucess", __func__);
  *p_handle = (Segment_Handle_t)p_ctx;
  return DVR_SUCCESS;
}

int segment_close(Segment_Handle_t handle)
{
  Segment_Context_t *p_ctx;

  p_ctx = (void *)handle;
  DVR_ASSERT(p_ctx);

  if (p_ctx->ts_fp) {
    fclose(p_ctx->ts_fp);
  }

  if (p_ctx->index_fp) {
    fclose(p_ctx->index_fp);
  }

  free(p_ctx);
  return 0;
}

ssize_t segment_read(Segment_Handle_t handle, void *buf, size_t count)
{
  Segment_Context_t *p_ctx;

  p_ctx = (Segment_Context_t *)handle;
  DVR_ASSERT(p_ctx);
  DVR_ASSERT(buf);
  DVR_ASSERT(p_ctx->ts_fp);

  return fread(buf, 1, count, p_ctx->ts_fp);
}

ssize_t segment_write(Segment_Handle_t handle, void *buf, size_t count)
{
  Segment_Context_t *p_ctx;

  p_ctx = (Segment_Context_t *)handle;
  DVR_ASSERT(p_ctx);
  DVR_ASSERT(buf);
  DVR_ASSERT(p_ctx->ts_fp);

  //return fwrite(p_ctx->ts_fp, 1, count, buf);
  return fwrite(buf, 1, count, p_ctx->ts_fp);
}

int segment_update_pts(Segment_Handle_t handle, uint64_t pts, off_t offset)
{
  Segment_Context_t *p_ctx;
  char buf[256];

  p_ctx = (Segment_Context_t *)handle;
  DVR_ASSERT(p_ctx);
  DVR_ASSERT(p_ctx->index_fp);

  if (p_ctx->first_pts == ULLONG_MAX) {
    p_ctx->first_pts = pts;
  }
  memset(buf, 0, sizeof(buf));
  if (p_ctx->last_pts == ULLONG_MAX) {
    /*Last pts is init value*/
    sprintf(buf, "{time=%llu, offset=%ld}\n", pts - p_ctx->first_pts, offset);
  } else {
    /*Last pts has valid value*/
    if (pts - p_ctx->last_pts > MAX_PTS_THRESHOLD) {
      /*Current pts has a transition*/
      sprintf(buf, "{time=%llu, offset=%ld}\n", p_ctx->last_pts - p_ctx->first_pts, offset);
    } else {
      /*This is a normal pts, record it*/
      sprintf(buf, "{time=%llu, offset=%ld}\n", pts - p_ctx->first_pts, offset);
    }
  }

  fputs(buf, p_ctx->index_fp);
  p_ctx->last_pts = pts;
  fflush(p_ctx->index_fp);
  fsync(fileno(p_ctx->index_fp));
  //fdatasync(fileno(p_ctx->index_fp));
  return DVR_SUCCESS;
}

off_t segment_seek(Segment_Handle_t handle, uint64_t time)
{
  Segment_Context_t *p_ctx;
  char buf[256];
  char value[256];
  uint64_t pts;
  off_t offset;
  char *p1, *p2;

  p_ctx = (Segment_Context_t *)handle;
  DVR_ASSERT(p_ctx);
  DVR_ASSERT(p_ctx->index_fp);
  DVR_ASSERT(p_ctx->ts_fp);

	memset(buf, 0, sizeof(buf));
  DVR_ASSERT(fseek(p_ctx->index_fp, 0, SEEK_SET) != -1);

	while (fgets(buf, sizeof(buf), p_ctx->index_fp) != NULL) {
    memset(value, 0, sizeof(value));
    if ((p1 = strstr(buf, "time="))) {
      p1 += 4;
      if ((p2 = strstr(buf, ","))) {
        memcpy(value, p1, p2 - p1);
      }
      pts = strtoull(value, NULL, 10);
    }

    memset(value, 0, sizeof(value));
    if ((p1 = strstr(buf, "offset="))) {
      p1 += 7;
      if ((p2 = strstr(buf, "}"))) {
        memcpy(value, p1, p2 - p1);
      }
      offset = strtoull(value, NULL, 10);
    }

    memset(buf, 0, sizeof(buf));
    DVR_DEBUG(1, "time=%llu, offset=%ld\n", pts, offset);
    if (time < pts) {
      DVR_ASSERT(fseeko(p_ctx->ts_fp, offset, SEEK_SET) != -1);
      return offset;
    }
	}

  return DVR_FAILURE;
}

off_t segment_tell(Segment_Handle_t handle)
{
  Segment_Context_t *p_ctx;

  p_ctx = (Segment_Context_t *)handle;
  DVR_ASSERT(p_ctx);
  DVR_ASSERT(p_ctx->ts_fp);

  return ftello(p_ctx->ts_fp);
}

off_t segment_dump_pts(Segment_Handle_t handle)
{
  Segment_Context_t *p_ctx;
  char buf[256];
  char value[256];
  uint64_t pts;
  off_t offset;
  char *p1, *p2;

  p_ctx = (Segment_Context_t *)handle;
  DVR_ASSERT(p_ctx);
  DVR_ASSERT(p_ctx->index_fp);
  DVR_ASSERT(p_ctx->ts_fp);

  memset(buf, 0, sizeof(buf));
  DVR_ASSERT(fseek(p_ctx->index_fp, 0, SEEK_SET) != -1);
  printf("start gets pts\n");
  while (fgets(buf, sizeof(buf), p_ctx->index_fp) != NULL) {
    printf("buf[%s]\n", buf);
    memset(value, 0, sizeof(value));
    if ((p1 = strstr(buf, "time="))) {
      p1 += 4;
      if ((p2 = strstr(buf, ","))) {
        memcpy(value, p1, p2 - p1);
      }
      pts = strtoull(value, NULL, 10);
    }

    memset(value, 0, sizeof(value));
    if ((p1 = strstr(buf, "offset="))) {
      p1 += 7;
      if ((p2 = strstr(buf, "}"))) {
        memcpy(value, p1, p2 - p1);
      }
      offset = strtoull(value, NULL, 10);
    }

    memset(buf, 0, sizeof(buf));
    printf("pts=%llu, offset=%ld\n", pts, offset);
  }

  return 0;
}
