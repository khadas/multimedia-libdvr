#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
  int ret;
  DVR_SegmentFile_t *segment_file = (DVR_SegmentFile_t*)arg;
  if (segment_file == NULL) {
    DVR_ERROR("Invalid segment_file pointer");
    return NULL;
  }

  pthread_detach(pthread_self());
  DVR_INFO("%s try to delete [%s-%lld]", __func__, segment_file->location, segment_file->id);
  ret = segment_delete(segment_file->location, segment_file->id);
  DVR_INFO("%s delete segment [%s-%lld] %s", __func__, segment_file->location, segment_file->id,
      ret == DVR_SUCCESS ? "success" : "failed");
  if (segment_file != NULL) {
    //malloc at delete api.free at this
    free(segment_file);
    segment_file = NULL;
  }
  return NULL;
}

int dvr_segment_delete(const char *location, uint64_t segment_id)
{

  pthread_t thread;
  DVR_SegmentFile_t *segment;

  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(strlen(location) < DVR_MAX_LOCATION_SIZE);
  DVR_INFO("In function %s, segment %s's id is %lld", __func__, location, segment_id);

  // Memory allocated here will be freed in segment deletion thread under normal conditions.
  // In case of thread creation failure, it will be freed right away.
  segment = (DVR_SegmentFile_t *)malloc(sizeof(DVR_SegmentFile_t));
  DVR_RETURN_IF_FALSE(segment != NULL);

  memset(segment->location, 0, sizeof(segment->location));
  memcpy(segment->location, location, strlen(location));
  segment->id = segment_id;

  int ret = pthread_create(&thread, NULL, dvr_segment_thread, segment);
  if (ret != 0) {
    if (segment != NULL) {
      free(segment);
      segment = NULL;
    }
  }
  return DVR_SUCCESS;
}

int dvr_segment_del_by_location(const char *location)
{
  FILE *fp;
  char cmd[DVR_MAX_LOCATION_SIZE + 64];

  DVR_RETURN_IF_FALSE(location);

  DVR_INFO("%s location:%s", __func__, location);
  {
    /* del file */
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "rm %s-* %s.list %s.stats %s.odb %s.dat", location, location, location, location, location);
    fp = popen(cmd, "r");
    DVR_RETURN_IF_FALSE(fp);
  }
  pclose(fp);
  DVR_INFO("%s location:%s end", __func__, location);
  return DVR_SUCCESS;
}

int dvr_segment_get_list(const char *location, uint32_t *p_segment_nb, uint64_t **pp_segment_ids)
{
  FILE *fp;
  char fpath[DVR_MAX_LOCATION_SIZE];
  uint32_t i = 0, j = 0, n = 0;
  char buf[DVR_MAX_LOCATION_SIZE + 10];
  uint64_t *p = NULL;
  char cmd[DVR_MAX_LOCATION_SIZE + 64];

  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(p_segment_nb);
  DVR_RETURN_IF_FALSE(pp_segment_ids);

  memset(fpath, 0, sizeof(fpath));
  sprintf(fpath, "%s.list", location);

  fp = fopen(fpath, "r");
  if (fp != NULL) { /*the list file exists*/
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
    DVR_INFO("%s location:%s segments:%d",  __func__, location, i);
  } else { /*the list file does not exist*/
    uint32_t id = 0;
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "ls -l %s-*.ts | wc -l", location);
    fp = popen(cmd, "r");
    DVR_RETURN_IF_FALSE(fp);
    memset(buf, 0, sizeof(buf));
    if (fgets(buf, sizeof(buf), fp) != NULL) {
      i = strtoul(buf, NULL, 10);
      pclose(fp);
      DVR_RETURN_IF_FALSE(i>0);
    } else {
      pclose(fp);
      DVR_ERROR("%s location:%s get null",  __func__, location);
      return DVR_FAILURE;
    }
    n = i;

    /*try to get the 1st segment id*/
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "ls %s-*.ts", location);
    fp = popen(cmd, "r");
    DVR_RETURN_IF_FALSE(fp);
    memset(buf, 0, sizeof(buf));
    j = 0;
    snprintf(fpath, sizeof(fpath), "%s-%%d.ts", location);

    // Tainted data issue originating from fgets seem false positive, so we
    // just suppress it here.
    // coverity[tainted_data]
    p = malloc(n * sizeof(uint64_t));
    if (p == NULL) {
      DVR_ERROR("%s, Failed to allocate memory with errno:%d (%s)",
          __func__,errno,strerror(errno));
      pclose(fp);
      return DVR_FAILURE;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
      if (sscanf(buf, fpath, &id) != 1) {
        DVR_INFO("%s location:%s  buf:%s not get id", __func__, location, buf);
        id = 0;
        n = n -1;
      } else {
        p[j++] = id;
      }
      memset(buf, 0, sizeof(buf));
    }
    DVR_DEBUG("%s location:%s  n=%d j=%d end", __func__, location, n, j);
    pclose(fp);
    *p_segment_nb = n;
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

  // Previous location strlen checking againest DVR_MAX_LOCATION_SIZE and
  // latter memset on open_params ensure that open_params.locatin is
  // null-terminated, so the Coverity STRING_NULL error is suppressed here.
  // coverity[string_null]
  ret = segment_open(&open_params, &segment_handle);
  if (ret == DVR_SUCCESS) {
    ret = segment_load_info(segment_handle, p_info);
    if (ret != DVR_SUCCESS) {
      DVR_ERROR("segment_load_info failed with return value %d",ret);
    }
  }
  DVR_DEBUG("%s, id:%lld, nb_pids:%d, duration:%ld ms, size:%zu, nb_packets:%d",
      __func__, p_info->id, p_info->nb_pids, p_info->duration, p_info->size, p_info->nb_packets);
  //DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);
  ret = segment_close(segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  return DVR_SUCCESS;
}

int dvr_segment_get_allInfo(const char *location, struct list_head *list)
{
  int ret;
  Segment_OpenParams_t open_params;
  Segment_Handle_t segment_handle;

  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(list);
  DVR_RETURN_IF_FALSE(strlen((const char *)location) < DVR_MAX_LOCATION_SIZE);

  memset(&open_params, 0, sizeof(open_params));
  memcpy(open_params.location, location, strlen(location));
  open_params.segment_id = 0;
  open_params.mode = SEGMENT_MODE_READ;

  // Previous location strlen checking againest DVR_MAX_LOCATION_SIZE and
  // latter memset on open_params ensure that open_params.locatin is
  // null-terminated, so the Coverity STRING_NULL error is suppressed here.
  // coverity[string_null]
  ret = segment_open(&open_params, &segment_handle);
  if (ret == DVR_SUCCESS) {
    ret = segment_load_allInfo(segment_handle, list);
    if (ret == DVR_FAILURE) {
      segment_close(segment_handle);
      return DVR_FAILURE;
    }
  }
  ret = segment_close(segment_handle);
  DVR_RETURN_IF_FALSE(ret == DVR_SUCCESS);

  return DVR_SUCCESS;
}


int dvr_segment_link(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids)
{
  return dvr_segment_link_op(location, nb_segments, p_segment_ids, SEGMENT_OP_NEW);
}

int dvr_segment_link_op(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids, int op)
{
  FILE *fp;
  char fpath[DVR_MAX_LOCATION_SIZE];
  uint32_t i;
  char buf[DVR_MAX_LOCATION_SIZE + 64];

  DVR_RETURN_IF_FALSE(location);
  DVR_RETURN_IF_FALSE(p_segment_ids);
  DVR_RETURN_IF_FALSE(strlen((const char *)location) < DVR_MAX_LOCATION_SIZE);

  DVR_INFO("%s op[%d] location:%s, nb_segments:%d", __func__, op, location, nb_segments);
  memset(fpath, 0, sizeof(fpath));
  sprintf(fpath, "%s.list", location);
  fp = fopen(fpath, (op == SEGMENT_OP_ADD) ? "a+" : "w+");
  if (!fp) {
    DVR_INFO("failed to open list file, err:%d:%s", errno, strerror(errno));
    return DVR_FAILURE;
  }
  for (i = 0; i< nb_segments; i++) {
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%lld\n", p_segment_ids[i]);
    fwrite(buf, 1, strlen(buf), fp);
  }

  fflush(fp);
  fsync(fileno(fp));
  fclose(fp);
  return DVR_SUCCESS;
}
