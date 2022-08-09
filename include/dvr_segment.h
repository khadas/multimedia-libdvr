/**
 * \file
 * \brief Segment operation module
 *
 * A record is separated to several segments.
 * Each segment contains many files.
 * \li TS file: stored the TS data.
 * \li Information file: stored the information of this segment.
 * \li Index file: stored the timestamp lookup table of this segment.
 */

#ifndef _DVR_SEGMENT_H_
#define _DVR_SEGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_types.h"

/**\brief Delete a segment
 * \param[in] location The record file's location
 * \param[in] segment_id The segment's index
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_delete(const char *location, uint64_t segment_id);

/**\brief Get the segment list of a record file
 * \param[in] location The record file's location
 * \param[out] p_segment_nb Return the segments number
 * \param[out] pp_segment_ids Return the segments index
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_get_list(const char *location, uint32_t *p_segment_nb, uint64_t **pp_segment_ids);

/**\brief Del all info of segment whose location is "*location"
 * \param[in] location The record of need del file's location
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_del_by_location(const char *location);


/**\brief Get the segment's information
 * \param[in] location The record file's location
 * \param[in] segment_id The segment index
 * \param[out] p_info Return the segment's information
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_get_info(const char *location, uint64_t segment_id, DVR_RecordSegmentInfo_t *p_info);

/**\brief Get the segment's information
 * \param[in] location The record file's location
 * \param[in] segment_id The segment index
 * \param[out] p_info Return the segment's information
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */

int dvr_segment_get_allInfo(const char *location, struct list_head *list);

/**\brief Link a segment group as the record file's list
 * \param[in] location The record file's location
 * \param[in] nb_segments The number of segments
 * \param[in] p_segment_ids The segments index in the group
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_segment_link(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids);


#define SEGMENT_OP_NEW 0
#define SEGMENT_OP_ADD 1
int dvr_segment_link_op(const char *location, uint32_t nb_segments, uint64_t *p_segment_ids, int op);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_SEGMENT_H_*/
