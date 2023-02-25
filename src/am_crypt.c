#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "am_crypt.h"

#define printf(a...) ((void)0)

/*
    Place your crypt logic in the TODO sections to complete the process
*/


typedef struct {
    void *des_cryptor;
    uint8_t cache[188];
    int cache_len;
} am_cryptor_t;


void *am_crypt_des_open(const uint8_t *key, const uint8_t *iv, int key_bits)
{
    am_cryptor_t *cryptor = (am_cryptor_t *)malloc(sizeof(am_cryptor_t));
    if (cryptor) {
        memset(cryptor, 0, sizeof(am_cryptor_t));

        {
            /*TODO:init your cryptor here*/
        }
    }
    return cryptor;
}

int am_crypt_des_close(void *cryptor)
{
    if (cryptor)
        free(cryptor);
    return 0;
}

int am_crypt_des_crypt(void* cryptor, uint8_t* dst,
               const uint8_t *src, int *len, int decrypt)
{
    int out_len = 0;
    int left = *len;
    int *p_out_len = len;
    const uint8_t *p_in = src;
    uint8_t *p_out = dst;
    uint8_t *p_cache = &((am_cryptor_t *)cryptor)->cache[0];
    int *p_cache_len = &((am_cryptor_t *)cryptor)->cache_len;

    /* Check parameters*/
    if (!p_in || !p_out) {
        printf("%s bad params, in:%p:%d, out:%p:%d\n",
            __func__, p_in, left,
            p_out, left);
        *p_out_len = 0;
        return -1;
    }

    /* If less than one ts packet, just cache the data */
    if (left + *p_cache_len < 188) {
        printf("%s in_len:%d, cache_len:%d, just cache the data\n",
            __func__, left, *p_cache_len);
        memcpy(p_cache + *p_cache_len, p_in, left);
        *p_cache_len += left;
        *p_out_len = 0;
        return 0;
    }

    if (*p_cache_len > 0) {
        /* p_out length must be at least more 188Bytes than p_in length */
#if 0
        if (*p_out_len - left < 188) {
            printf("%s p_out length %d is not enough big\n",
                __func__, *p_out_len);
            return -1;
        }
#endif
        /* Process cache data */
        memcpy(p_cache + *p_cache_len, p_in, 188 - ((am_cryptor_t *)cryptor)->cache_len);
        memcpy(p_out, p_cache, 188);

        {
            /*TODO:process your crypt on the pkt*/
        }

        left -=  (188 - *p_cache_len);
        p_in += (188 - *p_cache_len);
        p_out += 188;
        out_len = 188;
        printf("%s process cache data\n", __func__);
    }

    /* Process input buffer */
    memcpy(p_out, p_in, left);
    while (left > 0) {
        if (*p_in == 0x47) {
            if (left < 188) {
                printf("%s cache %#x bytes\n", __func__,
                     left);
                break;
            }

            memcpy(p_out, p_in, 188);
            {
                /*TODO:process your crypt on the pkt*/
            }

            p_in += 188;
            p_out += 188;
            left -= 188;
            out_len += 188;
        } else {
            p_in ++;
            p_out++;
            left --;
            out_len++;
            printf("%s not ts header, skip one byte\n", __func__);
        }
    }

    /* Cache remain data */
    if (left) {
        memcpy(p_cache, p_in, left);
    }

    *p_cache_len = left;
    *p_out_len = out_len;

    return 0;
}
