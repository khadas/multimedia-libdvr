#include <stdio.h>
#include "dvr_crypto.h"

int dvr_crypto_device_open(DVR_CryptoDeviceHandle_t *p_handle)
{
  return 0;
}

int dvr_crypto_device_run(DVR_CryptoDeviceHandle_t handle,
    uint8_t *buf_in, uint8_t *buf_out, DVR_CryptoParams_t *params)
{
  return 0;
}

#if 0
int dvr_crypto_device_register(DVR_CryptoDeviceHandle handle, DVR_CryptoFunction cb, void *userdata, int is_enc)
{
  return 0;
}
#endif

int dvr_crypto_device_close(DVR_CryptoDeviceHandle_t handle)
{
  return 0;
}
