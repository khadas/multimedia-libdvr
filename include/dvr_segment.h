#ifndef _DVR_SEGMENT_H_
#define _DVR_SEGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_common.h"

typedef struct DVR_Segment_s {
} DVR_Segment;

int dvr_segment_open(const char *location, uint64_t segment_id);

int dvr_segment_file_load(const char *location, uint64_t segment_id, DVR_SegmentStoreInfo *p_info);

int dvr_segment_file_del(const char *location, uint64_t segment_id);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_SEGMENT_H_*/
