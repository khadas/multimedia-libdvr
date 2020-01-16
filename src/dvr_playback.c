#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>


#include "dvr_common.h"

#include "dvr_playback.h"

#define VALID_PID(_pid_) ((_pid_)>0 && (_pid_)<0x1fff)

static int _dvr_playback_get_trick_stat(DVR_Playback_Handle_t handle)
{
  int state;
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

  if (player->handle == -1)
    return -1;

  state = playback_device_get_trick_stat(handle);

  return state;
}


//timeout wait sibnal
static int _dvr_playback_timeoutwait(DVR_Playback_Handle_t handle , int ms)
{
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

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
static int _dvr_playback_sendSignal(DVR_Playback_Handle_t handle)
{
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  pthread_cond_signal(&player->cond);
  return 0;
}

static int _dvr_get_next_chunkid(DVR_Playback_Handle_t handle) {

  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  DVR_Playback_Chunk_Info_t *chunk;

  int found = 0;

  list_for_each_entry(chunk, &player->chunk_list, head)
  {
    if (player->cur_chunkid == -1) {
      //get first chunk from list
      found = 1;
    } else if (chunk->chunk_id == player->cur_chunkid) {
      //find cur chunk, we need get next one
      found = 1;
      continue;
    }
    if (found == 1) {
      //get chunk info
      player->cur_chunkid = chunk->chunk_id;
      player->cur_chunk.chunk_id = chunk->chunk_id;
      player->cur_chunk.flags = chunk->flags;
      //cp location
      memcpy(player->cur_chunk.location, chunk->location, DVR_MAX_LOCATION_SIZE);
      //pids
      memcpy(&player->cur_chunk.pids, &chunk->pids, sizeof(DVR_Playback_Pids_t));
      found = 2;
    }
  }
  if (found != 2) {
    //list is null or reache list  end
    return -1;
  }
  return DVR_SUCCESS;
}
//open next chunk to play,if reach list end return errro.
static int _change_to_next_chunk(DVR_Playback_Handle_t handle)
{
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  Segment_OpenParams_t  params;
  int ret = DVR_SUCCESS;

  int id = _dvr_get_next_chunkid(handle);
  if (id < 0) {
    DVR_DEBUG(1, "not found chunk info");
    return -1;
  }

  if (player->r_handle != NULL) {
    segment_close(player->r_handle);
    player->r_handle = NULL;
  }

  memset(params.location, 0, DVR_MAX_LOCATION_SIZE);
  //cp chur chunk path to location
  strncpy(params.location, player->cur_chunk.location, strlen(player->cur_chunk.location));
  params.segment_id = (uint64_t)player->cur_chunk.chunk_id;
  params.mode = SEGMENT_MODE_READ;
  ret = segment_open(&params, &(player->r_handle));
  return ret;
}

static void* _dvr_playback_thread(void *arg)
{
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) arg;
  int need_open_chunk = 1;
  Playback_Device_WBufs_t     wbufs;

  int timeout = 10;//ms
  uint8_t *buf = NULL;
  int buf_len = player->openParams.blocksize > 0 ? player->openParams.blocksize : (256 * 1024);
  int real_read = 0;
  int real_write = 0;


  buf = malloc(buf_len);
  wbufs.flag = 0;
  wbufs.timeout = 10;
  wbufs.len = 0;

  int ret = _change_to_next_chunk((DVR_Playback_Handle_t)player);
  if (ret != DVR_SUCCESS) {
    if (buf != NULL) {
      free(buf);
      buf = NULL;
    }
    return NULL;
  }
  int trick_stat = 0;
  while (player->is_running/* || player->cmd.last_cmd != player->cmd.cur_cmd*/) {

    //check trick stat
    pthread_mutex_lock(&player->lock);
    trick_stat = _dvr_playback_get_trick_stat((DVR_Playback_Handle_t)player);
    if (trick_stat > 0)
    {
      if (player->cmd.cur_cmd == DVR_PlayBack_Cmd_Seek) {
        //check last cmd
        if(player->cmd.last_cmd == DVR_PlayBack_Cmd_Pause
          || (player->play_flag == DVR_PLAY_STARTED_PAUSEDLIVE
              && (player->cmd.last_cmd == DVR_PlayBack_Cmd_VStart
                  || player->cmd.last_cmd == DVR_PlayBack_Cmd_AStart
                  || player->cmd.last_cmd == DVR_PlayBack_Cmd_Start))) {

          //need change to pause state
          player->cmd.cur_cmd = DVR_PlayBack_Cmd_Pause;
          player->cmd.state = DVR_PlayBack_State_Pause;
          playback_device_pause(player->handle);
        }
      } else if (player->cmd.cur_cmd == DVR_PlayBack_Cmd_FF
              || player->cmd.cur_cmd == DVR_PlayBack_Cmd_FB) {

          //restart play stream if speed > 2
      }
    }
    pthread_mutex_unlock(&player->lock);

    int write = 0;
    int read = segment_read(player->r_handle, buf + real_read, buf_len - real_read);
    if (read == 0) {
      //file end.need to play next chunk
      int ret = _change_to_next_chunk((DVR_Playback_Handle_t)player);
      if (ret != DVR_SUCCESS) {
         if (player->openParams.is_timeshift) {
           //continue,timeshift mode, when read end,need wait cur recording chunk
           continue;
         } else {
           //play end
           if (real_read == 0) {
             DVR_DEBUG(1, "playback reach end chunk, exit playback");
             break;
           } else {
              //has data not inject to dev
              goto inject;
           }
         }
       }
      read = segment_read(player->r_handle, buf + real_read, buf_len - real_read);
    }
    real_read = real_read + read;
    if (real_read < buf_len ) {
      //continue to read
      _dvr_playback_timeoutwait((DVR_Playback_Handle_t)player, timeout);
      if (!player->is_running) {
         break;
      }
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
    write = playback_device_write(player->handle, &wbufs);
    real_write = real_write + write;
    wbufs.len = real_read - real_write;
    //check if left data
    if (real_write == real_read) {
      //this block write end
    } else {
      _dvr_playback_timeoutwait((DVR_Playback_Handle_t)player, timeout);
      if (!player->is_running) {
         break;
      }
      goto rewrite;
    }
  }
  return NULL;
}


static int _start_playback_thread(DVR_Playback_Handle_t handle)
{
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  if (player->is_running) {
      return 0;
  }
  int rc = pthread_create(&player->playback_thread, NULL, _dvr_playback_thread, (void*)player);
  if (rc > 0)
    player->is_running = 1;
  return 0;
}


static int _stop_playback_thread(DVR_Playback_Handle_t handle)
{
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  if (player->is_running)
  {
    player->is_running = DVR_FALSE;
    pthread_cond_signal(&player->cond);
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
int dvr_playback_open(DVR_Playback_Handle_t *p_handle, DVR_PlayBack_OpenParams_t *params) {

  Dvr_PlayBack_t *player;
  pthread_condattr_t  cattr;

  player = (Dvr_PlayBack_t*)malloc(sizeof(Dvr_PlayBack_t));

  pthread_mutex_init(&player->lock, NULL);
  pthread_condattr_init(&cattr);
  pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
  pthread_cond_init(&player->cond, &cattr);
  pthread_condattr_destroy(&cattr);

  //init chunk list head
  INIT_LIST_HEAD(&player->chunk_list);
  player->cmd.last_cmd = DVR_PlayBack_Cmd_Stop;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_Stop;
  player->cmd.speed.speed = PlayBack_Speed_X1;
  player->cmd.state = DVR_PlayBack_State_Stop;
  player->cmd.pos = 0;
  //store open params
  player->openParams.dmx = params->dmx;
  player->openParams.blocksize = params->blocksize;
  player->openParams.is_timeshift = params->is_timeshift;
  //init has audio and video
  player->has_video = DVR_FALSE;
  player->has_audio = DVR_FALSE;

  //device open
  playback_device_open(&player->handle, params);
  *p_handle = player;
  return DVR_SUCCESS;
}

/**\brief Close an dvr palyback
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_close(DVR_Playback_Handle_t handle) {

  DVR_ASSERT(handle);

  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

  playback_device_close(player->handle);
  //device close
  pthread_mutex_destroy(&player->lock);
  pthread_cond_destroy(&player->cond);
  free(player);
  return DVR_SUCCESS;
}

int dvr_playback_get_playinfo(int chunkid,
Playback_Device_VideoParams_t *vparam,
Playback_Device_AudioParams_t *aparam) {

    return 0;
}


/**\brief Start play audio and video, used start auido api and start video api
 * \param[in] handle playback handle
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_start(DVR_Playback_Handle_t handle, DVR_Playback_Flag_t flag) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  Playback_Device_VideoParams_t    vparams;
  Playback_Device_AudioParams_t    aparams;
  int chunkid = -1;

  int sync = DVR_PLAY_SYNC;
  player->play_flag = flag;
  //get chunk info and audio video pid fmt ;
  pthread_mutex_lock(&player->lock);
  dvr_playback_get_playinfo(chunkid, &vparams, &aparams);
  _start_playback_thread(handle);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio abnd video pis is all invalid, return error.
    DVR_DEBUG(0, "dvr play back start error, not found audio and video info");
    return -1;
  }

  if (sync == DVR_PLAY_SYNC) {
    if (VALID_PID(vparams.pid)) {
      player->has_video = DVR_TRUE;
      playback_device_video_start(player->handle , &vparams);
      //if set flag is pause live, we need set trick mode
      if (player->play_flag & DVR_PLAY_STARTED_PAUSEDLIVE == DVR_PLAY_STARTED_PAUSEDLIVE) {
        playback_device_trick_mode(player->handle, 1);
      }
    }
    if (VALID_PID(aparams.pid)) {
      player->has_audio = DVR_TRUE;
      playback_device_audio_start(player->handle , &aparams);
    }
    player->cmd.last_cmd = player->cmd.cur_cmd;
    player->cmd.cur_cmd = DVR_PlayBack_Cmd_Start;
    player->cmd.speed.speed = PlayBack_Speed_X1;
    player->cmd.state = DVR_PlayBack_State_Start;
    player->state = DVR_PlayBack_State_Start;
  } else {

    player->cmd.last_cmd = player->cmd.cur_cmd;
    player->cmd.cur_cmd = DVR_PlayBack_Cmd_Start;
    player->cmd.speed.speed =PlayBack_Speed_X1;
    player->cmd.state = DVR_PlayBack_State_Start;
    pthread_cond_signal(&player->cond);
  }
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief dvr play back add chunk info to chunk list
 * \param[in] handle playback handle
 * \param[in] info added chunk info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_add_chunk(DVR_Playback_Handle_t handle, DVR_Playback_Chunk_Info_t *info) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

  DVR_DEBUG(1, "add chunk id: %d", info->chunk_id);
  DVR_Playback_Chunk_Info_t *chunk;

  chunk = malloc(sizeof(DVR_Playback_Chunk_Info_t));
  memset(chunk, 0, sizeof(DVR_Playback_Chunk_Info_t));

  //not memcpy chun info.
  chunk->chunk_id = info->chunk_id;
  //cp location
  memcpy(chunk->location, info->location, DVR_MAX_LOCATION_SIZE);
  //pids
  memcpy(&chunk->pids, &info->pids, sizeof(DVR_Playback_Pids_t));
  chunk->flags = info->flags;
  pthread_mutex_lock(&player->lock);
  list_add_tail(&chunk->head, &player->chunk_list);
  pthread_mutex_unlock(&player->lock);

    return DVR_SUCCESS;
}
/**\brief dvr play back remove chunk info by chunkid
 * \param[in] handle playback handle
 * \param[in] chunkid need removed chunk id
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_remove_chunk(DVR_Playback_Handle_t handle, int chunk_id) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  DVR_DEBUG(1, "remove chunk id: %d", chunk_id);

  pthread_mutex_lock(&player->lock);
  DVR_Playback_Chunk_Info_t *chunk;
  list_for_each_entry(chunk, &player->chunk_list, head)
  {
    if (chunk->chunk_id == chunk_id) {
      list_del(&chunk->head);
      free(chunk);
      break;
    }
  }
  pthread_mutex_unlock(&player->lock);

  return DVR_SUCCESS;
}
/**\brief dvr play back add chunk info
 * \param[in] handle playback handle
 * \param[in] info added chunk info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_Update_Chunk_Flags(DVR_Playback_Handle_t handle,
  int chunk_id, DVR_Playback_Chunk_Flag_t flags) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  DVR_DEBUG(1, "update chunk id: %d flag:%d", chunk_id, flags);

  DVR_Playback_Chunk_Info_t *chunk;
  pthread_mutex_lock(&player->lock);
  list_for_each_entry(chunk, &player->chunk_list, head)
  {
    if (chunk->chunk_id != chunk_id) {
      continue;
    }
    // if encramble to free， only set flag and return;

    //if displayable to none, we need mute audio and video
    if (chunk_id == player->cur_chunkid) {
      if (chunk->flags & DVR_PLAY_DISPLAYABLE == DVR_PLAY_DISPLAYABLE &&
          flags & DVR_PLAY_DISPLAYABLE == 0) {
        //disable display
        playback_device_mute_audio(player->handle, 1);
        playback_device_mute_video(player->handle, 1);
      } else if (chunk->flags & DVR_PLAY_DISPLAYABLE == 0 &&
          flags & DVR_PLAY_DISPLAYABLE == DVR_PLAY_DISPLAYABLE) {
        //enable display
        playback_device_mute_audio(player->handle, 0);
        playback_device_mute_video(player->handle, 0);
      } else {
        //do nothing
      }
    } else {
      //do nothing
    }
    //continue , only set flag
    chunk->flags = flags;
  }
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}


int _do_check_pid_info(DVR_Playback_Handle_t handle, DVR_Playback_StreamParams_t  now_pid, DVR_Playback_StreamParams_t set_pid, int type) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

  if (now_pid.pid == set_pid.pid) {
    //do nothing
    return 0;
  } else {
    if (VALID_PID(now_pid.pid)) {
      //stop now stream
      if (type == 0) {
        //stop vieo
        playback_device_video_stop(player->handle);
      } else if (type == 1) {
        //stop audio
        playback_device_audio_stop(player->handle);
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
        Playback_Device_VideoParams_t vparams;
        vparams.pid = set_pid.pid;
        vparams.fmt = set_pid.fmt;
        dvr_playback_video_start(player->handle,&vparams);
      } else if (type == 1) {
        //start audio
        Playback_Device_AudioParams_t aparams;
        aparams.pid = set_pid.pid;
        aparams.fmt = set_pid.fmt;
        dvr_playback_audio_start(player->handle,&aparams);
      } else if (type == 2) {
        //start sub audio
        Playback_Device_AudioParams_t aparams;
        aparams.pid = set_pid.pid;
        aparams.fmt = set_pid.fmt;
        dvr_playback_audio_start(player->handle, &aparams);
      } else if (type == 3) {
        //pcr
      }
    }
  }
  return 0;
}
/**\brief dvr play back update chunk pids
 * if updated chunk is ongoing chunk, we need start new
 * add pid stream and stop remove pid stream.
 * \param[in] handle playback handle
 * \param[in] chunk_id need updated pids chunk id
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_Update_Chunk_Pids(DVR_Playback_Handle_t handle, int chunk_id, DVR_Playback_Pids_t *p_pids) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  DVR_DEBUG(1, "update chunk id: %d", chunk_id);

  DVR_Playback_Chunk_Info_t *chunk;
  pthread_mutex_lock(&player->lock);
  list_for_each_entry(chunk, &player->chunk_list, head)
  {
    if (chunk->chunk_id == chunk_id) {
      //check video pids, stop or restart
      _do_check_pid_info(handle, chunk->pids.vpid, p_pids->vpid, 0);
      //check audio pids stop or restart
      _do_check_pid_info(handle, chunk->pids.apid, p_pids->apid, 1);
      //check sub audio pids stop or restart
      _do_check_pid_info(handle, chunk->pids.sub_apid, p_pids->sub_apid, 2);
      //check pcr pids stop or restart
      _do_check_pid_info(handle, chunk->pids.pcrpid, p_pids->pcrpid, 3);
      //save pids info
      memcpy(&chunk->pids, p_pids, sizeof(DVR_Playback_Pids_t));
      break;
    }
  }
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Stop play, will stop video and audio
 * \param[in] handle playback handle
 * \param[in] clear is clear last frame
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_stop(DVR_Playback_Handle_t handle, DVR_Bool_t clear) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  pthread_mutex_lock(&player->lock);
  playback_device_video_stop(player->handle);
  playback_device_audio_stop(player->handle);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_Stop;
  player->cmd.state = DVR_PlayBack_State_Stop;
  player->state = DVR_PlayBack_State_Stop;
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
int dvr_playback_audio_start(DVR_Playback_Handle_t handle, Playback_Device_AudioParams_t *param) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  _start_playback_thread(handle);
  //start audio and video
  pthread_mutex_lock(&player->lock);
  player->has_audio = DVR_TRUE;
  playback_device_audio_start(player->handle , &param);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_AStart;
  player->cmd.state = DVR_PlayBack_State_Start;
  player->state = DVR_PlayBack_State_Start;
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Stop play audio
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_audio_stop(DVR_Playback_Handle_t handle) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  pthread_mutex_lock(&player->lock);
  player->has_audio = DVR_FALSE;
  playback_device_audio_stop(player->handle);

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_AStop;

  if (player->has_video == DVR_FALSE) {
    player->cmd.state = DVR_PlayBack_State_Stop;
    player->state = DVR_PlayBack_State_Stop;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.video is playing
  }
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Start play video
 * \param[in] handle playback handle
 * \param[in] params video playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_start(DVR_Playback_Handle_t handle, Playback_Device_VideoParams_t *param) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  _start_playback_thread(handle);
  //start audio and video
  pthread_mutex_lock(&player->lock);
  player->has_video = DVR_TRUE;
  playback_device_video_start(player->handle , param);
  //if set flag is pause live, we need set trick mode
  if (player->play_flag & DVR_PLAY_STARTED_PAUSEDLIVE == DVR_PLAY_STARTED_PAUSEDLIVE) {
     playback_device_trick_mode(player->handle, 1);
  }
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_VStart;
  player->cmd.state = DVR_PlayBack_State_Start;
  player->state = DVR_PlayBack_State_Start;
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Stop play video
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_stop(DVR_Playback_Handle_t handle) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  pthread_mutex_lock(&player->lock);
  player->has_video = DVR_FALSE;
  playback_device_video_stop(player->handle);

  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_VStop;

  if (player->has_audio == DVR_FALSE) {
    player->cmd.state = DVR_PlayBack_State_Stop;
    player->state = DVR_PlayBack_State_Stop;
    //destory thread
    _stop_playback_thread(handle);
  } else {
    //do nothing.audio is playing
  }
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}
/**\brief Pause play
 * \param[in] handle playback handle
 * \param[in] flush whether its internal buffers should be flushed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_pause(DVR_Playback_Handle_t handle, DVR_Bool_t flush) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  pthread_mutex_lock(&player->lock);
  playback_device_pause(player->handle);
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_Pause;
  player->cmd.state = DVR_PlayBack_State_Pause;
  player->state = DVR_PlayBack_State_Pause;
  pthread_mutex_unlock(&player->lock);
  return DVR_SUCCESS;
}

/**\brief seek
 * \param[in] handle playback handle
 * \param[in] time_offset time offset base cur chunk
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_seek(DVR_Playback_Handle_t handle, int chunk_id, int time_offset) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

  pthread_mutex_lock(&player->lock);
  int offset = -1;
  //get file offset by time
  offset = segment_seek(player->r_handle, time_offset);
  DVR_DEBUG(0, "seek get offset by time offset");
  //seek file
  DVR_DEBUG(0, "seek file offset");
  if (offset == -1) {
    //seek 2M data once for test
    //seek error
    offset = player->offset + (2*1014*1024);
  }
  player->offset = offset;

  //stop play
  DVR_DEBUG(0, "seek stop play, not inject data");
  if (player->has_video)
    playback_device_video_stop(player->handle);
  if (player->has_video)
    playback_device_audio_stop(player->handle);
  //start play
  Playback_Device_VideoParams_t    vparams;
  Playback_Device_AudioParams_t    aparams;
  int chunkid = player->cur_chunkid;

  int sync = DVR_PLAY_SYNC;
  //get chunk info and audio video pid fmt ;
  dvr_playback_get_playinfo(chunkid, &vparams, &aparams);
  //start audio and video
  if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
    //audio abnd video pis is all invalid, return error.
    DVR_DEBUG(0, "seek start dvr play back start error, not found audio and video info");
    pthread_mutex_unlock(&player->lock);
    return -1;
  }
  //add
  if (sync == DVR_PLAY_SYNC) {
    if (VALID_PID(vparams.pid)) {
      player->has_video;
      playback_device_video_start(player->handle , &vparams);
    }
    if (VALID_PID(aparams.pid)) {
      player->has_video;
      playback_device_audio_start(player->handle , &aparams);
    }
  }
  player->cmd.last_cmd = player->cmd.cur_cmd;
  player->cmd.cur_cmd = DVR_PlayBack_Cmd_Seek;
  player->cmd.state = DVR_PlayBack_State_Start;
  player->state = DVR_PlayBack_State_Start;
  pthread_mutex_unlock(&player->lock);

  return DVR_SUCCESS;
}
/**\brief Set play speed
 * \param[in] handle playback handle
 * \param[in] speed playback speed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_set_speed(DVR_Playback_Handle_t handle, Playback_Device_Speeds_t speed) {

  return DVR_SUCCESS;
}
/**\brief Get playback status
 * \param[in] handle playback handle
 * \param[out] p_status playback status
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_get_status(DVR_Playback_Handle_t handle,
   DVR_Playback_Status_t *p_status) {
  return DVR_SUCCESS;
}

void _dvr_dump_chunk(DVR_Playback_Chunk_Info_t *chunk) {
  if (chunk != NULL) {
    DVR_DEBUG(1, "chunk id: %d", chunk->chunk_id);
    DVR_DEBUG(1, "chunk flag: %d", chunk->flags);
    DVR_DEBUG(1, "chunk location: [%s]", chunk->location);
    DVR_DEBUG(1, "chunk vpid: 0x%x vfmt:0x%x", chunk->pids.vpid.pid,chunk->pids.vpid.fmt);
    DVR_DEBUG(1, "chunk apid: 0x%x afmt:0x%x", chunk->pids.apid.pid,chunk->pids.apid.fmt);
    DVR_DEBUG(1, "chunk pcr pid: 0x%x pcr fmt:0x%x", chunk->pids.pcrpid.pid,chunk->pids.pcrpid.fmt);
    DVR_DEBUG(1, "chunk sub apid: 0x%x sub afmt:0x%x", chunk->pids.sub_apid.pid,chunk->pids.sub_apid.fmt);
  }
}

int dvr_dump_chunkinfo(DVR_Playback_Handle_t handle, int chunk_id) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

  DVR_Playback_Chunk_Info_t *chunk;
  list_for_each_entry(chunk, &player->chunk_list, head)
  {
    if (chunk_id >= 0) {
      if (chunk->chunk_id == chunk_id) {
        _dvr_dump_chunk(chunk);
       break;
      }
    } else {
      //printf chunk info
      _dvr_dump_chunk(chunk);
    }
  }
  return 0;
}
