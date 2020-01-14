#ifndef _SEGMENT_H_H_
#define _SEGMENT_H_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dvr_common.h"

typedef void* Segment_Handle_t;

typedef enum {
  SEGMENT_MODE_READ,
  SEGMENT_MODE_WRITE,
  SEGMENT_MODE_MAX
} Segment_OpenMode_t;

typedef struct Segment_OpenParams_s {
  char                  location[DVR_MAX_LOCATION_SIZE];
  uint64_t              segment_id;
  Segment_OpenMode_t    mode;
} Segment_OpenParams_t;

/**\brief Open a segment for a target giving some open parameters
 * \param[out] p_handle Return the handle of the newly created segment
 * \param[in] params Segment open parameters
 * \return DVR_SUCCESS on success
 * \return error code
 */
int segment_open(Segment_OpenParams_t *params, Segment_Handle_t *p_handle);

/**\brief Close a segment
 * \param[in] handle Segment handle
 * \return DVR_SUCCESS on success
 * \return error code
 */
int segment_close(Segment_Handle_t handle);

/**\brief Read data from the giving segment
 * \param[out] buf The buffer of data
 * \param[in] handle Segment handle
 * \param[in] count The data count
 * \return The number of bytes read on success
 * \return error code
 */
ssize_t segment_read(Segment_Handle_t handle, void *buf, size_t count);

/**\brief Write data from the giving segment
 * \param[in] buf The buffer of data
 * \param[in] handle Segment handle
 * \param[in] count The data count
 * \return The number of bytes write on success
 * \return error code
 */
ssize_t segment_write(Segment_Handle_t handle, void *buf, size_t count);

/**\brief Update the pts and offset when record
 * \param[in] handle Segment handle
 * \param[in] pts Current pts
 * \param[in] offset Current segment offset
 * \return DVR_SUCCESS on success
 * \return error code
 */
int segment_update_pts(Segment_Handle_t handle, uint64_t pts, off_t offset);

/**\brief Seek the segment to the correct position which match the giving time
 * \param[in] handle Segment handle
 * \param[in] time The time offset
 * \return The segment current read position on success
 * \return error code
 */
off_t segment_seek(Segment_Handle_t handle, uint64_t time);

/**\brief Tell the position for the giving segment
 * \param[in] handle Segment handle
 * \return The segment current read position on success
 * \return error code
 */
off_t segment_tell(Segment_Handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /*END _SEGMENT_H_H_*/
