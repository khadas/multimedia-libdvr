#include <string.h>

#include "dvr_types.h"
#include "dvr_mutex.h"

#define merr(f,...) DVR_DEBUG_FL(1, "dvr_mutex", f, ##__VA_ARGS__)
#define mdbg(f,...) merr(f, ##__VA_ARGS__)

void _dvr_mutex_init(void *mutex)
{
   if (!mutex) {
      merr("null mutex\n");
      return;
   }
   dvr_mutex_t *mtx = (dvr_mutex_t*)mutex;
   memset(mtx, 0, sizeof(dvr_mutex_t));
   if (pthread_mutex_init(&mtx->lock, NULL) != 0) {
      merr("init mutex fail\n");
      return;
   }
   mtx->thread = 0;
   mtx->lock_cnt = 0;
   return;
}

void _dvr_mutex_lock(void *mutex)
{
   if (!mutex) {
      merr("null mutex\n");
      return;
   }
   dvr_mutex_t *mtx = (dvr_mutex_t*)mutex;
   if (pthread_equal(mtx->thread, pthread_self()) != 0) {
      mtx->lock_cnt++;
   } else {
      pthread_mutex_lock(&mtx->lock);
      mtx->thread = pthread_self();
      mtx->lock_cnt = 1;
   }
}

void _dvr_mutex_unlock(void *mutex)
{
   if (!mutex) {
      merr("null mutex\n");
      return;
   }
   dvr_mutex_t *mtx = (dvr_mutex_t*)mutex;
   if (pthread_equal(mtx->thread, pthread_self()) != 0) {
      mtx->lock_cnt--;
      if (mtx->lock_cnt == 0) {
         mtx->thread = 0;
         pthread_mutex_unlock(&mtx->lock);
      }
   } else {
      mdbg("not own mutex\n");
   }
}

void _dvr_mutex_destroy(void *mutex)
{
   if (!mutex) {
      merr("null mutex\n");
      return;
   }
   dvr_mutex_t *mtx = (dvr_mutex_t*)mutex;
   pthread_mutex_destroy(&mtx->lock);
}

int _dvr_mutex_save(void *mutex)
{
   if (!mutex) {
       merr("null mutex\n");
       return 0;
   }
   dvr_mutex_t *mtx = (dvr_mutex_t*)mutex;
   int cnt = mtx->lock_cnt;
   mtx->lock_cnt = 0;
   mtx->thread = 0;
   return cnt;
}

void _dvr_mutex_restore(void *mutex, int val)
{
   if (!mutex) {
       merr("null mutex\n");
       return;
   }
   dvr_mutex_t *mtx = (dvr_mutex_t*)mutex;
   mtx->lock_cnt = val;
   mtx->thread = pthread_self();
}

#ifdef DVR_MUTEX_DEBUG
void _dvr_mutex_init_dbg(void *mutex, const char *file, int line)
{
   mdbg("%s:%d\n", file, line);
   _dvr_mutex_init(mutex);
}
void _dvr_mutex_lock_dbg(void *mutex, const char *file, int line)
{
   mdbg("%s:%d\n", file, line);
   _dvr_mutex_lock(mutex);
}
void _dvr_mutex_unlock_dbg(void *mutex, const char *file, int line)
{
   mdbg("%s:%d\n", file, line);
   _dvr_mutex_unlock(mutex);
}
void _dvr_mutex_destroy_dbg(void *mutex, const char *file, int line)
{
   mdbg("%s:%d\n", file, line);
   _dvr_mutex_destroy(mutex);
}
int _dvr_mutex_save_dbg(void *mutex, const char *file, int line)
{
   mdbg("%s:%d\n", file, line);
   return _dvr_mutex_save(mutex);
}
void _dvr_mutex_restore_dbg(void *mutex, int val, const char *file, int line)
{
   mdbg("%s:%d\n", file, line);
   _dvr_mutex_restore(mutex, val);
}
#endif

