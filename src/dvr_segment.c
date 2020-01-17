#include <stdio.h>
#include "dvr_segment.h"

int dvr_segment_delete(const char *location, uint64_t segment_id)
{
  return 0;
}

int dvr_segment_get_list(const char *location, uint32_t *p_segment_nb, uint64_t **pp_segment_ids)
{
  return 0;
}

int dvr_segment_get_info(const char *location, uint64_t segment_id, Segment_StoreInfo_t *p_info)
{
  return 0;
}

int dvr_segment_link(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids)
{
  return 0;
}
