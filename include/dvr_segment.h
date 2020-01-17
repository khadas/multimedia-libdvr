/*
 * \file
 * Segment operation module
 */

#ifndef _DVR_SEGMENT_H_
#define _DVR_SEGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_types.h"

/**\brief Delete a segment
 * \param[in] location, The record file's location
 * \param[in] segment_id, The segment's index
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_delete(const char *location, uint64_t segment_id);

/**\brief Get the segment list of a record file
 * \param[in] location, The record file's location
 * \param[out] p_segment_nb, Return the segments number
 * \param[out] pp_segment_ids, Return the segments index
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_get_list(const char *location, uint32_t *p_segment_nb, uint64_t **pp_segment_ids);

/**\brief Get the segment's information
 * \param[in] location, The record file's location
 * \param[in] segment_id, The segment index
 * \param[out] p_info, Return the segment's information
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_get_info(const char *location, uint64_t segment_id, DVR_RecordSegmentInfo_t *p_info);

/**\brief Link a segment group as the record file's list
 * \param[in] location, The record file's location
 * \param[in] nb_segments, The number of segments
 * \param[in] p_segment_ids, The segments index in the group
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_link(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_SEGMENT_H_*/
