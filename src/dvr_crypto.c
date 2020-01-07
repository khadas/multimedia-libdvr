#include <stdio.h>
#include "dvr_crypto.h"

int dvr_crypto_device_open(DVR_CryptoDeviceHandle *p_handle)
{
  return 0;
}

int dvr_crypto_device_run(DVR_CryptoDeviceHandle handle,
    uint8_t *buf_in, uint8_t *buf_out, DVR_CryptoParams *params)
{
  return 0;
}

int dvr_crypto_device_register(DVR_CryptoDeviceHandle handle, DVR_CryptoFunction cb, void *userdata, int is_enc)
{
  return 0;
}

int dvr_crypto_device_close(DVR_CryptoDeviceHandle handle)
{
  return 0;
}
