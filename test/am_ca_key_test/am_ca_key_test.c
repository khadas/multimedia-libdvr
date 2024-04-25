#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:

 */
/**\file
 * \brief 解扰器测试程序
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-10-08: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <ca.h>
#include <am_ca.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fcntl.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * API functions
 ***************************************************************************/
#define USER_MAX 10
static char *u_odd_iv[USER_MAX] = {[0 ... USER_MAX-1] = NULL};
static char *u_odd_key[USER_MAX] = {[0 ... USER_MAX-1] = NULL};
static char *u_even_iv[USER_MAX] = {[0 ... USER_MAX-1] = NULL};
static char *u_even_key[USER_MAX] = {[0 ... USER_MAX-1] = NULL};
static char *u_00_iv[USER_MAX] = {[0 ... USER_MAX-1] = NULL};
static char *u_00_key[USER_MAX] = {[0 ... USER_MAX-1] = NULL};

extern int inject_file_and_rec_open(char *inject_name,int vpid, int apid, char *record_name);
extern int inject_file_and_rec_close(void);

int get_para(char *argv)
{
    #define CASE(name, len, type, var) \
        if (!strncmp(argv, name"=", (len) + 1)) { \
            sscanf(&argv[(len)+1], type, &var); \
            printf("param["name"] => "type"\n", var); \
        }
    #define CASESTR(name, len, type, var) \
        if (!strncmp(argv, name"=", (len) + 1)) { \
            var = &argv[(len) + 1]; \
            printf("param["name"] => "type"\n", var); \
        }

    CASESTR("odd_iv0", 7, "%s", u_odd_iv[0])
    else CASESTR("odd_iv1", 7, "%s", u_odd_iv[1])
    else CASESTR("odd_iv2", 7, "%s", u_odd_iv[2])
    else CASESTR("odd_iv3", 7, "%s", u_odd_iv[3])
    else CASESTR("odd_key0", 8, "%s", u_odd_key[0])
    else CASESTR("odd_key1", 8, "%s", u_odd_key[1])
    else CASESTR("odd_key2", 8, "%s", u_odd_key[2])
    else CASESTR("odd_key3", 8, "%s", u_odd_key[3])
    else CASESTR("even_iv0", 8, "%s", u_even_iv[0])
    else CASESTR("even_iv1", 8, "%s", u_even_iv[1])
    else CASESTR("even_iv2", 8, "%s", u_even_iv[2])
    else CASESTR("even_iv3", 8, "%s", u_even_iv[3])
    else CASESTR("even_key0", 9, "%s", u_even_key[0])
    else CASESTR("even_key1", 9, "%s", u_even_key[1])
    else CASESTR("even_key2", 9, "%s", u_even_key[2])
    else CASESTR("even_key3", 9, "%s", u_even_key[3])
    else CASESTR("00_iv0", 6, "%s", u_00_iv[0])
    else CASESTR("00_iv1", 6, "%s", u_00_iv[1])
    else CASESTR("00_iv2", 6, "%s", u_00_iv[2])
    else CASESTR("00_iv3", 6, "%s", u_00_iv[3])
    else CASESTR("00_key0", 7, "%s", u_00_key[0])
    else CASESTR("00_key1", 7, "%s", u_00_key[1])
    else CASESTR("00_key2", 7, "%s", u_00_key[2])
    else CASESTR("00_key3", 7, "%s", u_00_key[3])

    return 0;
}

static void get_key_from_para(char *para_org, int para_len, char *key, int *len)
{
    int i = 0;
    int dst = 0;
    char hi = 0;
    char lo = 0;

    printf("get key len:%d, key:%s\n", para_len, para_org);
    if (para_len <= 2)
        return ;
    para_len -= 2;

    dst = 2;

    printf("key:");
    while (para_len >= 2) {
        hi = 0;
        if (para_org[dst] >= 0x30 && para_org[dst] <= 0x39)    {
            hi = para_org[dst] - 0x30;
        } else if (para_org[dst] >= 0x41 && para_org[dst] <= 0x46) {
            hi = para_org[dst] - 0x37;
        } else if (para_org[dst] >= 0x61 && para_org[dst] <= 0x66) {
            hi = para_org[dst] - 0x57;
        }
        lo = 0;
        if (para_org[dst+1] >= 0x30 && para_org[dst+1] <= 0x39)    {
            lo = para_org[dst+1] - 0x30;
        } else if (para_org[dst+1] >= 0x41 && para_org[dst+1] <= 0x46) {
            lo = para_org[dst+1] - 0x37;
        } else if (para_org[dst+1] >= 0x61 && para_org[dst+1] <= 0x66) {
            lo = para_org[dst+1] - 0x57;
        }
        key[i] = (hi << 4) | lo;
        printf("0x%0x ", key[i]);
        i++;
        dst += 2;
        para_len -= 2;
    }
    printf("\n");
    *len = i;
}

int main(int argc, char **argv)
{
	int vpid=0, apid=0;
	char buf[1024];
	int dsc = 0, src = 0;
	int ret;
	int aes = 0, des = 0, sm4 = 0, csa2 = 0;
	int odd_type = CA_KEY_ODD_TYPE;
	int even_type = CA_KEY_EVEN_TYPE;
	char inject_name[256];
	int inject_file = 0;
	char record_name[256];
	int record_file = 0;
	int mode = 0;
	int kl = 0;
	int v_chan_index = 0;
	int a_chan_index = 0;

	int i;
	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "vid", 3))
			sscanf(argv[i], "vid=%i", &vpid);
		else if (!strncmp(argv[i], "aid", 3))
			sscanf(argv[i], "aid=%i", &apid);
		else if (!strncmp(argv[i], "dsc", 3))
			sscanf(argv[i], "dsc=%i", &dsc);
		else if (!strncmp(argv[i], "src", 3))
			sscanf(argv[i], "src=%i", &src);
		else if (!strncmp(argv[i], "aes", 3))
			aes = 1;
		else if (!strncmp(argv[i], "des", 3))
			des = 1;
		else if (!strncmp(argv[i], "sm4", 3))
			sm4 = 1;
		else if (!strncmp(argv[i], "csa2", 3))
			csa2 = 1;
		else if (!strncmp(argv[i], "mode", 4))
			sscanf(argv[i],"mode=%i", &mode);
		else if (!strncmp(argv[i], "kl", 2))
			sscanf(argv[i],"kl=%i", &kl);
		else if (!strncmp(argv[i], "inject", 6)) {
			memset(inject_name, 0, sizeof(inject_name));
			sscanf(argv[i], "inject=%s", (char *)&inject_name);
			inject_file = 1;
		}
		else if (!strncmp(argv[i], "rec", 3)) {
			memset(record_name, 0, sizeof(record_name));
			sscanf(argv[i], "rec=%s", (char *)&record_name);
			record_file = 1;
		}
		else if (!strncmp(argv[i], "help", 4)) {
			printf("Usage: %s [vid=pid] [aid=pid] [dsc=n] [src=n] [aes|des|sm4|csa2] [mode=0/1/2,0:ecb,1:cbc,2:idsa][kl]\n", argv[0]);
			printf("\t [inject=xxx] [rec=xxx] \n");
			printf("\t inject file and record for verify the descram function \n");
			printf("\t if no v/a specified, will set to current running v/a\n");
			exit(0);
		} else {
			get_para(argv[i]);
		}
	}

	printf("use dsc[%d] src[%d]\n", dsc, src);
	if (aes) {
		printf("aes mode\n");
	} else if (des) {
		printf("des mode\n");
	} else if (sm4) {
		printf("sm4 mode\n");
	} else if (csa2) {
		printf("csa2 mode\n");
	} else {
		printf("need mode setting\n");
		goto end;
	}

	ret = ca_open(dsc);
	if (ret != 0)
		goto end;

	printf("DSC [%d] Set Source [%d]\n", dsc, src);
#if 0
	ret = AM_DSC_SetSource(dsc, src);
	if(src==AM_DSC_SRC_BYPASS)
		goto end;
#endif
	if(vpid>0 || apid>0) {
		int algo = 0;
		char key[32];
		int key_len = 0;

		if (aes) {
			if (mode == 0) {
				algo = CA_ALGO_AES_ECB_CLR_END;
				printf("use AES ecb key\n");
			} else if (mode == 1) {
				algo = CA_ALGO_AES_CBC_CLR_END;
				printf("use AES cbc key\n");
			} else if (mode == 2 ) {
				algo = CA_ALGO_AES_CBC_IDSA;
				printf("use AES isda key\n");
			} else {
				printf("mode is invalid\n");
				return -1;
			}
		} else if (des) {
			algo = CA_ALGO_DES_SCTE41;
			printf("use DES key\n");
		} else if (csa2) {
			algo = CA_ALGO_CSA2;
			printf("use CSA2 key\n");
		} else {
			printf("mode is invalid\n");
			return -1;
		}

#define AM_CHECK_ERR(_x) do {\
		int ret = (_x);\
		if (ret != 0)\
			printf("ERROR (0x%x) %s\n", ret, #_x);\
	} while(0)

		if(vpid>0) {
			v_chan_index = ca_alloc_chan(dsc, vpid, algo, CA_DSC_COMMON_TYPE);
			if (v_chan_index < 0)
				goto end;
			if (u_odd_iv[0]) {
				memset(key, 0, sizeof(key));
				get_key_from_para(u_odd_iv[0], strlen(u_odd_iv[0]), key, &key_len);
				AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, CA_KEY_ODD_IV_TYPE, key_len, (char*)key));
			}
			if (u_even_iv[0]) {
				memset(key, 0, sizeof(key));
				get_key_from_para(u_even_iv[0], strlen(u_even_iv[0]), key, &key_len);
				AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, CA_KEY_EVEN_IV_TYPE, key_len, (char*)key));
			}
			if (u_odd_key[0]) {
				if (kl == 1) {
					printf("set key keyladder mode\n");
					AM_CHECK_ERR(ca_set_key (dsc, v_chan_index, odd_type, 0));
					AM_CHECK_ERR(ca_set_key (dsc, v_chan_index, even_type, 0));
				} else {
					memset(key, 0, sizeof(key));
					get_key_from_para(u_odd_key[0], strlen(u_odd_key[0]), key, &key_len);
					AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, CA_KEY_ODD_TYPE, key_len, (char*)key));
				}
			}
			if (u_even_key[0]) {
				if (kl == 1) {
					printf("set key keyladder mode\n");
					AM_CHECK_ERR(ca_set_key (dsc, v_chan_index, odd_type, 0));
					AM_CHECK_ERR(ca_set_key (dsc, v_chan_index, even_type, 0));
				} else {
					memset(key, 0, sizeof(key));
					get_key_from_para(u_even_key[0], strlen(u_even_key[0]), key, &key_len);
					AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, CA_KEY_EVEN_TYPE, key_len, (char*)key));
				}
			}
			printf("set default key for pid[%d]\n", vpid);
		}
		if(apid>0) {
			a_chan_index = ca_alloc_chan(dsc, apid, algo, CA_DSC_COMMON_TYPE);
			if (a_chan_index < 0)
				goto end;
			if (u_odd_iv[0]) {
				memset(key, 0, sizeof(key));
				get_key_from_para(u_odd_iv[0], strlen(u_odd_iv[0]), key, &key_len);
				AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, CA_KEY_ODD_IV_TYPE, key_len, (char*)key));
			}
			if (u_even_iv[0]) {
				memset(key, 0, sizeof(key));
				get_key_from_para(u_even_iv[0], strlen(u_even_iv[0]), key, &key_len);
				AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, CA_KEY_EVEN_IV_TYPE, key_len, (char*)key));
			}
			if (u_odd_key[0]) {
				if (kl == 1) {
					printf("set key keyladder mode\n");
					AM_CHECK_ERR(ca_set_key (dsc, a_chan_index, odd_type, 0));
					AM_CHECK_ERR(ca_set_key (dsc, a_chan_index, even_type, 0));
				} else {
					memset(key, 0, sizeof(key));
					get_key_from_para(u_odd_key[0], strlen(u_odd_key[0]), key, &key_len);
					AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, CA_KEY_ODD_TYPE, key_len, (char*)key));
				}
			}
			if (u_even_key[0]) {
				if (kl == 1) {
					printf("set key keyladder mode\n");
					AM_CHECK_ERR(ca_set_key (dsc, a_chan_index, odd_type, 0));
					AM_CHECK_ERR(ca_set_key (dsc, a_chan_index, even_type, 0));
				} else {
					memset(key, 0, sizeof(key));
					get_key_from_para(u_even_key[0], strlen(u_even_key[0]), key, &key_len);
					AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, CA_KEY_EVEN_TYPE, key_len, (char*)key));
				}
			}
			printf("set default key for pid[%d]\n", apid);
		}

		if (inject_file) {
			inject_file_and_rec_open(inject_name,vpid,apid,record_file ? record_name:NULL);
		}

		while(fgets(buf, 256, stdin))
		{
			if(!strncmp(buf, "quit", 4))
			{
				goto end;
			}
		}
	} else {
		printf("No A/V playing.\n");
	}

end:
	ca_free_chan(dsc, v_chan_index);
	ca_free_chan(dsc, a_chan_index);
	ca_close(dsc);
	if (inject_file)
		inject_file_and_rec_close();

	printf("all exit\n");
	return 0;
}
