#ifndef _DVR_CRYPTO_H_
#define _DVR_CRYPTO_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t DVR_CryptoDeviceHandle_t;

typedef enum {
  DVR_CRYPTO_VENDOR_AMLOGIC,
  DVR_CRYPTO_VENDOR_IRDETO,
  DVR_CRYPTO_VENDOR_VMX,
  DVR_CRYPTO_VENDOR_NAGRA
} DVR_CryptoVendorID_t;

typedef struct {
} DVR_CryptoIrdetoParams_t;

typedef struct {
} DVR_CryptoVmxParams_t;

typedef struct {
} DVR_CryptoAmlogicParams_t;

typedef struct {
} DVR_CryptoNagraParams_t;

typedef struct DVR_CryptoParams_s {
  int vendor_id;
  union {
    DVR_CryptoIrdetoParams_t irdeto;
    DVR_CryptoVmxParams_t vmx;
    DVR_CryptoAmlogicParams_t amlogic;
    DVR_CryptoNagraParams_t nagra;
  };
} DVR_CryptoParams_t;

typedef int (*DVR_CryptoFunction_t) (DVR_CryptoParams_t params_t, void *userdata);

int dvr_crypto_device_open(DVR_CryptoDeviceHandle_t *p_handle);

int dvr_crypto_device_run(DVR_CryptoDeviceHandle_t handle,
    uint8_t *buf_in, uint8_t *buf_out, DVR_CryptoParams_t *params);

//int dvr_crypto_device_register(DVR_CryptoDeviceHandle_t handle, DVR_CryptoFunction cb, void *userdata, int is_enc);

int dvr_crypto_device_close(DVR_CryptoDeviceHandle_t handle);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_CRYPTO_H_*/
