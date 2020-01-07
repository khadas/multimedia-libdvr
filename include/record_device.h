#ifndef _RECORD_DEVICE_H_
#define _RECORD_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Record_DeviceHandle_t;

typedef struct Record_DeviceOpenParams_t_s {
} Record_DeviceOpenParams_t;

int record_device_open(Record_DeviceHandle_t *p_handle, Record_DeviceOpenParams_t *params);

int record_device_close(Record_DeviceHandle_t handle);

int record_device_add_pid(Record_DeviceHandle_t handle, int pid);

int record_device_remove_pid(Record_DeviceHandle_t handle, int pid);

int record_device_start(Record_DeviceHandle_t handle);

int record_device_stop(Record_DeviceHandle_t handle);

ssize_t record_device_read(Record_DeviceHandle_t handle, void *buf, size_t len, int timeout);

#ifdef __cplusplus
}
#endif

#endif /*END _RECORD_DEVICE_H_*/
