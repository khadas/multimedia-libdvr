/**
 * \file
 * \brief Utility functions
 */

#ifndef _DVR_UTILS_H_
#define _DVR_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Write a string cmd to a file
 * \param[in] name File name
 * \param[in] cmd String command
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_file_echo(const char *name, const char *cmd);

/**\brief read sysfs file
 * \param[in] name, File name
 * \param[out] buf, store sysfs node value
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int dvr_file_read(const char *name, char *buf, int len);

/**\brief read property value
 * \param[in] name, property name
 * \param[out] buf, property value
 * \return DVR_SUCCESS On success
 * \return DVR_FAILURE On failure
 */
int dvr_prop_read(const char *name, char *buf, int len);

/**\brief write property value
 * \param[in] name, property name
 * \param[in] value, property value
 * \return DVR_SUCCESS On success
 * \return DVR_FAILURE On failure
 */
int dvr_prop_write(const char *name, char *value);

/**\brief read int type property value
 * \param[in] name, property name
 * \param[in] def, default property value in case any failure
 * \return int type property value. If any failure default value will be returned instead
 */
int dvr_prop_read_int(const char *name, int def);

/**\brief calculates the difference between ts1 and ts2
 * \param[in] ts1, higher bound of the timespec* whose length is calculated
 * \param[in] ts2, lower bound of the timespec* whose length is calculated
 * \param[out] ts3, The result timespec* of (ts1-ts2)
 * \return void
 */
void clock_timespec_subtract(struct timespec *ts1, struct timespec *ts2, struct timespec *ts3);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_UTILS_H_*/
