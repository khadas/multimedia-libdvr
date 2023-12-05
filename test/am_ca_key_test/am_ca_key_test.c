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

extern int inject_file_and_rec_open(char *inject_name,int vpid, int apid, char *record_name);
extern int inject_file_and_rec_close(void);

int main(int argc, char **argv)
{
	int dsccv, dscca;
	int vpid=0, apid=0;
	char buf[1024];
	char *p = buf;
	int dsc = 0, src = 0;
	int ret;
	int aes = 0, des = 0, sm4 = 0, csa2 = 0, iv = 0;
	int odd_type = CA_KEY_ODD_TYPE;
	int even_type = CA_KEY_EVEN_TYPE;
	int odd_iv_type = 0;
	int even_iv_type = 0;
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
			sscanf(argv[i], "inject=%s", &inject_name);
			inject_file = 1;
		}
		else if (!strncmp(argv[i], "rec", 3)) {
			memset(record_name, 0, sizeof(record_name));
			sscanf(argv[i], "rec=%s", &record_name);
			record_file = 1;
		}
		else if (!strncmp(argv[i], "help", 4)) {
			printf("Usage: %s [vid=pid] [aid=pid] [dsc=n] [src=n] [aes|des|sm4|csa2] [mode=0/1/2,0:ecb,1:cbc,2:idsa][kl]\n", argv[0]);
			printf("\t [inject=xxx] [rec=xxx] \n");
			printf("\t inject file and record for verify the descram function \n");
			printf("\t if no v/a specified, will set to current running v/a\n");
			exit(0);
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
		char aes_key_ecb[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
		char aes_key_cbc_iv[16] = {0x49, 0x72, 0x64, 0x65, 0x74, 0x6F, 0xA9, 0x43, 0x6F,0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74};
//		char aes_key_cbc_iv[16] = {0};
		char aes_key_cbc[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
		char aes_key_cbc_isda[16] = { 0xb2, 0x8e, 0xd9, 0x82, 0x9d, 0x91, 0xe6, 0x5d,0x8c, 0x15, 0x33, 0x51, 0xf7, 0x67, 0x0d, 0x4a};
		char aes_key_cbc_isda_iv[16] = {0};

		char sm4_ecb_key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
		char sm4_cbc_key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
		char sm4_cbc_key_iv[16] = {0};
		char sm4_isda_key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};;
		char sm4_isda_key_iv[16] = {0};

		char des_key[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
		/*char csa_key[8] = {0x11, 0x22, 0x33, 0x66, 0x55, 0x66, 0x77, 0x32};*/
		//char csa_key[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
		char csa_key_odd[8] = {0xe6, 0x2a, 0x3b, 0x4b, 0xd0, 0x0e, 0x38, 0x16};
		char csa_key_even[8] = {0xe6, 0x3c, 0x7c, 0x9e, 0x00, 0x43, 0xc6, 0x09};

		char *odd_key = NULL;
		char *even_key = NULL;
		char *key_iv = NULL;
		int algo = 0;
		int key_len = 0;

		if (aes) {
			if (mode == 0) {
				algo = CA_ALGO_AES_ECB_CLR_END;
				odd_key = aes_key_ecb;
				even_key = aes_key_ecb;
				key_len = 16;
				printf("use AES ecb key\n");
			} else if (mode == 1) {
				algo = CA_ALGO_AES_CBC_CLR_END;
				odd_key = aes_key_cbc;
				even_key = aes_key_cbc;
				key_iv = aes_key_cbc_iv;
				odd_iv_type = CA_KEY_ODD_IV_TYPE;
				even_iv_type = CA_KEY_EVEN_IV_TYPE;
				key_len = 16;
				printf("use AES cbc key\n");
			} else if (mode == 2 ) {
				algo = CA_ALGO_AES_CBC_IDSA;
				odd_key = aes_key_cbc_isda;
				even_key = aes_key_cbc_isda;
				key_iv = aes_key_cbc_isda_iv;
				printf("use AES isda key\n");
				odd_iv_type = CA_KEY_ODD_IV_TYPE;
				even_iv_type = CA_KEY_EVEN_IV_TYPE;
				key_len = 16;
			} else {
				printf("mode is invalid\n");
				return -1;
			}
		} else if (des) {
			algo = CA_ALGO_DES_SCTE41;
			odd_key = des_key;
			even_key = des_key;
			key_len = 8;
			printf("use DES key\n");
		} else if (csa2) {
			algo = CA_ALGO_CSA2;
			odd_key = csa_key_odd;
			even_key = csa_key_even;
			printf("use CSA2 key\n");
			key_len = 16;
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
			if (key_iv)
			{
				AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, odd_iv_type, key_len, (char*)key_iv));
				AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, even_iv_type, key_len, (char*)key_iv));
			}

			if (kl == 1) {
				printf("set key keyladder mode\n");
				AM_CHECK_ERR(ca_set_key (dsc, v_chan_index, odd_type, 0));
				AM_CHECK_ERR(ca_set_key (dsc, v_chan_index, even_type, 0));
			} else {
				printf("set key host mode\n");
				AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, odd_type, key_len, (char*)odd_key));
				AM_CHECK_ERR(ca_set_cw_key(dsc, v_chan_index, even_type, key_len, (char*)even_key));
			}
			printf("set default key for pid[%d]\n", vpid);
		}
		if(apid>0) {
			v_chan_index = ca_alloc_chan(dsc, apid, algo, CA_DSC_COMMON_TYPE);
			if (v_chan_index < 0)
				goto end;
			if (key_iv)
			{
				AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, odd_iv_type, key_len, (char*)key_iv));
				AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, even_iv_type, key_len, (char*)key_iv));
			}

			if (kl == 1) {
				printf("set key keyladder mode\n");
				AM_CHECK_ERR(ca_set_key (dsc, a_chan_index, odd_type, 0));
				AM_CHECK_ERR(ca_set_key (dsc, a_chan_index, even_type, 0));
			} else {
				printf("set key host mode\n");
				AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, odd_type, key_len, (char*)odd_key));
				AM_CHECK_ERR(ca_set_cw_key(dsc, a_chan_index, even_type, key_len, (char*)even_key));
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
