#ifndef _DVR_MUTEX_H_
#define _DVR_MUTEX_H_

#include <pthread.h>

typedef struct dvr_mutex_s
{
   pthread_mutex_t lock;
   pthread_t thread;
   int lock_cnt;
} dvr_mutex_t;

void _dvr_mutex_init(void *mutex);
void _dvr_mutex_lock(void *mutex);
void _dvr_mutex_unlock(void *mutex);
void _dvr_mutex_destroy(void *mutex);
int _dvr_mutex_save(void *mutex);
void _dvr_mutex_restore(void *mutex, int val);

#define DVR_MUTEX_DEBUG

#ifndef DVR_MUTEX_DEBUG

#define dvr_mutex_init(a)        _dvr_mutex_init(a)
#define dvr_mutex_lock(a)        _dvr_mutex_lock(a)
#define dvr_mutex_unlock(a)      _dvr_mutex_unlock(a)
#define dvr_mutex_destroy(a)     _dvr_mutex_destroy(a)
#define dvr_mutex_save(a)        _dvr_mutex_save(a)
#define dvr_mutex_restore(a, v)  _dvr_mutex_restore(a, v)

#else/*DEBUG*/

void _dvr_mutex_init_dbg(void *mutex, const char *file, int line);
void _dvr_mutex_lock_dbg(void *mutex, const char *file, int line);
void _dvr_mutex_unlock_dbg(void *mutex, const char *file, int line);
void _dvr_mutex_destroy_dbg(void *mutex, const char *file, int line);
int _dvr_mutex_save_dbg(void *mutex, const char *file, int line);
void _dvr_mutex_restore_dbg(void *mutex, int val, const char *file, int line);
#define dvr_mutex_init(a)        _dvr_mutex_init_dbg(a, __FUNCTION__, __LINE__)
#define dvr_mutex_lock(a)        _dvr_mutex_lock_dbg(a, __FUNCTION__, __LINE__)
#define dvr_mutex_unlock(a)      _dvr_mutex_unlock_dbg(a, __FUNCTION__, __LINE__)
#define dvr_mutex_destroy(a)     _dvr_mutex_destroy_dbg(a, __FUNCTION__, __LINE__)
#define dvr_mutex_save(a)        _dvr_mutex_save_dbg(a, __FUNCTION__, __LINE__)
#define dvr_mutex_restore(a, v)  _dvr_mutex_restore_dbg(a, v, __FUNCTION__, __LINE__)

#endif/*DEBUG*/

#endif/*_DVR_MUTEX_H_*/
