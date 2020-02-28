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

#include <amports/amstream.h>

#include "dvr_playback.h"

#define VALID_PID(_pid_) ((_pid_)>0 && (_pid_)<0x1fff)

#define FFFB_SLEEP_TIME    400

//
static int _dvr_playback_fffb(DVR_PlaybackHandle_t handle);
static int _do_check_pid_info(DVR_PlaybackHandle_t handle, DVR_StreamInfo_t  now_pid, DVR_StreamInfo_t set_pid, int type);
static int _dvr_get_cur_time(DVR_PlaybackHandle_t handle);
static int _dvr_get_end_time(DVR_PlaybackHandle_t handle);

//convert video and audio fmt
static int _dvr_convert_stream_fmt(int fmt, DVR_Bool_t is_audio) {
  int format = 0;
  if (is_audio == DVR_FALSE) {
    //for video fmt
    switch (fmt)
    {
        case DVR_VIDEO_FORMAT_MPEG1:
          format = VFORMAT_MPEG12;
          break;
        case DVR_VIDEO_FORMAT_MPEG2:
          format = VFORMAT_MPEG12;
          break;
        case DVR_VIDEO_FORMAT_HEVC:
          format = VFORMAT_HEVC;
          break;
        case DVR_VIDEO_FORMAT_H264:
          format = VFORMAT_H264;
          break;
    }
  } else {
    //for audio fmt
    switch (fmt)
    {
        case DVR_AUDIO_FORMAT_MPEG:
          format = AFORMAT_MPEG;
          break;
        case DVR_AUDIO_FORMAT_AC3:
          format = AFORMAT_AC3;
          break;
        case DVR_AUDIO_FORMAT_EAC3:
          format = AFORMAT_EAC3;
          break;
        case DVR_AUDIO_FORMAT_DTS:
          format = AFORMAT_DTS;
          break;
    }
  }
  return format;
}
static int _dvr_playback_get_trick_stat(DVR_PlaybackHandle_t handle)
{
  int state;
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  if (player->handle == NULL)
    return -1;

  state = playback_device_get_trick_stat(handle);

  return state;
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
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //pthread_cond_signal(&player->cond);
  return 0;
}

//send playback event
static int _dvr_playback_sent_event(DVR_PlaybackHandle_t handle, DVR_PlaybackEvent_t evt, DVR_Play_Notify_t *notify) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "into send event -----");
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
      break;
    case DVR_PLAYBACK_EVENT_REACHED_END:
      //GET STATE
      DVR_DEBUG(1, "reached end---");
      dvr_playback_get_status(handle, &(notify->play_status));
      break;
    default:
      break;
  }
  if (player->openParams.event_fn != NULL)
      player->openParams.event_fn(evt, (void*)notify, player->openParams.event_userdata);
  DVR_DEBUG(1, "into send event --end---");
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

//check is ongoing segment
static int _dvr_check_segment_ongoing(DVR_PlaybackHandle_t handle) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  Segment_StoreInfo_t info;
  int ret = DVR_FALSE;
  DVR_DEBUG(1, "into check ongoing---");
  if (player->r_handle != NULL)
    ret = segment_load_info(player->r_handle, &info);

  if (ret != DVR_SUCCESS) {
    DVR_DEBUG(1, "is not ongoing chunk--end---");
     return DVR_FALSE;
  }
  DVR_DEBUG(1, "is ongoing chunk--end---");
  return DVR_TRUE;
}
//get next segment id
static int _dvr_get_next_segmentId(DVR_PlaybackHandle_t handle) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;

  int found = 0;

  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (player->segment_is_open == DVR_FALSE) {
      //get first segment from list
      found = 1;
    } else if (segment->segment_id == player->cur_segment_id) {
      //find cur segment, we need get next one
      found = 1;
      continue;
    }
    if (found == 1) {
      //save segment info
      player->last_segment_id = player->cur_segment_id;
      player->last_segment.segment_id = player->cur_segment.flags;
      player->last_segment.flags = segment->flags;
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
    }
  }
  if (found != 2) {
    //list is null or reache list  end
    return -1;
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
  ret = segment_open(&params, &(player->r_handle));
  Segment_StoreInfo_t info;
  if (segment_load_info(player->r_handle, &info) == DVR_SUCCESS) {
    player->openParams.is_timeshift = DVR_FALSE;
  } else {
    player->openParams.is_timeshift = DVR_TRUE;
  }
  player->dur = info.duration;
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
    return 0;
  }
  uint64_t id = segment_id;
  if (id < 0) {
    DVR_DEBUG(1, "not found segment info");
    return DVR_FAILURE;
  }
  DVR_DEBUG(1, "start found segment[%lld][%lld] info", id,segment_id);

  if (player->r_handle != NULL) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }

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
    return DVR_FAILURE;
  }
  memset(params.location, 0, DVR_MAX_LOCATION_SIZE);
  //cp cur segment path to location
  strncpy(params.location, player->cur_segment.location, strlen(player->cur_segment.location));
  params.segment_id = (uint64_t)player->cur_segment.segment_id;
  params.mode = SEGMENT_MODE_READ;
  DVR_DEBUG(1, "open segment location[%s][%lld]cur flag[0x%x]", params.location, params.segment_id, player->cur_segment.flags);
  ret = segment_open(&params, &(player->r_handle));
  Segment_StoreInfo_t info;
  if (segment_load_info(player->r_handle, &info) == DVR_SUCCESS) {
    player->openParams.is_timeshift = DVR_FALSE;
  } else {
    player->openParams.is_timeshift = DVR_TRUE;
  }
  player->dur = info.duration;

  DVR_DEBUG(1, "player->dur [%d]cur id [%lld]cur flag [0x%x]\r\n", player->dur,player->cur_segment.segment_id, player->cur_segment.flags);
  return ret;
}


//get play info by segment id
static int _dvr_playback_get_playinfo(DVR_PlaybackHandle_t handle,
  uint64_t segment_id,
  Playback_DeviceVideoParams_t *vparam,
  Playback_DeviceAudioParams_t *aparam) {

  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_PlaybackSegmentInfo_t *segment;

  int found = 0;

  list_for_each_entry(segment, &player->segment_list, head)
  {
    if (segment_id == 0LL) {
      //get first segment from list
      found = 1;
    }
    if (segment->segment_id == segment_id) {
      found = 1;
    }
    if (found == 1) {
      //get segment info
      if (player->cur_segment_id > 0)
        player->cur_segment_id = segment->segment_id;
      DVR_DEBUG(1, "get play info id [%lld]", player->cur_segment_id);
      player->cur_segment.segment_id = segment->segment_id;
      player->cur_segment.flags = segment->flags;
      //pids
      memcpy(&player->cur_segment.pids, &segment->pids, sizeof(DVR_PlaybackPids_t));
      //
      vparam->fmt = _dvr_convert_stream_fmt(segment->pids.video.format, DVR_FALSE);
      vparam->pid = segment->pids.video.pid;
      aparam->fmt = _dvr_convert_stream_fmt(segment->pids.audio.format, DVR_TRUE);
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
  DVR_DEBUG(1, "call %s flag[0x%x]id[%lld]", __func__, player->cur_segment.flags, player->cur_segment.segment_id);
  if ((player->cur_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE) {
    //disable display
    DVR_DEBUG(1, "call %s unmute", __func__);
    playback_device_mute_audio(player->handle, 0);
    playback_device_mute_video(player->handle, 0);
  } else if ((player->cur_segment.flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0) {
    //enable display
    DVR_DEBUG(1, "call %s mute", __func__);
    playback_device_mute_audio(player->handle, 1);
    playback_device_mute_video(player->handle, 1);
  }
  return DVR_SUCCESS;
}
static void* _dvr_playback_thread(void *arg)
{
  DVR_Playback_t *player = (DVR_Playback_t *) arg;
  //int need_open_segment = 1;
  Playback_DeviceWBufs_t     wbufs;
  int ret = DVR_SUCCESS;

  int timeout = 50;//ms
  uint8_t *buf = NULL;
  int buf_len = player->openParams.block_size > 0 ? player->openParams.block_size : (256 * 1024);
  int real_read = 0;
  int real_write = 0;

  buf = malloc(buf_len);
  wbufs.flag = 0;
  wbufs.timeout = 10;
  wbufs.len = 0;

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
    trick_stat = _dvr_playback_get_trick_stat((DVR_PlaybackHandle_t)player);

    if (trick_stat > 0)
    {
      if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_SEEK) {
        //check last cmd
        if(player->cmd.last_cmd == DVR_PLAYBACK_CMD_PAUSE
          || (player->play_flag == DVR_PLAYBACK_STARTED_PAUSEDLIVE
              && (player->cmd.last_cmd == DVR_PLAYBACK_CMD_VSTART
                  || player->cmd.last_cmd == DVR_PLAYBACK_CMD_ASTART
                  || player->cmd.last_cmd == DVR_PLAYBACK_CMD_START))) {

          //need change to pause state
          player->cmd.cur_cmd = DVR_PLAYBACK_CMD_PAUSE;
          player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
          playback_device_pause(player->handle);
        }
      } else if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF
              || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB) {
          //restart play stream if speed > 2
          pthread_mutex_unlock(&player->lock);
          _dvr_playback_fffb((DVR_PlaybackHandle_t)player);
          pthread_mutex_lock(&player->lock);
       }
    }

    int write = 0;
    int read = segment_read(player->r_handle, buf + real_read, buf_len - real_read);
    pthread_mutex_unlock(&player->lock);

    //DVR_DEBUG(1, "read ts [%d]buf_len[%d]", read, buf_len);
    if (read == 0) {
    //file end.need to play next segment
      int ret = _change_to_next_segment((DVR_PlaybackHandle_t)player);
      if (ret != DVR_SUCCESS) {
         if (player->openParams.is_timeshift) {
          //send end event to hal
           DVR_Play_Notify_t notify;
           memset(&notify, 0 , sizeof(DVR_Play_Notify_t));
           notify.event = DVR_PLAYBACK_EVENT_REACHED_END;
           //get play statue not here
           _dvr_playback_sent_event((DVR_PlaybackHandle_t)player, DVR_PLAYBACK_EVENT_REACHED_END, &notify);

           //continue,timeshift mode, when read end,need wait cur recording segment
           DVR_DEBUG(1, "playback is timeshift");
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
      pthread_mutex_lock(&player->lock);

    }
    real_read = real_read + read;
    if (real_read < buf_len ) {
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
    wbufs.len = real_read;
    real_write = 0;
rewrite:
    wbufs.buf = buf + real_write;
    //read data
    //descramble data
    //read data to inject
    //DVR_DEBUG(1, "playback dev write---");
    write = playback_device_write(player->handle, &wbufs);
    real_write = real_write + write;
    wbufs.len = real_read - real_write;
    //check if left data
    if (real_write == real_read) {
      //this block write end
      real_read = 0;
      real_write = 0;
      continue;
    } else {
      pthread_mutex_lock(&player->lock);
      _dvr_playback_timeoutwait((DVR_PlaybackHandle_t)player, timeout);
      pthread_mutex_unlock(&player->lock);
      if (!player->is_running) {
         break;
      }
      goto rewrite;
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
    //pthread_cond_signal(&player->cond);
    pthread_join(player->playback_thread, NULL);
  }
  if (player->r_handle) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }
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
  player->cmd.pos = 0;
  //store open params
  player->openParams.dmx_dev_id = params->dmx_dev_id;
  player->openParams.block_size = params->block_size;
  player->openParams.is_timeshift = params->is_timeshift;
  player->openParams.event_fn = params->event_fn;
  player->openParams.event_userdata = params->event_userdata;

  player->has_pids = params->has_pids;

  player->handle = params->playback_handle;

  //init has audio and video
  player->has_video = DVR_FALSE;
  player->has_audio = DVR_FALSE;
  player->cur_segment_id = 0LL;
  player->last_segment_id = 0LL;
  player->segment_is_open = DVR_FALSE;

  //init ff fb time
  player->fffb_current = -1;
  player->fffb_start =-1;
  player->fffb_start_pcr = -1;
  //seek time
  player->seek_time = 0;
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
  DVR_DEBUG(1, "func: %s", __func__);
  if (player->state != DVR_PLAYBACK_STATE_STOP)
  {
    dvr_playback_stop(handle, DVR_TRUE);
  }
  playback_device_close(player->handle);
  //device close
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
  Playback_DeviceVideoParams_t    vparams;
  Playback_DeviceAudioParams_t    aparams;
  uint64_t segment_id = player->cur_segment_id;
  DVR_DEBUG(1, "[%s][%p]segment_id:[%lld]",__func__, handle, segment_id);

  int sync = DVR_PLAYBACK_SYNC;
  //can used start api to resume playback
  if (player->cmd.state == DVR_PLAYBACK_STATE_PAUSE) {
    return dvr_playback_resume(handle);
  }
  player->play_flag = flag;
  //get segment info and audio video pid fmt ;
  DVR_DEBUG(1, "lock func: %s %p", __func__, handle);
  pthread_mutex_lock(&player->lock);
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

  {
    if (VALID_PID(vparams.pid)) {
      player->has_video = DVR_TRUE;
      playback_device_video_start(player->handle , &vparams);
      //if set flag is pause live, we need set trick mode
      if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
        playback_device_trick_mode(player->handle, 1);
      }
    }
    if (VALID_PID(aparams.pid)) {
      player->has_audio = DVR_TRUE;
      playback_device_audio_start(player->handle , &aparams);
    }
    DVR_DEBUG(1, "player->cmd.cur_cmd:%d", player->cmd.cur_cmd);
    if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FB
      || player->cmd.cur_cmd == DVR_PLAYBACK_CMD_FF) {
      player->cmd.state = DVR_PLAYBACK_STATE_START;
      player->state = DVR_PLAYBACK_STATE_START;
      if (player->has_video == DVR_TRUE)
        playback_device_trick_mode(player->handle, 1);
      DVR_DEBUG(1, "set trick mode ---at start");
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
  if (player->has_pids == DVR_TRUE)
    memcpy(&segment->pids, &info->pids, sizeof(DVR_PlaybackPids_t));

  segment->pids.video.pid = info->pids.video.pid;
  segment->pids.video.format = info->pids.video.format;
  segment->pids.video.type = info->pids.video.type;

  segment->pids.audio.pid = info->pids.video.pid;
  segment->pids.audio.format = info->pids.video.format;
  segment->pids.audio.type = info->pids.video.type;

  segment->pids.ad.pid = info->pids.video.pid;
  segment->pids.ad.format = info->pids.video.format;
  segment->pids.ad.type = info->pids.video.type;

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
        playback_device_mute_audio(player->handle, 1);
        playback_device_mute_video(player->handle, 1);
      } else if ((segment->flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == 0 &&
          (flags & DVR_PLAYBACK_SEGMENT_DISPLAYABLE) == DVR_PLAYBACK_SEGMENT_DISPLAYABLE) {
        //enable display, unmute
        playback_device_mute_audio(player->handle, 0);
        playback_device_mute_video(player->handle, 0);
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
        playback_device_video_stop(player->handle);
        player->has_video = DVR_FALSE;
      } else if (type == 1) {
        //stop audio
        playback_device_audio_stop(player->handle);
        player->has_audio = DVR_FALSE;
      } else if (type == 2) {
        //stop sub audio
        playback_device_audio_stop(player->handle);
      } else if (type == 3) {
        //pcr
      }
    }
    if (VALID_PID(set_pid.pid)) {
      //start
      if (type == 0) {
        //start vieo
        Playback_DeviceVideoParams_t vparams;
        vparams.pid = set_pid.pid;
        vparams.fmt = _dvr_convert_stream_fmt(set_pid.format, DVR_FALSE);
        player->has_video = DVR_TRUE;
        playback_device_video_start(player->handle,&vparams);
      } else if (type == 1) {
        //start audio
        Playback_DeviceAudioParams_t aparams;
        aparams.pid = set_pid.pid;
        aparams.fmt = _dvr_convert_stream_fmt(set_pid.format, DVR_TRUE);
        player->has_audio = DVR_TRUE;
        playback_device_audio_start(player->handle,&aparams);
      } else if (type == 2) {
        //start sub audio
        Playback_DeviceAudioParams_t aparams;
        aparams.pid = set_pid.pid;
        aparams.fmt = _dvr_convert_stream_fmt(set_pid.format, DVR_TRUE);
        player->has_audio = DVR_TRUE;
        playback_device_audio_start(player->handle, &aparams);
      } else if (type == 3) {
        //pcr
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
  pthread_mutex_lock(&player->lock);
  DVR_DEBUG(1, "---into-stop---1--");

  playback_device_video_stop(player->handle);
  playback_device_audio_stop(player->handle);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_STOP;
  player->cmd.state = DVR_PLAYBACK_STATE_STOP;
  player->state = DVR_PLAYBACK_STATE_STOP;
  DVR_DEBUG(1, "unlock func: %s", __func__);
  pthread_mutex_unlock(&player->lock);
  //destory thread
  _stop_playback_thread(handle);
  return DVR_SUCCESS;
}
/**\brief Start play audio
 * \param[in] handle playback handle
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_audio_start(DVR_PlaybackHandle_t handle, Playback_DeviceAudioParams_t *param) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  DVR_DEBUG(1, "into func: %s", __func__);
  DVR_DEBUG(1, "[%s][%p]",__func__, handle);
  _start_playback_thread(handle);
  //start audio and video
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  player->has_audio = DVR_TRUE;
  playback_device_audio_start(player->handle , param);
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
  player->has_audio = DVR_FALSE;
  playback_device_audio_stop(player->handle);

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_ASTOP;

  if (player->has_video == DVR_FALSE) {
    player->cmd.state = DVR_PLAYBACK_STATE_STOP;
    player->state = DVR_PLAYBACK_STATE_STOP;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.video is playing
  }
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
int dvr_playback_video_start(DVR_PlaybackHandle_t handle, Playback_DeviceVideoParams_t *param) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  _start_playback_thread(handle);
  //start audio and video
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  player->has_video = DVR_TRUE;
  playback_device_video_start(player->handle , param);
  //if set flag is pause live, we need set trick mode
  if ((player->play_flag&DVR_PLAYBACK_STARTED_PAUSEDLIVE) == DVR_PLAYBACK_STARTED_PAUSEDLIVE) {
     playback_device_trick_mode(player->handle, 1);
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
  player->has_video = DVR_FALSE;
  playback_device_video_stop(player->handle);

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_VSTOP;

  if (player->has_audio == DVR_FALSE) {
    player->cmd.state = DVR_PLAYBACK_STATE_STOP;
    player->state = DVR_PLAYBACK_STATE_STOP;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.audio is playing
  }
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
  playback_device_pause(player->handle);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_PAUSE;
  player->cmd.state = DVR_PLAYBACK_STATE_PAUSE;
  player->state = DVR_PLAYBACK_STATE_PAUSE;
  DVR_DEBUG(1, "unlock func: %s", __func__);
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}

//not add lock
static int _dvr_cmd(DVR_PlaybackHandle_t handle, int cmd)
{
  DVR_Playback_t *player = (DVR_Playback_t *) handle;

  //get video params and audio params
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  Playback_DeviceVideoParams_t vparams;
  Playback_DeviceAudioParams_t aparams;
  uint64_t segmentid = player->cur_segment_id;

  _dvr_playback_get_playinfo(handle, segmentid, &vparams, &aparams);
  DVR_DEBUG(1, "unlock func: %s", __func__);
  pthread_mutex_unlock(&player->lock);

  switch (cmd) {
    case DVR_PLAYBACK_CMD_AVRESTART:
      //av restart
      dvr_playback_video_stop(player->handle);
      dvr_playback_audio_stop(player->handle);
      dvr_playback_video_start(player->handle, &vparams);
      dvr_playback_audio_start(player->handle, &aparams);
      break;
    case DVR_PLAYBACK_CMD_VRESTART:
      dvr_playback_video_stop(player->handle);
      dvr_playback_video_start(player->handle, &vparams);
      break;
    case DVR_PLAYBACK_CMD_VSTART:
      dvr_playback_video_start(player->handle, &vparams);
      break;
    case DVR_PLAYBACK_CMD_VSTOP:
      dvr_playback_video_stop(player->handle);
      break;
    case DVR_PLAYBACK_CMD_ARESTART:
      //a restart
      dvr_playback_audio_stop(player->handle);
      dvr_playback_audio_start(player->handle, &aparams);
      break;
    case DVR_PLAYBACK_CMD_ASTART:
      dvr_playback_audio_start(player->handle, &aparams);
      break;
    case DVR_PLAYBACK_CMD_ASTOP:
      dvr_playback_audio_stop(player->handle);
      break;
    case  DVR_PLAYBACK_CMD_ASTOPVRESTART:
      dvr_playback_audio_stop(player->handle);
      dvr_playback_video_stop(player->handle);
      dvr_playback_video_start(player->handle, &vparams);
      break;
    case DVR_PLAYBACK_CMD_ASTOPVSTART:
      dvr_playback_audio_stop(player->handle);
      dvr_playback_video_start(player->handle, &vparams);
      break;
    case DVR_PLAYBACK_CMD_VSTOPARESTART:
      dvr_playback_video_stop(player->handle);
      dvr_playback_audio_stop(player->handle);
      dvr_playback_audio_start(player->handle, &aparams);
      break;
    case DVR_PLAYBACK_CMD_STOP:
      break;
    case DVR_PLAYBACK_CMD_START:
      break;
    case DVR_PLAYBACK_CMD_ASTARTVRESTART:
      dvr_playback_video_stop(player->handle);
      dvr_playback_video_start(player->handle, &vparams);
      dvr_playback_audio_start(player->handle, &aparams);
      break;
    case DVR_PLAYBACK_CMD_VSTARTARESTART:
      dvr_playback_audio_stop(player->handle);
      dvr_playback_video_start(player->handle, &vparams);
      dvr_playback_audio_start(player->handle, &aparams);
      break;
    case DVR_PLAYBACK_CMD_FF:
    case DVR_PLAYBACK_CMD_FB:
      _dvr_playback_fffb(player->handle);
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
    playback_device_resume(player->handle);
    player->cmd.last_cmd = player->cmd.cur_cmd;
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_RESUME;
    player->cmd.state = DVR_PLAYBACK_STATE_START;
    player->state = DVR_PLAYBACK_STATE_START;
    DVR_DEBUG(1, "unlock func: %s", __func__);
    pthread_mutex_unlock(&player->lock);
  } else {
    //player->cmd.last_cmd = DVR_PLAYBACK_CMD_RESUME;
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
  DVR_DEBUG(1, "lock func: %s segment_id %llu  time_offset %u", __func__, segment_id, (uint32_t)time_offset);
  DVR_DEBUG(1, "[%s][%p]---player->state[%d]----",__func__, handle, player->state);
  pthread_mutex_lock(&player->lock);
  _dvr_get_cur_time(handle);
  _dvr_get_end_time(handle);

  int offset = -1;
  //open segment if id is not current segment
  _dvr_open_segment(handle, segment_id);
  DVR_DEBUG(1, "seek open id[%lld]flag[0x%x] time_offset %u", player->cur_segment.segment_id, player->cur_segment.flags, time_offset);
  //get file offset by time
  offset = segment_seek(player->r_handle, (uint64_t)time_offset);
  DVR_DEBUG(0, "seek get offset by time offset, offset=%d time_offset %u",offset, time_offset);
  //seek file
  if (offset == -1) {
    //seek 2M data once for test
    //seek error
    offset = player->offset + (2*1014*1024);
  }
  player->offset = offset;

  if (player->state == DVR_PLAYBACK_STATE_STOP) {
    //only seek file,not start
    DVR_DEBUG(1, "unlock func: %s", __func__);
    pthread_mutex_unlock(&player->lock);
    return 0;
  }
  //stop play
  DVR_DEBUG(0, "seek stop play, not inject data");
  if (player->has_video)
    playback_device_video_stop(player->handle);
  if (player->has_audio)
    playback_device_audio_stop(player->handle);
  //start play
  Playback_DeviceVideoParams_t    vparams;
  Playback_DeviceAudioParams_t    aparams;

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
      playback_device_video_start(player->handle , &vparams);
      if (player->cmd.cur_cmd == DVR_PLAYBACK_CMD_PAUSE) {
        //if is pause state. we need set trick mode.
        DVR_DEBUG(1, "seek set trick mode", __func__);
        playback_device_trick_mode(player->handle, 1);
      }
    }
    if (VALID_PID(aparams.pid)) {
      //player->has_video;
      playback_device_audio_start(player->handle , &aparams);
    }
  }

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PLAYBACK_CMD_SEEK;
  player->cmd.state = DVR_PLAYBACK_STATE_START;
  player->state = DVR_PLAYBACK_STATE_START;
  DVR_DEBUG(1, "unlock func: %s ---", __func__);
  pthread_mutex_unlock(&player->lock);

  return DVR_SUCCESS;
}

//get current segment current pcr time of read pos
static int _dvr_get_cur_time(DVR_PlaybackHandle_t handle) {
  //get cur time of segment
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  int cache = 500;//defalut es buf cache 500ms
  uint32_t cur = segment_tell_current_time(player->r_handle);
  return cur > cache ? cur - cache : cur;
}

//get current segment current pcr time of read pos
static int _dvr_get_end_time(DVR_PlaybackHandle_t handle) {
  //get cur time of segment
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //if (player->openParams.is_timeshift == DVR_TRUE) {
    //get dur real time
    return segment_tell_total_time(player->r_handle);
  //}
  //return player->dur;
}

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
    t_diff = 0;
  } else {
    player->fffb_current = _dvr_time_getClock();
    t_diff = player->fffb_current - player->fffb_start;
    seek_time = player->fffb_start_pcr + t_diff *player->speed;
    //seek segment pos
    if (player->r_handle) {
      segment_seek(player->r_handle, seek_time);
    } else {
      //
      DVR_DEBUG(1, "segment not open,can not seek");
    }
   }
  return 0;
}


//start replay
static int _dvr_playback_fffb_replay(DVR_PlaybackHandle_t handle) {
  //
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //stop
  if (player->has_video)
    playback_device_video_stop(player->handle);
  if (player->has_audio)
    playback_device_audio_stop(player->handle);
  //start video and audio

  Playback_DeviceVideoParams_t    vparams;
  Playback_DeviceAudioParams_t    aparams;
  uint64_t segment_id = 0;

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
    playback_device_video_start(player->handle , &vparams);
    //if set flag is pause live, we need set trick mode
    playback_device_trick_mode(player->handle, 1);
  }
  if (VALID_PID(aparams.pid)) {
    player->has_audio = DVR_TRUE;
    playback_device_audio_start(player->handle , &aparams);
  }

  //pthread_mutex_unlock(&player->lock);
  return 0;
}

static int _dvr_playback_fffb(DVR_PlaybackHandle_t handle) {
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  if (_dvr_time_getClock() < player->fffb_current + FFFB_SLEEP_TIME)
    return 0;
    DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);

  _dvr_playback_calculate_seekpos(handle);
  _dvr_playback_fffb_replay(handle);
  DVR_DEBUG(1, "unlock func: %s", __func__);

  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}

//start replay
static int _dvr_playback_replay(DVR_PlaybackHandle_t handle) {
  //
  DVR_Playback_t *player = (DVR_Playback_t *) handle;
  //stop
  if (player->has_video)
    playback_device_video_stop(player->handle);
  if (player->has_audio)
    playback_device_audio_stop(player->handle);
  //start video and audio

  Playback_DeviceVideoParams_t    vparams;
  Playback_DeviceAudioParams_t    aparams;
  uint64_t segment_id = 0LL;

  //get segment info and audio video pid fmt ;
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  _dvr_playback_get_playinfo(handle, segment_id, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio abnd video pis is all invalid, return error.
    DVR_DEBUG(0, "dvr play back restart error, not found audio and video info");
    DVR_DEBUG(1, "unlock func: %s", __func__);
    pthread_mutex_unlock(&player->lock);
    return -1;
  }

  if (VALID_PID(vparams.pid)) {
    player->has_video = DVR_TRUE;
    playback_device_video_start(player->handle , &vparams);
    //if set flag is pause live, we need set trick mode
    playback_device_trick_mode(player->handle, 1);
  }
  if (VALID_PID(aparams.pid)) {
    player->has_audio = DVR_TRUE;
    playback_device_audio_start(player->handle , &aparams);
  }
  DVR_DEBUG(1, "unlock func: %s", __func__);

  pthread_mutex_unlock(&player->lock);
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
  DVR_DEBUG(1, "lock func: %s", __func__);
  pthread_mutex_lock(&player->lock);
  if (player->cmd.cur_cmd != DVR_PLAYBACK_CMD_FF
    && player->cmd.cur_cmd != DVR_PLAYBACK_CMD_FB) {
    player->cmd.last_cmd = player->cmd.cur_cmd;
  }
  //only set mode and speed info, we will deal ff fb at playback thread.
  player->cmd.speed.mode = speed.mode;
  player->cmd.speed.speed = speed.speed;
  if (speed.mode == DVR_PLAYBACK_FAST_FORWARD)
    player->speed = speed.speed.speed/100;
   else
    player->speed = -speed.speed.speed/100;

  if (speed.mode == DVR_PLAYBACK_FAST_FORWARD)
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_FF;
  else
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_FB;
  //need exit ff fb
  if (speed.speed.speed == PLAYBACK_SPEED_X1) {
    player->cmd.cur_cmd = DVR_PLAYBACK_CMD_AVRESTART;
  }
  DVR_DEBUG(1, "unlock func: %s", __func__);
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
  DVR_DEBUG(1, "get stat start [%p]", handle);
  //pthread_mutex_lock(&player->lock);
  //DVR_DEBUG(1, "lock func: %s", __func__);
  DVR_DEBUG(1, "get stat start id[%lld]", player->cur_segment_id);

  p_status->state = player->state;
  p_status->segment_id = player->cur_segment_id;
  p_status->time_cur = _dvr_get_cur_time(handle);
  p_status->time_end = _dvr_get_end_time(handle);
  memcpy(&p_status->pids, &player->cur_segment.pids, sizeof(DVR_PlaybackPids_t));
  p_status->speed = player->cmd.speed.speed.speed;
  p_status->flags = player->cur_segment.flags;
  //pthread_mutex_unlock(&player->lock);
  //DVR_DEBUG(1, "unlock func: %s", __func__);
  DVR_DEBUG(1, "get stat start end [%d][%d] id[%lld]", p_status->time_cur, p_status->time_end, p_status->segment_id);
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
