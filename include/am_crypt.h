/***************************************************************************
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 ***************************************************************************/

#ifndef _AM_CRYPT_H
#define _AM_CRYPT_H

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Function prototypes
 ***************************************************************************/

void *am_crypt_des_open(const uint8_t *key, const uint8_t *iv, int key_bits);

int am_crypt_des_close(void *cryptor);

int am_crypt_des_crypt(void* cryptor, uint8_t* dst,
		       const uint8_t *src, uint32_t *len, int decrypt);

#ifdef __cplusplus
}
#endif

#endif

