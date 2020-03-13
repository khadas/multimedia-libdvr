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

#include "dvr_playback.h"

#define VALID_PID(_pid_) ((_pid_)>0 && (_pid_)<0x1fff)
#define IS_FFFB(_SPEED_) ((_SPEED_) > 1 && (_SPEED_) < -1)
#define IS_FB(_SPEED_) ((_SPEED_) < 0)

#define FFFB_SLEEP_TIME    1000
#define FB_DEFAULT_LEFT_TIME    (2000)
//
static int _dvr_playback_fffb(DVR_PlaybackHandle_t handle);
static int _do_check_pid_info(DVR_PlaybackHandle_t handle, DVR_StreamInfo_t  now_pid, DVR_StreamInfo_t set_pid, int type);
static int _dvr_get_cur_time(DVR_PlaybackHandle_t handle);
static int _dvr_get_end_time(DVR_PlaybackHandle_t handle);
static int _dvr_playback_calculate_seekpos(DVR_PlaybackHandle_t handle);
static int _dvr_playback_replay(DVR_PlaybackHandle_t handle, DVR_Bool_t trick) ;

static int first_frame = 0;

void _dvr_tsplayer_callback_test(void *user_data, am_tsplayer_event *event)
{
  DVR_DEBUG(1, "in callback test ");
  DVR_Playback_t *player = NULL;
  if (user_data != NULL) {
    DVR_Playback_t *player = (DVR_Playback_t *) user_data;
    DVR_DEBUG(1, "play speed [%d] in callback test ", player->speed);
  }
  switch (event->type) {
      case AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED:
      {
          DVR_DEBUG(1,"[evt] test AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED: %d x %d @%d\n",
              event->event.video_format.frame_width,
              event->event.video_format.frame_height,
              event->event.video_format.frame_rate);
          break;
      }
      case AM_TSPLAYER_EVENT_TYPE_MPEG_USERDATA:
      {
          uint8_t* pbuf = event->event.mpeg_user_data.data;
          uint32_t size = event->event.mpeg_user_data.len;
          DVR_DEBUG(1, "[evt] test AM_TSPLAYER_EVENT_TYPE_MPEG_USERDATA: %x-%x-%x-%x ,size %d\n",
              pbuf[0], pbuf[1], pbuf[2], pbuf[3], size);
          break;
      }
      case AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME:
      {
          DVR_DEBUG(1, "[evt] test  AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME\n");
          first_frame = 1;
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
    DVR_DEBUG(1, "play speed [%d] in callback", player->speed);
  }
  switch (event->type) {
    case AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED:
    {
        DVR_DEBUG(1,"[evt] AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED: %d x %d @%d\n",
            event->event.video_format.frame_width,
            event->event.video_format.frame_height,
            event->event.video_format.frame_rate);
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_MPEG_USERDATA:
    {
        uint8_t* pbuf = event->event.mpeg_user_data.data;
        uint32_t size = event->event.mpeg_user_data.len;
        DVR_DEBUG(1, "[evt] AM_TSPLAYER_EVENT_TYPE_MPEG_USERDATA: %x-%x-%x-%x ,size %d\n",
            pbuf[0], pbuf[1], pbuf[2], pbuf[3], size);
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME:
    {
        DVR_DEBUG(1, "[evt] AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME\n");
        first_frame = 1;
        break;
    }
    default:
      break;
  }
  if (player&&player->player_callback_func) {
    player->player_callback_func(player->player_callback_userdata, event);
  } else if (player == NULL){
    DVR_DEBUG(1, "player is null, get userdata error\n");
  } else {
    DVR_DEBUG(1, "player callback is null, get callback error\n");
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
    }
  }
  return format;
}
static int _dvr_playback_get_trick_stat(DVR_PlaybackHandle_t handle)
{
  int state = 0;
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player->handle == NULL)
    return -1;

  return first_frame;
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

//send signal
static int _dvr_playback_sendSignal(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;\
  DVR_DEBUG(1, "send signal lock");
  pthread_mutex_lock(&player->lock);
  DVR_DEBUG(1, "send signal got lock");
  pthread_cond_signal(&player->cond);
  DVR_DEBUG(1, "send signal unlock");
  pthread_mutex_unlock(&player->lock);
  return 0;
}

//send playback event
static int _dvr_playback_sent_event(DVR_PlaybackHandle_t handle, DVR_PlaybackEvent_t evt, DVR_Play_Notify_t *notify) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  switch (evt) {
    case DVR_PLAYBACK_EVENT_ERROR:
      break;
    case DVR_PLAYBACK_EVENT_TRANSITION_OK:
      //GET STATE
      DVR_DEBUG(1, "trans ok----");
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
      DVR_DEBUG(1, "reached begin---");
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    case DVR_PLAYBACK_EVENT_REACHED_END:
      //GET STATE
      DVR_DEBUG(1, "reached end---");
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    case DVR_PLAYBACK_EVENT_NOTIFY_PLAYTIME:
      //DVR_DEBUG(1, "send playtime---");
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
  ret = segment_ongoing(player->r_handle);
  if (ret != DVR_SUCCESS) {
     return DVR_FALSE;
  }
  return DVR_TRUE;
}
static int _dvr_init_fffb_time(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  player->fffb_current = -1;
  player->fffb_start = -1;
  player->fffb_start_pcr = -1;
  player->next_fffb_time = _dvr_time_getClock();
  return DVR_SUCCESS;
}
//get next segment id
static int _dvr_has_next_segmentId(DVR_PlaybackHandle_t handle, int segmentid) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;
  DVR_PlaybackSegmentInfo_t *pre_segment = NULL;

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
          DVR_DEBUG(1, "not has find next segment on fb mode");
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
    DVR_DEBUG(1, "not found next segment return failure");
    return DVR_FAILURE;
  }
  DVR_DEBUG(1, "found next segment return success");
  return DVR_SUCCESS;
}

//get next segment id
static int _dvr_get_next_segmentId(DVR_PlaybackHandle_t handle) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;
  DVR_PlaybackSegmentInfo_t *pre_segment = NULL;

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
          DVR_DEBUG(1, "not find next segment on fb mode");
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
      DVR_DEBUG(1, "cur flag[0x%x]segment->flags flag[0x%x] id [%lld]", player->cur_segment.flags, segment->flags, segment->segment_id);
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
    DVR_DEBUG(1, "fb last one cur flag[0x%x]segment->flags flag[0x%x] id [%lld]", player->cur_segment.flags, pre_segment->flags, pre_segment->segment_id);
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

  int id = _dvr_get_next_segmentId(handle);
  if (id < 0) {
    DVR_DEBUG(1, "not found segment info");
    return DVR_FAILURE;
  }

  if (player->r_handle != NULL) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }

  memset(params.location, 0, DVR_MAX_LOCATION_SIZE);
  //cp chur segment path to location
  memcpy(params.location, player->cur_segment.location, DVR_MAX_LOCATION_SIZE);
  params.segment_id = (uint64_t)player->cur_segment.segment_id;
  params.mode = SEGMENT_MODE_READ;
  DVR_DEBUG(1, "open segment info[%s][%lld]flag[0x%x]", params.location, params.segment_id, player->cur_segment.flags);
  pthread_mutex_lock(&player->segment_lock);
  ret = segment_open(&params, &(player->r_handle));
  pthread_mutex_unlock(&player->segment_lock);
  int total = _dvr_get_end_time( handle);
  pthread_mutex_lock(&player->segment_lock);
  if (IS_FB(player->speed)) {
      //seek end pos -FB_DEFAULT_LEFT_TIME
      segment_seek(player->r_handle, total - FB_DEFAULT_LEFT_TIME);
  }
  player->dur = total;
  pthread_mutex_unlock(&player->segment_lock);
  DVR_DEBUG(1, "next player->dur [%d] flag [0x%x]", player->dur, player->cur_segment.flags);
  return ret;
}

//open next segment to play,if reach list end return errro.
static int _dvr_open_segment(DVR_PlaybackHandle_t handle, uint64_t segment_id)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  Segment_OpenParams_t  params;
  int ret = DVR_SUCCESS;
  if (segment_id == player->cur_segment_id && player->segment_is_open == DVR_TRUE) {
    return DVR_SUCCESS;
  }
  uint64_t id = segment_id;
  if (id < 0) {
    DVR_DEBUG(1, "not found segment info");
    return DVR_FAILURE;
  }
  DVR_DEBUG(1, "start found segment[%lld][%lld] info", id,segment_id);
  pthread_mutex_lock(&player->segment_lock);

  DVR_PlaybackSegmentInfo_t *segment;

  int found = 0;

  list_for_each_entry(segment, &player->segment_list, head)
  {
    DVR_DEBUG(1, "see 1 location [%s]id[%lld]flag[%x]segment_id[%lld]", segment->location, segment->segment_id, segment->flags, segment_id);
    if (segment->segment_id == segment_id) {
      found = 1;
    }
    if (found == 1) {
      DVR_DEBUG(1, "found  [%s]id[%lld]flag[%x]segment_id[%lld]", segment->location, segment->segment_id, segment->flags, segment_id);
      //get segment info
      player->segment_is_open = DVR_TRUE;
      player->cur_segment_id = segment->segment_id;
      player->cur_segment.segment_id = segment->segment_id;
      player->cur_segment.flags = segment->flags;
      strncpy(player->cur_segment.location, segment->location, strlen(segment->location));//DVR_MAX_LOCATION_SIZE
      //pids
      memcpy(&player->cur_segment.pids, &segment->pids, sizeof(DVR_PlaybackPids_t));
      DVR_DEBUG(1, "cur found location [%s]id[%lld]flag[%x]", player->cur_segment.location, player->cur_segment.segment_id,player->cur_segment.flags);
      break;
    }
  }
  if (found == 0) {
    DVR_DEBUG(1, "not found segment info.error..");
    pthread_mutex_unlock(&player->segment_lock);
    return DVR_FAILURE;
  }
  memset(params.location, 0, DVR_MAX_LOCATION_SIZE);
  //cp cur segment path to location
  strncpy(params.location, player->cur_segment.location, strlen(player->cur_segment.location));
  params.segment_id = (uint64_t)player->cur_segment.segment_id;
  params.mode = SEGMENT_MODE_READ;
  DVR_DEBUG(1, "open segment location[%s][%lld]cur flag[0x%x]", params.location, params.segment_id, player->cur_segment.flags);
  if (player->r_handle != NULL) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }
  ret = segment_open(&params, &(player->r_handle));
  pthread_mutex_unlock(&player->segment_lock);
  player->dur = _dvr_get_end_time(handle);

  DVR_DEBUG(1, "player->dur [%d]cur id [%lld]cur flag [0x%x]\r\n", player->dur,player->cur_segment.segment_id, player->cur_segment.flags);
  return ret;
}


//get play info by segment id
static int _dvr_playback_get_playinfo(DVR_PlaybackHandle_t handle,
  uint64_t segment_id,
  am_tsplayer_video_params *vparam,
  am_tsplayer_audio_params *aparam) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;

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
      DVR_DEBUG(1, "get play info id [%lld]", player->cur_segment_id);
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
      DVR_DEBUG(1, "get play info sucess[0x%x]apid[0x%x]", vparam->pid, aparam->pid);
      found = 2;
      break;
    }
  }
  if (found != 2) {
    //list is null or reache list  end
    DVR_DEBUG(1, "get play info fail");
    return DVR_FAILURE;
  }

  return DVR_SUCCESS;
}
static int _dvr_replay_changed_pid(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  //compare cur segment
  //if (player->cmd.state == DVR_PLAYBACK_STATE_START)
  {
    //check video pids, stop or restart
    _do_check_pid_info(handle, player->last_segment.pids.video, player->cur_segment.pids.video, 0);
    //check audio pids stop or restart
    _do_check_pid_info(handle, player->last_segment.pids.audio, player->cur_segment.pids.audio, 1);
    //check sub audio pids stop or restart
    _do_check_pid_info(handle, player->last_segment.pids.ad, player->cur_segment.pids.ad, 2);
    //check pcr pids stop or restart
    _do_check_pid_info(handle, player->last_segment.pids.pcr, player->cur_segment.pids.pcr, 3);
  }
  return 0;
}

static int _dvr_check_cur_segment_flag(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "call %s flag[0x%x]id[%lld]last[0x%x][%d]", __func__, player->cur_segment.flags, player->cur_segment.segment_id, player->last_segment.flags, player->last_segment.segment_id);
  if ((player->cur_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE &&
    (player->last_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0) {
    //enable display
    DVR_DEBUG(1, "call %s unmute", __func__);
    AmTsPlayer_showVideo(player->handle);
    AmTsPlayer_setAudioMute(player->handle, 0, 0);
  } else if ((player->cur_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0 &&
    (player->last_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE) {
    //disable display
    DVR_DEBUG(1, "call %s mute", __func__);
    AmTsPlayer_hideVideo(player->handle);
    AmTsPlayer_setAudioMute(player->handle, 1, 1);
  }
  return DVR_SUCCESS;
}
static void* _dvr_playback_thread(void *arg)
{
  DVR_Playback_t *player = (DVR_Playback_t *) arg;
  //int need_open_segment = 1;
  am_tsplayer_input_buffer     wbufs;
  int ret = DVR_SUCCESS;

  int timeout = 100;//ms
  uint64_t write_timeout_ms = 50;
  uint8_t *buf = NULL;
  int buf_len = player->openParams.block_size > 0 ? player->openParams.block_size : (256 * 1024);
  int real_read = 0;
  DVR_Bool_t goto_rewrite = DVR_FALSE;

  buf = malloc(buf_len);
  wbufs.buf_type = TS_INPUT_BUFFER_TYPE_NORMAL;
  wbufs.buf_size = 0;

  if (player->segment_is_open == DVR_FALSE) {
    ret = _change_to_next_segment((DVR_PlaybackHandle_t)player);
  }

  if (ret != DVR_SUCCESS) {
    if (buf != NULL) {
      free(buf);
      buf = NULL;
    }
    DVR_DEBUG(1, "get segment error");
    return NULL;
  }
  //get play statue not here
  _dvr_playback_sent_transition_ok((DVR_PlaybackHandle_t)player);
  _dvr_check_cur_segment_flag((DVR_PlaybackHandle_t)player);

  int trick_stat = 0;
  while (player->is_running/* || player->cmd.last_cmd != player->cmd.cur_cmd*/) {

    //check trick stat
    pthread_mutex_lock(&player->lock);

    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_SEEK ||
      player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
      player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB ||player->speed > 1 ||player->speed <= -1 )
    {
      trick_stat = _dvr_playback_get_trick_stat((DVR_PlaybackHandle_t)player);
      if (trick_stat > 0) {
        DVR_DEBUG(1, "trick stat[%d] is > 0", trick_stat);
        if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_SEEK || (player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
          //check last cmd
          if(player->cmd.last_cmd == DVR_PLAYBACK_CMD_PAUSE
            || (player->play_flag == DVR_PLAYBACK_STARTED_PAUSEDLIVE
                && ( player->cmd.cur_cmd == DVR_PLAYBACK_CMD_START
                ||player->cmd.last_cmd == DVR_PLAYBACK_CMD_VSTART
                    || player->cmd.last_cmd == DVR_PLAYBACK_CMD_ASTART
                    || player->cmd.last_cmd == DVR_PLAYBACK_CMD_START))) {
            DVR_DEBUG(1, "pause play-------");
            //need change to pause state
            player->cmd.cur_cmd = DVR_PLAYBACK_CMD_PAUSE;
            player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
            //clear flag
            player->play_flag = 0;
            first_frame = 0;
            AmTsPlayer_pauseVideoDecoding(player->handle);
            AmTsPlayer_pauseAudioDecoding(player->handle);
            //playback_device_pause(player->handle);
          }
        } else if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF
                || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB
                ||player->speed > 1 ||player->speed < -1) {
            //restart play stream if speed > 2
            if (player->state == DVR_PLAYBACK_STATE_PAUSE ||
              _dvr_time_getClock() < player->next_fffb_time) {
              DVR_DEBUG(1, "fffb timeout----speed[%d] fffb cur[%d] cur sys[%d]", player->speed, player->fffb_current,_dvr_time_getClock());
              //used timeout wait need lock first,so we unlock and lock
              //pthread_mutex_unlock(&player->lock);
              //pthread_mutex_lock(&player->lock);
              _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
              pthread_mutex_unlock(&player->lock);
              continue;
            }
            DVR_DEBUG(1, "fffb play-------speed[%d][%d][%d]", player->speed, goto_rewrite, real_read);
            pthread_mutex_unlock(&player->lock);
            goto_rewrite = DVR_FALSE;
            real_read = 0;
            _dvr_playback_fffb((DVR_PlaybackHandle_t)player);
            pthread_mutex_lock(&player->lock);
        }
      }
    }

    if (player->state == DVR_PLAYBACK_STATE_PAUSE) {
      //pthread_mutex_unlock(&player->lock);
      //pthread_mutex_lock(&player->lock);
      if (0 && player->auto_pause == DVR_TRUE) {
        int end = _dvr_get_end_time((DVR_PlaybackHandle_t)player);
        int cur = _dvr_get_cur_time((DVR_PlaybackHandle_t)player);
        DVR_DEBUG(1, "in  thread cur[%d]end[%d]",cur, end);
        if (end -cur > 6*1000 || _dvr_has_next_segmentId((DVR_PlaybackHandle_t)player, player->cur_segment_id) == DVR_SUCCESS) {
          //resume
          pthread_mutex_unlock(&player->lock);
          player->auto_pause = DVR_FALSE;
          dvr_playback_resume((DVR_PlaybackHandle_t)player);
          pthread_mutex_lock(&player->lock);
        }
      }
      //check is need send time send end
      _dvr_playback_sent_playtime((DVR_PlaybackHandle_t)player);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      continue;
    }
    if (goto_rewrite == DVR_TRUE) {
      goto_rewrite = DVR_FALSE;
      pthread_mutex_unlock(&player->lock);
      //DVR_DEBUG(1, "rewrite-player->speed[%d]", player->speed);
      goto rewrite;
    }
    //.check is need send time send end
    _dvr_playback_sent_playtime((DVR_PlaybackHandle_t)player);

    int read = segment_read(player->r_handle, buf + real_read, buf_len - real_read);
    pthread_mutex_unlock(&player->lock);
    //if on fb mode and read file end , we need calculate pos to retry read.
    if (read == 0 && IS_FB(player->speed) && real_read == 0) {
      DVR_DEBUG(1, "recalculate read [%d] readed [%d]buf_len[%d]speed[%d]id=[%d]", read,real_read, buf_len, player->speed,player->cur_segment_id);
      _dvr_playback_calculate_seekpos((DVR_PlaybackHandle_t)player);
      pthread_mutex_lock(&player->lock);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      continue;
    }
    //DVR_DEBUG(1, "read ts [%d]buf_len[%d]speed[%d]real_read:%d", read, buf_len, player->speed, real_read);
    if (read == 0) {
      //file end.need to play next segment
      int ret = _change_to_next_segment((DVR_PlaybackHandle_t)player);
      //init fffb time if change segment
      _dvr_init_fffb_time((DVR_PlaybackHandle_t)player);
      if (ret != DVR_SUCCESS) {
         if (player->openParams.is_timeshift) {
          //send end event to hal
           DVR_Play_Notify_t notify;
           memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
           notify.event = DVR_PLAYBACK_EVENT_REACHED_END;
           //get play statue not here
           dvr_playback_pause((DVR_PlaybackHandle_t)player, DVR_FALSE);
           player->auto_pause = DVR_TRUE;
           _dvr_playback_sent_event((DVR_PlaybackHandle_t)player, DVR_PLAYBACK_EVENT_REACHED_END, &notify);
           //continue,timeshift mode, when read end,need wait cur recording segment
           DVR_DEBUG(1, "playback is timeshift send end");
           pthread_mutex_lock(&player->lock);
           _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
           pthread_mutex_unlock(&player->lock);
           continue;
         } else {
           //play end
           if (read == 0) {
             DVR_DEBUG(1, "playback reach end segment, exit playbackplayer->cur_segment_id [%lld] [%p]", player->cur_segment_id, (DVR_PlaybackHandle_t)player);
             //send reach end
             DVR_Play_Notify_t notify;
             memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
             notify.event = DVR_PLAYBACK_EVENT_REACHED_END;
             //get play statue not here
             dvr_playback_pause((DVR_PlaybackHandle_t)player, DVR_FALSE);
             player->auto_pause = DVR_TRUE;
             _dvr_playback_sent_event((DVR_PlaybackHandle_t)player, DVR_PLAYBACK_EVENT_REACHED_END, &notify);
             DVR_DEBUG(1, "playback reach end segment, send event end");
             continue;
           } else {
              //has data not inject to dev
              goto inject;
           }
         }
       }
      _dvr_playback_sent_transition_ok((DVR_PlaybackHandle_t)player);
      pthread_mutex_lock(&player->lock);
      _dvr_replay_changed_pid((DVR_PlaybackHandle_t)player);
      _dvr_check_cur_segment_flag((DVR_PlaybackHandle_t)player);
      read = segment_read(player->r_handle, buf + real_read, buf_len - real_read);
      pthread_mutex_unlock(&player->lock);
    }
    real_read = real_read + read;
    if (0 && real_read < buf_len ) {//no used
      if (player->openParams.is_timeshift) {
        //send end event to hal
        DVR_Play_Notify_t notify;
        memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
        notify.event = DVR_PLAYBACK_EVENT_REACHED_END;
        //get play statue not here
        _dvr_playback_sent_event((DVR_PlaybackHandle_t)player, DVR_PLAYBACK_EVENT_REACHED_END, &notify);

        //continue,timeshift mode, when read end,need wait cur recording segment
        DVR_DEBUG(1, "playback is timeshift---");
        pthread_mutex_lock(&player->lock);
        _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
        pthread_mutex_unlock(&player->lock);
        continue;
      }
      //continue to read
      pthread_mutex_lock(&player->lock);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      if (!player->is_running) {
         break;
      }
      DVR_DEBUG(1, "playback continue read");
      continue;
    }
inject:
    wbufs.buf_size = real_read;
rewrite:
    wbufs.buf_data = buf;
    //read data
    //descramble data
    //read data to inject
    if ( AmTsPlayer_writeData(player->handle, &wbufs, write_timeout_ms) == AM_TSPLAYER_OK) {
      real_read = 0;
      continue;
    } else {
      //DVR_DEBUG(1, "write time out");
      pthread_mutex_lock(&player->lock);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      if (!player->is_running) {
        DVR_DEBUG(1, "playback thread exit");
         break;
      }
      goto_rewrite = DVR_TRUE;
      //goto rewrite;
    }
  }
  DVR_DEBUG(1, "playback thread is end");
  return NULL;
}


static int _start_playback_thread(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "start thread------[%d]", player->is_running);
  if (player->is_running == DVR_TRUE) {
      return 0;
  }
  player->is_running = DVR_TRUE;
  int rc = pthread_create(&player->playback_thread, NULL, _dvr_playback_thread, (void*)player);
  if (rc < 0)
    player->is_running = DVR_FALSE;
  DVR_DEBUG(1, "start thread------[%d] end", player->is_running);
  return 0;
}


static int _stop_playback_thread(DVR_PlaybackHandle_t handle)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "stopthread------[%d]", player->is_running);
  if (player->is_running == DVR_TRUE)
  {
    player->is_running = DVR_FALSE;
    _dvr_playback_sendSignal(handle);
    //pthread_cond_signal(&player->cond);
    pthread_join(player->playback_thread, NULL);
  }
  if (player->r_handle) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }
  DVR_DEBUG(1, "stopthread------[%d]", player->is_running);
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

  player = (DVR_Playback_t*)malloc(sizeof(DVR_Playback_t));

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
  player->speed = 1;

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
    DVR_DEBUG(1, "playback open get callback[%p][%p][%p][%p]",player->player_callback_func, player->player_callback_userdata, _dvr_tsplayer_callback_test,player);
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
  //auto pause init
  player->auto_pause = DVR_FALSE;
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

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "func: %s, will resume", __func__);
  if (player->state != DVR_PLAYBACK_STATE_STOP)
  {
    dvr_playback_stop(handle, DVR_TRUE);
  }
  AmTsPlayer_resumeVideoDecoding(player->handle);
  AmTsPlayer_resumeAudioDecoding(player->handle);
  pthread_mutex_destroy(&player->lock);
  pthread_cond_destroy(&player->cond);

  if (player) {
    free(player);
    player = NULL;
  }
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
  uint64_t segment_id = player->cur_segment_id;
  DVR_DEBUG(1, "[%s][%p]segment_id:[%lld]",__func__, handle, segment_id);

  int sync = DVR_PLAYBACK_SYNC;
  player->auto_pause = DVR_FALSE;
  //can used start api to resume playback
  if (player->cmd.state == DVR_PLAYBACK_STATE_PAUSE) {
    return dvr_playback_resume(handle);
  }
  if (player->cmd.state == DVR_PLAYBACK_STATE_START) {
    DVR_DEBUG(1, "stat is start, not need into start play");
    return DVR_SUCCESS;
  }
  player->play_flag = flag;
  //get segment info and audio video pid fmt ;
  DVR_DEBUG(1, "lock func: %s %p", __func__, handle);
  pthread_mutex_lock(&player->lock);
  //resume player when start play
//  AmTsPlayer_resumeVideoDecoding(player->handle);
//  AmTsPlayer_resumeAudioDecoding(player->handle);
  _dvr_playback_get_playinfo(handle, segment_id, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio abnd video pis is all invalid, return error.
    DVR_DEBUG(0, "dvr play back start error, not found audio and video info");
    DVR_DEBUG(1, "unlock func: %s", __func__);
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
  int timeout = 0;//6000
  //time out wait green area
  while (timeout) {
    int cur = _dvr_get_cur_time(handle);
    int end = _dvr_get_end_time(handle);
    DVR_DEBUG(1, "func: %s  cur: %d end %d wait green area", __func__, cur, end);
    if (end - cur < 5000 && !IS_FB(player->speed)) {
      //pthread_mutex_unlock(&player->lock);
      //pthread_mutex_lock(&player->lock);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, 1000);
      //not unlock here. at end wil unlock
    } else {
      break;
    }
    timeout = timeout - 1000;
  }
  {
    if (VALID_PID(vparams.pid)) {
      player->has_video = DVR_TRUE;
      //if set flag is pause live, we need set trick mode
      if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE ||
        player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB
        || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF) {
        DVR_DEBUG(1, "set trick mode ---at pause live");
        AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
      } else {
        DVR_DEBUG(1, "set trick mode ---none");
        AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_NONE);
      }
      AmTsPlayer_setVideoParams(player->handle,  &vparams);
      AmTsPlayer_startVideoDecoding(player->handle);
    }
    if (VALID_PID(aparams.pid)) {
      player->has_audio = DVR_TRUE;
      AmTsPlayer_setAudioParams(player->handle,  &aparams);
      AmTsPlayer_startAudioDecoding(player->handle);
    }
    DVR_DEBUG(1, "player->cmd.cur_cmd:%d", player->cmd.cur_cmd);
    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB
      || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF) {
      player->cmd.state = DVR_PLAYBACK_STATE_START;
      player->state = DVR_PLAYBACK_STATE_START;
    } else {
      player->cmd.last_cmd = player->cmd.cur_cmd;
      player->cmd.cur_cmd = DVR_PLAYBACK_CMD_START;
      player->cmd.speed.speed.speed = PLAYBACK_SPEED_X1;
      player->cmd.state = DVR_PLAYBACK_STATE_START;
      player->state = DVR_PLAYBACK_STATE_START;
    }
  }
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "[%s][%p]",__func__, handle);

  DVR_DEBUG(1, "add segment id: %lld %p", info->segment_id, handle);
  DVR_PlaybackSegmentInfo_t *segment;

  segment = malloc(sizeof(DVR_PlaybackSegmentInfo_t));
  memset(segment, 0, sizeof(DVR_PlaybackSegmentInfo_t));

  //not memcpy chun info.
  segment->segment_id = info->segment_id;
  //cp location
  memcpy(segment->location, info->location, DVR_MAX_LOCATION_SIZE);

  DVR_DEBUG(1, "add location [%s]id[%lld]flag[%x]", segment->location, segment->segment_id, info->flags);
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

  DVR_DEBUG(1, "lock func: %s pid [0x%x][0x%x][0x%x][0x%x]", __func__,segment->pids.video.pid,segment->pids.audio.pid, info->pids.video.pid,info->pids.audio.pid);
  pthread_mutex_lock(&player->lock);
  list_add_tail(&segment->head, &player->segment_list);
  pthread_mutex_unlock(&player->lock);
  DVR_DEBUG(1, "unlock func: %s", __func__);

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
  DVR_DEBUG(1, "remove segment id: %lld", segment_id);
  if (segment_id == player->cur_segment_id) {
    DVR_DEBUG(1, "not suport remove curren segment id: %lld", segment_id);
    return DVR_FAILURE;
  }
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  DVR_PlaybackSegmentInfo_t *segment;
  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment->segment_id == segment_id) {
      list_del(&segment->head);
      free(segment);
      break;
    }
  }
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "update segment id: %lld flag:%d", segment_id, flags);

  DVR_PlaybackSegmentInfo_t *segment;
  DVR_DEBUG(1, "lock func: %s", __func__);
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
        //playback_device_mute_audio(player->handle, 1);
        //playback_device_mute_video(player->handle, 1);
      } else if ((segment->flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0 &&
          (flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE) {
        //enable display, unmute
        AmTsPlayer_showVideo(player->handle);
        AmTsPlayer_setAudioMute(player->handle, 0, 0);
        //playback_device_mute_audio(player->handle, 0);
        //playback_device_mute_video(player->handle, 0);
      } else {
        //do nothing
      }
    } else {
      //do nothing
    }
    //continue , only set flag
    segment->flags = flags;
  }
    DVR_DEBUG(1, "unlock func: %s", __func__);
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}


static int _do_check_pid_info(DVR_PlaybackHandle_t handle, DVR_StreamInfo_t  now_pid, DVR_StreamInfo_t set_pid, int type) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "do check pid %p", handle);
  DVR_DEBUG(1, "[%s][%p]",__func__, handle);

  if (now_pid.pid == set_pid.pid) {
    //do nothing
    return 0;
  } else if (player->cmd.state == DVR_PLAYBACK_STATE_START) {
    if (VALID_PID(now_pid.pid)) {
      //stop now stream
      if (type == 0) {
        //stop vieo
        AmTsPlayer_stopVideoDecoding(player->handle);
        player->has_video = DVR_FALSE;
      } else if (type == 1) {
        //stop audio
        AmTsPlayer_stopAudioDecoding(player->handle);
        player->has_audio = DVR_FALSE;
      } else if (type == 2) {
        //stop sub audio
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
        AmTsPlayer_setVideoParams(player->handle,  &vparams);
        AmTsPlayer_startVideoDecoding(player->handle);
        //playback_device_video_start(player->handle,&vparams);
      } else if (type == 1) {
        //start audio
        am_tsplayer_audio_params aparams;
        aparams.pid = set_pid.pid;
        aparams.codectype= _dvr_convert_stream_fmt(set_pid.format, DVR_TRUE);
        player->has_audio = DVR_TRUE;
        AmTsPlayer_setAudioParams(player->handle,  &aparams);
        AmTsPlayer_startAudioDecoding(player->handle);
        //playback_device_audio_start(player->handle,&aparams);
      } else if (type == 2) {
        am_tsplayer_audio_params aparams;
        aparams.pid = set_pid.pid;
        aparams.codectype= _dvr_convert_stream_fmt(set_pid.format, DVR_TRUE);
        player->has_audio = DVR_TRUE;
        AmTsPlayer_setAudioParams(player->handle,  &aparams);
        AmTsPlayer_startAudioDecoding(player->handle);
        //playback_device_audio_start(player->handle,&aparams);
      } else if (type == 3) {
        //pcr
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
  DVR_DEBUG(1, "update segment id: %lld", segment_id);
  DVR_DEBUG(1, "[%s][%p]",__func__, handle);

  DVR_PlaybackSegmentInfo_t *segment;
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  DVR_DEBUG(1, "get lock [%p] update segment id: %lld cur id %lld",handle, segment_id, player->cur_segment_id);

  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment->segment_id == segment_id) {

      if (player->cur_segment_id == segment_id) {
        if (player->cmd.state == DVR_PLAYBACK_STATE_FF
          || player->cmd.state == DVR_PLAYBACK_STATE_FF) {
          //do nothing when ff fb
          DVR_DEBUG(1, "now is ff fb, not to update cur segment info\r\n");
          DVR_DEBUG(1, "unlock func: %s", __func__);
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
      }
      //save pids info
      memcpy(&segment->pids, p_pids, sizeof(DVR_PlaybackPids_t));
      break;
    }
  }
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "---into-stop-----");
  DVR_DEBUG(1, "lock func: %s", __func__);
  _stop_playback_thread(handle);
  pthread_mutex_lock(&player->lock);
  DVR_DEBUG(1, "---into-stop---1--");
  AmTsPlayer_resumeVideoDecoding(player->handle);
  AmTsPlayer_resumeAudioDecoding(player->handle);
  AmTsPlayer_showVideo(player->handle);
  AmTsPlayer_stopVideoDecoding(player->handle);
  AmTsPlayer_stopAudioDecoding(player->handle);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_STOP;
  player->cmd.state = DVR_PLAYBACK_STATE_STOP;
  player->state = DVR_PLAYBACK_STATE_STOP;
  player->cur_segment_id = UINT64_MAX;
  player->segment_is_open = DVR_FALSE;
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "into func: %s", __func__);
  DVR_DEBUG(1, "[%s][%p]",__func__, handle);
  _start_playback_thread(handle);
  //start audio and video
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  player->has_audio = DVR_TRUE;
  AmTsPlayer_setAudioParams(player->handle, param);
  AmTsPlayer_startAudioDecoding(player->handle);
  //playback_device_audio_start(player->handle , param);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTART;
  player->cmd.state = DVR_PLAYBACK_STATE_START;
  player->state = DVR_PLAYBACK_STATE_START;
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);

  //playback_device_audio_stop(player->handle);
  if (player->has_video == DVR_FALSE) {
    player->cmd.state = DVR_PLAYBACK_STATE_STOP;
    player->state = DVR_PLAYBACK_STATE_STOP;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.video is playing
  }
  player->has_audio = DVR_FALSE;

  AmTsPlayer_stopAudioDecoding(player->handle);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTOP;

  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  _start_playback_thread(handle);
  //start audio and video
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  player->has_video = DVR_TRUE;
  AmTsPlayer_setAudioParams(player->handle, param);
  AmTsPlayer_startAudioDecoding(player->handle);

  //playback_device_video_start(player->handle , param);
  //if set flag is pause live, we need set trick mode
  if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
    DVR_DEBUG(1, "settrick mode at video start");
    AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
    //playback_device_trick_mode(player->handle, 1);
  }
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTART;
  player->cmd.state = DVR_PLAYBACK_STATE_START;
  player->state = DVR_PLAYBACK_STATE_START;
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);

  if (player->has_audio == DVR_FALSE) {
    player->cmd.state = DVR_PLAYBACK_STATE_STOP;
    player->state = DVR_PLAYBACK_STATE_STOP;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.audio is playing
  }
  player->has_video = DVR_FALSE;

  AmTsPlayer_stopVideoDecoding(player->handle);
  //playback_device_video_stop(player->handle);

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTOP;

  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  DVR_DEBUG(1, "get lock func: %s", __func__);
  AmTsPlayer_pauseVideoDecoding(player->handle);
  AmTsPlayer_pauseAudioDecoding(player->handle);

  //playback_device_pause(player->handle);
  if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
    player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
    player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
    player->state = DVR_PLAYBACK_STATE_PAUSE;
    player->auto_pause = DVR_FALSE;
  } else {
    player->cmd.last_cmd = player->cmd.cur_cmd;
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_PAUSE;
    player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
    player->state = DVR_PLAYBACK_STATE_PAUSE;
    player->auto_pause = DVR_FALSE;
  }
  pthread_mutex_unlock(&player->lock);
  DVR_DEBUG(1, "unlock func: %s", __func__);

  return DVR_SUCCESS;
}

//not add lock
static int _dvr_cmd(DVR_PlaybackHandle_t handle, int cmd)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  //get video params and audio params
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  am_tsplayer_video_params vparams;
  am_tsplayer_audio_params aparams;
  uint64_t segmentid = player->cur_segment_id;

  _dvr_playback_get_playinfo(handle, segmentid, &vparams, &aparams);
  DVR_DEBUG(1, "unlock func: %s cmd: %d", __func__, cmd);
  pthread_mutex_unlock(&player->lock);

  switch (cmd) {
    case DVR_PLAYBACK_CMD_AVRESTART:
      //av restart
      DVR_DEBUG(1, "do_cmd avrestart");
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

  if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_PAUSE) {
    DVR_DEBUG(1, "lock func: %s", __func__);
    pthread_mutex_lock(&player->lock);
    AmTsPlayer_resumeVideoDecoding(player->handle);
    AmTsPlayer_resumeAudioDecoding(player->handle);
    //playback_device_resume(player->handle);
    player->auto_pause = DVR_FALSE;
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
    DVR_DEBUG(1, "unlock func: %s", __func__);
    pthread_mutex_unlock(&player->lock);
  } else {
        player->auto_pause = DVR_FALSE;
    //player->cmd.last_cmd = DVR_PLAYBACK_CMD_RESUME;
    AmTsPlayer_resumeVideoDecoding(player->handle);
    AmTsPlayer_resumeAudioDecoding(player->handle);
    DVR_DEBUG(1, "func: %s, set start state", __func__);
    player->cmd.state = DVR_PLAYBACK_STATE_START;
    player->state = DVR_PLAYBACK_STATE_START;
    _dvr_cmd(handle, player->cmd.cur_cmd);
  }
  return DVR_SUCCESS;
}

/**\brief seek
 * \param[in] handle playback handle
 * \param[in] time_offset time offset base cur segment
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_seek(DVR_PlaybackHandle_t handle, uint64_t segment_id, uint32_t time_offset) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "lock func: %s segment_id %llu cur id %d time_offset %u cur end: %d", __func__, segment_id,player->cur_segment_id, (uint32_t)time_offset, _dvr_get_end_time(handle));
  DVR_DEBUG(1, "[%s][%p]---player->state[%d]----",__func__, handle, player->state);
  pthread_mutex_lock(&player->lock);
  DVR_DEBUG(1, "[%s][%p]---player->state[%d]---get lock-",__func__, handle, player->state);

  int offset = -1;
  //open segment if id is not current segment
  int ret = _dvr_open_segment(handle, segment_id);
  if (ret ==DVR_FAILURE) {
    DVR_DEBUG(1, "seek error at open segment");
    pthread_mutex_unlock(&player->lock);
    return DVR_FAILURE;
  }
  if (time_offset >_dvr_get_end_time(handle) &&_dvr_has_next_segmentId(handle, segment_id) == DVR_FAILURE) {
    if (segment_ongoing(player->r_handle) == DVR_SUCCESS) {
      DVR_DEBUG(1, "is ongoing segment when seek end, need return success");
      //pthread_mutex_unlock(&player->lock);
      //return DVR_SUCCESS;
      time_offset = _dvr_get_end_time(handle);
    } else {
      DVR_DEBUG(1, "is not ongoing segment when seek end, return failure");
      pthread_mutex_unlock(&player->lock);
      return DVR_FAILURE;
    }
  }

  DVR_DEBUG(1, "seek open id[%lld]flag[0x%x] time_offset %u", player->cur_segment.segment_id, player->cur_segment.flags, time_offset);
  //get file offset by time
  if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
    //forward playback.not seek end of file
    if (time_offset != 0 && time_offset > FB_DEFAULT_LEFT_TIME) {
      //default -2000ms
      time_offset = time_offset -FB_DEFAULT_LEFT_TIME;
    }
  }
  pthread_mutex_lock(&player->segment_lock);
  offset = segment_seek(player->r_handle, (uint64_t)time_offset);
  DVR_DEBUG(0, "---seek get offset by time offset, offset=%d time_offset %u",offset, time_offset);
  pthread_mutex_unlock(&player->segment_lock);
  player->offset = offset;

  _dvr_get_end_time(handle);
  //init fffb time
  player->fffb_current = _dvr_time_getClock();
  player->fffb_start = player->fffb_current;
  player->fffb_start_pcr = _dvr_get_cur_time(handle);
  player->next_fffb_time = player->fffb_current;

  if (player->state == DVR_PLAYBACK_STATE_STOP || player->state == DVR_PLAYBACK_STATE_PAUSE) {
    //only seek file,not start
    DVR_DEBUG(1, "unlock func: %s", __func__);
    pthread_mutex_unlock(&player->lock);
    return DVR_SUCCESS;
  }
  //stop play
  DVR_DEBUG(0, "seek stop play, not inject data has video[%d]audio[%d]", player->has_video, player->has_audio);
  if (player->has_video)
    AmTsPlayer_stopVideoDecoding(player->handle);
  if (player->has_audio)
    AmTsPlayer_stopAudioDecoding(player->handle);
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
    DVR_DEBUG(0, "seek start dvr play back start error, not found audio and video info");
    DVR_DEBUG(1, "unlock func: %s", __func__);
    pthread_mutex_unlock(&player->lock);
    return -1;
  }
  //add
  if (sync == DVR_PLAYBACK_SYNC) {
    if (VALID_PID(vparams.pid)) {
      //player->has_video;
      if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_PAUSE ||
        player->speed > 1||
        player->speed <= -1) {
        //if is pause state. we need set trick mode.
        DVR_DEBUG(1, "[%s]seek set trick mode player->speed [%d]", __func__, player->speed);
        AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
      }
      AmTsPlayer_setVideoParams(player->handle, &vparams);
      AmTsPlayer_startVideoDecoding(player->handle);
      //playback_device_video_start(player->handle , &vparams);
    }
    if (VALID_PID(aparams.pid)) {
      AmTsPlayer_setAudioParams(player->handle, &aparams);
      AmTsPlayer_startAudioDecoding(player->handle);
      //playback_device_audio_start(player->handle , &aparams);
    }
  }
  if (player->state == DVR_PLAYBACK_STATE_PAUSE &&
    ((player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
     player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) ||
    (player->cmd.last_cmd == DVR_PLAYBACK_CMD_FF ||
     player->cmd.last_cmd == DVR_PLAYBACK_CMD_FB))) {
    player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
    player->state = DVR_PLAYBACK_STATE_PAUSE;
  } else if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
    player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF ||
    player->speed > 1||
    player->speed <= -1) {
    DVR_DEBUG(1, "not set cmd to seek");
    //not pause state, we need not set cur cmd
  } else {
    DVR_DEBUG(1, "set cmd to seek");
    player->cmd.last_cmd = player->cmd.cur_cmd;
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_SEEK;
    player->cmd.state = DVR_PLAYBACK_STATE_START;
    player->state = DVR_PLAYBACK_STATE_START;
  }
  DVR_DEBUG(1, "unlock func: %s ---", __func__);
  pthread_mutex_unlock(&player->lock);

  return DVR_SUCCESS;
}

//get current segment current pcr time of read pos
static int _dvr_get_cur_time(DVR_PlaybackHandle_t handle) {
  //get cur time of segment
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  int cache = 500;//defalut es buf cache 500ms
  //AmTsPlayer_getDelayTime(player->handle);
  pthread_mutex_lock(&player->segment_lock);
  uint64_t cur = segment_tell_current_time(player->r_handle);
  pthread_mutex_unlock(&player->segment_lock);
  DVR_DEBUG(1, "get cur time [%lld]", cur);
  if (player->state == DVR_PLAYBACK_STATE_STOP) {
    cache = 0;
  }
  if (IS_FB(player->speed)) {
    //if is fb ,we need - had write data
    cache = -1000;
  }
  return (int)(cur > cache ? cur - cache : cur);
}

//get current segment current pcr time of read pos
static int _dvr_get_end_time(DVR_PlaybackHandle_t handle) {
  //get cur time of segment
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  pthread_mutex_lock(&player->segment_lock);
  uint64_t end = segment_tell_total_time(player->r_handle);
  DVR_DEBUG(1, "get tatal time [%lld]", end);
  pthread_mutex_unlock(&player->segment_lock);
  return (int)end;
}

#define FB_MIX_SEEK_TIME 1000
//start replay
static int _dvr_playback_calculate_seekpos(DVR_PlaybackHandle_t handle) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //calculate pcr seek time
  int t_diff = 0;
  int seek_time = 0;
  if (player->fffb_start == -1) {
    //set fffb start time ms
    player->fffb_start = _dvr_time_getClock();
    player->fffb_current = player->fffb_start;
    //get segment current time pos
    player->fffb_start_pcr = _dvr_get_cur_time(handle);
    DVR_DEBUG(1, "calculate seek posplayer->fffb_start_pcr[%d]ms, speed[%d]", player->fffb_start_pcr, player->speed);
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
      segment_seek(player->r_handle, seek_time);
      pthread_mutex_unlock(&player->segment_lock);
    } else {
      //
      DVR_DEBUG(1, "segment not open,can not seek");
    }
    DVR_DEBUG(1, "calculate seek pos seek_time[%d]ms, speed[%d]id[%lld]cur [%d]", seek_time, player->speed,player->cur_segment_id,  _dvr_get_cur_time(handle));
   }
  return seek_time;
}


//start replay
static int _dvr_playback_fffb_replay(DVR_PlaybackHandle_t handle) {
  //
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //stop
  if (player->has_video) {
    DVR_DEBUG(1, "fffb stop video");
    AmTsPlayer_stopVideoDecoding(player->handle);
  }
  if (player->has_audio) {
    DVR_DEBUG(1, "fffb stop audio");
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
    DVR_DEBUG(0, "dvr play back restart error, not found audio and video info");
    //pthread_mutex_unlock(&player->lock);
    return -1;
  }

  if (VALID_PID(vparams.pid)) {
    player->has_video = DVR_TRUE;
    DVR_DEBUG(1, "fffb start video");
    AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
    AmTsPlayer_setVideoParams(player->handle, &vparams);
    AmTsPlayer_startVideoDecoding(player->handle);
    //playback_device_video_start(player->handle , &vparams);
    //if set flag is pause live, we need set trick mode
    //playback_device_trick_mode(player->handle, 1);
  }
  if (VALID_PID(aparams.pid)) {
    player->has_audio = DVR_TRUE;
    DVR_DEBUG(1, "fffb start audio");
    AmTsPlayer_setAudioParams(player->handle, &aparams);
    AmTsPlayer_startAudioDecoding(player->handle);
    //playback_device_audio_start(player->handle , &aparams);
  }

  //pthread_mutex_unlock(&player->lock);
  return 0;
}

static int _dvr_playback_fffb(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  first_frame = 0;
  DVR_DEBUG(1, "lock func: %s  speed [%d]", __func__, player->speed);
  pthread_mutex_lock(&player->lock);
  DVR_DEBUG(1, "get lock func: %s  speed [%d]id [%lld]", __func__, player->speed, player->cur_segment_id);

  int seek_time = _dvr_playback_calculate_seekpos(handle);
  if (_dvr_has_next_segmentId(handle, player->cur_segment_id) == DVR_FAILURE && seek_time < FB_MIX_SEEK_TIME && IS_FB(player->speed)) {
      //seek time set 0
      seek_time = 0;
  }
  if (seek_time == 0 && IS_FB(player->speed)) {
    //for fb cmd, we need open pre segment.if reach first one segment, send begin event
    int ret = _change_to_next_segment((DVR_PlaybackHandle_t)player);
    if (ret != DVR_SUCCESS) {
      pthread_mutex_unlock(&player->lock);
      dvr_playback_pause(handle, DVR_FALSE);
      //send event here and pause
      DVR_Play_Notify_t notify;
      memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
      notify.event = DVR_PLAYBACK_EVENT_REACHED_BEGIN;
      //get play statue not here
      _dvr_playback_sent_event(handle, DVR_PLAYBACK_EVENT_REACHED_BEGIN, &notify);
      DVR_DEBUG(1, "*******************send begin event func: %s  speed [%d] cur [%d]", __func__, player->speed, _dvr_get_cur_time(handle));
      //change to pause
      return DVR_SUCCESS;
    }
    _dvr_playback_sent_transition_ok(handle);
    _dvr_init_fffb_time(handle);
    DVR_DEBUG(1, "*******************send trans ok event func: %s  speed [%d]", __func__, player->speed);
  }
  player->next_fffb_time =_dvr_time_getClock() + FFFB_SLEEP_TIME;
  _dvr_playback_fffb_replay(handle);

  pthread_mutex_unlock(&player->lock);
  DVR_DEBUG(1, "unlock func: %s", __func__);

  return DVR_SUCCESS;
}

//start replay, need get lock at extern
static int _dvr_playback_replay(DVR_PlaybackHandle_t handle, DVR_Bool_t trick) {
  //
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //stop
  if (player->has_video) {
    AmTsPlayer_stopVideoDecoding(player->handle);
    //playback_device_video_stop(player->handle);
  }

  if (player->has_audio) {
    AmTsPlayer_stopAudioDecoding(player->handle);
    //playback_device_audio_stop(player->handle);
  }
  //start video and audio

  am_tsplayer_video_params    vparams;
  am_tsplayer_audio_params    aparams;
  uint64_t segment_id = player->cur_segment_id;

  //get segment info and audio video pid fmt ;
  DVR_DEBUG(1, "lock func: %s", __func__);
  _dvr_playback_get_playinfo(handle, segment_id, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio and video pis is all invalid, return error.
    DVR_DEBUG(0, "dvr play back restart error, not found audio and video info");
    DVR_DEBUG(1, "unlock func: %s", __func__);
    return -1;
  }

  if (VALID_PID(vparams.pid)) {
    player->has_video = DVR_TRUE;
    if (trick == DVR_TRUE) {
      DVR_DEBUG(1, "settrick mode at replay");
      AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_PAUSE_NEXT);
    }
    else
      AmTsPlayer_setTrickMode(player->handle, AV_VIDEO_TRICK_MODE_NONE);
    AmTsPlayer_setVideoParams(player->handle, &vparams);
    AmTsPlayer_startVideoDecoding(player->handle);
    //playback_device_video_start(player->handle , &vparams);
    //if set flag is pause live, we need set trick mode
    //playback_device_trick_mode(player->handle, 1);
  }
  if (VALID_PID(aparams.pid)) {
    player->has_audio = DVR_TRUE;
    AmTsPlayer_startAudioDecoding(player->handle);
    AmTsPlayer_setAudioParams(player->handle, &aparams);
    //playback_device_audio_start(player->handle , &aparams);
  }
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_START;
  player->cmd.speed.speed.speed = PLAYBACK_SPEED_X1;
  player->speed = PLAYBACK_SPEED_X1/100;
  player->cmd.state = DVR_PLAYBACK_STATE_START;
  player->state = DVR_PLAYBACK_STATE_START;
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "lock func: %s speed [%d]", __func__, speed);
  pthread_mutex_lock(&player->lock);
  if (player->cmd.cur_cmd != DVR_PLAYBACK_CMD_FF
    && player->cmd.cur_cmd != DVR_PLAYBACK_CMD_FB) {
    player->cmd.last_cmd = player->cmd.cur_cmd;
  }
  //only set mode and speed info, we will deal ff fb at playback thread.
  if ((speed.speed.speed == PLAYBACK_SPEED_X1 )) {
    //we think x1 and s2 is normal speed. is not ff fb.
    if (speed.speed.speed == PLAYBACK_SPEED_X1) {
      //if last speed is x2 or s2, we need stop fast
      //AmTsPlayer_stopFast(player->handle);
    }
  } else {
      if (speed.mode == DVR_PLAYBACK_FAST_FORWARD)
        player->cmd.cur_cmd = DVR_PLAYBACK_CMD_FF;
      else
        player->cmd.cur_cmd = DVR_PLAYBACK_CMD_FB;
  }
  player->cmd.speed.mode = speed.mode;
  player->cmd.speed.speed = speed.speed;
  player->speed = speed.speed.speed/100;
  if (speed.speed.speed == PLAYBACK_SPEED_X1 &&
    (player->state == DVR_PLAYBACK_STATE_FB ||
    player->state == DVR_PLAYBACK_STATE_FF)) {
    //restart play at normal speed exit ff fb
    DVR_DEBUG(1, "set speed normal and replay playback");
    _dvr_playback_replay(handle, DVR_FALSE);
  } else if (speed.speed.speed == PLAYBACK_SPEED_X1 &&
    (player->state == DVR_PLAYBACK_STATE_PAUSE)) {
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_AVRESTART;
    DVR_DEBUG(1, "set speed normal at pause state ,set cur cmd");
  }
  //need exit ff fb
  DVR_DEBUG(1, "unlock func: %s speed[%d]cmd[%d]", __func__, player->speed, player->cmd.cur_cmd);
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
  //pthread_mutex_lock(&player->lock);
  //DVR_DEBUG(1, "lock func: %s", __func__);
  DVR_DEBUG(1, "get stat start id[%lld]", player->cur_segment_id);

  p_status->state = player->state;
  if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE &&
    player->state == DVR_PLAYBACK_STATE_START) {
    p_status->state = DVR_PLAYBACK_STATE_PAUSE;
  }
  p_status->segment_id = player->cur_segment_id;
  p_status->time_end = _dvr_get_end_time(handle);
  p_status->time_cur = _dvr_get_cur_time(handle);

  memcpy(&p_status->pids, &player->cur_segment.pids, sizeof(DVR_PlaybackPids_t));
  p_status->speed = player->cmd.speed.speed.speed;
  p_status->flags = player->cur_segment.flags;
  //pthread_mutex_unlock(&player->lock);
  //DVR_DEBUG(1, "unlock func: %s", __func__);
  DVR_DEBUG(1, "get stat end [%d][%d] id[%lld]", p_status->time_cur, p_status->time_end, p_status->segment_id);
  return DVR_SUCCESS;
}

void _dvr_dump_segment(DVR_PlaybackSegmentInfo_t *segment) {
  if (segment != NULL) {
    DVR_DEBUG(1, "segment id: %lld", segment->segment_id);
    DVR_DEBUG(1, "segment flag: %d", segment->flags);
    DVR_DEBUG(1, "segment location: [%s]", segment->location);
    DVR_DEBUG(1, "segment vpid: 0x%x vfmt:0x%x", segment->pids.video.pid,segment->pids.video.format);
    DVR_DEBUG(1, "segment apid: 0x%x afmt:0x%x", segment->pids.audio.pid,segment->pids.audio.format);
    DVR_DEBUG(1, "segment pcr pid: 0x%x pcr fmt:0x%x", segment->pids.pcr.pid,segment->pids.pcr.format);
    DVR_DEBUG(1, "segment sub apid: 0x%x sub afmt:0x%x", segment->pids.ad.pid,segment->pids.ad.format);
  }
}

int dvr_dump_segmentinfo(DVR_PlaybackHandle_t handle, uint64_t segment_id) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

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
