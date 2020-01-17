#ifndef _DVR_SEGMENT_FILE_H_
#define _DVR_SEGMENT_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_types.h"

typedef struct Segment_StoreInfo_s {
  uint64_t            id;
  uint32_t            nb_pids;
  DVR_StreamPid_t     pids[DVR_MAX_RECORD_PIDS_COUNT];
  time_t              duration;
  size_t              size;
  uint32_t            nb_packets;
} Segment_StoreInfo_t;

int segment_file_store(const char *location, uint64_t segment_id, Segment_StoreInfo_t *p_info);

int segment_file_load(const char *location, uint64_t segment_id, Segment_StoreInfo_t *p_info);

int segment_file_del(const char *location, uint64_t segment_id);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_SEGMENT_FILE_H_*/
