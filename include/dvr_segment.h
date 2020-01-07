#ifndef _DVR_SEGMENT_H_
#define _DVR_SEGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_common.h"

typedef struct DVR_SegmentInfo_s {
} DVR_SegmentInfo;

int dvr_segment_delete(const char *location, uint64_t segment_id);

int dvr_segment_get_list(const char *location, uint32_t *p_segment_nb, uint64_t **pp_segment_ids);

int dvr_segment_get_info(const char *location, uint64_t segment_id, DVR_SegmentInfo *p_info);

int dvr_segment_link(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_SEGMENT_H_*/
