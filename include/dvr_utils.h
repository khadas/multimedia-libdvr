/**
 * \file
 * \brief Utility functions
 */

#ifndef _DVR_UTILS_H_
#define _DVR_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif


/**\brief read sysfs node
 *  name file name
 *  buf  store sysfs value buf
 *  len  buf len
 */
typedef void (*DVR_Read_Sysfs_Cb)(const char *name, char *buf, int len);

/**\brief write sysfs
 *  name file name
 *  cmd  write to sysfs node cmd
 */
typedef void (*DVR_Write_Sysfs_Cb)(const char *name, const char *cmd);

/**\brief read prop
 *  name prop name
 *  buf  store prop value buf
 *  len  buf len
 */
typedef void (*DVR_Read_Prop_Cb)(const char *name, char *buf, int len);

/**\brief write prop
 *  name prop name
 *  cmd  write to prop node cmd
 */
typedef void (*DVR_Write_Prop_Cb)(const char *name, const char *cmd);


/**\brief regist rw sysfs cb
 * \param[in] fun callback
 * \return
 *   - DVR_SUCCESS
 *   - error
 */
int dvr_register_rw_sys(DVR_Read_Sysfs_Cb RCb, DVR_Write_Sysfs_Cb WCb);

/**\brief unregist rw sys cb
 */
int dvr_unregister_rw_sys(void);


/**\brief regist rw prop cb
 * \param[in] fun callback
 * \return
 *   - DVR_SUCCESS
 *   - error
 */

int dvr_register_rw_prop(DVR_Read_Prop_Cb RCb, DVR_Write_Prop_Cb WCb);

/**\brief unregist rw prop cb */
int dvr_unregister_rw_prop(void);


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


/**\brief Write a string cmd to a prop
 * \param[in] name, prop name
 * \param[in] cmd, String command
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */

int dvr_prop_echo(const char *name, const char *cmd);


/**\brief read prop value
 * \param[in] name, prop name
 * \param[out] buf, store prop node value
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */

int dvr_prop_read(const char *name, char *buf, int len);

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
