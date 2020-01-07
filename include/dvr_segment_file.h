#ifndef _DVR_SEGMENT_FILE_H_
#define _DVR_SEGMENT_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_common.h"

typedef struct DVR_SegmentStoreInfo_s {
  uint64_t            id;
  uint32_t            nb_pids;
  DVR_PidInfo         pids[DVR_MAX_RECORD_PIDS];
  time_t              duration;
  size_t              size;
  uint32_t            nb_packets;
} DVR_SegmentStoreInfo;

int dvr_segment_file_store(const char *location, uint64_t segment_id, DVR_SegmentStoreInfo *p_info);

int dvr_segment_file_load(const char *location, uint64_t segment_id, DVR_SegmentStoreInfo *p_info);

int dvr_segment_file_del(const char *location, uint64_t segment_id);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_SEGMENT_FILE_H_*/
