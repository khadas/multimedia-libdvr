
#ifndef AM_KEY_H
#define AM_KEY_H
#include "aml_key.h"

extern int key_open (void);
extern int key_close(int fd);
extern int key_malloc(int fd, int key_userid, int key_algo, int is_iv);
extern int key_free(int fd, int key_index);
extern int key_set(int fd, int key_index, char *key, int key_len);
#endif

