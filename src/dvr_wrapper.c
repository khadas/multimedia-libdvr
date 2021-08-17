#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "dvr_types.h"
#include "dvr_record.h"
#include "dvr_crypto.h"
#include "dvr_playback.h"
#include "dvr_segment.h"

#include "AmTsPlayer.h"

#include "list.h"

#include "dvr_wrapper.h"

#define DVR_WRAPPER_DEBUG(_level, _fmt...) \
  DVR_DEBUG_FL(_level, "wrapper", _fmt)

/*duration of data to resume if paused with EVT_REACHED_END in timeshifting*/
#define TIMESHIFT_DATA_DURATION_TO_RESUME (600)
/*a tolerant gap*/
#define DVR_PLAYBACK_END_GAP              (1000)

enum {
  W_REC      = 1,
  W_PLAYBACK = 2,
};

enum {
  U_PIDS     = 0x01,
  U_STAT     = 0x02,
  U_ALL      = U_PIDS | U_STAT,
};

typedef struct {
  /*make lock the 1st item in the structure*/
  pthread_mutex_t               lock;

  /*rec or play*/
  int                           type;

  /*valid if (sn != 0)*/
  unsigned long                 sn;
  unsigned long                 sn_linked;

  struct list_head              segments;                    /**<head-add list*/
  uint64_t                      current_segment_id;          /**<id of the current segment*/

  union {
    struct {
      DVR_WrapperRecordOpenParams_t   param_open;
      DVR_RecordStartParams_t         param_start;
      DVR_RecordStartParams_t         param_update;
      DVR_RecordHandle_t              recorder;
      DVR_RecordEventFunction_t       event_fn;
      void                            *event_userdata;

      /*total status = seg_status + status + obsolete*/
      DVR_RecordStatus_t              seg_status;            /**<status of current segment*/
      DVR_WrapperRecordStatus_t       status;                /**<status of remaining segments*/
      uint64_t                        next_segment_id;

      DVR_WrapperInfo_t               obsolete;             /**<data obsolete due to the max limit*/
    } record;

    struct {
      DVR_WrapperPlaybackOpenParams_t param_open;
      DVR_PlaybackHandle_t            player;
      DVR_PlaybackEventFunction_t     event_fn;
      void                            *event_userdata;

      /*total status = seg_status + status*/
      DVR_PlaybackStatus_t            seg_status;
      DVR_WrapperPlaybackStatus_t     status;
      DVR_PlaybackPids_t              pids_req;
      DVR_PlaybackEvent_t             last_event;
      float                           speed;
      DVR_Bool_t                      reach_end;

      DVR_WrapperInfo_t               obsolete;
    } playback;
  };
} DVR_WrapperCtx_t;

typedef struct {
  struct list_head head;
  unsigned long sn;

  /* rec or playback */
  int type;

  union {
    struct {
      DVR_RecordEvent_t event;
      DVR_RecordStatus_t status;
    } record;
    struct {
      DVR_PlaybackEvent_t event;
      DVR_Play_Notify_t status;
    } playback;
  };
} DVR_WrapperEventCtx_t;

typedef struct {
  pthread_mutex_t lock;
  char            *name;
  int             running;
  pthread_cond_t  cond;
  pthread_t       thread;
  int             type;
} DVR_WrapperThreadCtx_t;

typedef struct {
  struct list_head head;

  DVR_RecordSegmentInfo_t seg_info;
  DVR_PlaybackSegmentInfo_t playback_info;
} DVR_WrapperPlaybackSegmentInfo_t;

typedef struct {
  struct list_head head;

  DVR_RecordSegmentInfo_t info;
} DVR_WrapperRecordSegmentInfo_t;

/* serial num generater */
static unsigned long sn = 1;
static pthread_mutex_t sn_lock = PTHREAD_MUTEX_INITIALIZER;

static inline unsigned long get_sn()
{
  unsigned long no;

  pthread_mutex_lock(&sn_lock);
  no = sn++;
  if (!no)
    no = sn++;
  pthread_mutex_unlock(&sn_lock);

  return no;
}

/* entity ctx */
#define DVR_WRAPPER_MAX 10

static DVR_WrapperCtx_t record_list[DVR_WRAPPER_MAX] =
{
  [0 ... (DVR_WRAPPER_MAX - 1)] =
  {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .type = W_REC,
  }
};

static DVR_WrapperCtx_t playback_list[DVR_WRAPPER_MAX] =
{
  [0 ... (DVR_WRAPPER_MAX - 1)] =
  {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .type = W_PLAYBACK,
  }
};

/* events lists */
static struct list_head record_evt_list = LIST_HEAD_INIT(record_evt_list);
static struct list_head playback_evt_list = LIST_HEAD_INIT(playback_evt_list);

static pthread_mutex_t record_evt_list_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t playback_evt_list_lock = PTHREAD_MUTEX_INITIALIZER;

static DVR_WrapperThreadCtx_t wrapper_thread[2] =
{
  [0] =
  {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .running = 0,
    .name = "record",
    .type = W_REC,
  },
  [1] =
  {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .running = 0,
    .name = "playback",
    .type = W_PLAYBACK,
  },
};

/*now only support one timeshift now*/
static unsigned long sn_timeshift_record;
static unsigned long sn_timeshift_playback;

static void *wrapper_task(void *arg);
static inline int process_handleEvents(DVR_WrapperEventCtx_t *evt, DVR_WrapperCtx_t *ctx);

static DVR_Result_t wrapper_record_event_handler(DVR_RecordEvent_t event, void *params, void *userdata);
static DVR_Result_t wrapper_playback_event_handler(DVR_PlaybackEvent_t event, void *params, void *userdata);

static int process_generateRecordStatus(DVR_WrapperCtx_t *ctx, DVR_WrapperRecordStatus_t *status);
static int process_generatePlaybackStatus(DVR_WrapperCtx_t *ctx, DVR_WrapperPlaybackStatus_t *status);

static int get_timespec_timeout(int timeout, struct timespec *ts)
{
  struct timespec ots;
  int left, diff;

  clock_gettime(CLOCK_MONOTONIC, &ots);

  ts->tv_sec  = ots.tv_sec + timeout / 1000;
  ts->tv_nsec = ots.tv_nsec;

  left = timeout % 1000;
  left *= 1000000;
  diff = 1000000000 - ots.tv_nsec;

  if (diff <= left) {
    ts->tv_sec++;
    ts->tv_nsec = left-diff;
  } else {
    ts->tv_nsec += left;
  }

  return 0;
}

static DVR_WrapperEventCtx_t *ctx_getEvent(struct list_head *list, pthread_mutex_t *list_lock)
{
  DVR_WrapperEventCtx_t *pevt;

  pthread_mutex_lock(list_lock);
  if (list_empty(list))
    pevt = NULL;
  else {
    pevt = list_first_entry(list, DVR_WrapperEventCtx_t, head);
    list_del(&pevt->head);
  }
  pthread_mutex_unlock(list_lock);

  return pevt;
}

static inline DVR_WrapperEventCtx_t *ctx_getRecordEvent()
{
  return ctx_getEvent(&record_evt_list, &record_evt_list_lock);
}

static inline DVR_WrapperEventCtx_t *ctx_getPlaybackEvent()
{
  return ctx_getEvent(&playback_evt_list, &playback_evt_list_lock);
}

static int ctx_addEvent(struct list_head *list, pthread_mutex_t *lock, DVR_WrapperEventCtx_t *evt)
{
  DVR_WrapperEventCtx_t *padd;
  padd = (DVR_WrapperEventCtx_t *)calloc(1, sizeof(DVR_WrapperEventCtx_t));
  DVR_RETURN_IF_FALSE(padd);

  *padd = *evt;
  pthread_mutex_lock(lock);
  list_add_tail(&padd->head, list);
  pthread_mutex_unlock(lock);
  return DVR_SUCCESS;
}

static inline void ctx_freeEvent(DVR_WrapperEventCtx_t *evt)
{
  free(evt);
}

/*useless*/
static void ctx_cleanOutdatedEvents(struct list_head *evt_list,
    pthread_mutex_t *evt_list_lock,
    DVR_WrapperCtx_t *list)
{
  DVR_WrapperEventCtx_t *pevt, *pevt_tmp;
  unsigned long sns[DVR_WRAPPER_MAX];
  int cnt = 0;
  int i;
  int found = 0;

  /*copy all valid sns*/
  for (i = 0; i < DVR_WRAPPER_MAX; i++) {
    sns[cnt] = list[i].sn;
    if (!sns[cnt])
      cnt++;
  }

  /*free evts that not belong to any valid sns*/
  pthread_mutex_lock(evt_list_lock);
  list_for_each_entry_safe(pevt, pevt_tmp, evt_list, head) {
    for (i = 0; i < cnt; i++) {
      if (pevt->sn == sns[i]) {
        found = 1;
        break;
      }
    }
    if (!found) {
      list_del(&pevt->head);
      ctx_freeEvent(pevt);
    }
  }
  pthread_mutex_unlock(evt_list_lock);
}

static inline void ctx_cleanOutdatedRecordEvents()
{
  ctx_cleanOutdatedEvents(&record_evt_list, &record_evt_list_lock, record_list);
}

static inline void ctx_cleanOutdatedPlaybackEvents()
{
  ctx_cleanOutdatedEvents(&playback_evt_list, &playback_evt_list_lock, playback_list);
}

//check this play is recording file
//return 0 if not the recording
//else return record id
static inline int ctx_isPlay_recording(char *play_location)
{
  int i;
  DVR_WrapperCtx_t *cnt;

  for (i = 0; i < DVR_WRAPPER_MAX; i++) {
    cnt = &record_list[i];
    //DVR_WRAPPER_DEBUG(1, "[%d]sn[%d]R:[%s]P:[%s] ...\n", i, cnt->sn, cnt->record.param_open.location, play_location);
    if (!strcmp(cnt->record.param_open.location, play_location)) {
      DVR_WRAPPER_DEBUG(1, "[%d]sn[%d]R:[%s]P:[%s] .found..\n", i, cnt->sn, cnt->record.param_open.location, play_location);
      return cnt->sn;
    }
  }
  DVR_WRAPPER_DEBUG(1, " not found play is recing [%d]", DVR_WRAPPER_MAX);
  return 0;
}
//check this record is playing file
//return 0 if not the playing
//else return playback id
static inline int ctx_isRecord_playing(char *rec_location)
{
    int i;
    DVR_WrapperCtx_t *cnt;
    for (i = 0; i < DVR_WRAPPER_MAX; i++) {
      cnt = &playback_list[i];
      //DVR_WRAPPER_DEBUG(1, "[%d]sn[%d]P[%s]R[%s] ...\n", i, cnt->sn, cnt->playback.param_open.location, rec_location);
      if (!strcmp(cnt->playback.param_open.location, rec_location)) {
        DVR_WRAPPER_DEBUG(1, "[%d]sn[%d]P[%s]R[%s] ..found.\n",i, cnt->sn, cnt->playback.param_open.location, rec_location);
        return cnt->sn;
      }
    }
    DVR_WRAPPER_DEBUG(1, " not found rec is playing [%d]", DVR_WRAPPER_MAX);
    return 0;
}

static inline DVR_WrapperCtx_t *ctx_get(unsigned long sn, DVR_WrapperCtx_t *list)
{
  int i;
  for (i = 0; i < DVR_WRAPPER_MAX; i++) {
    if (list[i].sn == sn)
      return &list[i];
  }
  return NULL;
}

static inline void ctx_reset(DVR_WrapperCtx_t *ctx)
{
  memset((char *)ctx + offsetof(DVR_WrapperCtx_t, sn),
    0,
    sizeof(DVR_WrapperCtx_t) - offsetof(DVR_WrapperCtx_t, sn));
}

static inline int ctx_valid(DVR_WrapperCtx_t *ctx)
{
  return (ctx->sn != 0);
}

static inline DVR_WrapperCtx_t *ctx_getRecord(unsigned long sn)
{
  return ctx_get(sn, record_list);
}

static inline DVR_WrapperCtx_t *ctx_getPlayback(unsigned long sn)
{
  return ctx_get(sn, playback_list);
}

static int wrapper_requestThread(DVR_WrapperThreadCtx_t *ctx, void *(thread_fn)(void *))
{
  pthread_mutex_lock(&ctx->lock);
  if (ctx->running == 0) {
    pthread_condattr_t attr;
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&ctx->cond, &attr);
    pthread_condattr_destroy(&attr);
    DVR_WRAPPER_DEBUG(1, "start wrapper thread(%s) ...\n", ctx->name);
    pthread_create(&ctx->thread, NULL, thread_fn, ctx);
    DVR_WRAPPER_DEBUG(1, "wrapper thread(%s) started\n", ctx->name);
  }
  ctx->running++;
  pthread_mutex_unlock(&ctx->lock);
  return 0;
}

static int wrapper_releaseThread(DVR_WrapperThreadCtx_t *ctx)
{
  pthread_mutex_lock(&ctx->lock);
  ctx->running--;
  if (!ctx->running) {
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    DVR_WRAPPER_DEBUG(1, "stop wrapper thread(%s) ...\n", ctx->name);
    pthread_join(ctx->thread, NULL);
    DVR_WRAPPER_DEBUG(1, "wrapper thread(%s) stopped\n", ctx->name);

    pthread_mutex_lock(&ctx->lock);
    if (!ctx->running) /*protect*/
        pthread_cond_destroy(&ctx->cond);
  }
  pthread_mutex_unlock(&ctx->lock);
  return 0;
}

#define WRAPPER_THREAD_RECORD    (&wrapper_thread[0])
#define WRAPPER_THREAD_PLAYBACK  (&wrapper_thread[1])

static inline int wrapper_requestThreadFor(DVR_WrapperCtx_t *ctx)
{
  DVR_WrapperThreadCtx_t *thread_ctx = (ctx->type == W_REC)?
    WRAPPER_THREAD_RECORD : WRAPPER_THREAD_PLAYBACK;
  return wrapper_requestThread(thread_ctx, wrapper_task);
}

static inline int wrapper_releaseThreadFor(DVR_WrapperCtx_t *ctx)
{
  DVR_WrapperThreadCtx_t *thread_ctx = (ctx->type == W_REC)?
    WRAPPER_THREAD_RECORD : WRAPPER_THREAD_PLAYBACK;
  return wrapper_releaseThread(thread_ctx);
}

static inline int wrapper_releaseThreadForType(int type)
{
  DVR_WrapperThreadCtx_t *thread_ctx = (type == W_REC)?
    WRAPPER_THREAD_RECORD : WRAPPER_THREAD_PLAYBACK;
  return wrapper_releaseThread(thread_ctx);
}

static inline void wrapper_threadSignal(DVR_WrapperThreadCtx_t *thread_ctx)
{
  pthread_cond_signal(&thread_ctx->cond);
}

static inline int wrapper_threadWait(DVR_WrapperThreadCtx_t *thread_ctx)
{
  struct timespec rt;
  get_timespec_timeout(200, &rt);
  pthread_cond_timedwait(&thread_ctx->cond, &thread_ctx->lock, &rt);
  return 0;
}

static inline void wrapper_threadSignalForType(int type)
{
  DVR_WrapperThreadCtx_t *thread_ctx = (type == W_REC) ?
    WRAPPER_THREAD_RECORD : WRAPPER_THREAD_PLAYBACK;
  wrapper_threadSignal(thread_ctx);
}

static inline void wrapper_threadSignalFor(DVR_WrapperCtx_t *ctx)
{
  DVR_WrapperThreadCtx_t *thread_ctx = (ctx->type == W_REC) ?
    WRAPPER_THREAD_RECORD : WRAPPER_THREAD_PLAYBACK;
  wrapper_threadSignal(thread_ctx);
}

static inline int wrapper_threadWaitFor(DVR_WrapperCtx_t *ctx)
{
  DVR_WrapperThreadCtx_t *thread_ctx = (ctx->type == W_REC) ?
    WRAPPER_THREAD_RECORD : WRAPPER_THREAD_PLAYBACK;
  wrapper_threadWait(thread_ctx);
  return 0;
}

static void get_timeout_real(int timeout, struct timespec *ts)
{
  struct timespec ots;
  int left, diff;

  clock_gettime(CLOCK_REALTIME, &ots);

  ts->tv_sec  = ots.tv_sec + timeout/1000;
  ts->tv_nsec = ots.tv_nsec;

  left = timeout % 1000;
  left *= 1000000;
  diff = 1000000000-ots.tv_nsec;

  if (diff <= left) {
    ts->tv_sec++;
    ts->tv_nsec = left-diff;
  } else {
    ts->tv_nsec += left;
  }
}

/*return condition, locked if condition == true*/
static int wrapper_mutex_lock_if(pthread_mutex_t *lock, int *condition)
{
  int r2;
  do {
    struct timespec rt2;
    /*android use real time for mutex timedlock*/
    get_timeout_real(10, &rt2);
    r2 = pthread_mutex_timedlock(lock, &rt2);
  } while (*condition && (r2 == ETIMEDOUT));

  if (!(*condition) && (r2 == 0))
    pthread_mutex_unlock(lock);

  return *condition;
}

static void *wrapper_task(void *arg)
{
  DVR_WrapperThreadCtx_t *tctx = (DVR_WrapperThreadCtx_t *)arg;
  DVR_WrapperEventCtx_t *evt;

  pthread_mutex_lock(&tctx->lock);

  while (tctx->running) {
    {
      int ret;

      evt = (tctx->type == W_REC)? ctx_getRecordEvent() : ctx_getPlaybackEvent();
      if (!evt)
        ret = wrapper_threadWait(tctx);
    }

    while (evt) {
      DVR_WrapperCtx_t *ctx = (evt->type == W_REC)?
        ctx_getRecord(evt->sn) : ctx_getPlayback(evt->sn);
      if (ctx == NULL) {
        DVR_WRAPPER_DEBUG(1, "warp not get ctx.free event..\n");
        goto processed;
      }
      DVR_WRAPPER_DEBUG(1, "start name(%s) sn(%d) running(%d) type(%d)\n", tctx->name, (int)ctx->sn, tctx->running, tctx->type);
      if (tctx->running) {
        /*
          continue not break,
          make all events consumed, or mem leak
        */
        if (!wrapper_mutex_lock_if(&ctx->lock, &tctx->running))
            goto processed;

        if (ctx_valid(ctx)) {
          /*double check after lock*/
          if (evt->sn == ctx->sn)
            process_handleEvents(evt, ctx);
        }
        pthread_mutex_unlock(&ctx->lock);
      }

processed:
      ctx_freeEvent(evt);

      evt = (tctx->type == W_REC)? ctx_getRecordEvent() : ctx_getPlaybackEvent();
    }
    DVR_WRAPPER_DEBUG(1, "start name(%s) running(%d) type(%d) con...\n", tctx->name, tctx->running, tctx->type);
  }

  pthread_mutex_unlock(&tctx->lock);
  DVR_WRAPPER_DEBUG(1, "end name(%s) running(%d) type(%d) end...\n", tctx->name, tctx->running, tctx->type);
  return NULL;
}

static inline int ctx_addRecordEvent(DVR_WrapperEventCtx_t *evt)
{
  pthread_mutex_lock(&WRAPPER_THREAD_RECORD->lock);
  if (ctx_addEvent(&record_evt_list, &record_evt_list_lock, evt) == 0)
    wrapper_threadSignalForType(evt->type);
  pthread_mutex_unlock(&WRAPPER_THREAD_RECORD->lock);
  return 0;
}

static inline int ctx_addPlaybackEvent(DVR_WrapperEventCtx_t *evt)
{
  pthread_mutex_lock(&WRAPPER_THREAD_PLAYBACK->lock);
  if (ctx_addEvent(&playback_evt_list, &playback_evt_list_lock, evt) == 0)
      wrapper_threadSignalForType(evt->type);
  pthread_mutex_unlock(&WRAPPER_THREAD_PLAYBACK->lock);
  return 0;
}

static inline void ctx_freeSegments(DVR_WrapperCtx_t *ctx)
{
  DVR_WrapperPlaybackSegmentInfo_t *pseg, *pseg_tmp;
  list_for_each_entry_safe(pseg, pseg_tmp, &ctx->segments, head) {
    list_del(&pseg->head);
    free(pseg);
  }
}

static inline void _updatePlaybackSegment(DVR_WrapperPlaybackSegmentInfo_t *pseg,
    DVR_RecordSegmentInfo_t *seg_info, int update_flags, DVR_WrapperCtx_t *ctx)
{
  (void)ctx;
  if ((update_flags & U_PIDS) && (update_flags & U_STAT))
    pseg->seg_info = *seg_info;
  else if (update_flags & U_PIDS) {
    pseg->seg_info.nb_pids = seg_info->nb_pids;
    memcpy(pseg->seg_info.pids, seg_info->pids, sizeof(pseg->seg_info.pids));
  } else if (update_flags & U_STAT) {
    pseg->seg_info.duration = seg_info->duration;
    pseg->seg_info.size = seg_info->size;
    pseg->seg_info.nb_packets = seg_info->nb_packets;
  }
  //update current segment duration on timeshift mode
  if (ctx->playback.param_open.is_timeshift
      || ctx_isPlay_recording(ctx->playback.param_open.location))
    dvr_playback_update_duration(ctx->playback.player,pseg->seg_info.id,pseg->seg_info.duration);
  /*no changes
  DVR_PlaybackSegmentFlag_t flags;
  pseg->playback_info.segment_id = pseg->seg_info.id;
  strncpy(pseg->playback_info.location,
    ctx->playback.param_open.location, sizeof(pseg->playback_info.location));
  pseg->playback_info.pids = ctx->playback.pids_req;
  flags = DVR_PLAYBACK_SEGMENT_DISPLAYABLE | DVR_PLAYBACK_SEGMENT_CONTINUOUS;
  if (ctx->record.param_open.flags | DVR_RECORD_FLAG_SCRAMBLED)
    flags |= DVR_PLAYBACK_SEGMENT_ENCRYPTED;
  pseg->playback_info.flags = flags;
  */
}

static int wrapper_updatePlaybackSegment(DVR_WrapperCtx_t *ctx, DVR_RecordSegmentInfo_t *seg_info, int update_flags)
{
  DVR_WrapperPlaybackSegmentInfo_t *pseg;

  DVR_WRAPPER_DEBUG(1, "timeshift, update playback segments(wrapper), seg:%lld t/s/p(%ld/%zu/%u)\n",
    seg_info->id, seg_info->duration, seg_info->size, seg_info->nb_packets);

  if (list_empty(&ctx->segments)) {
    DVR_WRAPPER_DEBUG(1, "timeshift, update while no segment exists, ignore\n");
    return DVR_SUCCESS;
  }

  /*normally, the last segment added will be updated*/
  pseg =
    list_first_entry(&ctx->segments, DVR_WrapperPlaybackSegmentInfo_t, head);
  if (pseg->seg_info.id == seg_info->id) {
    _updatePlaybackSegment(pseg, seg_info, update_flags, ctx);
  } else {
    list_for_each_entry_reverse(pseg, &ctx->segments, head) {
      if (pseg->seg_info.id == seg_info->id) {
        _updatePlaybackSegment(pseg, seg_info, update_flags, ctx);
        break;
      }
    }
  }

  /*need to notify the dvr_playback*/
  if ((ctx->playback.param_open.is_timeshift/*should must be timeshift*/
      || ctx_isPlay_recording(ctx->playback.param_open.location))
    && ctx->playback.last_event == DVR_PLAYBACK_EVENT_REACHED_END
    && ctx->playback.seg_status.state == DVR_PLAYBACK_STATE_PAUSE) {
    if (
      /*there's $TIMESHIFT_DATA_DURATION_TO_RESUME more of data in the current segment playing*/
      (ctx->playback.seg_status.segment_id == seg_info->id
      && (seg_info->duration >= ((time_t)ctx->playback.seg_status.time_cur + TIMESHIFT_DATA_DURATION_TO_RESUME)))
      ||
      /*or there's a new segment and has $TIMESHIFT_DATA_DURATION_TO_RESUME of data*/
      (ctx->playback.seg_status.segment_id != seg_info->id
      && (seg_info->duration >= TIMESHIFT_DATA_DURATION_TO_RESUME))
      )
    {
      int error;
      //clear end event
      if (ctx->playback.last_event == DVR_PLAYBACK_EVENT_REACHED_END)
          ctx->playback.last_event = DVR_PLAYBACK_EVENT_TRANSITION_OK;

      error = dvr_playback_resume(ctx->playback.player);
      DVR_WRAPPER_DEBUG(1, "timeshift, resume playback(sn:%ld) (%d) id/dur: rec(%lld/%ld) play(%lld/%u)\n",
        ctx->sn, error,
        seg_info->id, seg_info->duration,
        ctx->playback.seg_status.segment_id, ctx->playback.seg_status.time_cur);
    }
  }

  return DVR_SUCCESS;
}

static void _updateRecordSegment(DVR_WrapperRecordSegmentInfo_t *pseg,
  DVR_RecordSegmentInfo_t *seg_info, int update_flags, DVR_WrapperCtx_t *ctx)
{
  (void)ctx;
  if ((update_flags & U_PIDS) && (update_flags & U_STAT))
    pseg->info = *seg_info;
  else if (update_flags & U_PIDS) {
    pseg->info.nb_pids = seg_info->nb_pids;
    memcpy(pseg->info.pids, seg_info->pids, sizeof(pseg->info.pids));
  } else if (update_flags & U_STAT) {
    pseg->info.duration = seg_info->duration;
    pseg->info.size = seg_info->size;
    pseg->info.nb_packets = seg_info->nb_packets;
  }
}

static int wrapper_updateRecordSegment(DVR_WrapperCtx_t *ctx, DVR_RecordSegmentInfo_t *seg_info, int update_flags)
{
  DVR_WrapperRecordSegmentInfo_t *pseg = NULL;

  /*normally, the last segment added will be updated*/
  if (!list_empty(&ctx->segments)) {
    pseg =
      list_first_entry(&ctx->segments, DVR_WrapperRecordSegmentInfo_t, head);
    if (pseg->info.id == seg_info->id) {
      _updateRecordSegment(pseg, seg_info, update_flags, ctx);
    } else {
      list_for_each_entry_reverse(pseg, &ctx->segments, head) {
        if (pseg->info.id == seg_info->id) {
          _updateRecordSegment(pseg, seg_info, update_flags, ctx);
          break;
        }
      }
    }
  }

  /*timeshift, update the segment for playback*/
  /*
    the playback should grab the segment info other than the id,
    and the id will be updated by each segment-add during the recording
  */
  /*
    the playback paused if no data been checked from recording,
         should resume the player later when there's more data
  */
  int sn = 0;
  if (ctx->record.param_open.is_timeshift ||
    (sn = ctx_isRecord_playing(ctx->record.param_open.location))) {
    DVR_WrapperCtx_t *ctx_playback;
    if (ctx->record.param_open.is_timeshift)
      ctx_playback = ctx_getPlayback(sn_timeshift_playback);
    else
      ctx_playback = ctx_getPlayback(sn);

    if (ctx_playback) {
      pthread_mutex_lock(&ctx_playback->lock);
      if (ctx_valid(ctx_playback)
          && (ctx_playback->sn == sn_timeshift_playback ||
              ctx_playback->sn == sn)) {
          wrapper_updatePlaybackSegment(ctx_playback, seg_info, update_flags);
      }
      pthread_mutex_unlock(&ctx_playback->lock);
    }
  }

  return DVR_SUCCESS;
}

static int wrapper_addPlaybackSegment(DVR_WrapperCtx_t *ctx,
    DVR_RecordSegmentInfo_t *seg_info,
    DVR_PlaybackPids_t *p_pids,
    DVR_PlaybackSegmentFlag_t flags)
{
  DVR_WrapperPlaybackSegmentInfo_t *pseg;
  int error;

  error = 0;
  pseg = (DVR_WrapperPlaybackSegmentInfo_t *)calloc(1, sizeof(DVR_WrapperPlaybackSegmentInfo_t));
  if (!pseg) {
    error = DVR_FAILURE;
    DVR_WRAPPER_DEBUG(1, "memory fail\n");
    return error;
  }

  /*copy the orignal segment info*/
  pseg->seg_info = *seg_info;
  /*generate the segment info used in playback*/
  pseg->playback_info.segment_id = pseg->seg_info.id;
  strncpy(pseg->playback_info.location, ctx->playback.param_open.location, sizeof(pseg->playback_info.location));
  pseg->playback_info.pids = *p_pids;
  pseg->playback_info.flags = flags;
  list_add(&pseg->head, &ctx->segments);
  pseg->playback_info.duration = pseg->seg_info.duration;

  error = dvr_playback_add_segment(ctx->playback.player, &pseg->playback_info);
  if (error) {
    DVR_WRAPPER_DEBUG(1, "fail to add segment %lld (%d)\n", pseg->playback_info.segment_id, error);
  } else {
    ctx->playback.status.info_full.time += pseg->seg_info.duration;
    ctx->playback.status.info_full.size += pseg->seg_info.size;
    ctx->playback.status.info_full.pkts += pseg->seg_info.nb_packets;
  }

  return error;
}

static int wrapper_addRecordSegment(DVR_WrapperCtx_t *ctx, DVR_RecordSegmentInfo_t *seg_info)
{
  DVR_WrapperRecordSegmentInfo_t *pseg;
  int error;

  error = 0;
  pseg = (DVR_WrapperRecordSegmentInfo_t *)calloc(1, sizeof(DVR_WrapperRecordSegmentInfo_t));
  if (!pseg) {
    error = DVR_FAILURE;
    DVR_WRAPPER_DEBUG(1, "memory fail\n");
  }
  pseg->info = *seg_info;
  list_add(&pseg->head, &ctx->segments);
  
  if (ctx->record.param_open.is_timeshift ||
      (sn = ctx_isRecord_playing(ctx->record.param_open.location))) {

    DVR_WrapperCtx_t *ctx_playback;
    if (ctx->record.param_open.is_timeshift)
      ctx_playback = ctx_getPlayback(sn_timeshift_playback);
    else
      ctx_playback = ctx_getPlayback(sn);

    DVR_WRAPPER_DEBUG(1, "ctx_playback ---- add segment\n");

    if (ctx_playback) {
      pthread_mutex_lock(&ctx_playback->lock);
      if (ctx_valid(ctx_playback)) {
        DVR_PlaybackSegmentFlag_t flags;

        /*only if playback has started, the previous segments have been loaded*/
        if (!list_empty(&ctx_playback->segments)) {
          flags = DVR_PLAYBACK_SEGMENT_DISPLAYABLE | DVR_PLAYBACK_SEGMENT_CONTINUOUS;
          if (ctx->record.param_open.flags & DVR_RECORD_FLAG_SCRAMBLED)
            flags |= DVR_PLAYBACK_SEGMENT_ENCRYPTED;
          wrapper_addPlaybackSegment(ctx_playback, seg_info, &ctx_playback->playback.pids_req, flags);
        }
      } else {
            DVR_WRAPPER_DEBUG(1, "ctx_playback ---- not valid\n");
      }
      pthread_mutex_unlock(&ctx_playback->lock);
    }
  } else {
    DVR_WRAPPER_DEBUG(1, "ctx_playback -sn[%d]-\n", sn);
    dvr_segment_link_op(ctx->record.param_open.location, 1, &seg_info->id, LSEG_OP_ADD);
  }

  return error;
}

static int wrapper_removePlaybackSegment(DVR_WrapperCtx_t *ctx, DVR_RecordSegmentInfo_t *seg_info)
{
  int error = -1;
  DVR_WrapperPlaybackSegmentInfo_t *pseg = NULL, *pseg_tmp;

  DVR_WRAPPER_DEBUG(1, "timeshift, remove playback(sn:%ld) segment(%lld) ...\n", ctx->sn, seg_info->id);

  list_for_each_entry_safe_reverse(pseg, pseg_tmp, &ctx->segments, head) {
    if (pseg->seg_info.id == seg_info->id) {

      if (ctx->current_segment_id == seg_info->id) {
        DVR_WrapperPlaybackSegmentInfo_t *next_seg;

        /*drive the player out of this will-be-deleted segment*/
        next_seg = list_prev_entry(pseg, head);

        if (ctx->playback.speed != 100.0f) {
          error = dvr_playback_resume(ctx->playback.player);
          DVR_WRAPPER_DEBUG(1, "timeshift, playback(sn:%ld), resume for new start (%d)\n", ctx->sn, error);
        }

        error = dvr_playback_seek(ctx->playback.player, next_seg->seg_info.id, 0);
        DVR_WRAPPER_DEBUG(1, "timeshift, playback(sn:%ld), seek(seg:%llu 0) from new start (%d)\n", ctx->sn, next_seg->seg_info.id, error);

        if (ctx->playback.speed == 0.0f) {
          error = dvr_playback_pause(ctx->playback.player, DVR_FALSE);
          DVR_WRAPPER_DEBUG(1, "timeshift, playback(sn:%ld), keep last paused from new start (%d)\n", ctx->sn, error);
        } else if (ctx->playback.speed != 100.0f) {
          DVR_PlaybackSpeed_t dvr_speed = {
             .speed = { ctx->playback.speed },
             .mode = ( ctx->playback.speed > 0) ? DVR_PLAYBACK_FAST_FORWARD : DVR_PLAYBACK_FAST_BACKWARD
          };
          error = dvr_playback_set_speed(ctx->playback.player, dvr_speed);
          DVR_WRAPPER_DEBUG(1, "timeshift, playback(sn:%ld), keep last speed(x%f) from new start (%d)\n", ctx->sn,ctx->playback.speed, error);
        }
      }

      error = dvr_playback_remove_segment(ctx->playback.player, seg_info->id);
      if (error) {
        /*remove playack segment fail*/
        DVR_WRAPPER_DEBUG(1, "timeshift, playback(sn:%ld), failed to remove segment(%llu) (%d)\n", ctx->sn, seg_info->id, error);
      }

      list_del(&pseg->head);

      /*record the obsolete*/
      ctx->playback.obsolete.time += pseg->seg_info.duration;
      ctx->playback.obsolete.size += pseg->seg_info.size;
      ctx->playback.obsolete.pkts += pseg->seg_info.nb_packets;
      dvr_playback_set_obsolete(ctx->playback.player, ctx->playback.obsolete.time);
      free(pseg);
      break;
    }
  }

  DVR_WRAPPER_DEBUG(1, "timeshift, remove playback(sn:%ld) segment(%lld) =(%d)\n", ctx->sn, seg_info->id, error);

  return error;
}

static int wrapper_removeRecordSegment(DVR_WrapperCtx_t *ctx, DVR_WrapperRecordSegmentInfo_t *seg_info)
{
  int error;
  DVR_WrapperRecordSegmentInfo_t *pseg, *pseg_tmp;

  DVR_WRAPPER_DEBUG(1, "timeshift, remove record(sn:%ld) segment(%lld) ...\n", ctx->sn, seg_info->info.id);

  /*if timeshifting, notify the playback first, then deal with record*/
  if (ctx->record.param_open.is_timeshift) {
    DVR_WrapperCtx_t *ctx_playback = ctx_getPlayback(sn_timeshift_playback);

    if (ctx_playback) {
      pthread_mutex_lock(&ctx_playback->lock);
      if (ctx_valid(ctx_playback)
        && ctx_playback->sn == sn_timeshift_playback
        && !list_empty(&ctx_playback->segments)) {
        error = wrapper_removePlaybackSegment(ctx_playback, &seg_info->info);
      }
      pthread_mutex_unlock(&ctx_playback->lock);
    }
  }

  list_for_each_entry_safe_reverse(pseg, pseg_tmp, &ctx->segments, head) {
    if (pseg->info.id == seg_info->info.id) {
      list_del(&pseg->head);

      /*record the obsolete*/
      ctx->record.obsolete.time += pseg->info.duration;
      ctx->record.obsolete.size += pseg->info.size;
      ctx->record.obsolete.pkts += pseg->info.nb_packets;

      free(pseg);
      break;
    }
  }

  error = dvr_segment_delete(ctx->record.param_open.location, seg_info->info.id);

  DVR_WRAPPER_DEBUG(1, "timeshift, remove record(sn:%ld) segment(%lld) =(%d)\n", ctx->sn, seg_info->info.id, error);

  return error;
}

int dvr_wrapper_open_record (DVR_WrapperRecord_t *rec, DVR_WrapperRecordOpenParams_t *params)
{
  int error;
  DVR_WrapperCtx_t *ctx;
  DVR_RecordOpenParams_t open_param;

  DVR_RETURN_IF_FALSE(rec);
  DVR_RETURN_IF_FALSE(params);

  /*get a free ctx*/
  ctx = ctx_getRecord(0);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "open record(dmx:%d) .istf(%d)..time (%ld)ms max size(%lld)byte seg size(%lld)byte\n",
  params->dmx_dev_id, params->is_timeshift, params->max_time, params->max_size, params->segment_size);

  ctx_reset(ctx);

  ctx->record.param_open = *params;
  ctx->record.event_fn = params->event_fn;
  ctx->record.event_userdata = params->event_userdata;
  ctx->record.next_segment_id = 0;
  ctx->current_segment_id = 0;
  INIT_LIST_HEAD(&ctx->segments);
  ctx->sn = get_sn();

  wrapper_requestThreadFor(ctx);

  memset(&open_param, 0, sizeof(DVR_RecordOpenParams_t));
  open_param.fend_dev_id = params->fend_dev_id;
  open_param.dmx_dev_id = params->dmx_dev_id;
  open_param.data_from_memory = 0;
  open_param.flags = params->flags;
  open_param.notification_size = 500*1024;
  open_param.flush_size = params->flush_size;
  open_param.ringbuf_size = params->ringbuf_size;
  open_param.event_fn = wrapper_record_event_handler;
  open_param.event_userdata = (void*)ctx->sn;

  error = dvr_record_open(&ctx->record.recorder, &open_param);
  if (error) {
    DVR_WRAPPER_DEBUG(1, "record(dmx:%d) open fail(error:%d).\n", params->dmx_dev_id, error);
    ctx_reset(ctx);
    pthread_mutex_unlock(&ctx->lock);
    wrapper_releaseThreadForType(ctx->type);
    return DVR_FAILURE;
  }
  if (params->is_timeshift)
    sn_timeshift_record = ctx->sn;

  DVR_WRAPPER_DEBUG(1, "record(dmx:%d) openned ok(sn:%ld).\n", params->dmx_dev_id, ctx->sn);

  error = dvr_record_set_encrypt_callback(ctx->record.recorder, params->crypto_fn, params->crypto_data);
  if (error) {
    DVR_WRAPPER_DEBUG(1, "record(dmx:%d) set encrypt callback fail(error:%d).\n", params->dmx_dev_id, error);
  }

  pthread_mutex_unlock(&ctx->lock);

  *rec = (DVR_WrapperRecord_t)ctx->sn;
  return DVR_SUCCESS;
}

int dvr_wrapper_close_record (DVR_WrapperRecord_t rec)
{
  DVR_WrapperCtx_t *ctx;
  DVR_RecordSegmentInfo_t seg_info;
  int error;

  DVR_RETURN_IF_FALSE(rec);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "close record(sn:%ld)\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  memset(&seg_info, 0, sizeof(seg_info));
  error = dvr_record_stop_segment(ctx->record.recorder, &seg_info);

  error = dvr_record_close(ctx->record.recorder);

  if (ctx->record.param_open.is_timeshift)
    sn_timeshift_record = 0;

  ctx_freeSegments(ctx);

  DVR_WRAPPER_DEBUG(1, "record(sn:%ld) closed = (%d).\n", ctx->sn, error);
  ctx_reset(ctx);
  pthread_mutex_unlock(&ctx->lock);

  wrapper_releaseThreadForType(ctx->type);

  return error;
}

int dvr_wrapper_start_record (DVR_WrapperRecord_t rec, DVR_WrapperRecordStartParams_t *params)
{
  DVR_WrapperCtx_t *ctx;
  DVR_RecordStartParams_t *start_param;
  int i;
  int error;

  DVR_RETURN_IF_FALSE(rec);
  DVR_RETURN_IF_FALSE(params);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "start record(sn:%ld, location:%s) ...\n", ctx->sn, ctx->record.param_open.location);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  start_param = &ctx->record.param_start;
  memset(start_param, 0, sizeof(*start_param));
  strncpy(start_param->location, ctx->record.param_open.location, sizeof(start_param->location));
  start_param->segment.segment_id = ctx->record.next_segment_id++;
  start_param->segment.nb_pids = params->pids_info.nb_pids;
  for (i = 0; i < params->pids_info.nb_pids; i++) {
    start_param->segment.pids[i] = params->pids_info.pids[i];
    start_param->segment.pid_action[i] = DVR_RECORD_PID_CREATE;
  }
  dvr_segment_del_by_location(start_param->location);
  {
    /*sync to update for further use*/
    DVR_RecordStartParams_t *update_param;
    update_param = &ctx->record.param_update;
    memcpy(update_param, start_param, sizeof(*update_param));
    for (i = 0; i < update_param->segment.nb_pids; i++)
      update_param->segment.pid_action[i] = DVR_RECORD_PID_KEEP;
  }

  error = dvr_record_start_segment(ctx->record.recorder, start_param);
  {
    DVR_RecordSegmentInfo_t new_seg_info =
      { .id = start_param->segment.segment_id, };
    wrapper_addRecordSegment(ctx, &new_seg_info);
  }

  DVR_WRAPPER_DEBUG(1, "record(sn:%ld) started = (%d)\n", ctx->sn, error);

  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_stop_record (DVR_WrapperRecord_t rec)
{
  DVR_WrapperCtx_t *ctx;
  DVR_RecordSegmentInfo_t seg_info;
  int error;

  DVR_RETURN_IF_FALSE(rec);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "stop record(sn:%ld) ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  memset(&seg_info, 0, sizeof(seg_info));
  error = dvr_record_stop_segment(ctx->record.recorder, &seg_info);
  wrapper_updateRecordSegment(ctx, &seg_info, U_ALL);

  DVR_WRAPPER_DEBUG(1, "record(sn:%ld) stopped = (%d)\n", ctx->sn, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_pause_record (DVR_WrapperRecord_t rec)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(rec);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "pause record(sn:%ld) ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = dvr_record_pause(ctx->record.recorder);

  DVR_WRAPPER_DEBUG(1, "record(sn:%ld) pauseed = (%d)\n", ctx->sn, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_resume_record (DVR_WrapperRecord_t rec)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(rec);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "resume record(sn:%ld) ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = dvr_record_resume(ctx->record.recorder);

  DVR_WRAPPER_DEBUG(1, "record(sn:%ld) resumed = (%d)\n", ctx->sn, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_update_record_pids (DVR_WrapperRecord_t rec, DVR_WrapperUpdatePidsParams_t *params)
{
  DVR_WrapperCtx_t *ctx;
  DVR_RecordStartParams_t *start_param;
  DVR_RecordSegmentInfo_t seg_info;;
  int i;
  int error;

  DVR_RETURN_IF_FALSE(rec);
  DVR_RETURN_IF_FALSE(params);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "update record(sn:%ld)\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  start_param = &ctx->record.param_update;
  memset(start_param, 0, sizeof(*start_param));
  strncpy(start_param->location, ctx->record.param_open.location, sizeof(start_param->location));
  start_param->segment.segment_id = ctx->record.next_segment_id++;
  start_param->segment.nb_pids = params->nb_pids;
  for (i = 0; i < params->nb_pids; i++) {
    start_param->segment.pids[i] = params->pids[i];
    start_param->segment.pid_action[i] = params->pid_action[i];
  }
  error = dvr_record_next_segment(ctx->record.recorder, start_param, &seg_info);
  {
    DVR_RecordSegmentInfo_t new_seg_info =
      { .id = start_param->segment.segment_id, };
    wrapper_updateRecordSegment(ctx, &seg_info, U_PIDS);
    wrapper_addRecordSegment(ctx, &new_seg_info);
  }

  DVR_WRAPPER_DEBUG(1, "record(sn:%ld) updated = (%d)\n", ctx->sn, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_get_record_status(DVR_WrapperRecord_t rec, DVR_WrapperRecordStatus_t *status)
{
  DVR_WrapperCtx_t *ctx;
  DVR_WrapperRecordStatus_t s;
  int error;

  DVR_RETURN_IF_FALSE(rec);
  DVR_RETURN_IF_FALSE(status);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "get record(sn:%ld) status ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = process_generateRecordStatus(ctx, &s);

  DVR_WRAPPER_DEBUG(1, "record(sn:%ld) state/time/size/pkts(%d/%ld/%lld/%u) (%d)\n",
    ctx->sn,
    s.state,
    s.info.time,
    s.info.size,
    s.info.pkts,
    error);

  *status = s;

  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_record_is_secure_mode(DVR_WrapperRecord_t rec)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(rec);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  error = dvr_record_is_secure_mode(ctx->record.recorder);
  pthread_mutex_unlock(&ctx->lock);
  return error;
}

int dvr_wrapper_set_record_secure_buffer (DVR_WrapperRecord_t rec,  uint8_t *p_secure_buf, uint32_t len)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(rec);
  DVR_RETURN_IF_FALSE(p_secure_buf);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  error = dvr_record_set_secure_buffer(ctx->record.recorder, p_secure_buf, len);
  pthread_mutex_unlock(&ctx->lock);
  return error;
}

int dvr_wrapper_set_record_decrypt_callback (DVR_WrapperRecord_t rec,  DVR_CryptoFunction_t func, void *userdata)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(rec);
  DVR_RETURN_IF_FALSE(func);

  ctx = ctx_getRecord((unsigned long)rec);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  error = dvr_record_set_encrypt_callback(ctx->record.recorder, func, userdata);
  pthread_mutex_unlock(&ctx->lock);
  return error;
}


int dvr_wrapper_open_playback (DVR_WrapperPlayback_t *playback, DVR_WrapperPlaybackOpenParams_t *params)
{
  DVR_WrapperCtx_t *ctx;
  DVR_PlaybackOpenParams_t open_param;
  int error;

  DVR_RETURN_IF_FALSE(playback);
  DVR_RETURN_IF_FALSE(params);
  DVR_RETURN_IF_FALSE(params->playback_handle);

  /*get a free ctx*/
  ctx = ctx_getPlayback(0);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "open playback(dmx:%d) ..vendor[%d].\n", params->dmx_dev_id, params->vendor);

  ctx_reset(ctx);

  ctx->playback.param_open = *params;
  ctx->playback.event_fn = params->event_fn;
  ctx->playback.event_userdata = params->event_userdata;
  ctx->current_segment_id = 0;
  INIT_LIST_HEAD(&ctx->segments);
  ctx->sn = get_sn();

  wrapper_requestThreadFor(ctx);

  memset(&open_param, 0, sizeof(DVR_PlaybackOpenParams_t));
  open_param.dmx_dev_id = params->dmx_dev_id;
  open_param.block_size = params->block_size;
  open_param.is_timeshift = params->is_timeshift;
  //open_param.notification_size = 10*1024; //not supported
  open_param.event_fn = wrapper_playback_event_handler;
  open_param.event_userdata = (void*)ctx->sn;
  /*open_param.has_pids = 0;*/
  open_param.is_notify_time = params->is_notify_time;
  open_param.player_handle = (am_tsplayer_handle)params->playback_handle;
  open_param.vendor = params->vendor;


  error = dvr_playback_open(&ctx->playback.player, &open_param);
  if (error) {
    DVR_WRAPPER_DEBUG(1, "playback(dmx:%d) openned fail(error:%d).\n", params->dmx_dev_id, error);
    ctx_reset(ctx);
    pthread_mutex_unlock(&ctx->lock);
    wrapper_releaseThreadForType(ctx->type);
    return DVR_FAILURE;
  }
  if (params->is_timeshift)
    sn_timeshift_playback = ctx->sn;

  DVR_WRAPPER_DEBUG(1, "hanyh: playback(dmx:%d) openned ok(sn:%ld).\n", params->dmx_dev_id, ctx->sn);
  error = dvr_playback_set_decrypt_callback(ctx->playback.player, params->crypto_fn, params->crypto_data);
  if (error) {
    DVR_WRAPPER_DEBUG(1, "playback set deccrypt callback fail(error:%d).\n", error);
  }
  pthread_mutex_unlock(&ctx->lock);

  *playback = (DVR_WrapperPlayback_t)ctx->sn;
  return DVR_SUCCESS;
}

int dvr_wrapper_close_playback (DVR_WrapperPlayback_t playback)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(playback);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "close playback(sn:%ld)\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  if (ctx->playback.param_open.is_timeshift)
    sn_timeshift_playback = 0;

  /*try stop first*/
  error = dvr_playback_stop(ctx->playback.player, DVR_TRUE);

  {
    /*remove all segments*/
    DVR_WrapperPlaybackSegmentInfo_t *pseg;

    list_for_each_entry(pseg, &ctx->segments, head) {
      error = dvr_playback_remove_segment(ctx->playback.player, pseg->playback_info.segment_id);
      DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) remove seg(%lld) (%d)\n",
        ctx->sn, pseg->playback_info.segment_id, error);
    }
    ctx_freeSegments(ctx);
  }

  error = dvr_playback_close(ctx->playback.player);

  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) closed.\n", ctx->sn);
  ctx_reset(ctx);
  pthread_mutex_unlock(&ctx->lock);

  wrapper_releaseThreadForType(ctx->type);

  return error;
}

int dvr_wrapper_start_playback (DVR_WrapperPlayback_t playback, DVR_PlaybackFlag_t flags, DVR_PlaybackPids_t *p_pids)
{
  DVR_WrapperCtx_t *ctx;
  int error;
  uint64_t *p_segment_ids;
  uint32_t segment_nb;
  uint32_t i;
  DVR_RecordSegmentInfo_t seg_info_1st;
  int got_1st_seg;
  DVR_WrapperCtx_t *ctx_record;/*for timeshift*/
  DVR_Bool_t is_timeshift = DVR_FALSE;

  DVR_RETURN_IF_FALSE(playback);
  DVR_RETURN_IF_FALSE(p_pids);

  ctx_record = NULL;

  /*lock the recorder to avoid changing the recording segments*/
  ctx_record = ctx_getRecord(sn_timeshift_record);

  if (ctx_record) {
    pthread_mutex_lock(&ctx_record->lock);
    if (!ctx_valid(ctx_record)
      || ctx_record->sn != sn_timeshift_record) {
      DVR_WRAPPER_DEBUG(1, "timeshift, record is not for timeshifting, FATAL error found\n");
      pthread_mutex_unlock(&ctx_record->lock);
      is_timeshift  = DVR_FALSE;
    } else {
      is_timeshift  = DVR_TRUE;
    }
  }

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "start playback(sn:%ld) (%s)\n\t flags(0x%x) v/a/ad/sub/pcr(%d:%d %d:%d %d:%d %d:%d %d)\n",
    ctx->sn,
    ctx->playback.param_open.location,
    flags,
    p_pids->video.pid, p_pids->video.format,
    p_pids->audio.pid, p_pids->audio.format,
    p_pids->ad.pid, p_pids->ad.format,
    p_pids->subtitle.pid, p_pids->subtitle.format,
    p_pids->pcr.pid);

  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  if (ctx->playback.param_open.is_timeshift) {
    /*lock the recorder to avoid changing the recording segments*/
    if (is_timeshift == DVR_FALSE) {
      DVR_WRAPPER_DEBUG(1, "timeshift, record is not for timeshifting, FATAL error return\n");
      pthread_mutex_unlock(&ctx->lock);
      return DVR_FAILURE;
    } else {
      DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) record(sn:%ld) locked ok due to timeshift\n",
        ctx->sn, ctx_record->sn);
    }
  }

  /*obtain all segments in a list*/
  segment_nb = 0;
  p_segment_ids = NULL;
  error = dvr_segment_get_list(ctx->playback.param_open.location, &segment_nb, &p_segment_ids);
  if (!error) {
    got_1st_seg = 0;
    DVR_WRAPPER_DEBUG(1, "get list segment_nb::%d",segment_nb);
    for (i = 0; i < segment_nb; i++) {
      DVR_RecordSegmentInfo_t seg_info;
      DVR_PlaybackSegmentFlag_t flags;

      error = dvr_segment_get_info(ctx->playback.param_open.location, p_segment_ids[i], &seg_info);
      if (error) {
        error = DVR_FAILURE;
        DVR_WRAPPER_DEBUG(1, "fail to get seg info (location:%s, seg:%llu), (error:%d)\n",
          ctx->playback.param_open.location, p_segment_ids[i], error);
        break;
      }
      //add check if has audio  or video pid. if not exist. not add segment to playback
      int ii = 0;
      int has_av = 0;
      for (ii = 0; ii < seg_info.nb_pids; ii++) {
        int type = (seg_info.pids[ii].type >> 24) & 0x0f;
        if (type == DVR_STREAM_TYPE_VIDEO ||
          type == DVR_STREAM_TYPE_AUDIO ||
          type == DVR_STREAM_TYPE_AD) {
         DVR_WRAPPER_DEBUG(1, "success to get seg av info \n");
          DVR_WRAPPER_DEBUG(1, "success to get seg av info type[0x%x][%d] [%d][%d][%d]\n",(seg_info.pids[ii].type >> 24)&0x0f,seg_info.pids[ii].pid,
          DVR_STREAM_TYPE_VIDEO,
          DVR_STREAM_TYPE_AUDIO,
          DVR_STREAM_TYPE_AD);
          has_av = 1;
          //break;
        } else {
          DVR_WRAPPER_DEBUG(1, "error to get seg av info type[0x%x][%d] [%d][%d][%d]\n",(seg_info.pids[ii].type >> 24)&0x0f,seg_info.pids[ii].pid,
          DVR_STREAM_TYPE_VIDEO,
          DVR_STREAM_TYPE_AUDIO,
          DVR_STREAM_TYPE_AD);
        }
      }
      if (has_av == 0) {
        DVR_WRAPPER_DEBUG(1, "fail to get seg av info \n");
        continue;
      } else {
        DVR_WRAPPER_DEBUG(1, "success to get seg av info \n");
      }
      flags = DVR_PLAYBACK_SEGMENT_DISPLAYABLE | DVR_PLAYBACK_SEGMENT_CONTINUOUS;
      error = wrapper_addPlaybackSegment(ctx, &seg_info, p_pids, flags);
      if (error)
        break;

      /*copy the 1st segment*/
      if (got_1st_seg == 0) {
          seg_info_1st = seg_info;
          got_1st_seg = 1;
      }
    }
    free(p_segment_ids);

    /* return if no segment or fail to add */
    if (!error && got_1st_seg) {

      /*copy the obsolete infomation, must for timeshifting*/
      if (ctx->playback.param_open.is_timeshift && ctx_record) {
        ctx->playback.obsolete = ctx_record->record.obsolete;
      }

      DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) (%d) segments added\n", ctx->sn, i);

      ctx->playback.reach_end = DVR_FALSE;
      if ((flags&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE)
        ctx->playback.speed = 0.0f;
      else
        ctx->playback.speed = 100.0f;

      ctx->playback.pids_req = *p_pids;
      //calualte segment id and pos
      if (dvr_playback_check_limit(ctx->playback.player)) {
        pthread_mutex_unlock(&ctx->lock);
        dvr_wrapper_seek_playback(playback, 0);
        pthread_mutex_lock(&ctx->lock);
        error = dvr_playback_start(ctx->playback.player, flags);
      } else {
        error = dvr_playback_seek(ctx->playback.player, seg_info_1st.id, 0);
        error = dvr_playback_start(ctx->playback.player, flags);
        DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) seek(seg:%llu 0) for start (%d)\n",
          ctx->sn, seg_info_1st.id, error);
      }
      DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) started (%d)\n", ctx->sn, error);
    }
  }

  if (ctx->playback.param_open.is_timeshift) {
    /*unlock the recorder locked above*/
    if (ctx_record && ctx_valid(ctx_record)) {
        pthread_mutex_unlock(&ctx_record->lock);
        DVR_WRAPPER_DEBUG(1, "playback(sn:%ld), record(sn:%ld) unlocked ok due to timeshift\n",
          ctx->sn, ctx_record->sn);
    }
  }
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_stop_playback (DVR_WrapperPlayback_t playback)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(playback);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "stop playback(sn:%ld) ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = dvr_playback_stop(ctx->playback.player, DVR_TRUE);

  {
    /*remove all segments*/
    DVR_WrapperPlaybackSegmentInfo_t *pseg;

    list_for_each_entry(pseg, &ctx->segments, head) {
      error = dvr_playback_remove_segment(ctx->playback.player, pseg->playback_info.segment_id);
      DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) remove seg(%lld) (%d)\n",
        ctx->sn, pseg->playback_info.segment_id, error);
    }
    ctx_freeSegments(ctx);
  }

  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) stopped (%d)\n", ctx->sn, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_pause_playback (DVR_WrapperPlayback_t playback)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(playback);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "pause playback(sn:%ld) ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);
  //clear end event
  if (ctx->playback.last_event == DVR_PLAYBACK_EVENT_REACHED_END)
      ctx->playback.last_event = DVR_PLAYBACK_EVENT_TRANSITION_OK;

  error = dvr_playback_pause(ctx->playback.player, DVR_FALSE);

  ctx->playback.speed = 0.0f;

  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) paused (%d)\n", ctx->sn, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_resume_playback (DVR_WrapperPlayback_t playback)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(playback);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);
  //if set limit.we need check if seek to valid data when resume
  uint32_t time_offset = ctx->playback.status.info_cur.time + ctx->playback.status.info_obsolete.time;
  if (dvr_playback_check_limit(ctx->playback.player)) {
    int expired = dvr_playback_calculate_expiredlen(ctx->playback.player);
    if (expired > time_offset) {
      DVR_WRAPPER_DEBUG(1, "seek before resume reset offset playback(sn:%ld) (off:%d expired:%d)\n",
        ctx->sn, time_offset, expired);
      time_offset = expired;
      dvr_wrapper_seek_playback(playback, time_offset);
    }
  }
  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "resume playback(sn:%ld) ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = dvr_playback_resume(ctx->playback.player);
  ctx->playback.speed = 100.0f;

  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) resumed (%d)\n", ctx->sn, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_set_playback_speed (DVR_WrapperPlayback_t playback, float speed)
{
  DVR_WrapperCtx_t *ctx;
  int error;
  DVR_PlaybackSpeed_t dvr_speed = {
     .speed = { speed },
     .mode = (speed > 0) ? DVR_PLAYBACK_FAST_FORWARD : DVR_PLAYBACK_FAST_BACKWARD
  };

  DVR_RETURN_IF_FALSE(playback);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  DVR_WRAPPER_DEBUG(1, "speed playback(sn:%ld) (x%f) .(x%f)..\n", ctx->sn, speed, ctx->playback.speed);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = dvr_playback_set_speed(ctx->playback.player, dvr_speed);

  if (ctx->playback.speed != 0.0f && ctx->playback.speed != 100.0f
      && ctx->playback.last_event == DVR_PLAYBACK_EVENT_REACHED_BEGIN
      && ctx->playback.seg_status.state == DVR_PLAYBACK_STATE_PAUSE) {
    DVR_WRAPPER_DEBUG(1, "x%f -> x%f, paused, do resume first\n", ctx->playback.speed, speed);
    error = dvr_playback_resume(ctx->playback.player);
  } else if (ctx->playback.speed == 0.0f
      && speed != 0.0f
      && speed != 100.0f) {
    /*libdvr do not support pause with speed=0, will not be here*/
    DVR_WRAPPER_DEBUG(1, "x%f -> x%f, do resume first\n", ctx->playback.speed, speed);
    error = dvr_playback_resume(ctx->playback.player);
  }

  ctx->playback.speed = speed;

  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) speeded(x%f) (%d)\n",
    ctx->sn, speed, error);
  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_setlimit_playback (DVR_WrapperPlayback_t playback, uint64_t time, int32_t limit)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(playback);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "setlimit playback(sn:%ld) (time:%lld limit:%d) ...\n", ctx->sn, time, limit);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = dvr_playback_setlimit(ctx->playback.player, time, limit);
  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) setlimit(time:%lld limit:%d) ...\n", ctx->sn, time, limit);

  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_seek_playback (DVR_WrapperPlayback_t playback, uint32_t time_offset)
{
  DVR_WrapperCtx_t *ctx;
  int error;
  DVR_WrapperPlaybackSegmentInfo_t *pseg;
  uint64_t segment_id;
  uint32_t off;
  uint64_t last_segment_id;
  uint32_t pre_off;

  DVR_RETURN_IF_FALSE(playback);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "seek playback(sn:%ld) (off:%d) ...\n", ctx->sn, time_offset);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  off = 0;
  segment_id = 0;
  pre_off = 0;
  last_segment_id = 0;

  //if set limit info we need check ts data is
  //expired when seek
  if (dvr_playback_check_limit(ctx->playback.player)) {
    int expired = dvr_playback_calculate_expiredlen(ctx->playback.player);
    if (expired > time_offset) {
      DVR_WRAPPER_DEBUG(1, "seek reset offset playback(sn:%ld) (off:%d expired:%d)\n",
        ctx->sn, time_offset, expired);
      time_offset = expired;
    }
  }

  list_for_each_entry_reverse(pseg, &ctx->segments, head) {
    segment_id = pseg->seg_info.id;

    if ((ctx->playback.obsolete.time + pre_off + pseg->seg_info.duration) > time_offset)
        break;

    last_segment_id = pseg->seg_info.id;
    pre_off += pseg->seg_info.duration;
  }

  if (last_segment_id == segment_id) {
    /*1.only one seg with id:0, 2.offset exceeds the total duration*/
    off = time_offset;
  } else if (ctx->playback.obsolete.time >= time_offset) {
    off = 0;
  } else {
    off = time_offset - pre_off - ctx->playback.obsolete.time;
  }

  DVR_WRAPPER_DEBUG(1, "seek playback(sn:%ld) (seg:%lld, off:%d)\n",
    ctx->sn, segment_id, off);
  error = dvr_playback_seek(ctx->playback.player, segment_id, off);
  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) seeked(off:%d) (%d)\n", ctx->sn, time_offset, error);

  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_update_playback (DVR_WrapperPlayback_t playback, DVR_PlaybackPids_t *p_pids)
{
  DVR_WrapperCtx_t *ctx;
  int error;
  DVR_WrapperPlaybackSegmentInfo_t *pseg;

  DVR_RETURN_IF_FALSE(playback);
  DVR_RETURN_IF_FALSE(p_pids);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "update playback(sn:%ld) v/a(%d:%d/%d:%d) ...\n",
    ctx->sn,
    p_pids->video.pid, p_pids->video.format,
    p_pids->audio.pid, p_pids->audio.format);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  ctx->playback.pids_req = *p_pids;

  error = 0;
  list_for_each_entry_reverse(pseg, &ctx->segments, head) {
    /*should update the whole list of segments*/
    /*if (pseg->seg_info.id == ctx->current_segment_id)*/ {
      /*list_for_each_entry_from(pseg, &ctx->segments, head)*/ {
        /*check udpate for pids*/
        if (memcmp(&pseg->playback_info.pids, p_pids, sizeof(*p_pids)) != 0) {
          pseg->playback_info.pids = *p_pids;
          error = dvr_playback_update_segment_pids(ctx->playback.player, pseg->seg_info.id, p_pids);
          if (error) {
            DVR_WRAPPER_DEBUG(1, "failed to playback(sn:%ld) update segment(id:%lld) pids (%d)\n",
              ctx->sn, pseg->seg_info.id, error);
            /*do not break, let list updated*/
          }
        }
      }
      /*break;*/
    }
  }

  DVR_WRAPPER_DEBUG(1, "update playback(sn:%ld) v/a(%d:%d/%d:%d) (%d)\n",
    ctx->sn,
    p_pids->video.pid, p_pids->video.format,
    p_pids->audio.pid, p_pids->audio.format,
    error);

  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_get_playback_status(DVR_WrapperPlayback_t playback, DVR_WrapperPlaybackStatus_t *status)
{
  DVR_WrapperCtx_t *ctx;
  DVR_WrapperPlaybackStatus_t s;
  DVR_PlaybackStatus_t play_status;
  int error;

  DVR_RETURN_IF_FALSE(playback);
  DVR_RETURN_IF_FALSE(status);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);

  DVR_WRAPPER_DEBUG(1, "get playback(sn:%ld) status ...\n", ctx->sn);
  DVR_RETURN_IF_FALSE_WITH_UNLOCK(ctx_valid(ctx), &ctx->lock);

  error = dvr_playback_get_status(ctx->playback.player, &play_status);
  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) get status (%d)\n", ctx->sn, error);

  ctx->playback.seg_status = play_status;
  error = process_generatePlaybackStatus(ctx, &s);

  if (ctx->playback.reach_end == DVR_TRUE && ctx->playback.param_open.is_timeshift == DVR_FALSE) {
    //reach end need set full time to cur.so app can exist playback.
    DVR_WRAPPER_DEBUG(1, "set cur time to full time, reach end occur");
    s.info_cur.time = s.info_full.time;
  }
  DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) state/cur/full/obsl(%d/%ld/%ld/%ld) (%d)\n",
    ctx->sn,
    s.state,
    s.info_cur.time,
    s.info_full.time,
    s.info_obsolete.time,
    error);

  *status = s;

  pthread_mutex_unlock(&ctx->lock);

  return error;
}

int dvr_wrapper_set_playback_secure_buffer (DVR_WrapperPlayback_t playback,  uint8_t *p_secure_buf, uint32_t len)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(playback);
  DVR_RETURN_IF_FALSE(p_secure_buf);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  error = dvr_playback_set_secure_buffer(ctx->playback.player, p_secure_buf, len);
  pthread_mutex_unlock(&ctx->lock);
  return error;
}

int dvr_wrapper_set_playback_decrypt_callback (DVR_WrapperPlayback_t playback,  DVR_CryptoFunction_t func, void *userdata)
{
  DVR_WrapperCtx_t *ctx;
  int error;

  DVR_RETURN_IF_FALSE(playback);
  DVR_RETURN_IF_FALSE(func);

  ctx = ctx_getPlayback((unsigned long)playback);
  DVR_RETURN_IF_FALSE(ctx);

  pthread_mutex_lock(&ctx->lock);
  error = dvr_playback_set_decrypt_callback(ctx->playback.player, func, userdata);
  pthread_mutex_unlock(&ctx->lock);
  return error;
}

static DVR_Result_t wrapper_record_event_handler(DVR_RecordEvent_t event, void *params, void *userdata)
{
  DVR_WrapperEventCtx_t evt;

  DVR_RETURN_IF_FALSE(userdata);

  evt.sn = (unsigned long)userdata;
  evt.type = W_REC;
  evt.record.event = event;
  evt.record.status = *(DVR_RecordStatus_t *)params;
  DVR_WRAPPER_DEBUG(1, "evt[sn:%ld, record, evt:0x%x]\n", evt.sn, evt.record.event);
  return ctx_addRecordEvent(&evt);
}

static DVR_Result_t wrapper_playback_event_handler(DVR_PlaybackEvent_t event, void *params, void *userdata)
{
  DVR_WrapperEventCtx_t evt;

  DVR_RETURN_IF_FALSE(userdata);

  evt.sn = (unsigned long)userdata;
  evt.type = W_PLAYBACK;
  evt.playback.event = event;
  evt.playback.status = *(DVR_Play_Notify_t *)params;
  DVR_WRAPPER_DEBUG(1, "evt[sn:%ld, playbck, evt:0x%x]\n", evt.sn, evt.playback.event);
  return ctx_addPlaybackEvent(&evt);
}

static inline int process_notifyRecord(DVR_WrapperCtx_t *ctx, DVR_RecordEvent_t evt, DVR_WrapperRecordStatus_t *status)
{
  DVR_WRAPPER_DEBUG(1, "notify(sn:%ld) evt(0x%x) statistic:time/size/pkts(%ld/%lld/%u) obsl:(%ld/%llu/%u)\n",
    ctx->sn,
    evt,
    status->info.time,
    status->info.size,
    status->info.pkts,
    status->info_obsolete.time,
    status->info_obsolete.size,
    status->info_obsolete.pkts);

  if (ctx->record.event_fn)
    return ctx->record.event_fn(evt, status, ctx->record.event_userdata);
  return 0;
}

static inline int record_startNextSegment(DVR_WrapperCtx_t *ctx)
{
  DVR_RecordStartParams_t param;
  DVR_RecordSegmentInfo_t seg_info;
  int i;
  int error;

  memcpy(&param, &ctx->record.param_update, sizeof(param));
  memset(&ctx->record.param_update.segment, 0, sizeof(ctx->record.param_update.segment));
  ctx->record.param_update.segment.segment_id = ctx->record.next_segment_id++;
  for (i = 0; i < param.segment.nb_pids; i++) {
    if (param.segment.pid_action[i] != DVR_RECORD_PID_CLOSE) {
      ctx->record.param_update.segment.pids[ctx->record.param_update.segment.nb_pids] = param.segment.pids[i];
      ctx->record.param_update.segment.pid_action[ctx->record.param_update.segment.nb_pids] = DVR_RECORD_PID_KEEP;
      ctx->record.param_update.segment.nb_pids++;
    }
  }
  error = dvr_record_next_segment(ctx->record.recorder, &ctx->record.param_update, &seg_info);
  {
    DVR_RecordSegmentInfo_t new_seg_info =
      { .id = ctx->record.param_update.segment.segment_id, };
    wrapper_updateRecordSegment(ctx, &seg_info, U_ALL);
    wrapper_addRecordSegment(ctx, &new_seg_info);
  }

  DVR_WRAPPER_DEBUG(1, "record next segment(%llu)=(%d)\n", ctx->record.param_update.segment.segment_id, error);
  return error;
}

static inline int record_removeSegment(DVR_WrapperCtx_t *ctx, DVR_WrapperRecordSegmentInfo_t *pseg)
{
  return wrapper_removeRecordSegment(ctx, pseg);
}

/*should run periodically to update the current status*/
static int process_generateRecordStatus(DVR_WrapperCtx_t *ctx, DVR_WrapperRecordStatus_t *status)
{
  /*the current seg is not covered in the statistics*/
  DVR_WrapperRecordSegmentInfo_t *pseg;

  /*re-calculate the all segments*/
  memset(&ctx->record.status, 0, sizeof(ctx->record.status));

  ctx->record.status.state = ctx->record.seg_status.state;
  ctx->record.status.pids.nb_pids = ctx->record.seg_status.info.nb_pids;
  memcpy(ctx->record.status.pids.pids,
    ctx->record.seg_status.info.pids,
    sizeof(ctx->record.status.pids.pids));
  ctx->current_segment_id = ctx->record.seg_status.info.id;

  list_for_each_entry_reverse(pseg, &ctx->segments, head) {
    if (pseg->info.id != ctx->record.seg_status.info.id) {
      ctx->record.status.info.time += pseg->info.duration;
      ctx->record.status.info.size += pseg->info.size;
      ctx->record.status.info.pkts += pseg->info.nb_packets;
    }
  }

  ctx->record.status.info_obsolete = ctx->record.obsolete;

  wrapper_updateRecordSegment(ctx, &ctx->record.seg_status.info, U_ALL);

  if (status) {
    *status = ctx->record.status;
    status->info.time += ctx->record.seg_status.info.duration;
    status->info.size += ctx->record.seg_status.info.size;
    status->info.pkts += ctx->record.seg_status.info.nb_packets;
  }

  return DVR_SUCCESS;
}


static int process_handleRecordEvent(DVR_WrapperEventCtx_t *evt, DVR_WrapperCtx_t *ctx)
{
  DVR_WrapperRecordStatus_t status;

  memset(&status, 0, sizeof(status));

  DVR_WRAPPER_DEBUG(1, "evt (sn:%ld) 0x%x (state:%d)\n",
    evt->sn, evt->record.event, evt->record.status.state);
  if (ctx->record.param_update.segment.segment_id != evt->record.status.info.id) {
    DVR_WRAPPER_DEBUG(1, "evt (sn:%ld) cur id:0x%x (event id:%d)\n",
    evt->sn, (int)ctx->record.param_update.segment.segment_id, (int)evt->record.status.info.id);
    return 0;
  }
  switch (evt->record.event)
  {
    case DVR_RECORD_EVENT_STATUS:
    {
      switch (evt->record.status.state)
      {
        case DVR_RECORD_STATE_OPENED:
        case DVR_RECORD_STATE_CLOSED:
        {
          ctx->record.seg_status = evt->record.status;

          status.state = evt->record.status.state;
          process_notifyRecord(ctx, evt->record.event, &status);
        } break;
        case DVR_RECORD_STATE_STARTED:
        {
          ctx->record.seg_status = evt->record.status;

          process_generateRecordStatus(ctx, &status);
          process_notifyRecord(ctx, evt->record.event, &status);

          /*restart to next segment*/
          if (ctx->record.param_open.segment_size
              && evt->record.status.info.size >= ctx->record.param_open.segment_size) {
            DVR_WRAPPER_DEBUG(1, "start new segment for record(%lu), reaches segment size limit, cur(%zu) max(%lld)\n",
              ctx->sn,
              evt->record.status.info.size,
              ctx->record.param_open.segment_size);
            if (record_startNextSegment(ctx) != DVR_SUCCESS) {
              /*should notify the recording's stop*/
              int error = dvr_record_close(ctx->record.recorder);
              DVR_WRAPPER_DEBUG(1, "stop record(%lu)=%d, failed to start new segment for recording.",
                ctx->sn, error);
              status.state = DVR_RECORD_STATE_CLOSED;
              process_notifyRecord(ctx, DVR_RECORD_EVENT_WRITE_ERROR, &status);
            }
          }

          if (ctx->record.param_open.is_timeshift
              && ctx->record.param_open.max_time
              && status.info.time >= ctx->record.param_open.max_time) {
            DVR_WrapperRecordSegmentInfo_t *pseg;

            /*as the player do not support null playlist,
              there must be one segment existed at any time,
              we have to keep two segments before remove one*/
            pseg = list_last_entry(&ctx->segments, DVR_WrapperRecordSegmentInfo_t, head);
            if (pseg == list_first_entry(&ctx->segments, DVR_WrapperRecordSegmentInfo_t, head)) {
              /*only one segment, waiting for more*/
              DVR_WRAPPER_DEBUG(1, "warning: the size(%lld) of max_time(%ld) of record < max size of segment(%lld)\n",
                status.info.size,
                ctx->record.param_open.max_time,
                ctx->record.param_open.segment_size);
            } else {
              /*timeshifting, remove the 1st segment and notify the player*/
              record_removeSegment(ctx, pseg);

              process_generateRecordStatus(ctx, &status);
              process_notifyRecord(ctx, evt->record.event, &status);
            }
          }

          if (ctx->record.param_open.is_timeshift
              && ctx->record.param_open.max_size
              && status.info.size >= ctx->record.param_open.max_size) {
            DVR_WrapperRecordSegmentInfo_t *pseg;

            /*as the player do not support null playlist,
              there must be one segment existed at any time,
              we have to keep two segments before remove one*/
            pseg = list_last_entry(&ctx->segments, DVR_WrapperRecordSegmentInfo_t, head);
            if (pseg == list_first_entry(&ctx->segments, DVR_WrapperRecordSegmentInfo_t, head)) {
              /*only one segment, waiting for more*/
              DVR_WRAPPER_DEBUG(1, "warning: the size(%lld) of record < max size of segment(%lld)\n",
                status.info.size,
                ctx->record.param_open.segment_size);
            } else {
              record_removeSegment(ctx, pseg);

              process_generateRecordStatus(ctx, &status);
              process_notifyRecord(ctx, evt->record.event, &status);
            }
          }
        } break;
        case DVR_RECORD_STATE_STOPPED:
        {
          ctx->record.seg_status = evt->record.status;

          process_generateRecordStatus(ctx, &status);
          process_notifyRecord(ctx, evt->record.event, &status);
        } break;
        default:
        break;
      }
    } break;
    case DVR_RECORD_EVENT_WRITE_ERROR: {
      ctx->record.seg_status = evt->record.status;
      status.state = evt->record.status.state;
      process_notifyRecord(ctx, evt->record.event, &status);
    }break;
    default:
    break;
  }
  return DVR_SUCCESS;
}

static inline int process_notifyPlayback(DVR_WrapperCtx_t *ctx, DVR_PlaybackEvent_t evt, DVR_WrapperPlaybackStatus_t *status)
{
  DVR_WRAPPER_DEBUG(1, "notify(sn:%ld) evt(0x%x) statistics:state/cur/full/obsl(%d/%ld/%ld/%ld)\n",
    ctx->sn,
    evt,
    status->state,
    status->info_cur.time,
    status->info_full.time,
    status->info_obsolete.time);

  if (ctx->playback.event_fn)
    return ctx->playback.event_fn(evt, status, ctx->playback.event_userdata);
  return 0;
}

/*should run periodically to update the current status*/
static int process_generatePlaybackStatus(DVR_WrapperCtx_t *ctx, DVR_WrapperPlaybackStatus_t *status)
{
  /*the current seg is not covered in the statistics*/
  DVR_WrapperPlaybackSegmentInfo_t *pseg;

  memset(&ctx->playback.status, 0, sizeof(ctx->playback.status));
  ctx->playback.status.pids = ctx->playback.pids_req;

  ctx->playback.status.state = ctx->playback.seg_status.state;
  ctx->playback.status.speed = ctx->playback.seg_status.speed;
  ctx->playback.status.flags = ctx->playback.seg_status.flags;
  ctx->current_segment_id = ctx->playback.seg_status.segment_id;

  list_for_each_entry_reverse(pseg, &ctx->segments, head) {
    if (pseg->seg_info.id == ctx->playback.seg_status.segment_id)
        break;
    ctx->playback.status.info_cur.time += pseg->seg_info.duration;
    ctx->playback.status.info_cur.size += pseg->seg_info.size;
    ctx->playback.status.info_cur.pkts += pseg->seg_info.nb_packets;
  }
  list_for_each_entry_reverse(pseg, &ctx->segments, head) {
    ctx->playback.status.info_full.time += pseg->seg_info.duration;
    ctx->playback.status.info_full.size += pseg->seg_info.size;
    ctx->playback.status.info_full.pkts += pseg->seg_info.nb_packets;
  }

  if (status) {
    *status = ctx->playback.status;
    /*deal with current, lack size and pkts with the current*/
    status->info_cur.time += ctx->playback.seg_status.time_cur;
    status->info_obsolete.time = ctx->playback.obsolete.time;
  }

  return DVR_SUCCESS;
}

static int process_handlePlaybackEvent(DVR_WrapperEventCtx_t *evt, DVR_WrapperCtx_t *ctx)
{
  DVR_WRAPPER_DEBUG(1, "evt (sn:%ld) 0x%x (state:%d) cur(%lld:%u/%u)\n",
    evt->sn, evt->playback.event,
    evt->playback.status.play_status.state,
    evt->playback.status.play_status.segment_id,
    evt->playback.status.play_status.time_cur,
    evt->playback.status.play_status.time_end);

  /*evt PLAYTIME will break the last logic, do not save*/
  if (evt->playback.event != DVR_PLAYBACK_EVENT_NOTIFY_PLAYTIME
      && evt->playback.event != DVR_PLAYBACK_EVENT_NODATA
      && evt->playback.event != DVR_PLAYBACK_EVENT_DATARESUME
      )
    ctx->playback.last_event = evt->playback.event;

  switch (evt->playback.event)
  {
    case DVR_PLAYBACK_EVENT_FIRST_FRAME:
    case DVR_PLAYBACK_EVENT_REACHED_END:
    case DVR_PLAYBACK_EVENT_TRANSITION_OK:
    case DVR_PLAYBACK_EVENT_NOTIFY_PLAYTIME:
    case DVR_PLAYBACK_EVENT_ERROR:
    case DVR_PLAYBACK_EVENT_REACHED_BEGIN:
    case DVR_PLAYBACK_EVENT_NODATA:
    case DVR_PLAYBACK_EVENT_DATARESUME:
    {
      DVR_WrapperPlaybackStatus_t status;

      /*copy status of segment*/
      ctx->playback.seg_status = evt->playback.status.play_status;

      /*generate status of the whole playback*/
      process_generatePlaybackStatus(ctx, &status);

      if (evt->playback.event == DVR_PLAYBACK_EVENT_REACHED_END) {
        DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) error event:0x%x\n", evt->sn, evt->playback.event);
        if (ctx->playback.param_open.is_timeshift
          || ctx_isPlay_recording(ctx->playback.param_open.location)) {
          /*wait for more data in recording*/
        } else if ((status.info_cur.time + DVR_PLAYBACK_END_GAP) >= ctx->playback.status.info_full.time) {
          process_notifyPlayback(ctx, evt->playback.event, &status);
        } else {
          ctx->playback.reach_end = DVR_TRUE;
        }
      } else if (evt->playback.event != DVR_PLAYBACK_EVENT_REACHED_BEGIN) {
        process_notifyPlayback(ctx, evt->playback.event, &status);
      }
    } break;
    case DVR_PLAYBACK_EVENT_TRANSITION_FAILED:
    case DVR_PLAYBACK_EVENT_KEY_FAILURE:
    case DVR_PLAYBACK_EVENT_NO_KEY:
    {
      DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) error event:0x%x\n", evt->sn, evt->playback.event);
    } break;
    default:
    {
      DVR_WRAPPER_DEBUG(1, "playback(sn:%ld) unknown event:0x%x\n", evt->sn, evt->playback.event);
    } break;
  }
  return 0;
}

static inline int process_handleEvents(DVR_WrapperEventCtx_t *evt, DVR_WrapperCtx_t *ctx)
{
  return (evt->type == W_REC)? process_handleRecordEvent(evt, ctx) : process_handlePlaybackEvent(evt, ctx);
}

