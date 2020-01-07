#ifndef _RECORD_DEVICE_H_
#define _RECORD_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Record_DeviceHandle;

typedef struct Record_DeviceOpenParams_s {
} Record_DeviceOpenParams;

int record_device_open(Record_DeviceHandle *p_handle, Record_DeviceOpenParams *params);

int record_device_close(Record_DeviceHandle handle);

int record_device_add_pid(Record_DeviceHandle handle, int pid);

int record_device_remove_pid(Record_DeviceHandle handle, int pid);

int record_device_start(Record_DeviceHandle handle);

int record_device_stop(Record_DeviceHandle handle);

ssize_t record_device_read(Record_DeviceHandle handle, void *buf, size_t len, int timeout);

#ifdef __cplusplus
}
#endif

#endif /*END _RECORD_DEVICE_H_*/
