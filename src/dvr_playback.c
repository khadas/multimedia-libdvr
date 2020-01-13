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


//timeout wait sibnal
int _dvr_playback_timeedwait(DVR_Playback_Handle_t handle , int ms)
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
int _dvr_playback_sendSignal(DVR_Playback_Handle_t handle)
{
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
    pthread_cond_signal(&player->cond);
    return 0;
}

void* _dvr_playback_thread(void *arg)
{
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) arg;
    int need_open_chunk = 1;
    while (player->is_running/* || player->cmd.last_cmd != player->cmd.cur_cmd*/) {
        //check trick stat
        //playback_device_get_trick_stat();
        //read data to inject
        //read data
        //check is need descramble
        //inject to dev
        //check if left data
    }

    return NULL;
}


int _start_playback_thread(DVR_Playback_Handle_t handle)
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


int _stop_playback_thread(DVR_Playback_Handle_t handle)
{
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

    return 0;
}


/**\brief Open an dvr palyback
 * \param[out] p_handle dvr playback addr
 * \param[in] params dvr playback open parameters
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_open(DVR_Playback_Handle_t *p_handle, DVR_PlayBack_openParams *params) {

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
    player->cmd.speed.speed = DVR_PlayBack_Speed_X1;
    player->cmd.state = DVR_PlayBack_State_Stop;

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
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

    playback_device_close(player->handle);

    return DVR_SUCCESS;
}

int dvr_playback_get_playinfo(int chunkid, Playback_VideoParams *vparam, Playback_AudioParams *aparam) {

    return 0;
}


/**\brief Start play audio and video, used start auido api and start video api
 * \param[in] handle playback handle
 * \param[in] params audio playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_start(DVR_Playback_Handle_t handle, DVR_Play_Flag_t flag) {
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
    Playback_VideoParams    vparams;
    Playback_AudioParams    aparams;
    int chunkid = -1;

    int sync = DVR_PLAY_SYNC;
    player->play_flag = flag;
    //get chunk info and audio video pid fmt ;
    dvr_playback_get_playinfo(chunkid, &vparams, &aparams);
    _start_playback_thread(handle);
    //start audio and video

    if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
        //audio abnd video pis is all invalid, return error.
        DVR_DEBUG(0, "dvr play back start error, not found audio and video info");
        return -1;
    }

    if (sync == DVR_PLAY_SYNC) {
        if (VALID_PID(vparams.pid))
            playback_device_video_start(player->handle , &vparams);
        if (VALID_PID(aparams.pid))
            playback_device_audio_start(player->handle , &aparams);
    } else {
        player->cmd.last_cmd = DVR_PlayBack_Cmd_Stop;
        player->cmd.cur_cmd = DVR_PlayBack_Cmd_Start;
        player->cmd.speed.speed = DVR_PlayBack_Speed_X1;
        player->cmd.state = DVR_PlayBack_State_Start;
        pthread_cond_signal(&player->cond);
    }
    return DVR_SUCCESS;
}
/**\brief dvr play back add chunk info to chunk list
 * \param[in] handle playback handle
 * \param[in] info added chunk info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_add_chunk(DVR_Playback_Handle_t handle, DVR_Play_Chunk_Info_t *info) {
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;

    DVR_DEBUG(1, "add chunk id: %d", info->chunk_id);
    DVR_Play_Chunk_Info_t *chunk;

    chunk = malloc(sizeof(DVR_Play_Chunk_Info_t));
    memset(chunk, 0, sizeof(DVR_Play_Chunk_Info_t));

    //not memcpy chun info.
    chunk->chunk_id = info->chunk_id;
    //cp location
    memcpy(chunk->location, info->location, DVR_MAX_LOCATION_SIZE);
    //pids
    memcpy(&chunk->pids, &info->pids, sizeof(DVR_Play_Pids_t));
    chunk->flags = info->flags;

    list_add_tail(&chunk->head, &player->chunk_list);

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

    DVR_Play_Chunk_Info_t *chunk;
    list_for_each_entry(chunk, &player->chunk_list, head)
    {
        if (chunk->chunk_id == chunk_id) {
            list_del(&chunk->head);
            free(chunk);
            break;
        }
    }
    return DVR_SUCCESS;
}
/**\brief dvr play back add chunk info
 * \param[in] handle playback handle
 * \param[in] info added chunk info,con vpid fmt apid fmt.....
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_Update_Chunk_Flags(DVR_Playback_Handle_t handle,
    int chunk_id, DVR_Play_Chunk_Flag_t flags) {
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
    DVR_DEBUG(1, "update chunk id: %d flag:%d", chunk_id, flags);

    DVR_Play_Chunk_Info_t *chunk;
    list_for_each_entry(chunk, &player->chunk_list, head)
    {
        if (chunk->chunk_id != chunk_id) {
            continue;
        }
        // if encramble to free， only set flag and return;

        //if displayable to none, we need mute audio and video
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
        //continue , only set flag
        chunk->flags = flags;
    }
    return DVR_SUCCESS;
}


int _do_check_pid_info(DVR_Playback_Handle_t handle, DVR_PID_t now_pid, DVR_PID_t set_pid, int type) {
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
                Playback_VideoParams vparams;
                vparams.pid = set_pid.pid;
                vparams.fmt = set_pid.fmt;
                dvr_playback_video_start(player->handle,&vparams);
            } else if (type == 1) {
                //start audio
                Playback_AudioParams aparams;
                aparams.pid = set_pid.pid;
                aparams.fmt = set_pid.fmt;
                dvr_playback_audio_start(player->handle,&aparams);
            } else if (type == 2) {
                //start sub audio
                Playback_AudioParams aparams;
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
int dvr_playback_Update_Chunk_Pids(DVR_Playback_Handle_t handle, int chunk_id, DVR_Play_Pids_t *p_pids) {
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
    DVR_DEBUG(1, "update chunk id: %d", chunk_id);

    DVR_Play_Chunk_Info_t *chunk;
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
            memcpy(&chunk->pids, p_pids, sizeof(DVR_Play_Pids_t));
            break;
        }
    }
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
    playback_device_video_stop(player->handle);
    playback_device_audio_stop(player->handle);
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
int dvr_playback_audio_start(DVR_Playback_Handle_t handle, Playback_AudioParams *param) {
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
    _start_playback_thread(handle);
    //start audio and video
    playback_device_audio_start(player->handle , &param);
    return DVR_SUCCESS;
}
/**\brief Stop play audio
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_audio_stop(DVR_Playback_Handle_t handle) {
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
    playback_device_audio_stop(player->handle);
    return DVR_SUCCESS;
}
/**\brief Start play video
 * \param[in] handle playback handle
 * \param[in] params video playback params,contains fmt and pid...
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_start(DVR_Playback_Handle_t handle, Playback_VideoParams *param) {
    Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
    _start_playback_thread(handle);
    //start audio and video
    playback_device_video_start(player->handle , param);
  return DVR_SUCCESS;
}
/**\brief Stop play video
 * \param[in] handle playback handle
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_video_stop(DVR_Playback_Handle_t handle) {
  Dvr_PlayBack_t *player = (Dvr_PlayBack_t *) handle;
  playback_device_video_stop(player->handle);
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
    playback_device_pause(player->handle);
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

    int offset = -1;
    //get file offset by time
    DVR_DEBUG(0, "seek get offset by time offset");
    //seek file
    DVR_DEBUG(0, "seek file offset");
    if (offset == -1) {
        //seek 2M data once for test
        offset = player->offset + (2*1014*1024);
    }
    player->offset = offset;
    //stop play
    DVR_DEBUG(0, "seek stop play, not inject data");
    playback_device_video_stop(player->handle);
    playback_device_audio_stop(player->handle);
    //start play
    Playback_VideoParams    vparams;
    Playback_AudioParams    aparams;
    int chunkid = player->cur_chunkid;

    int sync = DVR_PLAY_SYNC;
    //get chunk info and audio video pid fmt ;
    dvr_playback_get_playinfo(chunkid, &vparams, &aparams);
    //start audio and video
    if (!VALID_PID(vparams.pid) && !VALID_PID(aparams.pid)) {
        //audio abnd video pis is all invalid, return error.
        DVR_DEBUG(0, "seek start dvr play back start error, not found audio and video info");
        return -1;
    }

    if (sync == DVR_PLAY_SYNC) {
        if (VALID_PID(vparams.pid))
            playback_device_video_start(player->handle , &vparams);
        if (VALID_PID(aparams.pid))
            playback_device_audio_start(player->handle , &aparams);
    }

  return DVR_SUCCESS;
}
/**\brief Set play speed
 * \param[in] handle playback handle
 * \param[in] speed playback speed
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_set_speed(DVR_Playback_Handle_t handle, Playback_Speed speed) {

  return DVR_SUCCESS;
}
/**\brief Get playback status
 * \param[in] handle playback handle
 * \param[out] p_status playback status
 * \retval DVR_SUCCESS On success
 * \return Error code
 */
int dvr_playback_get_status(DVR_Playback_Handle_t handle,
  DVR_Play_Status_t *p_status) {
  return DVR_SUCCESS;
}

void _dvr_dump_chunk(DVR_Play_Chunk_Info_t *chunk) {
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

    DVR_Play_Chunk_Info_t *chunk;
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
