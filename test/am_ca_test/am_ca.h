#ifndef _AM_CA_H_
#define _AM_CA_H_
int ca_open (int devno);
int ca_alloc_chan (int devno, unsigned int pid, int algo, int dsc_type);
int ca_free_chan (int devno, int index);
int ca_set_key (int devno, int index, int parity, int key_index);
int ca_close (int devno);
#endif
