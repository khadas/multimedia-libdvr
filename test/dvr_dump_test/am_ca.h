#ifndef _CAS_DRV_WRAPPER_H_
#define _CAS_DRV_WRAPPER_H_
int ca_init(void);
int ca_open (int devno);
int ca_alloc_chan (int devno, unsigned int pid, int algo, int dsc_type);
int ca_free_chan (int devno, int index);
int ca_set_key (int devno, int index, int parity, int key_index);
int ca_set_scb(int devno, int index, int scb, int scb_as_is);
int ca_close (int devno);
#endif
