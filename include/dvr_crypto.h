#ifndef _DVR_CRYPTO_H_
#define _DVR_CRYPTO_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t DVR_CryptoDeviceHandle;

typedef struct DVR_CryptoParams_s {
} DVR_CryptoParams;

typedef int (*DVR_CryptoFunction) (DVR_CryptoParams params, void *userdata);

int dvr_crypto_device_open(DVR_CryptoDeviceHandle *p_handle);

int dvr_crypto_device_run(DVR_CryptoDeviceHandle handle,
    uint8_t *buf_in, uint8_t *buf_out, DVR_CryptoParams *params);

int dvr_crypto_device_register(DVR_CryptoDeviceHandle handle, DVR_CryptoFunction cb, void *userdata, int is_enc);

int dvr_crypto_device_close(DVR_CryptoDeviceHandle handle);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_CRYPTO_H_*/
