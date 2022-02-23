/*
 * Copyright (C) 2014-2017 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _KEY_TABLE_API_H_
#define _KEY_TABLE_API_H_

/* key userid */
#define MKL_USER_M2M_0       (0)
#define MKL_USER_M2M_1       (1)
#define MKL_USER_M2M_2       (2)
#define MKL_USER_M2M_3       (3)
#define MKL_USER_M2M_4       (4)
#define MKL_USER_M2M_5       (5)
#define MKL_USER_LOC_DEC     (8)
#define MKL_USER_NETWORK     (9)
#define MKL_USER_LOC_ENC     (10)

/* key algorithm */
#define MKL_USAGE_AES        (0)
#define MKL_USAGE_TDES       (1)
#define MKL_USAGE_DES        (2)
#define MKL_USAGE_NDL        (7)
#define MKL_USAGE_ND         (8)
#define MKL_USAGE_CSA3       (9)
#define MKL_USAGE_CSA2       (10)
#define MKL_USAGE_HMAC       (13)

/* key flag */
#define MKL_FLAG_ENC_ONLY    (1)
#define MKL_FLAG_DEC_ONLY    (2)
#define MKL_FLAG_ENC_DEC     (3)

/* key level */
#define MKL_KEY_LEVEL_0      (0)
#define MKL_KEY_LEVEL_1      (1)
#define MKL_KEY_LEVEL_2      (2)
#define MKL_KEY_LEVEL_3      (3)
#define MKL_KEY_LEVEL_4      (4)
#define MKL_KEY_LEVEL_5      (5)
#define MKL_KEY_LEVEL_6      (6)
#define MKL_KEY_LEVEL_7      (7)

/* tee private */
#define MKL_KEY_TEE_ONLY     (1)
#define MKL_KEY_REE_OPEN     (0)

/* key source */
#define KEY_FROM_CERT        (15)
#define KEY_FROM_NSK         (14)
#define KEY_FROM_MSR3        (13)
#define KEY_FROM_VO          (12)
#define KEY_FROM_SPKL        (11)
#define KEY_FROM_SPHOST      (10)
#define KEY_FROM_TEEKL_MSR2  (9)
#define KEY_FROM_TEEKL_ETSI  (8)
#define KEY_FROM_TEEKL_NAGRA (7)
#define KEY_FROM_TEEKL_AML   (6)
#define KEY_FROM_TEE_HOST    (5)
#define KEY_FROM_REEKL_MSR2  (4)
#define KEY_FROM_REEKL_ETSI  (3)
#define KEY_FROM_REEKL_NAGRA (2)
#define KEY_FROM_REEKL_AML   (1)
#define KEY_FROM_REE_HOST    (0)

typedef struct key_info_s {
    int key_userid;
    int key_algo;
    int key_flag;
    int key_level;
    int tee_priv;
    int key_source;
    uint8_t key_data[16];
}key_info_t;

int keytable_alloc(int key_source, int *kte);

int keytable_set_key(int kte, struct key_info_s key_info);

int keytable_free(int kte);

#endif /* _KEY_TABLE_API_H_ */
