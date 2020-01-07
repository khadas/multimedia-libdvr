#include <stdio.h>
#include "record_device.h"

int record_device_open(Record_DeviceHandle *p_handle, Record_DeviceOpenParams *params)
{
  return 0;
}

int record_device_close(Record_DeviceHandle handle)
{
  return 0;
}

int record_device_add_pid(Record_DeviceHandle handle, int pid)
{
  return 0;
}

int record_device_remove_pid(Record_DeviceHandle handle, int pid)
{
  return 0;
}

int record_device_start(Record_DeviceHandle handle)
{
  return 0;
}

int record_device_stop(Record_DeviceHandle handle)
{
  return 0;
}

ssize_t record_device_read(Record_DeviceHandle handle, void *buf, size_t len, int timeout)
{
  return 0;
}
