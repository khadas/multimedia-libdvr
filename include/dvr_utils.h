#ifndef _DVR_UTILS_H_
#define _DVR_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Write a string cmd to a file
 * \param[in] name, File name
 * \param[in] cmd, String command
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int DVR_FileEcho(const char *name, const char *cmd);
#ifdef __cplusplus
}
#endif

#endif /*END _DVR_UTILS_H_*/
