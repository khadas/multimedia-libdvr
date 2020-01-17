#include <stdio.h>
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
  ret = segment_delete(file.location, file.id);
  DVR_DEBUG(1, "%s delete segment [%s-%lld] %s", __func__, file.location, file.id,
      ret == DVR_SUCCESS ? "success" : "failed");

  return NULL;
}
int dvr_segment_delete(const char *location, uint64_t segment_id)
{
  pthread_t thread;
  static DVR_SegmentFile_t segment;

  DVR_DEBUG(1, "%s in, %s,id:%lld", __func__, location, segment_id);
  DVR_ASSERT(location);
  DVR_ASSERT(strlen(location) < DVR_MAX_LOCATION_SIZE);
  memset(segment.location, 0, sizeof(segment.location));
  memcpy(segment.location, location, strlen(location));
  segment.id = segment_id;
  pthread_create(&thread, NULL, dvr_segment_thread, &segment);

  return DVR_SUCCESS;
}

int dvr_segment_get_list(const char *location, uint32_t *p_segment_nb, uint64_t **pp_segment_ids)
{
  return DVR_SUCCESS;
}

int dvr_segment_get_info(const char *location, uint64_t segment_id, DVR_RecordSegmentInfo_t *p_info)
{
  int ret;
  Segment_OpenParams_t open_params;
  Segment_Handle_t segment_handle;

  DVR_ASSERT(location);
  DVR_ASSERT(p_info);
  DVR_ASSERT(strlen((const char *)location) < DVR_MAX_LOCATION_SIZE);

  memset(&open_params, 0, sizeof(open_params));
  memcpy(open_params.location, location, strlen(location));
  open_params.segment_id = segment_id;
  open_params.mode = SEGMENT_MODE_READ;
  ret = segment_open(&open_params, &segment_handle);
  DVR_ASSERT(ret == DVR_SUCCESS);

  ret = segment_load_info(segment_handle, p_info);
  DVR_ASSERT(ret == DVR_SUCCESS);
  DVR_DEBUG(1, "%s, id:%lld, nb_pids:%d, duration:%ld ms, size:%zu, nb_packets:%d",
      __func__, p_info->id, p_info->nb_pids, p_info->duration, p_info->size, p_info->nb_packets);

  ret = segment_close(segment_handle);
  DVR_ASSERT(ret == DVR_SUCCESS);

  return DVR_SUCCESS;
}

int dvr_segment_link(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids)
{
  return DVR_SUCCESS;
}
