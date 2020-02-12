#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "dvr_segment.h"
#include <segment.h>

/**\brief DVR segment file information*/
typedef struct {
  char              location[DVR_MAX_LOCATION_SIZE];      /**< DVR record file location*/
  uint64_t          id;                                   /**< DVR Segment id*/
} DVR_SegmentFile_t;

void *dvr_segment_thread(void *arg)
{
  DVR_SegmentFile_t file;
  int ret;

  memcpy(&file, arg, sizeof(DVR_SegmentFile_t));
  DVR_DEBUG(1, "%s try to delete [%s-%lld]", __func__, file.location, file.id);
  ret = segment_delete(file.location, file.id);
  DVR_DEBUG(1, "%s delete segment [%s-%lld] %s", __func__, file.location, file.id,
      ret == DVR_SUCCESS ? "success" : "failed");

  return NULL;
}

int dvr_segment_delete(const char *location, uint64_t segment_id)
{
  return DVR_SUCCESS;
  pthread_t thread;
  static DVR_SegmentFile_t segment;

  DVR_DEBUG(1, "%s in, %s,id:%lld", __func__, location, segment_id);
  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(strlen(location) < DVR_MAX_LOCATION_SIZE);
  memset(segment.location, 0, sizeof(segment.location));
  memcpy(segment.location, location, strlen(location));
  segment.id = segment_id;
  pthread_create(&thread, NULL, dvr_segment_thread, &segment);
  usleep(10*1000);

  return DVR_SUCCESS;
}

int dvr_segment_get_list(const char *location, uint32_t *p_segment_nb, uint64_t **pp_segment_ids)
{
  FILE *fp;
  char fpath[DVR_MAX_LOCATION_SIZE];
  uint32_t i = 0, j = 0;
  char buf[64];
  uint64_t *p = NULL;
  char cmd[256];

  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(p_segment_nb);
  DVR_RETURN_IF_FALSE(pp_segment_ids);

  DVR_DEBUG(1, "%s location:%s", __func__, location);
  memset(fpath, 0, sizeof(fpath));
  sprintf(fpath, "%s.list", location);

  if (access(fpath, 0) != -1) {
    /*the list file is exist*/
    fp = fopen(fpath, "r");
    DVR_RETURN_IF_FALSE(fp);
    /*get segment numbers*/
	  while (fgets(buf, sizeof(buf), fp) != NULL) {
      i++;
    }
    *p_segment_nb = i;
    rewind(fp);
    /*malloc*/
    p = malloc(i * sizeof(uint64_t));
    i = 0;
    /*set value*/
    memset(buf, 0, sizeof(buf));
	  while (fgets(buf, sizeof(buf), fp) != NULL) {
      p[i++] = strtoull(buf, NULL, 10);
      memset(buf, 0, sizeof(buf));
    }
    *pp_segment_ids = p;
    fclose(fp);
  } else {
    /*the list file does not exist*/
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "ls -l %s*.ts | wc -l", location);
    fp = popen(cmd, "r");
    DVR_RETURN_IF_FALSE(fp);
    memset(buf, 0, sizeof(buf));
    if (fgets(buf, sizeof(buf), fp) != NULL) {
      i = strtoull(buf, NULL, 10);
      pclose(fp);
    } else {
      pclose(fp);
      return DVR_FAILURE;
    }

    *p_segment_nb = i;
    p = malloc(i * sizeof(uint64_t));
    for (i = 0;;i++) {
      memset(fpath, 0, sizeof(fpath));
      sprintf(fpath, "%s-%04d.ts", location, i);
      if (access(fpath, 0) != -1) {
        p[j++] = i;
      }
      if (j >= *p_segment_nb) {
        break;
      }
    }
    *pp_segment_ids = p;
  }
  return DVR_SUCCESS;
}

int dvr_segment_get_info(const char *location, uint64_t segment_id, DVR_RecordSegmentInfo_t *p_info)
{
  int ret;
  Segment_OpenParams_t open_params;
  Segment_Handle_t segment_handle;

  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(p_info);
  DVR_RETURN_IF_FALSE(strlen((const char *)location) < DVR_MAX_LOCATION_SIZE);

  memset(&open_params, 0, sizeof(open_params));
  memcpy(open_params.location, location, strlen(location));
  open_params.segment_id = segment_id;
  open_params.mode = SEGMENT_MODE_READ;
  ret = segment_open(&open_params, &segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  ret = segment_load_info(segment_handle, p_info);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  DVR_DEBUG(1, "%s, id:%lld, nb_pids:%d, duration:%ld ms, size:%zu, nb_packets:%d",
      __func__, p_info->id, p_info->nb_pids, p_info->duration, p_info->size, p_info->nb_packets);

  ret = segment_close(segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  return DVR_SUCCESS;
}

int dvr_segment_link(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids)
{
  FILE *fp;
  char fpath[DVR_MAX_LOCATION_SIZE];
  uint32_t i;
  char buf[64];

  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(p_segment_ids);
  DVR_RETURN_IF_FALSE(strlen((const char *)location) < DVR_MAX_LOCATION_SIZE);

  DVR_DEBUG(1, "%s location:%s, nb_segments:%d", __func__, location, nb_segments);
  memset(fpath, 0, sizeof(fpath));
  sprintf(fpath, "%s.list", location);
  fp = fopen(fpath, "w+");
  for (i = 0; i< nb_segments; i++) {
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%lld", p_segment_ids[i]);
    fwrite(buf, 1, strlen(buf), fp);
  }

  fflush(fp);
  fsync(fileno(fp));
  fclose(fp);
  return DVR_SUCCESS;
}
