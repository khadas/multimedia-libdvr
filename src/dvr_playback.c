#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include "dvr_playback.h"

#define DVR_PB_DG(_level, _fmt, ...) \
  DVR_DEBUG(_level, "playback %-30.30s:%d " _fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)


#define VALID_PID(_pid_) ((_pid_)>0 && (_pid_)<0x1fff)


#define FF_SPEED (2.0f)
#define FB_SPEED (-1.0f)
#define IS_FFFB(_SPEED_) ((_SPEED_) > FF_SPEED && (_SPEED_) < FB_SPEED)
#define IS_FB(_SPEED_) ((_SPEED_) < FB_SPEED)

#define IS_KERNEL_SPEED(_SPEED_) (((_SPEED_) == PLAYBACK_SPEED_X2) || ((_SPEED_) == PLAYBACK_SPEED_X1) || ((_SPEED_) == PLAYBACK_SPEED_S2) || ((_SPEED_) == PLAYBACK_SPEED_S4) || ((_SPEED_) == PLAYBACK_SPEED_S8))
#define IS_FAST_SPEED(_SPEED_) (((_SPEED_) == PLAYBACK_SPEED_X2) || ((_SPEED_) == PLAYBACK_SPEED_S2) || ((_SPEED_) == PLAYBACK_SPEED_S4) || ((_SPEED_) == PLAYBACK_SPEED_S8))


#define FFFB_SLEEP_TIME    (1000)//500ms
#define FB_DEFAULT_LEFT_TIME    (10000)
//if tsplayer delay time < 200 and no data can read, we will pause
#define MIN_TSPLAYER_DELAY_TIME (200)

#define MAX_CACHE_TIME    (30000)

static int write_success = 0;
//
static int _dvr_playback_fffb(DVR_PlaybackHandle_t handle);
static int _do_check_pid_info(DVR_PlaybackHandle_t handle, DVR_StreamInfo_t  now_pid, DVR_StreamInfo_t set_pid, int type);
static int _dvr_get_cur_time(DVR_PlaybackHandle_t handle);
static int _dvr_get_end_time(DVR_PlaybackHandle_t handle);
static int _dvr_playback_calculate_seekpos(DVR_PlaybackHandle_t handle);
static int _dvr_playback_replay(DVR_PlaybackHandle_t handle, DVR_Bool_t trick) ;


static char* _cmd_toString(int cmd)
{

  char *string[DVR_PLAYBACK_CMD_NONE+1]={
        "start",
        "stop",
        "vstart",
        "astart",
        "vstop",
        "astop",
        "vrestart",
        "arestart",
        "avrestart",
        "vstopastart",
        "astopvstart",
        "vstoparestart",
        "astopvrestart",
        "vstartarestart",
        "astartvrestart",
        "pause",
        "resume",
        "seek",
        "ff",
        "fb",
        "NONE"
  };

  if (cmd > DVR_PLAYBACK_CMD_NONE) {
    return "unkown";
  } else {
    return string[cmd];
  }
}


static char* _dvr_playback_state_toString(int stat)
{
  char *string[DVR_PLAYBACK_STATE_FB+1]={
        "start",
        "stop",
        "pause",
        "ff",
        "fb"
  };

  if (stat > DVR_PLAYBACK_STATE_FB) {
    return "unkown";
  } else {
    return string[stat];
  }
}

static DVR_Bool_t _dvr_support_speed(int speed) {

  DVR_Bool_t ret = DVR_FALSE;

  switch (speed) {
    case PLAYBACK_SPEED_FBX2:
    case PLAYBACK_SPEED_FBX4:
    case PLAYBACK_SPEED_FBX8:
    case PLAYBACK_SPEED_FBX16:
    case PLAYBACK_SPEED_FBX12:
    case PLAYBACK_SPEED_FBX32:
    case PLAYBACK_SPEED_FBX48:
    case PLAYBACK_SPEED_FBX64:
    case PLAYBACK_SPEED_FBX128:
    case PLAYBACK_SPEED_S2:
    case PLAYBACK_SPEED_S4:
    case PLAYBACK_SPEED_S8:
    case PLAYBACK_SPEED_X1:
    case PLAYBACK_SPEED_X2:
    case PLAYBACK_SPEED_X4:
    case PLAYBACK_SPEED_X3:
    case PLAYBACK_SPEED_X5:
    case PLAYBACK_SPEED_X6:
    case PLAYBACK_SPEED_X7:
    case PLAYBACK_SPEED_X8:
    case PLAYBACK_SPEED_X12:
    case PLAYBACK_SPEED_X16:
    case PLAYBACK_SPEED_X32:
    case PLAYBACK_SPEED_X48:
    case PLAYBACK_SPEED_X64:
    case PLAYBACK_SPEED_X128:
      ret = DVR_TRUE;
      break;
    default:
      DVR_PB_DG(1, "not support speed is set [%d]", speed);
      break;
  }
  return ret;
}
void _dvr_tsplayer_callback_test(void *user_data, am_tsplayer_event *event)
{
  DVR_PB_DG(1, "in callback test ");
  DVR_Playback_t *player = NULL;
  if (user_data != NULL) {
    player = (DVR_Playback_t *) user_data;
    DVR_PB_DG(1, "play speed [%f] in callback test ", player->speed);
  }
  switch (event->type) {
      case AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED:
      {
          DVR_PB_DG(1,"[evt] test AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED: %d x %d @%d\n",
              event->event.video_format.frame_width,
              event->event.video_format.frame_height,
              event->event.video_format.frame_rate);
          break;
      }
      case AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME:
      {
          DVR_PB_DG(1, "[evt] test  AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME\n");
          player->first_frame = 1;
          break;
      }
      default:
          break;
    }
}
void _dvr_tsplayer_callback(void *user_data, am_tsplayer_event *event)
{
  DVR_Playback_t *player = NULL;
  if (user_data != NULL) {
    player = (DVR_Playback_t *) user_data;
    DVR_PB_DG(1, "play speed [%f] in-- callback", player->speed);
  }
  switch (event->type) {
    case AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED:
    {
        DVR_PB_DG(1,"[evt] AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED: %d x %d @%d\n",
            event->event.video_format.frame_width,
            event->event.video_format.frame_height,
            event->event.video_format.frame_rate);
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME:
    {
        DVR_PB_DG(1, "[evt] AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME\n");
        if (player != NULL)
          player->first_frame = 1;
        break;
    }
    default:
      DVR_PB_DG(1, "[evt]unkown event [%d]\n", event->type);
      break;
  }
  if (player&&player->player_callback_func) {
    DVR_PB_DG(1, "player is nonull, --call callback\n");
    player->player_callback_func(player->player_callback_userdata, event);
  } else if (player == NULL){
    DVR_PB_DG(1, "player is null, get userdata error\n");
  } else {
    DVR_PB_DG(1, "player callback is null, get callback error\n");
  }
}

//convert video and audio fmt
static int _dvr_convert_stream_fmt(int fmt, DVR_Bool_t is_audio) {
  int format = 0;
  if (is_audio == DVR_FALSE) {
    //for video fmt
    switch (fmt)
    {
        case DVR_VIDEO_FORMAT_MPEG1:
          format = AV_VIDEO_CODEC_MPEG1;
          break;
        case DVR_VIDEO_FORMAT_MPEG2:
          format = AV_VIDEO_CODEC_MPEG2;
          break;
        case DVR_VIDEO_FORMAT_HEVC:
          format = AV_VIDEO_CODEC_H265;
          break;
        case DVR_VIDEO_FORMAT_H264:
          format = AV_VIDEO_CODEC_H264;
          break;
        case DVR_VIDEO_FORMAT_VP9:
          format = AV_VIDEO_CODEC_VP9;
          break;
    }
  } else {
    //for audio fmt
    switch (fmt)
    {
        case DVR_AUDIO_FORMAT_MPEG:
          format = AV_AUDIO_CODEC_MP2;
          break;
        case DVR_AUDIO_FORMAT_AC3:
          format = AV_AUDIO_CODEC_AC3;
          break;
        case DVR_AUDIO_FORMAT_EAC3:
          format = AV_AUDIO_CODEC_EAC3;
          break;
        case DVR_AUDIO_FORMAT_DTS:
          format = AV_AUDIO_CODEC_DTS;
          break;
        case DVR_AUDIO_FORMAT_AAC:
          format = AV_AUDIO_CODEC_AAC;
          break;
        case DVR_AUDIO_FORMAT_LATM:
          format = AV_AUDIO_CODEC_LATM;
          break;
        case DVR_AUDIO_FORMAT_PCM:
          format = AV_AUDIO_CODEC_PCM;
          break;
    }
  }
  return format;
}
static int _dvr_playback_get_trick_stat(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL || player->handle == NULL)
    return -1;

  return player->first_frame;
}

//get sys time ms
static int _dvr_time_getClock(void)
{
  struct timespec ts;
  int ms;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  ms = ts.tv_sec*1000+ts.tv_nsec/1000000;

  return ms;
}


//timeout wait sibnal
static int _dvr_playback_timeoutwait(DVR_PlaybackHandle_t handle , int ms)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;


  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  //ms为毫秒，换算成秒
  ts.tv_sec += ms/1000;
  //在outtime的基础上，增加ms毫秒
  //outtime.tv_nsec为纳秒，1微秒=1000纳秒
  //tv_nsec此值再加上剩余的毫秒数 ms%1000，有可能超过1秒。需要特殊处理
  uint64_t  us = ts.tv_nsec/1000 + 1000 * (ms % 1000); //微秒
  //us的值有可能超过1秒，
  ts.tv_sec += us / 1000000;
  us = us % 1000000;
  ts.tv_nsec = us * 1000;//换算成纳秒
  pthread_cond_timedwait(&player->cond, &player->lock, &ts);
  return 0;
}
//get tsplay delay time ms
static int _dvr_playback_get_delaytime(DVR_PlaybackHandle_t handle ) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  int64_t cache = 0;
  if (player == NULL || player->handle == NULL) {
    DVR_PB_DG(1, "tsplayer delay time error, handle is NULL");
    return 0;
  }
  AmTsPlayer_getDelayTime(player->handle, &cache);
  DVR_PB_DG(1, "tsplayer cache time [%lld]ms", cache);
  return cache;
}
//send signal
static int _dvr_playback_sendSignal(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;\

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  pthread_mutex_lock(&player->lock);
  pthread_cond_signal(&player->cond);
  pthread_mutex_unlock(&player->lock);
  return 0;
}

//send playback event
static int _dvr_playback_sent_event(DVR_PlaybackHandle_t handle, DVR_PlaybackEvent_t evt, DVR_Play_Notify_t *notify) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  switch (evt) {
    case DVR_PLAYBACK_EVENT_ERROR:
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    case DVR_PLAYBACK_EVENT_TRANSITION_OK:
      //GET STATE
      DVR_PB_DG(1, "trans ok EVENT");
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    case DVR_PLAYBACK_EVENT_TRANSITION_FAILED:
      break;
    case DVR_PLAYBACK_EVENT_KEY_FAILURE:
      break;
    case DVR_PLAYBACK_EVENT_NO_KEY:
      break;
    case DVR_PLAYBACK_EVENT_REACHED_BEGIN:
      //GET STATE
      DVR_PB_DG(1, "reached begin EVENT");
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    case DVR_PLAYBACK_EVENT_REACHED_END:
      //GET STATE
      DVR_PB_DG(1, "reached end EVENT");
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    case DVR_PLAYBACK_EVENT_NOTIFY_PLAYTIME:
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    default:
      break;
  }
  if (player->openParams.event_fn != NULL)
      player->openParams.event_fn(evt, (void*)notify, player->openParams.event_userdata);
  return DVR_SUCCESS;
}
static int _dvr_playback_sent_transition_ok(DVR_PlaybackHandle_t handle)
{
  DVR_Play_Notify_t notify;
  memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
  notify.event = DVR_PLAYBACK_EVENT_TRANSITION_OK;
  //get play statue not here
  _dvr_playback_sent_event(handle, DVR_PLAYBACK_EVENT_TRANSITION_OK, &notify);
  return DVR_SUCCESS;
}

static int _dvr_playback_sent_playtime(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (1) {
    return DVR_SUCCESS;
  }
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  if (player->send_time ==0) {
    player->send_time = _dvr_time_getClock() + 1000;
  } else if (player->send_time > _dvr_time_getClock()) {
    return DVR_SUCCESS;
  }
  player->send_time = _dvr_time_getClock() + 1000;
  DVR_Play_Notify_t notify;
  memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
  notify.event = DVR_PLAYBACK_EVENT_NOTIFY_PLAYTIME;
  //get play statue not here
  _dvr_playback_sent_event(handle, DVR_PLAYBACK_EVENT_NOTIFY_PLAYTIME, &notify);
  return DVR_SUCCESS;
}

//check is ongoing segment
static int _dvr_check_segment_ongoing(DVR_PlaybackHandle_t handle) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  int ret = DVR_FAILURE;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  ret = segment_ongoing(player->r_handle);
  if (ret != DVR_SUCCESS) {
     return DVR_FALSE;
  }
  return DVR_TRUE;
}


static int _dvr_init_fffb_t(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  player->fffb_start = _dvr_time_getClock();
  DVR_PB_DG(1, " player->fffb_start:%d", player->fffb_start);
  player->fffb_current = player->fffb_start;
  //get segment current time pos
  player->fffb_start_pcr = _dvr_get_cur_time(handle);
  //player->fffb_current = -1;
  //player->fffb_start = -1;
  //player->fffb_start_pcr = -1;
  player->next_fffb_time = _dvr_time_getClock();

  return DVR_SUCCESS;
}

static int _dvr_init_fffb_time(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  player->fffb_start = _dvr_time_getClock();
  DVR_PB_DG(1, " player->fffb_start:%d", player->fffb_start);
  player->fffb_current = player->fffb_start;
  //get segment current time pos
  player->fffb_start_pcr = _dvr_get_cur_time(handle);
  //player->fffb_current = -1;
  //player->fffb_start = -1;
  //player->fffb_start_pcr = -1;
  player->next_fffb_time = _dvr_time_getClock();
  player->last_send_time_id = UINT64_MAX;
  return DVR_SUCCESS;
}
//get next segment id
static int _dvr_has_next_segmentId(DVR_PlaybackHandle_t handle, int segmentid) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;
  DVR_PlaybackSegmentInfo_t *pre_segment = NULL;

  if (player == NULL) {
    DVR_PB_DG(1, " player is NULL");
    return DVR_FAILURE;
  }

  int found = 0;
  int found_eq_id = 0;
  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (player->segment_is_open == DVR_FALSE) {
      //get first segment from list, case segment is not open
      if (!IS_FB(player->speed))
        found = 1;
    } else if (segment->segment_id == segmentid) {
      //find cur segment, we need get next one
      found_eq_id = 1;
      if (!IS_FB(player->speed)) {
        found = 1;
        continue;
      } else {
        //if is fb mode.we need used pre segment
         if (pre_segment != NULL) {
           found = 1;
         } else {
          //not find next id.
          DVR_PB_DG(1, "not has find next segment on fb mode");
          return DVR_FAILURE;
         }
      }
    }
    if (found == 1) {
      found = 2;
      break;
    }
  }
  if (found != 2) {
    //list is null or reache list  end
    DVR_PB_DG(1, "not found next segment return failure");
    return DVR_FAILURE;
  }
  DVR_PB_DG(1, "found next segment return success");
  return DVR_SUCCESS;
}

//get next segment id
static int _dvr_get_next_segmentId(DVR_PlaybackHandle_t handle) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;
  DVR_PlaybackSegmentInfo_t *pre_segment = NULL;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  int found = 0;
  int found_eq_id = 0;

  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (player->segment_is_open == DVR_FALSE) {
      //get first segment from list, case segment is not open
      if (!IS_FB(player->speed))
        found = 1;
    } else if (segment->segment_id == player->cur_segment_id) {
      //find cur segment, we need get next one
      found_eq_id = 1;
      if (!IS_FB(player->speed)) {
        found = 1;
        continue;
      } else {
        //if is fb mode.we need used pre segment
         if (pre_segment != NULL) {
           found = 1;
         } else {
          //not find next id.
          DVR_PB_DG(1, "not find next segment on fb mode");
          return DVR_FAILURE;
         }
      }
    }
    if (found == 1) {
      if (IS_FB(player->speed)) {
        //used pre segment
        segment = pre_segment;
      }
      //save segment info
      player->last_segment_id = player->cur_segment_id;
      player->last_segment.segment_id = player->cur_segment.segment_id;
      player->last_segment.flags = player->cur_segment.flags;
      memcpy(player->last_segment.location, player->cur_segment.location, DVR_MAX_LOCATION_SIZE);
      //pids
      memcpy(&player->last_segment.pids, &player->cur_segment.pids, sizeof(DVR_PlaybackPids_t));

      //get segment info
      player->segment_is_open = DVR_TRUE;
      player->cur_segment_id = segment->segment_id;
      player->cur_segment.segment_id = segment->segment_id;
      player->cur_segment.flags = segment->flags;
      DVR_PB_DG(1, "set cur id cur flag[0x%x]segment->flags flag[0x%x] id [%lld]", player->cur_segment.flags, segment->flags, segment->segment_id);
      memcpy(player->cur_segment.location, segment->location, DVR_MAX_LOCATION_SIZE);
      //pids
      memcpy(&player->cur_segment.pids, &segment->pids, sizeof(DVR_PlaybackPids_t));
      found = 2;
      break;
    }
    pre_segment = segment;
  }
  if (player->segment_is_open == DVR_FALSE && IS_FB(player->speed)) {
    //used the last one segment to open
    //get segment info
    player->segment_is_open = DVR_TRUE;
    player->cur_segment_id = pre_segment->segment_id;
    player->cur_segment.segment_id = pre_segment->segment_id;
    player->cur_segment.flags = pre_segment->flags;
    DVR_PB_DG(1, "set cur id fb last one cur flag[0x%x]segment->flags flag[0x%x] id [%lld]", player->cur_segment.flags, pre_segment->flags, pre_segment->segment_id);
    memcpy(player->cur_segment.location, pre_segment->location, DVR_MAX_LOCATION_SIZE);
    //pids
    memcpy(&player->cur_segment.pids, &pre_segment->pids, sizeof(DVR_PlaybackPids_t));
    return DVR_SUCCESS;
  }
  if (found != 2) {
    //list is null or reache list  end
    return DVR_FAILURE;
  }
  return DVR_SUCCESS;
}
//open next segment to play,if reach list end return errro.
static int _change_to_next_segment(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  Segment_OpenParams_t  params;
  int ret = DVR_SUCCESS;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  pthread_mutex_lock(&player->segment_lock);

  ret = _dvr_get_next_segmentId(handle);
  if (ret == DVR_FAILURE) {
    DVR_PB_DG(1, "not found segment info");
    pthread_mutex_unlock(&player->segment_lock);
    return DVR_FAILURE;
  }

  if (player->r_handle != NULL) {
    DVR_PB_DG(1, "close segment");
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }

  memset(params.location, 0, DVR_MAX_LOCATION_SIZE);
  //cp chur segment path to location
  memcpy(params.location, player->cur_segment.location, DVR_MAX_LOCATION_SIZE);
  params.segment_id = (uint64_t)player->cur_segment.segment_id;
  params.mode = SEGMENT_MODE_READ;
  DVR_PB_DG(1, "open segment location[%s]id[%lld]flag[0x%x]", params.location, params.segment_id, player->cur_segment.flags);

  ret = segment_open(&params, &(player->r_handle));
  if (ret == DVR_FAILURE) {
    DVR_PB_DG(1, "open segment error");
  }
  pthread_mutex_unlock(&player->segment_lock);
  int total = _dvr_get_end_time( handle);
  pthread_mutex_lock(&player->segment_lock);
  if (IS_FB(player->speed)) {
      //seek end pos -FB_DEFAULT_LEFT_TIME
      segment_seek(player->r_handle, total - FB_DEFAULT_LEFT_TIME, player->openParams.block_size);
      DVR_PB_DG(1, "seek pos [%d]", total - FB_DEFAULT_LEFT_TIME);
  }
  player->dur = total;
  pthread_mutex_unlock(&player->segment_lock);
  DVR_PB_DG(1, "next segment dur [%d] flag [0x%x]", player->dur, player->cur_segment.flags);
  return ret;
}

//open next segment to play,if reach list end return errro.
static int _dvr_open_segment(DVR_PlaybackHandle_t handle, uint64_t segment_id)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  Segment_OpenParams_t  params;
  int ret = DVR_SUCCESS;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  if (segment_id == player->cur_segment_id && player->segment_is_open == DVR_TRUE) {
    return DVR_SUCCESS;
  }
  uint64_t id = segment_id;
  if (id < 0) {
    DVR_PB_DG(1, "not found segment info");
    return DVR_FAILURE;
  }
  DVR_PB_DG(1, "start found segment[%lld]info", id);
  pthread_mutex_lock(&player->segment_lock);

  DVR_PlaybackSegmentInfo_t *segment;

  int found = 0;

  list_for_each_entry(segment, &player->segment_list, head)
  {
    DVR_PB_DG(1, "see 1 location [%s]id[%lld]flag[%x]segment_id[%lld]", segment->location, segment->segment_id, segment->flags, segment_id);
    if (segment->segment_id == segment_id) {
      found = 1;
    }
    if (found == 1) {
      DVR_PB_DG(1, "found  [%s]id[%lld]flag[%x]segment_id[%lld]", segment->location, segment->segment_id, segment->flags, segment_id);
      //get segment info
      player->segment_is_open = DVR_TRUE;
      player->cur_segment_id = segment->segment_id;
      player->cur_segment.segment_id = segment->segment_id;
      player->cur_segment.flags = segment->flags;
      strncpy(player->cur_segment.location, segment->location, sizeof(segment->location));//DVR_MAX_LOCATION_SIZE
      //pids
      memcpy(&player->cur_segment.pids, &segment->pids, sizeof(DVR_PlaybackPids_t));
      DVR_PB_DG(1, "cur found location [%s]id[%lld]flag[%x]", player->cur_segment.location, player->cur_segment.segment_id,player->cur_segment.flags);
      break;
    }
  }
  if (found == 0) {
    DVR_PB_DG(1, "not found segment info.error..");
    pthread_mutex_unlock(&player->segment_lock);
    return DVR_FAILURE;
  }
  memset(params.location, 0, DVR_MAX_LOCATION_SIZE);
  //cp cur segment path to location
  strncpy(params.location, player->cur_segment.location, sizeof(player->cur_segment.location));
  params.segment_id = (uint64_t)player->cur_segment.segment_id;
  params.mode = SEGMENT_MODE_READ;
  DVR_PB_DG(1, "open segment location[%s][%lld]cur flag[0x%x]", params.location, params.segment_id, player->cur_segment.flags);
  if (player->r_handle != NULL) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }
  ret = segment_open(&params, &(player->r_handle));
  if (ret == DVR_FAILURE) {
    DVR_PB_DG(1, "segment opne error");
  }
  pthread_mutex_unlock(&player->segment_lock);
  player->dur = _dvr_get_end_time(handle);

  DVR_PB_DG(1, "player->dur [%d]cur id [%lld]cur flag [0x%x]\r\n", player->dur,player->cur_segment.segment_id, player->cur_segment.flags);
  return ret;
}


//get play info by segment id
static int _dvr_playback_get_playinfo(DVR_PlaybackHandle_t handle,
  uint64_t segment_id,
  am_tsplayer_video_params *vparam,
  am_tsplayer_audio_params *aparam) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  int found = 0;

  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment_id == UINT64_MAX) {
      //get first segment from list
      found = 1;
    }
    if (segment->segment_id == segment_id) {
      found = 1;
    }
    if (found == 1) {
      //get segment info
      if (player->cur_segment_id != UINT64_MAX)
        player->cur_segment_id = segment->segment_id;
      DVR_PB_DG(1, "get play info id [%lld]", player->cur_segment_id);
      player->cur_segment.segment_id = segment->segment_id;
      player->cur_segment.flags = segment->flags;
      //pids
      player->cur_segment.pids.video.pid = segment->pids.video.pid;
      player->cur_segment.pids.video.format = segment->pids.video.format;
      player->cur_segment.pids.video.type = segment->pids.video.type;
      player->cur_segment.pids.audio.pid = segment->pids.audio.pid;
      player->cur_segment.pids.audio.format = segment->pids.audio.format;
      player->cur_segment.pids.audio.type = segment->pids.audio.type;
      player->cur_segment.pids.ad.pid = segment->pids.ad.pid;
      player->cur_segment.pids.ad.format = segment->pids.ad.format;
      player->cur_segment.pids.ad.type = segment->pids.ad.type;
      player->cur_segment.pids.pcr.pid = segment->pids.pcr.pid;
      //
      vparam->codectype = _dvr_convert_stream_fmt(segment->pids.video.format, DVR_FALSE);
      vparam->pid = segment->pids.video.pid;
      aparam->codectype = _dvr_convert_stream_fmt(segment->pids.audio.format, DVR_TRUE);
      aparam->pid = segment->pids.audio.pid;
      DVR_PB_DG(1, "get play info sucess[0x%x]apid[0x%x]vfmt[%d]afmt[%d]", vparam->pid, aparam->pid, vparam->codectype, aparam->codectype);
      found = 2;
      break;
    }
  }
  if (found != 2) {
    //list is null or reache list  end
    DVR_PB_DG(1, "get play info fail");
    return DVR_FAILURE;
  }

  return DVR_SUCCESS;
}
static int _dvr_replay_changed_pid(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  //compare cur segment
  //if (player->cmd.state == DVR_PLAYBACK_STATE_START)
  {
    //check video pids, stop or restart
    _do_check_pid_info(handle, player->last_segment.pids.video, player->cur_segment.pids.video, 0);
    //check audio pids stop or restart
    DVR_PB_DG(1, ":last apid: %d  set apid: %d", player->last_segment.pids.audio.pid,player->cur_segment.pids.audio.pid);
    _do_check_pid_info(handle, player->last_segment.pids.audio, player->cur_segment.pids.audio, 1);
    //check sub audio pids stop or restart
    _do_check_pid_info(handle, player->last_segment.pids.ad, player->cur_segment.pids.ad, 2);
    //check pcr pids stop or restart
    _do_check_pid_info(handle, player->last_segment.pids.pcr, player->cur_segment.pids.pcr, 3);
  }
  return DVR_SUCCESS;
}

static int _dvr_check_cur_segment_flag(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  DVR_PB_DG(1, "flag[0x%x]id[%lld]last[0x%x][%llu]", player->cur_segment.flags, player->cur_segment.segment_id, player->last_segment.flags, player->last_segment.segment_id);
  if ((player->cur_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE &&
    (player->last_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0) {
    //enable display
    DVR_PB_DG(1, "unmute");
    AmTsPlayer_showVideo(player->handle);
    AmTsPlayer_setAudioMute(player->handle, 0, 0);
  } else if ((player->cur_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0 &&
    (player->last_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE) {
    //disable display
    DVR_PB_DG(1, "mute");
    AmTsPlayer_hideVideo(player->handle);
    AmTsPlayer_setAudioMute(player->handle, 1, 1);
  }
  return DVR_SUCCESS;
}
static DVR_Bool_t _dvr_pauselive_decode_sucess(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_TRUE;
  }
  if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
      if (player->first_frame == 1) {
        return DVR_TRUE;
      } else {
        return DVR_FALSE;
      }
  } else {
    return DVR_TRUE;
  }
}
static void* _dvr_playback_thread(void *arg)
{
  DVR_Playback_t *player = (DVR_Playback_t *) arg;
  //int need_open_segment = 1;
  am_tsplayer_input_buffer     wbufs;
  am_tsplayer_input_buffer     dec_bufs;
  int ret = DVR_SUCCESS;

  int timeout = 300;//ms
  uint64_t write_timeout_ms = 50;
  uint8_t *buf = NULL;
  int buf_len = player->openParams.block_size > 0 ? player->openParams.block_size : (256 * 1024);
  DVR_Bool_t b_writed_whole_block = player->openParams.block_size > 0 ? DVR_TRUE:DVR_FALSE;

  int dec_buf_size = buf_len + 188;
  int real_read = 0;
  DVR_Bool_t goto_rewrite = DVR_FALSE;

  if (player->is_secure_mode) {
    if (dec_buf_size > player->secure_buffer_size) {
      DVR_PB_DG(1, "playback blocksize too large");
      return NULL;
    }
  }
  buf = malloc(buf_len);
  if (!buf) {
    DVR_PB_DG(1, "Malloc buffer failed");
    return NULL;
  }
  wbufs.buf_type = TS_INPUT_BUFFER_TYPE_NORMAL;
  wbufs.buf_size = 0;

  dec_bufs.buf_data = malloc(dec_buf_size);
  if (!dec_bufs.buf_data) {
    DVR_PB_DG(1, "Malloc dec buffer failed");
    return NULL;
  }
  dec_bufs.buf_type = TS_INPUT_BUFFER_TYPE_NORMAL;
  dec_bufs.buf_size = dec_buf_size;

  if (player->segment_is_open == DVR_FALSE) {
    ret = _change_to_next_segment((DVR_PlaybackHandle_t)player);
  }

  if (ret != DVR_SUCCESS) {
    if (buf != NULL) {
      free(buf);
      buf = NULL;
    }
    free(dec_bufs.buf_data);
    DVR_PB_DG(1, "get segment error");
    return NULL;
  }
  //get play statue not here
  _dvr_playback_sent_transition_ok((DVR_PlaybackHandle_t)player);
  _dvr_check_cur_segment_flag((DVR_PlaybackHandle_t)player);
  //set video show
  AmTsPlayer_showVideo(player->handle);

  int trick_stat = 0;
  while (player->is_running/* || player->cmd.last_cmd != player->cmd.cur_cmd*/) {

    //check trick stat
    pthread_mutex_lock(&player->lock);

    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_SEEK ||
      player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
      player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB ||
      player->speed > FF_SPEED ||player->speed <= FB_SPEED ||
      (player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE)
    {
      trick_stat = _dvr_playback_get_trick_stat((DVR_PlaybackHandle_t)player);
      if (trick_stat > 0) {
        DVR_PB_DG(1, "trick stat[%d] is > 0 cur cmd[%d]last cmd[%d]flag[0x%x]", player->cmd.cur_cmd, player->cmd.last_cmd, player->play_flag);
        if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_SEEK || (player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
          //check last cmd
          if (player->cmd.last_cmd == DVR_PLAYBACK_CMD_PAUSE
            || ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE
                && ( player->cmd.cur_cmd == DVR_PLAYBACK_CMD_START
                ||player->cmd.last_cmd == DVR_PLAYBACK_CMD_VSTART
                    || player->cmd.last_cmd == DVR_PLAYBACK_CMD_ASTART
                    || player->cmd.last_cmd == DVR_PLAYBACK_CMD_START))) {
            DVR_PB_DG(1, "pause play-------cur cmd[%d]last cmd[%d]flag[0x%x]", player->cmd.cur_cmd, player->cmd.last_cmd, player->play_flag);
            //need change to pause state
            player->cmd.cur_cmd = DVR_PLAYBACK_CMD_PAUSE;
            player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
            player->state = DVR_PLAYBACK_STATE_PAUSE;
            //clear flag
            player->play_flag = player->play_flag & (~DVR_PLAYBACK_STARTED_PAUSEDLIVE);
            player->first_frame = 0;
            AmTsPlayer_pauseVideoDecoding(player->handle);
            AmTsPlayer_pauseAudioDecoding(player->handle);
          } else {
            DVR_PB_DG(1, "clear first frame value-------");
            player->first_frame = 0;
          }
        } else if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF
                || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB
                ||player->speed > FF_SPEED ||player->speed < FB_SPEED) {
            //restart play stream if speed > 2
            if (player->state == DVR_PLAYBACK_STATE_PAUSE) {
              DVR_PB_DG(1, "fffb pause state----speed[%f] fffb cur[%d] cur sys[%d] [%s] [%d]", player->speed, player->fffb_current,_dvr_time_getClock(),_dvr_playback_state_toString(player->state), player->next_fffb_time);
              //used timeout wait need lock first,so we unlock and lock
              //pthread_mutex_unlock(&player->lock);
              //pthread_mutex_lock(&player->lock);
              _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
              pthread_mutex_unlock(&player->lock);
              continue;
            } else if (_dvr_time_getClock() < player->next_fffb_time) {
              DVR_PB_DG(1, "fffb timeout-to pause video---speed[%f] fffb cur[%d] cur sys[%d] [%s] [%d]", player->speed, player->fffb_current,_dvr_time_getClock(),_dvr_playback_state_toString(player->state), player->next_fffb_time);
              //used timeout wait need lock first,so we unlock and lock
              //pthread_mutex_unlock(&player->lock);
              //pthread_mutex_lock(&player->lock);
              AmTsPlayer_pauseVideoDecoding(player->handle);
              _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
              pthread_mutex_unlock(&player->lock);
              continue;

            }
            DVR_PB_DG(1, "fffb play-------speed[%f][%d][%d]", player->speed, goto_rewrite, real_read);
            pthread_mutex_unlock(&player->lock);
            goto_rewrite = DVR_FALSE;
            real_read = 0;
            player->play_flag = player->play_flag & (~DVR_PLAYBACK_STARTED_PAUSEDLIVE);
            player->first_frame = 0;
            _dvr_playback_fffb((DVR_PlaybackHandle_t)player);
            player->fffb_play = DVR_FALSE;
            pthread_mutex_lock(&player->lock);
        }
      }else if (player->fffb_play == DVR_TRUE){
        //for first into fffb when reset speed
        if (player->state == DVR_PLAYBACK_STATE_PAUSE ||
          _dvr_time_getClock() < player->next_fffb_time) {
          DVR_PB_DG(1, "fffb timeout-fffb play---speed[%f] fffb cur[%d] cur sys[%d] [%s] [%d]", player->speed, player->fffb_current,_dvr_time_getClock(),_dvr_playback_state_toString(player->state), player->next_fffb_time);
          //used timeout wait need lock first,so we unlock and lock
          //pthread_mutex_unlock(&player->lock);
          //pthread_mutex_lock(&player->lock);
          _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
          pthread_mutex_unlock(&player->lock);
          continue;
        }
        DVR_PB_DG(1, "fffb replay-------speed[%f][%d][%d][%s][%d]", player->speed, goto_rewrite, real_read, _dvr_playback_state_toString(player->state), player->cmd);
        pthread_mutex_unlock(&player->lock);
        goto_rewrite = DVR_FALSE;
        real_read = 0;
        player->play_flag = player->play_flag & (~DVR_PLAYBACK_STARTED_PAUSEDLIVE);
        player->first_frame = 0;
        _dvr_playback_fffb((DVR_PlaybackHandle_t)player);
        pthread_mutex_lock(&player->lock);
        player->fffb_play = DVR_FALSE;
      }
    }

    if (player->state == DVR_PLAYBACK_STATE_PAUSE) {
      //check is need send time send end
      _dvr_playback_sent_playtime((DVR_PlaybackHandle_t)player);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      continue;
    }
    //when seek action is done. we need drop write timeout data.
    if (player->drop_ts == DVR_TRUE) {
      goto_rewrite = DVR_FALSE;
      real_read = 0;
      player->drop_ts = DVR_FALSE;
    }
    if (goto_rewrite == DVR_TRUE) {
      goto_rewrite = DVR_FALSE;
      pthread_mutex_unlock(&player->lock);
      //DVR_PB_DG(1, "rewrite-player->speed[%f]", player->speed);
      goto rewrite;
    }
    //.check is need send time send end
    _dvr_playback_sent_playtime((DVR_PlaybackHandle_t)player);
    pthread_mutex_lock(&player->segment_lock);
    int read = segment_read(player->r_handle, buf + real_read, buf_len - real_read);
    pthread_mutex_unlock(&player->segment_lock);
    pthread_mutex_unlock(&player->lock);
    if (read < 0 && errno == EIO) {
      //EIO ERROR, EXIT THRAD
      DVR_PB_DG(1, "read error.EIO error, exit thread");
      DVR_Play_Notify_t notify;
      memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
      notify.event = DVR_PLAYBACK_EVENT_ERROR;
      _dvr_playback_sent_event((DVR_PlaybackHandle_t)player,DVR_PLAYBACK_EVENT_ERROR, &notify);
      goto end;
    } else if (read < 0) {
      DVR_PB_DG(1, "read error.:%d EIO:%d", errno, EIO);
    }
    //if on fb mode and read file end , we need calculate pos to retry read.
    if (read == 0 && IS_FB(player->speed) && real_read == 0) {
      DVR_PB_DG(1, "recalculate read [%d] readed [%d]buf_len[%d]speed[%f]id=[%llu]", read,real_read, buf_len, player->speed,player->cur_segment_id);
      _dvr_playback_calculate_seekpos((DVR_PlaybackHandle_t)player);
      pthread_mutex_lock(&player->lock);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      continue;
    }
    //DVR_PB_DG(1, "read ts [%d]buf_len[%d]speed[%f]real_read:%d", read, buf_len, player->speed, real_read);
    if (read == 0) {
      //file end.need to play next segment
      int ret = _change_to_next_segment((DVR_PlaybackHandle_t)player);
      //init fffb time if change segment
       _dvr_init_fffb_time((DVR_PlaybackHandle_t)player);

      int delay = _dvr_playback_get_delaytime((DVR_PlaybackHandle_t)player);
      //del delay time max check.
      if (ret != DVR_SUCCESS &&
        (delay <= MIN_TSPLAYER_DELAY_TIME ||
          /*delay > MAX_CACHE_TIME ||*/
          player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF) &&
        _dvr_pauselive_decode_sucess((DVR_PlaybackHandle_t)player)) {
         //send end event to hal
         DVR_Play_Notify_t notify;
         memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
         notify.event = DVR_PLAYBACK_EVENT_REACHED_END;
         //get play statue not here
         dvr_playback_pause((DVR_PlaybackHandle_t)player, DVR_FALSE);
         _dvr_playback_sent_event((DVR_PlaybackHandle_t)player, DVR_PLAYBACK_EVENT_REACHED_END, &notify);
         //continue,timeshift mode, when read end,need wait cur recording segment
         DVR_PB_DG(1, "playback is  send end delay:[%d]", delay);
         pthread_mutex_lock(&player->lock);
         _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
         pthread_mutex_unlock(&player->lock);
         continue;
       } else if (ret != DVR_SUCCESS) {
         //not send event and pause,sleep and go to next time to recheck
         DVR_PB_DG(1, "delay:%d pauselive:%d", delay, _dvr_pauselive_decode_sucess((DVR_PlaybackHandle_t)player));
         pthread_mutex_lock(&player->lock);
         _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
         pthread_mutex_unlock(&player->lock);
         continue;
       }
       //change next segment success case
      _dvr_playback_sent_transition_ok((DVR_PlaybackHandle_t)player);
      pthread_mutex_lock(&player->lock);
      DVR_PB_DG(1, "_dvr_replay_changed_pid:start");
      _dvr_replay_changed_pid((DVR_PlaybackHandle_t)player);
      _dvr_check_cur_segment_flag((DVR_PlaybackHandle_t)player);
      read = segment_read(player->r_handle, buf + real_read, buf_len - real_read);
      pthread_mutex_unlock(&player->lock);
    }
    real_read = real_read + read;
    wbufs.buf_size = real_read;
    wbufs.buf_data = buf;
    //check read data len,iflen < 0, we need continue
    if (wbufs.buf_size <= 0 || wbufs.buf_data == NULL) {
      DVR_PB_DG(1, "error occur read_read [%d],buf=[%p]",wbufs.buf_size, wbufs.buf_data);
      real_read = 0;
      continue;
    }
    //if need write whole block size, we need check read buf len is eq block size.
    if (b_writed_whole_block == DVR_TRUE) {
      //buf_len is block size value.
      if (real_read < buf_len) {
        //coontinue to read data from file
        DVR_PB_DG(1, "read buf len[%d] is < block size [%d]", real_read, buf_len);
        pthread_mutex_lock(&player->lock);
         _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
         pthread_mutex_unlock(&player->lock);
        continue;
      } else if (real_read > buf_len) {
        DVR_PB_DG(1, "read buf len[%d] is > block size [%d],this error occur", real_read, buf_len);
      }
    }

    if (player->dec_func) {
      DVR_CryptoParams_t crypto_params;

      memset(&crypto_params, 0, sizeof(crypto_params));
      crypto_params.type = DVR_CRYPTO_TYPE_DECRYPT;
      memcpy(crypto_params.location, player->cur_segment.location, strlen(player->cur_segment.location));
      crypto_params.segment_id = player->cur_segment.segment_id;
      crypto_params.offset = segment_tell_position(player->r_handle);

      crypto_params.input_buffer.type = DVR_BUFFER_TYPE_NORMAL;
      crypto_params.input_buffer.addr = (size_t)buf;
      crypto_params.input_buffer.size = real_read;

      if (player->is_secure_mode) {
        crypto_params.output_buffer.type = DVR_BUFFER_TYPE_SECURE;
        crypto_params.output_buffer.addr = (size_t)player->secure_buffer;
        crypto_params.output_buffer.size = dec_buf_size;
        ret = player->dec_func(&crypto_params, player->dec_userdata);
        wbufs.buf_data = player->secure_buffer;
        wbufs.buf_type = TS_INPUT_BUFFER_TYPE_SECURE;
      } else {
        crypto_params.output_buffer.type = DVR_BUFFER_TYPE_NORMAL;
        crypto_params.output_buffer.addr = (size_t)dec_bufs.buf_data;
        crypto_params.output_buffer.size = dec_buf_size;
        ret = player->dec_func(&crypto_params, player->dec_userdata);
        wbufs.buf_data = dec_bufs.buf_data;
        wbufs.buf_type = TS_INPUT_BUFFER_TYPE_NORMAL;
      }
      if (ret != DVR_SUCCESS) {
        DVR_PB_DG(1, "decrypt failed");
      }
      wbufs.buf_size = crypto_params.output_size;
    }
rewrite:
    if (player->drop_ts == DVR_TRUE) {
      //need drop ts data when seek occur.we need read next loop,drop this ts data
      goto_rewrite = DVR_FALSE;
      real_read = 0;
      player->drop_ts = DVR_FALSE;
      continue;
    }
    ret = AmTsPlayer_writeData(player->handle, &wbufs, write_timeout_ms);
    if (ret == AM_TSPLAYER_OK) {
      real_read = 0;
      write_success++;
      continue;
    } else {
      DVR_PB_DG(1, "write time out write_success:%d", write_success);
      write_success = 0;
      pthread_mutex_lock(&player->lock);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      if (!player->is_running) {
        DVR_PB_DG(1, "playback thread exit");
         break;
      }
      goto_rewrite = DVR_TRUE;
      //goto rewrite;
    }
  }
end:
  DVR_PB_DG(1, "playback thread is end");
  free(buf);
  free(dec_bufs.buf_data);
  return NULL;
}


static int _start_playback_thread(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  DVR_PB_DG(1, "start thread is_running:[%d]", player->is_running);
  if (player->is_running == DVR_TRUE) {
    return 0;
  }
  player->is_running = DVR_TRUE;
  int rc = pthread_create(&player->playback_thread, NULL, _dvr_playback_thread, (void*)player);
  if (rc < 0)
    player->is_running = DVR_FALSE;
  return 0;
}


static int _stop_playback_thread(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  DVR_PB_DG(1, "stopthread------[%d]", player->is_running);
  if (player->is_running == DVR_TRUE)
  {
    player->is_running = DVR_FALSE;
    _dvr_playback_sendSignal(handle);
    pthread_join(player->playback_thread, NULL);
  }
  if (player->r_handle) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }
  DVR_PB_DG(1, ":end");
  return 0;
}

/**\brief Open an dvr palyback
 * \param[out] p_handle dvr playback addr
 * \param[in] params dvr playback open parameters
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_open(DVR_PlaybackHandle_t *p_handle, DVR_PlaybackOpenParams_t *params) {

  DVR_Playback_t *player;
  pthread_condattr_t  cattr;

  player = (DVR_Playback_t*)calloc(1, sizeof(DVR_Playback_t));

  pthread_mutex_init(&player->lock, NULL);
  pthread_mutex_init(&player->segment_lock, NULL);
  pthread_condattr_init(&cattr);
  pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
  pthread_cond_init(&player->cond, &cattr);
  pthread_condattr_destroy(&cattr);

  //init segment list head
  INIT_LIST_HEAD(&player->segment_list);
  player->cmd.last_cmd = DVR_PLAYBACK_CMD_STOP;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_STOP;
  player->cmd.speed.speed.speed = PLAYBACK_SPEED_X1;
  player->cmd.state = DVR_PLAYBACK_STATE_STOP;
  player->state = DVR_PLAYBACK_STATE_STOP;
  player->cmd.pos = 0;
  player->speed = 1.0f;

  //store open params
  player->openParams.dmx_dev_id = params->dmx_dev_id;
  player->openParams.block_size = params->block_size;
  player->openParams.is_timeshift = params->is_timeshift;
  player->openParams.event_fn = params->event_fn;
  player->openParams.event_userdata = params->event_userdata;

  player->has_pids = params->has_pids;

  player->handle = params->player_handle ;

  AmTsPlayer_getCb(player->handle, &player->player_callback_func, &player->player_callback_userdata);
  //for test get callback
  if (0 && player->player_callback_func == NULL) {
    AmTsPlayer_registerCb(player->handle, _dvr_tsplayer_callback_test, player);
    AmTsPlayer_getCb(player->handle, &player->player_callback_func, &player->player_callback_userdata);
    DVR_PB_DG(1, "playback open get callback[%p][%p][%p][%p]",player->player_callback_func, player->player_callback_userdata, _dvr_tsplayer_callback_test,player);
  }
  AmTsPlayer_registerCb(player->handle, _dvr_tsplayer_callback, player);

  //init has audio and video
  player->has_video = DVR_FALSE;
  player->has_audio = DVR_FALSE;
  player->cur_segment_id = UINT64_MAX;
  player->last_segment_id = 0LL;
  player->segment_is_open = DVR_FALSE;

  //init ff fb time
  player->fffb_current = -1;
  player->fffb_start =-1;
  player->fffb_start_pcr = -1;
  //seek time
  player->seek_time = 0;
  player->send_time = 0;

  //init secure stuff
  player->dec_func = NULL;
  player->dec_userdata = NULL;
  player->is_secure_mode = 0;
  player->secure_buffer = NULL;
  player->secure_buffer_size = 0;
  player->drop_ts = DVR_FALSE;

  player->fffb_play = DVR_FALSE;

  player->last_send_time_id = UINT64_MAX;
  player->last_cur_time = 0;

  *p_handle = player;
  return DVR_SUCCESS;
}

/**\brief Close an dvr palyback
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_close(DVR_PlaybackHandle_t handle) {

  DVR_ASSERT(handle);
  DVR_PB_DG(1, ":into");
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  if (player->state != DVR_PLAYBACK_STATE_STOP)
  {
    DVR_PB_DG(1, "player->state %s", _dvr_playback_state_toString(player->state));
    dvr_playback_stop(handle, DVR_TRUE);
    DVR_PB_DG(1, "player->state %s", _dvr_playback_state_toString(player->state));
  } else {
    DVR_PB_DG(1, ":is stoped state");
  }
  DVR_PB_DG(1, ":into");
  pthread_mutex_destroy(&player->lock);
  pthread_cond_destroy(&player->cond);

  if (player) {
    free(player);
    player = NULL;
  }
  DVR_PB_DG(1, ":end");
  return DVR_SUCCESS;
}

/**\brief Start play audio and video, used start auido api and start video api
 * \param[in] handle playback handle
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_start(DVR_PlaybackHandle_t handle, DVR_PlaybackFlag_t flag) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  am_tsplayer_video_params    vparams;
  am_tsplayer_audio_params    aparams;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  uint64_t segment_id = player->cur_segment_id;
  DVR_PB_DG(1, "[%p]segment_id:[%lld]", handle, segment_id);

  player->first_frame = 0;
  //can used start api to resume playback
  if (player->cmd.state == DVR_PLAYBACK_STATE_PAUSE) {
    return dvr_playback_resume(handle);
  }
  if (player->cmd.state == DVR_PLAYBACK_STATE_START) {
    DVR_PB_DG(1, "stat is start, not need into start play");
    return DVR_SUCCESS;
  }
  player->play_flag = flag;
  //get segment info and audio video pid fmt ;
  DVR_PB_DG(1, "lock flag:0x%x", flag);
  pthread_mutex_lock(&player->lock);
  _dvr_playback_get_playinfo(handle, segment_id, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio abnd video pis is all invalid, return error.
    DVR_PB_DG(0, "unlock dvr play back start error, not found audio and video info");
    pthread_mutex_unlock(&player->lock);
    DVR_Play_Notify_t notify;
    memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
    notify.event = DVR_PLAYBACK_EVENT_TRANSITION_FAILED;
    notify.info.error_reason = DVR_PLAYBACK_PID_ERROR;
    notify.info.transition_failed_data.segment_id = segment_id;
    //get play statue not here
    _dvr_playback_sent_event(handle, DVR_PLAYBACK_EVENT_TRANSITION_FAILED, &notify);
    return -1;
  }

  {
    if (VALID_PID(vparams.pid)) {
      player->has_video = DVR_TRUE;
      //if set flag is pause live, we need set trick mode
      if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
        DVR_PB_DG(1, "set trick mode -pauselive flag--");
        AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
      } else if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB
        || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF) {
        DVR_PB_DG(1, "set trick mode -fffb--at pause live");
        AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
      } else {
        DVR_PB_DG(1, "set trick mode ---none");
        AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_NONE);
      }
      AmTsPlayer_setVideoParams(player->handle,  &vparams);
      AmTsPlayer_startVideoDecoding(player->handle);
    }

    DVR_PB_DG(1, "player->cmd.cur_cmd:%d vpid[0x%x]apis[0x%x]", player->cmd.cur_cmd, vparams.pid, aparams.pid);
    player->last_send_time_id = UINT64_MAX;
    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB
      || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF) {
      player->cmd.state = DVR_PLAYBACK_STATE_START;
      player->state = DVR_PLAYBACK_STATE_START;
    } else {
      player->cmd.last_cmd = player->cmd.cur_cmd;
      player->cmd.cur_cmd = DVR_PLAYBACK_CMD_START;
      if (IS_FAST_SPEED(player->cmd.speed.speed.speed)) {
        //set fast play
        DVR_PB_DG(1, "start fast");
        AmTsPlayer_startFast(player->handle, (float)player->cmd.speed.speed.speed/100.0f);
      } else {
        if (VALID_PID(aparams.pid)) {
          DVR_PB_DG(1, "start audio");
          player->has_audio = DVR_TRUE;
          AmTsPlayer_setAudioParams(player->handle,  &aparams);
          AmTsPlayer_startAudioDecoding(player->handle);
        }
      }
      player->cmd.state = DVR_PLAYBACK_STATE_START;
      player->state = DVR_PLAYBACK_STATE_START;
    }
  }
  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);
  _start_playback_thread(handle);
  return DVR_SUCCESS;
}
/**\brief dvr play back add segment info to segment list
 * \param[in] handle playback handle
 * \param[in] info added segment info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_add_segment(DVR_PlaybackHandle_t handle, DVR_PlaybackSegmentInfo_t *info) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  DVR_PB_DG(1, "add segment id: %lld %p", info->segment_id, handle);
  DVR_PlaybackSegmentInfo_t *segment;

  segment = malloc(sizeof(DVR_PlaybackSegmentInfo_t));
  memset(segment, 0, sizeof(DVR_PlaybackSegmentInfo_t));

  //not memcpy chun info.
  segment->segment_id = info->segment_id;
  //cp location
  memcpy(segment->location, info->location, DVR_MAX_LOCATION_SIZE);

  DVR_PB_DG(1, "add location [%s]id[%lld]flag[%x]", segment->location, segment->segment_id, info->flags);
  segment->flags = info->flags;

  //pids
  segment->pids.video.pid = info->pids.video.pid;
  segment->pids.video.format = info->pids.video.format;
  segment->pids.video.type = info->pids.video.type;

  segment->pids.audio.pid = info->pids.audio.pid;
  segment->pids.audio.format = info->pids.audio.format;
  segment->pids.audio.type = info->pids.audio.type;

  segment->pids.ad.pid = info->pids.ad.pid;
  segment->pids.ad.format = info->pids.ad.format;
  segment->pids.ad.type = info->pids.ad.type;

  segment->pids.pcr.pid = info->pids.pcr.pid;

  DVR_PB_DG(1, "lock pid [0x%x][0x%x][0x%x][0x%x]", segment->pids.video.pid,segment->pids.audio.pid, info->pids.video.pid,info->pids.audio.pid);
  pthread_mutex_lock(&player->lock);
  list_add_tail(&segment->head, &player->segment_list);
  pthread_mutex_unlock(&player->lock);
  DVR_PB_DG(1, "unlock");

  return DVR_SUCCESS;
}
/**\brief dvr play back remove segment info by segment_id
 * \param[in] handle playback handle
 * \param[in] segment_id need removed segment id
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_remove_segment(DVR_PlaybackHandle_t handle, uint64_t segment_id) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PB_DG(1, "remove segment id: %lld", segment_id);
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  if (segment_id == player->cur_segment_id) {
    DVR_PB_DG(1, "not suport remove curren segment id: %lld", segment_id);
    return DVR_FAILURE;
  }
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  DVR_PlaybackSegmentInfo_t *segment = NULL;
  DVR_PlaybackSegmentInfo_t *segment_tmp = NULL;
  list_for_each_entry_safe(segment, segment_tmp, &player->segment_list, head)
  {
    if (segment->segment_id == segment_id) {
      list_del(&segment->head);
      free(segment);
      break;
    }
  }
  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);

  return DVR_SUCCESS;
}
/**\brief dvr play back add segment info
 * \param[in] handle playback handle
 * \param[in] info added segment info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_update_segment_flags(DVR_PlaybackHandle_t handle,
  uint64_t segment_id, DVR_PlaybackSegmentFlag_t flags) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PB_DG(1, "update segment id: %lld flag:%d", segment_id, flags);
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  DVR_PlaybackSegmentInfo_t *segment;
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment->segment_id != segment_id) {
      continue;
    }
    // if encramble to free， only set flag and return;

    //if displayable to none, we need mute audio and video
    if (segment_id == player->cur_segment_id) {
      if ((segment->flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE
        && (flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0) {
        //disable display, mute
        AmTsPlayer_hideVideo(player->handle);
        AmTsPlayer_setAudioMute(player->handle, 1, 1);
      } else if ((segment->flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0 &&
          (flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE) {
        //enable display, unmute
        AmTsPlayer_showVideo(player->handle);
        AmTsPlayer_setAudioMute(player->handle, 0, 0);
      } else {
        //do nothing
      }
    } else {
      //do nothing
    }
    //continue , only set flag
    segment->flags = flags;
  }
  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}


static int _do_check_pid_info(DVR_PlaybackHandle_t handle, DVR_StreamInfo_t  now_pid, DVR_StreamInfo_t set_pid, int type) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  DVR_PB_DG(1, " do check");
  if (now_pid.pid == set_pid.pid) {
    //do nothing
    return 0;
  } else if (player->cmd.state == DVR_PLAYBACK_STATE_START) {
    if (VALID_PID(now_pid.pid)) {
      //stop now stream
      if (type == 0) {
        //stop vieo
        DVR_PB_DG(1, "stop video");
        AmTsPlayer_stopVideoDecoding(player->handle);
        player->has_video = DVR_FALSE;
      } else if (type == 1) {
        //stop audio
        DVR_PB_DG(1, "stop audio");
        AmTsPlayer_stopAudioDecoding(player->handle);
        player->has_audio = DVR_FALSE;
      } else if (type == 2) {
        //stop sub audio
        DVR_PB_DG(1, "stop ad");
        AmTsPlayer_disableADMix(player->handle);
      } else if (type == 3) {
        //pcr
      }
    }
    if (VALID_PID(set_pid.pid)) {
      //start
      if (type == 0) {
        //start vieo
        am_tsplayer_video_params vparams;
        vparams.pid = set_pid.pid;
        vparams.codectype = _dvr_convert_stream_fmt(set_pid.format, DVR_FALSE);
        player->has_video = DVR_TRUE;
        DVR_PB_DG(1, "start video pid[%d]fmt[%d]",vparams.pid, vparams.codectype);
        AmTsPlayer_setVideoParams(player->handle,  &vparams);
        AmTsPlayer_startVideoDecoding(player->handle);
        //playback_device_video_start(player->handle,&vparams);
      } else if (type == 1) {
        //start audio
        am_tsplayer_audio_params aparams;
        aparams.pid = set_pid.pid;
        aparams.codectype= _dvr_convert_stream_fmt(set_pid.format, DVR_TRUE);
        player->has_audio = DVR_TRUE;
        DVR_PB_DG(1, "start audio pid[%d]fmt[%d]",aparams.pid, aparams.codectype);
        AmTsPlayer_setAudioParams(player->handle,  &aparams);
        AmTsPlayer_startAudioDecoding(player->handle);
        //playback_device_audio_start(player->handle,&aparams);
      } else if (type == 2) {
        am_tsplayer_audio_params aparams;
        aparams.pid = set_pid.pid;
        aparams.codectype= _dvr_convert_stream_fmt(set_pid.format, DVR_TRUE);
        player->has_audio = DVR_TRUE;
        DVR_PB_DG(1, "start ad audio pid[%d]fmt[%d]",aparams.pid, aparams.codectype);
        AmTsPlayer_setADParams(player->handle,  &aparams);
        AmTsPlayer_enableADMix(player->handle);
        //playback_device_audio_start(player->handle,&aparams);
      } else if (type == 3) {
        //pcr
        DVR_PB_DG(1, "start set pcr [%d]", set_pid.pid);
        AmTsPlayer_setPcrPid(player->handle, set_pid.pid);
      }
      //audio and video all close
      if (!player->has_audio && !player->has_video) {
        player->state = DVR_PLAYBACK_STATE_STOP;
      }
    }
  }
  return 0;
}
/**\brief dvr play back update segment pids
 * if updated segment is ongoing segment, we need start new
 * add pid stream and stop remove pid stream.
 * \param[in] handle playback handle
 * \param[in] segment_id need updated pids segment id
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_update_segment_pids(DVR_PlaybackHandle_t handle, uint64_t segment_id, DVR_PlaybackPids_t *p_pids) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  DVR_PlaybackSegmentInfo_t *segment;
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  DVR_PB_DG(1, "get lock update segment id: %lld cur id %lld", segment_id, player->cur_segment_id);

  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment->segment_id == segment_id) {

      if (player->cur_segment_id == segment_id) {
        if (player->cmd.state == DVR_PLAYBACK_STATE_FF
          || player->cmd.state == DVR_PLAYBACK_STATE_FF) {
          //do nothing when ff fb
          DVR_PB_DG(1, "unlock now is ff fb, not to update cur segment info\r\n");
          pthread_mutex_unlock(&player->lock);
          return 0;
        }

        //if segment is on going segment,we need stop start stream
        if (player->cmd.state == DVR_PLAYBACK_STATE_START) {
          pthread_mutex_unlock(&player->lock);
          //check video pids, stop or restart
          _do_check_pid_info((DVR_PlaybackHandle_t)player, segment->pids.video, p_pids->video, 0);
          //check audio pids stop or restart
          _do_check_pid_info((DVR_PlaybackHandle_t)player, segment->pids.audio, p_pids->audio, 1);
          //check sub audio pids stop or restart
          _do_check_pid_info((DVR_PlaybackHandle_t)player, segment->pids.ad, p_pids->ad, 2);
          //check pcr pids stop or restart
          _do_check_pid_info((DVR_PlaybackHandle_t)player, segment->pids.pcr, p_pids->pcr, 3);
          pthread_mutex_lock(&player->lock);
        } else if (player->cmd.state == DVR_PLAYBACK_STATE_PAUSE) {
          //if state is pause, we need process at resume api. we only record change info
            int v_cmd = DVR_PLAYBACK_CMD_NONE;
            int a_cmd = DVR_PLAYBACK_CMD_NONE;
            if (VALID_PID(segment->pids.video.pid)
              && VALID_PID(p_pids->video.pid)
              && segment->pids.video.pid != p_pids->video.pid) {
              //restart video
              v_cmd = DVR_PLAYBACK_CMD_VRESTART;
            }
            if (!VALID_PID(segment->pids.video.pid)
                && VALID_PID(p_pids->video.pid)
                && segment->pids.video.pid != p_pids->video.pid) {
                //start video
                v_cmd = DVR_PLAYBACK_CMD_VSTART;
            }
            if (VALID_PID(segment->pids.video.pid)
                && !VALID_PID(p_pids->video.pid)
                && segment->pids.video.pid != p_pids->video.pid) {
                //stop video
                v_cmd = DVR_PLAYBACK_CMD_VSTOP;
            }
            if (VALID_PID(segment->pids.audio.pid)
              && VALID_PID(p_pids->audio.pid)
              && segment->pids.audio.pid != p_pids->audio.pid) {
              //restart audio
              a_cmd = DVR_PLAYBACK_CMD_ARESTART;
            }
            if (!VALID_PID(segment->pids.audio.pid)
                && VALID_PID(p_pids->audio.pid)
                && segment->pids.audio.pid != p_pids->audio.pid) {
                //start audio
                a_cmd = DVR_PLAYBACK_CMD_ASTART;
            }
            if (VALID_PID(segment->pids.audio.pid)
                && !VALID_PID(p_pids->audio.pid)
                && segment->pids.audio.pid != p_pids->audio.pid) {
                //stop audio
                a_cmd = DVR_PLAYBACK_CMD_ASTOP;
            }
            if (a_cmd == DVR_PLAYBACK_CMD_NONE
              && v_cmd == DVR_PLAYBACK_CMD_NONE) {
              //do nothing
            } else if (a_cmd == DVR_PLAYBACK_CMD_NONE
              || v_cmd == DVR_PLAYBACK_CMD_NONE) {
              player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
              player->cmd.cur_cmd = a_cmd != DVR_PLAYBACK_CMD_NONE ? a_cmd : v_cmd;
            } else if (a_cmd != DVR_PLAYBACK_CMD_NONE
              && v_cmd != DVR_PLAYBACK_CMD_NONE) {
              if (v_cmd == DVR_PLAYBACK_CMD_VRESTART
                && (a_cmd == DVR_PLAYBACK_CMD_ARESTART)) {
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_AVRESTART;
              }else if (v_cmd == DVR_PLAYBACK_CMD_VRESTART
                && a_cmd == DVR_PLAYBACK_CMD_ASTART) {
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTARTVRESTART;
              } else {
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTOPVRESTART;
              }

              if (v_cmd == DVR_PLAYBACK_CMD_VSTART
                && (a_cmd == DVR_PLAYBACK_CMD_ARESTART)) {
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTARTARESTART;
              } else if (v_cmd == DVR_PLAYBACK_CMD_VSTART
                && a_cmd == DVR_PLAYBACK_CMD_ASTART) {
                //not occur this case
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_START;
              } else {
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTOPVSTART;
              }

              if (v_cmd == DVR_PLAYBACK_CMD_VSTOP
                && a_cmd == DVR_PLAYBACK_CMD_ASTART) {
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTOPASTART;
              } else if (v_cmd == DVR_PLAYBACK_CMD_VSTOP
                && a_cmd == DVR_PLAYBACK_CMD_ARESTART) {
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTOPARESTART;
              } else {
                //not occur this case
                player->cmd.last_cmd =DVR_PLAYBACK_CMD_PAUSE;
                player->cmd.cur_cmd = DVR_PLAYBACK_CMD_STOP;
              }
            }
        }
        memcpy(&player->cur_segment.pids, p_pids, sizeof(DVR_PlaybackPids_t));
      }
      //save pids info
      DVR_PB_DG(1, ":apid :%d %d", segment->pids.audio.pid, p_pids->audio.pid);
      memcpy(&segment->pids, p_pids, sizeof(DVR_PlaybackPids_t));
      DVR_PB_DG(1, ":cp apid :%d %d", segment->pids.audio.pid, p_pids->audio.pid);
      break;
    }
  }
  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Stop play, will stop video and audio
 * \param[in] handle playback handle
 * \param[in] clear is clear last frame
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_stop(DVR_PlaybackHandle_t handle, DVR_Bool_t clear) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  if (player->state == DVR_PLAYBACK_STATE_STOP) {
    DVR_PB_DG(1, ":playback is stoped");
    return DVR_SUCCESS;
  }
  if (player->state == DVR_PLAYBACK_STATE_STOP) {
    DVR_PB_DG(1, ":playback is stoped");
    return DVR_SUCCESS;
  }
  DVR_PB_DG(1, ":into");
  _stop_playback_thread(handle);
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  DVR_PB_DG(1, ":get lock into stop fast");
  AmTsPlayer_stopFast(player->handle);
  if (player->has_video) {
    AmTsPlayer_resumeVideoDecoding(player->handle);
  }
  if (player->has_audio) {
    AmTsPlayer_resumeAudioDecoding(player->handle);
  }
  DVR_PB_DG(1, ":into");
  if (player->has_video) {
    player->has_video = DVR_FALSE;
    AmTsPlayer_showVideo(player->handle);
    AmTsPlayer_stopVideoDecoding(player->handle);
  }
  DVR_PB_DG(1, ":into");
  if (player->has_audio) {
    player->has_audio = DVR_FALSE;
    AmTsPlayer_stopAudioDecoding(player->handle);
  }
  DVR_PB_DG(1, ":into");

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_STOP;
  player->cmd.state = DVR_PLAYBACK_STATE_STOP;
  player->state = DVR_PLAYBACK_STATE_STOP;
  player->cur_segment_id = UINT64_MAX;
  player->segment_is_open = DVR_FALSE;
  DVR_PB_DG(1, "unlock");
  DVR_PB_DG(1, "player->state %s", _dvr_playback_state_toString(player->state));
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Start play audio
 * \param[in] handle playback handle
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */

int dvr_playback_audio_start(DVR_PlaybackHandle_t handle, am_tsplayer_audio_params *param) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  _start_playback_thread(handle);
  //start audio and video
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  player->has_audio = DVR_TRUE;
  AmTsPlayer_setAudioParams(player->handle, param);
  AmTsPlayer_startAudioDecoding(player->handle);
  //playback_device_audio_start(player->handle , param);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTART;
  player->cmd.state = DVR_PLAYBACK_STATE_START;
  player->state = DVR_PLAYBACK_STATE_START;
  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Stop play audio
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_audio_stop(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  //playback_device_audio_stop(player->handle);
  if (player->has_video == DVR_FALSE) {
    player->cmd.state = DVR_PLAYBACK_STATE_STOP;
    player->state = DVR_PLAYBACK_STATE_STOP;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.video is playing
  }
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);

  player->has_audio = DVR_FALSE;

  AmTsPlayer_stopAudioDecoding(player->handle);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTOP;

  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Start play video
 * \param[in] handle playback handle
 * \param[in] params video playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_start(DVR_PlaybackHandle_t handle, am_tsplayer_video_params *param) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  _start_playback_thread(handle);
  //start audio and video
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  player->has_video = DVR_TRUE;
  AmTsPlayer_setVideoParams(player->handle, param);
  AmTsPlayer_startVideoDecoding(player->handle);

  //playback_device_video_start(player->handle , param);
  //if set flag is pause live, we need set trick mode
  if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
    DVR_PB_DG(1, "settrick mode at video start");
    AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
    //playback_device_trick_mode(player->handle, 1);
  }
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTART;
  player->cmd.state = DVR_PLAYBACK_STATE_START;
  player->state = DVR_PLAYBACK_STATE_START;
  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Stop play video
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_stop(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  if (player->has_audio == DVR_FALSE) {
    player->cmd.state = DVR_PLAYBACK_STATE_STOP;
    player->state = DVR_PLAYBACK_STATE_STOP;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.audio is playing
  }

  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);

  player->has_video = DVR_FALSE;

  AmTsPlayer_stopVideoDecoding(player->handle);
  //playback_device_video_stop(player->handle);

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTOP;

  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Pause play
 * \param[in] handle playback handle
 * \param[in] flush whether its internal buffers should be flushed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_pause(DVR_PlaybackHandle_t handle, DVR_Bool_t flush) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  DVR_PB_DG(1, "get lock");
  if (player->has_video)
    AmTsPlayer_pauseVideoDecoding(player->handle);
  if (player->has_video)
    AmTsPlayer_pauseAudioDecoding(player->handle);

  //playback_device_pause(player->handle);
  if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
    player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
    player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
    player->state = DVR_PLAYBACK_STATE_PAUSE;
  } else {
    player->cmd.last_cmd = player->cmd.cur_cmd;
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_PAUSE;
    player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
    player->state = DVR_PLAYBACK_STATE_PAUSE;
  }
  pthread_mutex_unlock(&player->lock);
  DVR_PB_DG(1, "unlock");

  return DVR_SUCCESS;
}

//not add lock
static int _dvr_cmd(DVR_PlaybackHandle_t handle, int cmd)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  //get video params and audio params
  DVR_PB_DG(1, "lock");
  pthread_mutex_lock(&player->lock);
  am_tsplayer_video_params vparams;
  am_tsplayer_audio_params aparams;
  uint64_t segmentid = player->cur_segment_id;

  _dvr_playback_get_playinfo(handle, segmentid, &vparams, &aparams);
  DVR_PB_DG(1, "unlock cmd: %d", cmd);
  pthread_mutex_unlock(&player->lock);

  switch (cmd) {
    case DVR_PLAYBACK_CMD_AVRESTART:
      //av restart
      DVR_PB_DG(1, "do_cmd avrestart");
      _dvr_playback_replay((DVR_PlaybackHandle_t)player, DVR_FALSE);
      break;
    case DVR_PLAYBACK_CMD_VRESTART:
      dvr_playback_video_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_video_start((DVR_PlaybackHandle_t)player, &vparams);
      break;
    case DVR_PLAYBACK_CMD_VSTART:
      dvr_playback_video_start((DVR_PlaybackHandle_t)player, &vparams);
      break;
    case DVR_PLAYBACK_CMD_VSTOP:
      dvr_playback_video_stop((DVR_PlaybackHandle_t)player);
      break;
    case DVR_PLAYBACK_CMD_ARESTART:
      //a restart
      dvr_playback_audio_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_audio_start((DVR_PlaybackHandle_t)player, &aparams);
      break;
    case DVR_PLAYBACK_CMD_ASTART:
      dvr_playback_audio_start((DVR_PlaybackHandle_t)player, &aparams);
      break;
    case DVR_PLAYBACK_CMD_ASTOP:
      dvr_playback_audio_stop((DVR_PlaybackHandle_t)player);
      break;
    case  DVR_PLAYBACK_CMD_ASTOPVRESTART:
      dvr_playback_audio_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_video_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_video_start((DVR_PlaybackHandle_t)player, &vparams);
      break;
    case DVR_PLAYBACK_CMD_ASTOPVSTART:
      dvr_playback_audio_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_video_start((DVR_PlaybackHandle_t)player, &vparams);
      break;
    case DVR_PLAYBACK_CMD_VSTOPARESTART:
      dvr_playback_video_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_audio_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_audio_start((DVR_PlaybackHandle_t)player, &aparams);
      break;
    case DVR_PLAYBACK_CMD_STOP:
      break;
    case DVR_PLAYBACK_CMD_START:
      break;
    case DVR_PLAYBACK_CMD_ASTARTVRESTART:
      dvr_playback_video_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_video_start((DVR_PlaybackHandle_t)player, &vparams);
      dvr_playback_audio_start((DVR_PlaybackHandle_t)player, &aparams);
      break;
    case DVR_PLAYBACK_CMD_VSTARTARESTART:
      dvr_playback_audio_stop((DVR_PlaybackHandle_t)player);
      dvr_playback_video_start((DVR_PlaybackHandle_t)player, &vparams);
      dvr_playback_audio_start((DVR_PlaybackHandle_t)player, &aparams);
      break;
    case DVR_PLAYBACK_CMD_FF:
    case DVR_PLAYBACK_CMD_FB:
      _dvr_playback_fffb((DVR_PlaybackHandle_t)player);
      break;
    default:
      break;
  }
  return DVR_SUCCESS;
}

/**\brief Resume play
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_resume(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_PAUSE) {
    DVR_PB_DG(1, "lock");
    pthread_mutex_lock(&player->lock);
    if (player->has_video) {
      AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_NONE);
      AmTsPlayer_resumeVideoDecoding(player->handle);
    }
    if (player->has_audio) {
      AmTsPlayer_resumeAudioDecoding(player->handle);
    }
    //check is has audio param,if has audio .we need start audio,
    //we will stop audio when ff fb, if reach end, we will pause.so we need
    //start audio when resume play

    am_tsplayer_video_params vparams;
    am_tsplayer_audio_params aparams;
    uint64_t segmentid = player->cur_segment_id;
    _dvr_playback_get_playinfo(handle, segmentid, &vparams, &aparams);
    //valid audio pid, start audio
    if (player->has_audio == DVR_FALSE && VALID_PID(aparams.pid)) {
      player->has_audio = DVR_TRUE;
      AmTsPlayer_setAudioParams(player->handle, &aparams);
      AmTsPlayer_startAudioDecoding(player->handle);
    } else {
      DVR_PB_DG(1, "aparams.pid:%d player->has_audio:%d", aparams.pid, player->has_audio);
    }
    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
      player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
      player->cmd.state = DVR_PLAYBACK_STATE_START;
      player->state = DVR_PLAYBACK_STATE_START;
    } else {
      player->cmd.last_cmd = player->cmd.cur_cmd;
      player->cmd.cur_cmd = DVR_PLAYBACK_CMD_RESUME;
      player->cmd.state = DVR_PLAYBACK_STATE_START;
      player->state = DVR_PLAYBACK_STATE_START;
    }
    DVR_PB_DG(1, "unlock");
    pthread_mutex_unlock(&player->lock);
  } else if (player->state == DVR_PLAYBACK_STATE_PAUSE){
    if (player->has_video)
      AmTsPlayer_resumeVideoDecoding(player->handle);
    if (player->has_audio)
      AmTsPlayer_resumeAudioDecoding(player->handle);
    DVR_PB_DG(1, "set start state cur cmd[%d]", player->cmd.cur_cmd);
    player->cmd.state = DVR_PLAYBACK_STATE_START;
    player->state = DVR_PLAYBACK_STATE_START;
    _dvr_cmd(handle, player->cmd.cur_cmd);
  } else {
    if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE)
    {
      pthread_mutex_lock(&player->lock);
      //clear flag
      DVR_PB_DG(1, "clear pause live flag cur cmd[%d]", player->cmd.cur_cmd);
      player->play_flag = player->play_flag & (~DVR_PLAYBACK_STARTED_PAUSEDLIVE);
      AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_NONE);
      pthread_mutex_unlock(&player->lock);
    }
  }
  return DVR_SUCCESS;
}

static DVR_Bool_t _dvr_check_playinfo_changed(DVR_PlaybackHandle_t handle, int segment_id, int set_seg_id){

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment = NULL;
  DVR_PlaybackSegmentInfo_t *cur_segment = NULL;
  DVR_PlaybackSegmentInfo_t *set_segment = NULL;

  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment->segment_id == segment_id) {
      cur_segment = segment;
    }
    if (segment->segment_id == set_seg_id) {
      set_segment = segment;
    }
    if (cur_segment != NULL && set_segment != NULL) {
      break;
    }
  }
  if (cur_segment == NULL || set_segment == NULL) {
    DVR_PB_DG(1, "set segmen or cur segment is null");
    return DVR_TRUE;
  }
  if (cur_segment->pids.video.format != set_segment->pids.video.format ||
    cur_segment->pids.video.pid != set_segment->pids.video.pid ||
    cur_segment->pids.audio.format != set_segment->pids.audio.format ||
    cur_segment->pids.audio.pid != set_segment->pids.audio.pid) {
    DVR_PB_DG(1, "cur v[%d]a[%d] set v[%d]a[%d]",cur_segment->pids.video.pid,cur_segment->pids.audio.pid,set_segment->pids.video.pid,set_segment->pids.audio.pid);
    return DVR_TRUE;
  }
  DVR_PB_DG(1, "play info not change");
  return DVR_FALSE;
}

/**\brief seek
 * \param[in] handle playback handle
 * \param[in] time_offset time offset base cur segment
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_seek(DVR_PlaybackHandle_t handle, uint64_t segment_id, uint32_t time_offset) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  DVR_PB_DG(1, "lock segment_id %llu cur id %llu time_offset %u cur end: %d player->state:%d", segment_id,player->cur_segment_id, (uint32_t)time_offset, _dvr_get_end_time(handle), player->state);
  pthread_mutex_lock(&player->lock);

  int offset = -1;
  DVR_Bool_t replay = _dvr_check_playinfo_changed(handle, player->cur_segment_id, segment_id);
  DVR_PB_DG(1, "player->state[%d]-replay[%d]--get lock-", player->state, replay);

  //open segment if id is not current segment
  int ret = _dvr_open_segment(handle, segment_id);
  if (ret ==DVR_FAILURE) {
    DVR_PB_DG(1, "seek error at open segment");
    pthread_mutex_unlock(&player->lock);
    return DVR_FAILURE;
  }
  if (time_offset >_dvr_get_end_time(handle) &&_dvr_has_next_segmentId(handle, segment_id) == DVR_FAILURE) {
    if (segment_ongoing(player->r_handle) == DVR_SUCCESS) {
      DVR_PB_DG(1, "is ongoing segment when seek end, need return success");
      //pthread_mutex_unlock(&player->lock);
      //return DVR_SUCCESS;
      time_offset = _dvr_get_end_time(handle);
    } else {
      DVR_PB_DG(1, "is not ongoing segment when seek end, return failure");
      pthread_mutex_unlock(&player->lock);
      return DVR_FAILURE;
    }
  }

  DVR_PB_DG(1, "seek open id[%lld]flag[0x%x] time_offset %u", player->cur_segment.segment_id, player->cur_segment.flags, time_offset);
  //get file offset by time
  if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
    //forward playback.not seek end of file
    if (time_offset != 0 && time_offset > FB_DEFAULT_LEFT_TIME) {
      //default -2000ms
      time_offset = time_offset -FB_DEFAULT_LEFT_TIME;
    }
  }
  pthread_mutex_lock(&player->segment_lock);
  player->drop_ts = DVR_TRUE;
  offset = segment_seek(player->r_handle, (uint64_t)time_offset, player->openParams.block_size);
  DVR_PB_DG(0, "seek get offset by time offset, offset=%d time_offset %u",offset, time_offset);
  pthread_mutex_unlock(&player->segment_lock);
  player->offset = offset;

  _dvr_get_end_time(handle);
  //init fffb time
  player->fffb_current = _dvr_time_getClock();
  player->fffb_start = player->fffb_current;
  player->fffb_start_pcr = _dvr_get_cur_time(handle);
  player->next_fffb_time = player->fffb_current;
  //pause state if need to replayer false
  if (player->state == DVR_PLAYBACK_STATE_STOP ||
    (player->state == DVR_PLAYBACK_STATE_PAUSE)) {
    //only seek file,not start
    DVR_PB_DG(1, "unlock");
    pthread_mutex_unlock(&player->lock);
    return DVR_SUCCESS;
  }
  //stop play
  DVR_PB_DG(0, "seek stop play, not inject data has video[%d]audio[%d]", player->has_video, player->has_audio);
  if (player->has_video) {
    player->has_video = DVR_FALSE;
    AmTsPlayer_stopVideoDecoding(player->handle);
  }

  if (player->has_audio) {
    player->has_audio =DVR_FALSE;
    AmTsPlayer_stopAudioDecoding(player->handle);
  }
  //start play
  am_tsplayer_video_params    vparams;
  am_tsplayer_audio_params    aparams;

  player->cur_segment_id = segment_id;

  int sync = DVR_PLAYBACK_SYNC;
  //get segment info and audio video pid fmt ;
  _dvr_playback_get_playinfo(handle, segment_id, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio abnd video pis is all invalid, return error.
    DVR_PB_DG(0, "unlock seek start dvr play back start error, not found audio and video info");
    pthread_mutex_unlock(&player->lock);
    return -1;
  }
  //add
  if (sync == DVR_PLAYBACK_SYNC) {
    if (VALID_PID(vparams.pid)) {
      //player->has_video;
      if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_PAUSE ||
        player->speed > 1.0f||
        player->speed <= -1.0f) {
        //if is pause state. we need set trick mode.
        DVR_PB_DG(1, "seek set trick mode player->speed [%f]", player->speed);
        AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
      }
      AmTsPlayer_setVideoParams(player->handle, &vparams);
      AmTsPlayer_startVideoDecoding(player->handle);
      player->has_video = DVR_TRUE;
    }
    if (VALID_PID(aparams.pid)) {
      DVR_PB_DG(1, "start audio seek");
      AmTsPlayer_setAudioParams(player->handle, &aparams);
      AmTsPlayer_startAudioDecoding(player->handle);
      player->has_audio = DVR_TRUE;
    }
  }
  if (player->state == DVR_PLAYBACK_STATE_PAUSE &&
    ((player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
     player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) ||
    (player->cmd.last_cmd == DVR_PLAYBACK_CMD_FF ||
     player->cmd.last_cmd == DVR_PLAYBACK_CMD_FB))) {
    player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
    player->state = DVR_PLAYBACK_STATE_PAUSE;
    DVR_PB_DG(1, "set state pause in seek");
  } else if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
    player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
    player->speed > 1.0f||
    player->speed <= -1.0f) {
    DVR_PB_DG(1, "not set cmd to seek");
    //not pause state, we need not set cur cmd
  } else {
    DVR_PB_DG(1, "set cmd to seek");
    player->cmd.last_cmd = player->cmd.cur_cmd;
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_SEEK;
    player->cmd.state = DVR_PLAYBACK_STATE_START;
    player->state = DVR_PLAYBACK_STATE_START;
  }
  player->last_send_time_id = UINT64_MAX;
  DVR_PB_DG(1, "unlock");
  pthread_mutex_unlock(&player->lock);

  return DVR_SUCCESS;
}

static int _dvr_get_cur_time(DVR_PlaybackHandle_t handle) {
  //get cur time of segment
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL || player->handle == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  int64_t cache = 0;//defalut es buf cache 500ms
  AmTsPlayer_getDelayTime(player->handle, &cache);
  pthread_mutex_lock(&player->segment_lock);
  uint64_t cur = segment_tell_current_time(player->r_handle);
  pthread_mutex_unlock(&player->segment_lock);
  DVR_PB_DG(1, "get cur time [%lld] cache:%lld cur id [%lld]", cur, cache, player->cur_segment_id);
  if (player->state == DVR_PLAYBACK_STATE_STOP) {
    cache = 0;
  }
  int cur_time = (int)(cur > cache ? cur - cache : 0);
  return cur_time;
}

//get current segment current pcr time of read pos
static int _dvr_get_end_time(DVR_PlaybackHandle_t handle) {
  //get cur time of segment
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  pthread_mutex_lock(&player->segment_lock);
  uint64_t end = segment_tell_total_time(player->r_handle);
  DVR_PB_DG(1, "get tatal time [%lld]", end);
  pthread_mutex_unlock(&player->segment_lock);
  return (int)end;
}

#define FB_MIX_SEEK_TIME 2000
//start replay
static int _dvr_playback_calculate_seekpos(DVR_PlaybackHandle_t handle) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //calculate pcr seek time
  int t_diff = 0;
  int seek_time = 0;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  if (player->fffb_start == -1) {
    //set fffb start time ms
    player->fffb_start = _dvr_time_getClock();
    player->fffb_current = player->fffb_start;
    //get segment current time pos
    player->fffb_start_pcr = _dvr_get_cur_time(handle);
    DVR_PB_DG(1, "calculate seek pos player->fffb_start_pcr[%d]ms, speed[%f]", player->fffb_start_pcr, player->speed);
    t_diff = 0;
    //default first time 1ms seek
    seek_time = FB_MIX_SEEK_TIME;
  } else {
    player->fffb_current = _dvr_time_getClock();
    t_diff = player->fffb_current - player->fffb_start;
    //if speed is < 0, cmd is fb.
    seek_time = player->fffb_start_pcr + t_diff *player->speed;
    if (seek_time <= 0) {
      //need seek to pre one segment
      seek_time = 0;
    }
    //seek segment pos
    if (player->r_handle) {
      pthread_mutex_lock(&player->segment_lock);
      if (segment_seek(player->r_handle, seek_time, player->openParams.block_size) == DVR_FAILURE) {
        seek_time = 0;
      }
      pthread_mutex_unlock(&player->segment_lock);
    } else {
      //
      DVR_PB_DG(1, "segment not open,can not seek");
    }
    DVR_PB_DG(1, "calculate seek pos seek_time[%d]ms, speed[%f]id[%lld]cur [%d]", seek_time, player->speed,player->cur_segment_id,  _dvr_get_cur_time(handle));
   }
  return seek_time;
}


//start replay
static int _dvr_playback_fffb_replay(DVR_PlaybackHandle_t handle) {
  //
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  //stop
  if (player->has_video) {
    DVR_PB_DG(1, "fffb stop video");
    AmTsPlayer_stopVideoDecoding(player->handle);
  }
  if (player->has_audio) {
    DVR_PB_DG(1, "fffb stop audio");
    player->has_audio =DVR_FALSE;
    AmTsPlayer_stopAudioDecoding(player->handle);
  }

  //start video and audio

  am_tsplayer_video_params    vparams;
  am_tsplayer_audio_params    aparams;
  uint64_t segment_id = player->cur_segment_id;

  //get segment info and audio video pid fmt ;
  //pthread_mutex_lock(&player->lock);
  _dvr_playback_get_playinfo(handle, segment_id, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio abnd video pis is all invalid, return error.
    DVR_PB_DG(0, "dvr play back restart error, not found audio and video info");
    //pthread_mutex_unlock(&player->lock);
    return -1;
  }

  if (VALID_PID(vparams.pid)) {
    player->has_video = DVR_TRUE;
    DVR_PB_DG(1, "fffb start video");
    AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_NONE);
    AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
    AmTsPlayer_setVideoParams(player->handle, &vparams);
    AmTsPlayer_startVideoDecoding(player->handle);
    //playback_device_video_start(player->handle , &vparams);
    //if set flag is pause live, we need set trick mode
    //playback_device_trick_mode(player->handle, 1);
  }
  if (0 && VALID_PID(aparams.pid)) {
    player->has_audio = DVR_TRUE;
    DVR_PB_DG(1, "fffb start audio");
    AmTsPlayer_setAudioParams(player->handle, &aparams);
    AmTsPlayer_startAudioDecoding(player->handle);
    //playback_device_audio_start(player->handle , &aparams);
  }
  //fffb mode need stop fast;
  DVR_PB_DG(1, "stop fast");
  AmTsPlayer_stopFast(player->handle);
  //pthread_mutex_unlock(&player->lock);
  return 0;
}

static int _dvr_playback_fffb(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  player->first_frame = 0;
  DVR_PB_DG(1, "lock  speed [%f]", player->speed);
  pthread_mutex_lock(&player->lock);

  int seek_time = _dvr_playback_calculate_seekpos(handle);
  DVR_PB_DG(1, "get lock  speed [%f]id [%lld]seek_time[%d]", player->speed, player->cur_segment_id, seek_time);

  if (_dvr_has_next_segmentId(handle, player->cur_segment_id) == DVR_FAILURE && seek_time < FB_MIX_SEEK_TIME && IS_FB(player->speed)) {
      //seek time set 0
      seek_time = 0;
  }
  if (seek_time == 0) {
    //for fb cmd, we need open pre segment.if reach first one segment, send begin event
    int ret = _change_to_next_segment((DVR_PlaybackHandle_t)player);
    if (ret != DVR_SUCCESS && IS_FB(player->speed)) {
      pthread_mutex_unlock(&player->lock);
      dvr_playback_pause(handle, DVR_FALSE);
      //send event here and pause
      DVR_Play_Notify_t notify;
      memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
      notify.event = DVR_PLAYBACK_EVENT_REACHED_BEGIN;
      //get play statue not here
      _dvr_playback_sent_event(handle, DVR_PLAYBACK_EVENT_REACHED_BEGIN, &notify);
      DVR_PB_DG(1, "*******************send begin event  speed [%f] cur [%d]", player->speed, _dvr_get_cur_time(handle));
      //change to pause
      return DVR_SUCCESS;
    }
    _dvr_playback_sent_transition_ok(handle);
    _dvr_init_fffb_time(handle);
    DVR_PB_DG(1, "*******************send trans ok event  speed [%f]", player->speed);
  }
  player->next_fffb_time =_dvr_time_getClock() + FFFB_SLEEP_TIME;
  _dvr_playback_fffb_replay(handle);

  pthread_mutex_unlock(&player->lock);
  DVR_PB_DG(1, "unlock");

  return DVR_SUCCESS;
}

//start replay, need get lock at extern
static int _dvr_playback_replay(DVR_PlaybackHandle_t handle, DVR_Bool_t trick) {
  //
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  //stop
  if (player->has_video) {
    player->has_video = DVR_FALSE;
    AmTsPlayer_stopVideoDecoding(player->handle);
  }

  if (player->has_audio) {
    player->has_audio = DVR_FALSE;
    AmTsPlayer_stopAudioDecoding(player->handle);
  }
  //start video and audio

  am_tsplayer_video_params    vparams;
  am_tsplayer_audio_params    aparams;
  uint64_t segment_id = player->cur_segment_id;

  //get segment info and audio video pid fmt ;
  DVR_PB_DG(1, "into");
  _dvr_playback_get_playinfo(handle, segment_id, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio and video pis is all invalid, return error.
    DVR_PB_DG(0, "dvr play back restart error, not found audio and video info");
    return -1;
  }

  if (VALID_PID(vparams.pid)) {
    player->has_video = DVR_TRUE;
    if (trick == DVR_TRUE) {
      DVR_PB_DG(1, "settrick mode at replay");
      AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
    }
    else {
      AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_NONE);
    }
    AmTsPlayer_setVideoParams(player->handle, &vparams);
    AmTsPlayer_startVideoDecoding(player->handle);
  }

  if (IS_FAST_SPEED(player->cmd.speed.speed.speed)) {
    DVR_PB_DG(1, "start fast");
    AmTsPlayer_startFast(player->handle, (float)player->cmd.speed.speed.speed/(float)100);
    player->speed = (float)player->cmd.speed.speed.speed/100.0f;
  } else {
    if (VALID_PID(aparams.pid)) {
      player->has_audio = DVR_TRUE;
      DVR_PB_DG(1, "start audio");
      AmTsPlayer_startAudioDecoding(player->handle);
      AmTsPlayer_setAudioParams(player->handle, &aparams);
    }
    DVR_PB_DG(1, "stop fast");
    AmTsPlayer_stopFast(player->handle);
    player->cmd.speed.speed.speed = PLAYBACK_SPEED_X1;
    player->speed = (float)PLAYBACK_SPEED_X1/100.0f;
  }
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_START;
  player->cmd.state = DVR_PLAYBACK_STATE_START;
  player->state = DVR_PLAYBACK_STATE_START;
  return 0;
}


/**\brief Set play speed
 * \param[in] handle playback handle
 * \param[in] speed playback speed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_set_speed(DVR_PlaybackHandle_t handle, DVR_PlaybackSpeed_t speed) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  DVR_PB_DG(1, "lock func: speed [%d]", speed.speed.speed);
  if (_dvr_support_speed(speed.speed.speed) == DVR_FALSE) {
    DVR_PB_DG(1, " func: not support speed [%d]", speed.speed.speed);
    return DVR_FAILURE;
  }
  pthread_mutex_lock(&player->lock);
  if (player->cmd.cur_cmd != DVR_PLAYBACK_CMD_FF
    && player->cmd.cur_cmd != DVR_PLAYBACK_CMD_FB) {
    player->cmd.last_cmd = player->cmd.cur_cmd;
  }
  if (player->state != DVR_PLAYBACK_STATE_PAUSE &&
    IS_KERNEL_SPEED(speed.speed.speed)) {
     //case 1. cur speed is 100,set 200 50 25 12 .
     //we think x1 and x2 s1/2 s 1/4 s 1/8 is normal speed. is not ff fb.
     if (IS_KERNEL_SPEED(player->cmd.speed.speed.speed)) {
      //if last speed is x2 or s2, we need stop fast
      if (speed.speed.speed == PLAYBACK_SPEED_X1) {
        // resume audio and stop fast play
        DVR_PB_DG(1, "stop fast");
        AmTsPlayer_stopFast(player->handle);
        pthread_mutex_unlock(&player->lock);
        _dvr_cmd(handle, DVR_PLAYBACK_CMD_ASTART);
        pthread_mutex_lock(&player->lock);
      } else {
        //set play speed and if audio is start, stop audio.
        if (player->has_audio) {
          DVR_PB_DG(1, "fast play stop audio");
          AmTsPlayer_stopAudioDecoding(player->handle);
          player->has_audio = DVR_FALSE;
        }
        DVR_PB_DG(1, "start fast");
        AmTsPlayer_startFast(player->handle, (float)speed.speed.speed/(float)100);
      }
      player->fffb_play = DVR_FALSE;
      player->cmd.speed.mode = DVR_PLAYBACK_KERNEL_SUPPORT;
      player->cmd.speed.speed = speed.speed;
      player->speed = (float)speed.speed.speed/(float)100;
      pthread_mutex_unlock(&player->lock);
      return DVR_SUCCESS;
     }
    //case 2. not start play
    if (player->state == DVR_PLAYBACK_STATE_STOP) {
      //only set speed.and return;
      player->cmd.speed.mode = DVR_PLAYBACK_KERNEL_SUPPORT;
      player->cmd.speed.speed = speed.speed;
      player->speed = (float)speed.speed.speed/(float)100;
      player->fffb_play = DVR_FALSE;
      pthread_mutex_unlock(&player->lock);
      return DVR_SUCCESS;
    }
    //case 3 fffb mode
    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
      player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
      //restart play at normal speed exit ff fb
      DVR_PB_DG(1, "set speed normal and replay playback");
      player->cmd.speed.mode = DVR_PLAYBACK_KERNEL_SUPPORT;
      player->cmd.speed.speed = speed.speed;
      player->speed = (float)speed.speed.speed/(float)100;
      _dvr_playback_replay(handle, DVR_FALSE);
      player->fffb_play = DVR_FALSE;
      pthread_mutex_unlock(&player->lock);
      return DVR_SUCCESS;
    }
  }
  else if (player->state == DVR_PLAYBACK_STATE_PAUSE &&
    IS_KERNEL_SPEED(speed.speed.speed)) {
     //case 1. cur speed is kernel support speed,set kernel speed.
    if (IS_KERNEL_SPEED(player->cmd.speed.speed.speed)) {
     //if last speed is x2 or s2, we need stop fast
     if (speed.speed.speed == PLAYBACK_SPEED_X1) {
        // resume audio and stop fast play
        DVR_PB_DG(1, "stop fast");
        AmTsPlayer_stopFast(player->handle);
        pthread_mutex_unlock(&player->lock);
        _dvr_cmd(handle, DVR_PLAYBACK_CMD_ASTART);
        pthread_mutex_lock(&player->lock);
      } else {
        //set play speed and if audio is start, stop audio.
        if (player->has_audio) {
          DVR_PB_DG(1, "fast play stop audio at pause");
          AmTsPlayer_stopAudioDecoding(player->handle);
          player->has_audio = DVR_FALSE;
       }
        DVR_PB_DG(1, "start fast");
        AmTsPlayer_startFast(player->handle, (float)speed.speed.speed/(float)100);
     }
     player->cmd.speed.mode = DVR_PLAYBACK_KERNEL_SUPPORT;
     player->cmd.speed.speed = speed.speed;
     player->speed = (float)speed.speed.speed/(float)100;
     player->fffb_play = DVR_FALSE;
     pthread_mutex_unlock(&player->lock);
     return DVR_SUCCESS;
    }
    //case 2 fffb mode
    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
      player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
      //restart play at normal speed exit ff fb
      DVR_PB_DG(1, "set speed x1 s2 and replay playback");
      player->cmd.speed.mode = DVR_PLAYBACK_KERNEL_SUPPORT;
      player->cmd.speed.speed = speed.speed;
      player->speed = (float)speed.speed.speed/(float)100;
      player->cmd.cur_cmd = DVR_PLAYBACK_CMD_AVRESTART;
      player->fffb_play = DVR_FALSE;
      pthread_mutex_unlock(&player->lock);
      return DVR_SUCCESS;
    }
  }
  if (IS_KERNEL_SPEED(speed.speed.speed)) {
    //we think x1 and s2 s4 s8 x2is normal speed. is not ff fb.
    player->fffb_play = DVR_FALSE;
  } else {
      if ((float)speed.speed.speed > 1.0f)
        player->cmd.cur_cmd = DVR_PLAYBACK_CMD_FF;
      else
        player->cmd.cur_cmd = DVR_PLAYBACK_CMD_FB;
      player->fffb_play = DVR_TRUE;
  }
  DVR_Bool_t init_last_time = DVR_FALSE;
  if (player->speed > 0.0f && speed.speed.speed < 0) {
    init_last_time = DVR_TRUE;
  } else if (player->speed < 0.0f && speed.speed.speed > 0) {
    init_last_time = DVR_TRUE;
  }
  player->cmd.speed.mode = speed.mode;
  player->cmd.speed.speed = speed.speed;
  player->speed = (float)speed.speed.speed/(float)100;
#if 0
  if (abs((int)(player->speed)) > 1) {
    //seek  speed*1000 ms at fb mode
    pthread_mutex_lock(&player->segment_lock);
    uint64_t cur = segment_tell_current_time(player->r_handle);
    int diff = abs((int)(player->speed * 1000));
    DVR_PB_DG(1, " cur:%ull diff:%d", cur, diff);
     if (cur > diff)
      cur = cur - diff;
     else
      cur = 0;
    segment_seek(player->r_handle, cur, player->openParams.block_size);
    pthread_mutex_unlock(&player->segment_lock);
  }
#endif
  //reset fffb time, if change speed value
  _dvr_init_fffb_t(handle);
  if (init_last_time == DVR_TRUE)
    player->last_send_time_id = UINT64_MAX;

  if (speed.speed.speed == PLAYBACK_SPEED_X1 &&
    (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
    player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB)) {
    //restart play at normal speed exit ff fb
    DVR_PB_DG(1, "set speed normal and replay playback");
    _dvr_playback_replay(handle, DVR_FALSE);
  } else if (speed.speed.speed == PLAYBACK_SPEED_X1 &&
    (player->state == DVR_PLAYBACK_STATE_PAUSE)) {
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_AVRESTART;
    DVR_PB_DG(1, "set speed normal at pause state ,set cur cmd");
  }
  DVR_PB_DG(1, "unlock  speed[%f]cmd[%d]", player->speed, player->cmd.cur_cmd);
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Get playback status
 * \param[in] handle playback handle
 * \param[out] p_status playback status
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_get_status(DVR_PlaybackHandle_t handle,
  DVR_PlaybackStatus_t *p_status) {
//
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  p_status->state = player->state;
  //when got first frame we will change to pause state.this only from start play to got first frame
  if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE &&
    player->state == DVR_PLAYBACK_STATE_START) {
    p_status->state = DVR_PLAYBACK_STATE_PAUSE;
  }

  p_status->time_end = _dvr_get_end_time(handle);
  p_status->time_cur = _dvr_get_cur_time(handle);
  if (player->last_send_time_id == UINT64_MAX) {
    player->last_send_time_id = player->cur_segment_id;
    player->last_cur_time = p_status->time_cur;
  }
  if (player->last_send_time_id == player->cur_segment_id) {
    if (player->speed > 0.0f ) {
      //ff
      if (p_status->time_cur < player->last_cur_time ) {
        DVR_PB_DG(1, "get ff time error last[%d]cur[%d]diff[%d]", player->last_cur_time, p_status->time_cur, player->last_cur_time - p_status->time_cur);
        p_status->time_cur = player->last_cur_time;
      } else {
        player->last_cur_time = p_status->time_cur;
      }
    } else if (player->speed < -1.0f){
      //fb
      if (p_status->time_cur > player->last_cur_time ) {
        DVR_PB_DG(1, "get fb time error last[%d]cur[%d]diff[%d]", player->last_cur_time, p_status->time_cur, p_status->time_cur - player->last_cur_time );
        p_status->time_cur = player->last_cur_time;
      } else {
        player->last_cur_time = p_status->time_cur;
      }
    }
  } else {
    player->last_cur_time = p_status->time_cur;
  }
  player->last_send_time_id = player->cur_segment_id;
  p_status->segment_id = player->cur_segment_id;

  memcpy(&p_status->pids, &player->cur_segment.pids, sizeof(DVR_PlaybackPids_t));
  p_status->speed = player->cmd.speed.speed.speed;
  p_status->flags = player->cur_segment.flags;
  DVR_PB_DG(1, "player real state[%s]state[%s]cur[%d]end[%d] id[%lld]playflag[%d]speed[%f]",
  _dvr_playback_state_toString(player->state),
  _dvr_playback_state_toString(p_status->state),
  p_status->time_cur, p_status->time_end,
  p_status->segment_id,player->play_flag,
  player->speed);
  return DVR_SUCCESS;
}

void _dvr_dump_segment(DVR_PlaybackSegmentInfo_t *segment) {
  if (segment != NULL) {
    DVR_PB_DG(1, "segment id: %lld", segment->segment_id);
    DVR_PB_DG(1, "segment flag: %d", segment->flags);
    DVR_PB_DG(1, "segment location: [%s]", segment->location);
    DVR_PB_DG(1, "segment vpid: 0x%x vfmt:0x%x", segment->pids.video.pid,segment->pids.video.format);
    DVR_PB_DG(1, "segment apid: 0x%x afmt:0x%x", segment->pids.audio.pid,segment->pids.audio.format);
    DVR_PB_DG(1, "segment pcr pid: 0x%x pcr fmt:0x%x", segment->pids.pcr.pid,segment->pids.pcr.format);
    DVR_PB_DG(1, "segment sub apid: 0x%x sub afmt:0x%x", segment->pids.ad.pid,segment->pids.ad.format);
  }
}

int dvr_dump_segmentinfo(DVR_PlaybackHandle_t handle, uint64_t segment_id) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player == NULL) {
    DVR_PB_DG(1, "player is NULL");
    return DVR_FAILURE;
  }

  DVR_PlaybackSegmentInfo_t *segment;
  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment_id >= 0) {
      if (segment->segment_id == segment_id) {
        _dvr_dump_segment(segment);
       break;
      }
    } else {
      //printf segment info
      _dvr_dump_segment(segment);
    }
  }
  return 0;
}

int dvr_playback_set_decrypt_callback(DVR_PlaybackHandle_t handle, DVR_CryptoFunction_t func, void *userdata)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_RETURN_IF_FALSE(player);
  DVR_RETURN_IF_FALSE(func);

  DVR_PB_DG(1, "in ");
  pthread_mutex_lock(&player->lock);

  player->dec_func = func;
  player->dec_userdata = userdata;

  pthread_mutex_unlock(&player->lock);
  DVR_PB_DG(1, "out ");
  return DVR_SUCCESS;
}

int dvr_playback_set_secure_buffer(DVR_PlaybackHandle_t handle, uint8_t *p_secure_buf, uint32_t len)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_RETURN_IF_FALSE(player);
  DVR_RETURN_IF_FALSE(p_secure_buf);
  DVR_RETURN_IF_FALSE(len);

  DVR_PB_DG(1, "in ");
  pthread_mutex_lock(&player->lock);

  player->is_secure_mode = 1;
  player->secure_buffer = p_secure_buf;
  player->secure_buffer_size = len;

  pthread_mutex_unlock(&player->lock);
  DVR_PB_DG(1, "out");
  return DVR_SUCCESS;
}
