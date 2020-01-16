/**
 * \file
 * Crypto function definition.
 */

#ifndef _DVR_CRYPTO_H_
#define _DVR_CRYPTO_H_

#ifdef __cplusplus
extern "C" {
#endif

/**Work type.*/
typedef enum {
  DVR_CRYPTO_TYPE_ENCRYPT, /**< Encrypt.*/
  DVR_CRYPTO_TYPE_DECRYPT  /**< Decrypt.*/
} DVR_CryptoType_t;

/**Crypto parameters.*/
typedef struct DVR_CryptoParams_s {
  DVR_CryptoType_t type;                            /**< Work type.*/
  char             location[DVR_MAX_LOCATION_SIZE]; /**< Location of the record file.*/
  int              segment_id;                      /**< Current segment's index.*/
  loff_t           offset;                          /**< Current offset in the segment file.*/
  DVR_Buffer_t     input_buffer;                    /**< Input data buffer.*/
  DVR_Buffer_t     output_buffer;                   /**< Output data buffer.*/
  size_t           output_size;                     /**< Output data size in bytes.*/
} DVR_CryptoParams_t;

/**Crypto function.*/
typedef DVR_Result (*DVR_CryptoFunction_t) (DVR_CryptoParams_t *params_t, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_CRYPTO_H_*/
