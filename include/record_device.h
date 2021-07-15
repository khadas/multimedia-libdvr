#ifndef _RECORD_DEVICE_H_
#define _RECORD_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**\brief DVR record handle*/
typedef void* Record_DeviceHandle_t;

/**\brief DVR record open parameters*/
typedef struct Record_DeviceOpenParams_t_s {
  int         fend_dev_id;  /**< frontend device id*/
  int         dmx_dev_id;   /**< demux device id*/
  uint32_t    buf_size;     /**< dvr record buffer size*/
  uint32_t    ringbuf_size;     /**< dvr record ring buffer size*/
} Record_DeviceOpenParams_t;

/**\brief Open a DVR record device
 * \param[out] p_handle, DVR device handle
 * \param[in] params, DVR device open parameters
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int record_device_open(Record_DeviceHandle_t *p_handle, Record_DeviceOpenParams_t *params);

/**\brief Close a DVR record device
 * \param[in] handle, DVR device handle
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int record_device_close(Record_DeviceHandle_t handle);

/**\brief Add a pid to DVR record handle
 * \param[in] handle, DVR device handle
 * \param[in] pid, pid value
 * \return DVR_SUCCESS On success
 * \return Error code
 */
int record_device_add_pid(Record_DeviceHandle_t handle, int pid);

/**\brief Remoe a pid from the DVR record handle
 * \param[in] handle, DVR device handle
 * \param[in] pid, pid value
 * \return DVR_SUCCESS On success
 * \return Error code
 */
int record_device_remove_pid(Record_DeviceHandle_t handle, int pid);

/**\brief Start a DVR record device
 * \param[in] handle, DVR device handle
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int record_device_start(Record_DeviceHandle_t handle);

/**\brief Stop a DVR record device
 * \param[in] handle, DVR device handle
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int record_device_stop(Record_DeviceHandle_t handle);

/**\brief Read data from the DVR record device
 * \param[in] handle, DVR device handle
 * \param[out] buf, the data buffer
 * \param[in] len, the data length
 * \param[in] timeout, unit on ms
 * \return The actual length on Success
 * \return Error code On failure
 */
int record_device_read(Record_DeviceHandle_t handle, void *buf, size_t len, int timeout);

/**\brief Configure secure buffer for the given record device
 * \param[in] handle, DVR device handle
 * \param[out] sec_buf, secure buffer address
 * \param[in] len, secure buffer length
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */
int record_device_set_secure_buffer(Record_DeviceHandle_t handle, uint8_t *sec_buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /*END _RECORD_DEVICE_H_*/
