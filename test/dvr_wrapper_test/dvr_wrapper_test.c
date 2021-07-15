/**
 * \page dvb_wrapper_test
 * \section Introduction
 * test code with dvb_wrapper_xxxx APIs.
 * It supports:
 * \li Record
 * \li Playback
 * \li Timeshift
 *
 * \section Usage
 *
 * Help msg will be shown if the test runs without parameters.\n
 * There are some general concepts for the parameters:
 * \li pid:fmt  pid format pair of a stream
 * \li assign value "1" to the parameter to enable a feature specified
 * \li millisecond is used as time unit
 * \li no limit to the parameter order
 *
 * For timeshift:
 * \code
 *   dvr_wrapper_test [v=pid:fmt] [a=pid:fmt] [tsin=n] [dur=s] [pause=n]
 * \endcode
 * For recording:
 * \code
 *   dvr_wrapper_test [v=pid:fmt] [a=pid:fmt] [tsin=n] [dur=s] [recfile=x]
 * \endcode
 * For playback:
 * \code
 *   dvr_wrapper_test [file=x] [pause=n]
 * \endcode
 * the timeshift backgroud recording file will be located in /data as:
 * \code
 *   /data/9999-xxxx.*
 * \endcode
 *
 * \section FormatCode Format Code
 *
 * video
 * \li 0:DVR_VIDEO_FORMAT_MPEG1  MPEG1 video
 * \li 1:DVR_VIDEO_FORMAT_MPEG2  MPEG2 video
 * \li 2:DVR_VIDEO_FORMAT_H264   H264 video
 * \li 3:DVR_VIDEO_FORMAT_HEVC   HEVC video
 *
 * audio
 * \li 0:DVR_AUDIO_FORMAT_MPEG  MPEG audio
 * \li 1:DVR_AUDIO_FORMAT_AC3   AC3 audio
 * \li 2:DVR_AUDIO_FORMAT_EAC3  EAC3 audio
 * \li 3:DVR_AUDIO_FORMAT_DTS   DTS audio
 * \li 4:DVR_AUDIO_FORMAT_AAC   AAC audio
 * \li 5:DVR_AUDIO_FORMAT_HEAAC HE AAC audio
 *
 * \section ops Operations in the test:
 * \li pause\n
 *             pause the playback
 * \li resume\n
 *             resume the playback
 * \li f <speed>\n
 *             set the playback speed
 * \li seek \<time in ms\>\n
 *             seek the playback
 * \li aud <pid:fmt>\n
 *             change the audio pid:fmt
 * \li info\n
 *             show the record info
 * \li stat\n
 *             show the playback status
 * \li quit\n
 *             quit the test
 * \endsection
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

/*{{ third party header files */
#define  AV_AUDIO_STEREO        AV_AUDIO_STEREO_TSP
#define  AV_AUDIO_RIGHT         AV_AUDIO_RIGHT_TSP
#define  AV_AUDIO_LEFT          AV_AUDIO_LEFT_TSP
#define  AV_AUDIO_MONO          AV_AUDIO_MONO_TSP
#define  AV_AUDIO_MULTICHANNEL  AV_AUDIO_MULTICHANNEL_TSP
#define  AV_VIDEO_CODEC_AUTO    AV_VIDEO_CODEC_AUTO_TSP
#define  AV_VIDEO_CODEC_H264    AV_VIDEO_CODEC_H264_TSP
#define  AV_VIDEO_CODEC_H265    AV_VIDEO_CODEC_H265_TSP
#define  AV_VIDEO_CODEC_MPEG1   AV_VIDEO_CODEC_MPEG1_TSP
#define  AV_VIDEO_CODEC_MPEG2   AV_VIDEO_CODEC_MPEG2_TSP
#define  AV_VIDEO_CODEC_VP9     AV_VIDEO_CODEC_VP9_TSP
#define  AV_AUDIO_CODEC_AUTO   AV_AUDIO_CODEC_AUTO_TSP
#define  AV_AUDIO_CODEC_MP2    AV_AUDIO_CODEC_MP2_TSP
#define  AV_AUDIO_CODEC_MP3    AV_AUDIO_CODEC_MP3_TSP
#define  AV_AUDIO_CODEC_AC3    AV_AUDIO_CODEC_AC3_TSP
#define  AV_AUDIO_CODEC_EAC3   AV_AUDIO_CODEC_EAC3_TSP
#define  AV_AUDIO_CODEC_DTS    AV_AUDIO_CODEC_DTS_TSP
#define  AV_AUDIO_CODEC_AAC    AV_AUDIO_CODEC_AAC_TSP
#define  AV_AUDIO_CODEC_AC3    AV_AUDIO_CODEC_AC3_TSP

#include "AmTsPlayer.h"

#undef  AV_AUDIO_RIGHT
#undef  AV_AUDIO_LEFT
#undef  AV_AUDIO_MONO
#undef  AV_AUDIO_MULTICHANNEL
#undef  AV_VIDEO_CODEC_AUTO
#undef  AV_VIDEO_CODEC_H264
#undef  AV_VIDEO_CODEC_H265
#undef  AV_VIDEO_CODEC_MPEG1
#undef  AV_VIDEO_CODEC_MPEG2
#undef  AV_VIDEO_CODEC_VP9
#undef  AV_AUDIO_CODEC_AUTO
#undef  AV_AUDIO_CODEC_MP2
#undef  AV_AUDIO_CODEC_MP3
#undef  AV_AUDIO_CODEC_AC3
#undef  AV_AUDIO_CODEC_EAC3
#undef  AV_AUDIO_CODEC_DTS
#undef  AV_AUDIO_CODEC_AAC
#undef  AV_AUDIO_CODEC_AC3
/*}} third party header files */

#include "dvr_segment.h"
#include "dvr_wrapper.h"
#include "dvb_utils.h"

typedef struct
{
    uint8_t         :6;
    uint8_t  af_flag:1;
    uint8_t         :1;
    uint8_t  af     :4;
    uint8_t         :4;
    uint16_t reserved;
    uint32_t pts;
} USERDATA_AFD_t;


/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#define DMX_DEV_DVR 1
#define DMX_DEV_AV 0
#define ASYNC_FIFO_ID 0
#define AV_DEV_NO 0

#define DVR_STREAM_TYPE_TO_TYPE(_t) (((_t) >> 24) & 0xF)
#define DVR_STREAM_TYPE_TO_FMT(_t)  ((_t) & 0xFFFFFF)


#define rec_log_printf(...) __android_log_print(ANDROID_LOG_INFO, "rec" TAG_EXT, __VA_ARGS__)

#define TSP_EVT(fmt, ...)   fprintf(stdout, "TsPlayer:" fmt, ##__VA_ARGS__)
#define REC_EVT(fmt, ...)   fprintf(stdout, "recorder:" fmt, ##__VA_ARGS__)
#define PLAY_EVT(fmt, ...)  fprintf(stdout, "player:" fmt, ##__VA_ARGS__)
#define INF(fmt, ...)       fprintf(stdout, fmt, ##__VA_ARGS__)
#define ERR(fmt, ...)       fprintf(stderr, "error:" fmt, ##__VA_ARGS__)
#define RESULT(fmt, ...)    fprintf(stdout, fmt, ##__VA_ARGS__)

static int vpid=0x1fff, apid=0x1fff, vfmt=0, afmt=0;
static int duration=60;
static int size=1024*1024*1024;
static int tssrc=0;
static int pause=0;

static int mode = 0;
static char filename[512] = { 0 };
static char rec_filename[512] = { 0 };
static char *pfilename = "/data/9999";

static int playback_pending = 0;

static   DVR_WrapperRecord_t recorder;

static   DVR_WrapperPlayback_t player;
static   DVR_PlaybackPids_t play_pids;
static   am_tsplayer_handle tsplayer_handle;

static int get_dvr_info(char *location, int *apid, int *afmt, int *vpid, int *vfmt);
static int start_playback(int apid, int afmt, int vpid, int vfmt);

static void display_usage(void)
{
    INF( "==================\n");
    INF( "*pause\n");
    INF( "*resume\n");
    INF( "*f speed(100,200,300,-100,-200..)\n");
    INF( "*seek time_in_msecond\n");
    INF( "*aud <pid>:<fmt>\n");
    INF( "*info (print record info)\n");
    INF( "*stat (print playback status)\n");
    INF( "*quit\n");
    INF( "==================\n");
}

int start_test(void)
{
    DVR_Bool_t go = DVR_TRUE;
    char buf[256];
    int error;

    display_usage();

    while (go) {
        if (fgets(buf, sizeof(buf), stdin)) {

            if (!strncmp(buf, "quit", 4)) {
                go = DVR_FALSE;
                continue;
            }
            else if (!strncmp(buf, "pause", 5)) {
                error = dvr_wrapper_pause_playback(player);
                INF( "pause=(%d)\n", error);
            }
            else if (!strncmp(buf, "resume", 6)) {
                error = dvr_wrapper_resume_playback(player);
                INF( "resume=(%d)\n", error);
            }
            else if (!strncmp(buf, "f", 1)) {
                float speed;
                sscanf(buf + 1, "%f", &speed);
                error = dvr_wrapper_set_playback_speed(player, speed);
                INF( "speed(%f)=(%d)\n", speed, error);
            }
            else if (!strncmp(buf, "seek", 4)) {
                int time;
                sscanf(buf + 4, "%d", &time);
                error = dvr_wrapper_seek_playback(player, time);
                INF( "seek(%d)=(%d)\n", time, error);
            }
            else if (!strncmp(buf, "aud", 3)) {
                int apid = 0x1fff, afmt = 0;
                sscanf(buf + 3, "%i:%i", &apid, &afmt);
                play_pids.audio.pid = apid;
                play_pids.audio.format = afmt;
                error = dvr_wrapper_update_playback(player, &play_pids);
                INF( "update(aud=%d:%d)=(%d)\n", apid, afmt, error);
            }
            else if (!strncmp(buf, "info", 4)) {
                INF("rec info of [%s].\n", pfilename);
                get_dvr_info(pfilename, NULL, NULL, NULL, NULL);
            }
            else if (!strncmp(buf, "stat", 4)) {
                DVR_WrapperPlaybackStatus_t status;
                INF("playback status:\n");
                error = dvr_wrapper_get_playback_status(player, &status);
                RESULT("state:%d\n", status.state);
                RESULT("curr(time/size/pkts):%lu:%llu:%u\n",
                    status.info_cur.time,
                    status.info_cur.size,
                    status.info_cur.pkts);
                RESULT("full(time/size/pkts):%lu:%llu:%u\n",
                    status.info_full.time,
                    status.info_full.size,
                    status.info_full.pkts);
                RESULT("speed:%f\n", status.speed);
            }
            else if (!strncmp(buf, "recpause", 8)) {
                INF("rec paused.\n");
            }
            else if (!strncmp(buf, "recresume", 9)) {
                INF("rec resumed.\n");
            }
            else {
                ERR("Unkown command: %s\n", buf);
                display_usage();
            }
        }
    }

    return 0;
}

static void tsplayer_callback(void *user_data, am_tsplayer_event *event)
{
   if (event)
   {
      switch (event->type)
      {
          case AM_TSPLAYER_EVENT_TYPE_PTS:
          {
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_PTS: stream_type:%d, pts[%llu]\n",
                  event->event.pts.stream_type,
                  event->event.pts.pts);
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_DTV_SUBTITLE:
          {
              uint8_t* pbuf = event->event.mpeg_user_data.data;
              uint32_t size = event->event.mpeg_user_data.len;
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_DTV_SUBTITLE: %x-%x-%x-%x ,size %d\n",
                  pbuf[0], pbuf[1], pbuf[2], pbuf[3], size);
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_USERDATA_CC:
          {
              uint8_t* pbuf = event->event.mpeg_user_data.data;
              uint32_t size = event->event.mpeg_user_data.len;
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_USERDATA_CC: %x-%x-%x-%x ,size %d\n",
                  pbuf[0], pbuf[1], pbuf[2], pbuf[3], size);
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_USERDATA_AFD:
          {
              uint8_t* pbuf = event->event.mpeg_user_data.data;
              uint32_t size = event->event.mpeg_user_data.len;
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_USERDATA_AFD: %x-%x-%x-%x ,size %d\n",
                  pbuf[0], pbuf[1], pbuf[2], pbuf[3], size);
              USERDATA_AFD_t afd = *((USERDATA_AFD_t *)pbuf);
              afd.reserved = afd.pts = 0;
              TSP_EVT("[evt] video afd changed: flg[0x%x] fmt[0x%x]\n", afd.af_flag, afd.af);
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED:
          {
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED: [width:height] [%d x %d] @%d aspectratio[%d]\n",
              event->event.video_format.frame_width,
              event->event.video_format.frame_height,
              event->event.video_format.frame_rate,
              event->event.video_format.frame_aspectratio);
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_AUDIO_CHANGED:
          {
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_AUDIO_CHANGED: sample_rate:%d, channels:%d\n",
                  event->event.audio_format.sample_rate,
                  event->event.audio_format.channels);
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_DATA_LOSS:
          {
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_DATA_LOSS\n");
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_DATA_RESUME:
          {
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_DATA_RESUME\n");
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_SCRAMBLING:
          {
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_SCRAMBLING: stream_type:%d is_scramling[%d]\n",
                  event->event.scramling.stream_type,
                  event->event.scramling.scramling);
              break;
          }
          case AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME:
          {
              TSP_EVT("[evt] AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME: ## VIDEO_AVAILABLE ##\n");
              break;
          }
          default:
              break;
      }
   }
}

static void log_rec_evt(DVR_WrapperRecordStatus_t *status, void *user)
{
  char *state[] = {
    /*DVR_RECORD_STATE_OPENED*/ "open",        /**< Record state is opened*/
    /*DVR_RECORD_STATE_STARTED*/"start",       /**< Record state is started*/
    /*DVR_RECORD_STATE_STOPPED*/"stop",        /**< Record state is stopped*/
    /*DVR_RECORD_STATE_CLOSED*/ "close",       /**< Record state is closed*/
  };
  REC_EVT("[%s] state[%s(0x%x)] time/size/pkts[%lu/%llu/%u]\n",
    (char *)user,
    state[status->state],
    status->state,
    status->info.time,
    status->info.size,
    status->info.pkts
    );
}

static DVR_Result_t RecEventHandler(DVR_RecordEvent_t event, void *params, void *userdata)
{
   if (userdata != NULL)
   {
      DVR_WrapperRecordStatus_t *status = (DVR_WrapperRecordStatus_t *)params;

      switch (event)
      {
         case DVR_RECORD_EVENT_STATUS:
            log_rec_evt(status, userdata);
            if (playback_pending
              && status->state == DVR_RECORD_STATE_STARTED
              && status->info.time > 2000) {
              playback_pending = 0;
              start_playback(apid, afmt, vpid, vfmt);
            }
         break;
         default:
            REC_EVT("Unhandled recording event 0x%x from (%s)\n", event, (char *)userdata);
         break;
      }
   }
   return DVR_SUCCESS;
}

static void log_play_evt(DVR_WrapperPlaybackStatus_t *status, void *user)
{
  char *state[] = {
  /*DVR_PLAYBACK_STATE_START*/ "start",       /**< start play */
  /*DVR_PLAYBACK_STATE_STOP*/  "stop",        /**< stop */
  /*DVR_PLAYBACK_STATE_PAUSE*/ "pause",       /**< pause */
  /*DVR_PLAYBACK_STATE_FF*/    "ff",          /**< fast forward */
  /*DVR_PLAYBACK_STATE_FB*/    "fb"           /**< fast backword */
  };
  PLAY_EVT("[%s] state[%s(0x%x)] time[%lu/%lu] size[%llu/%llu] pkts[%u/%u] (cur/full)\n",
    (char *)user,
    state[status->state],
    status->state,
    status->info_cur.time, status->info_full.time,
    status->info_cur.size, status->info_full.size,
    status->info_cur.pkts, status->info_full.pkts
    );
}

static DVR_Result_t PlayEventHandler(DVR_PlaybackEvent_t event, void *params, void *userdata)
{
   if (userdata != NULL)
   {
      switch (event)
      {
         case DVR_PLAYBACK_EVENT_TRANSITION_OK:
            /**< Update the current player information*/
           log_play_evt((DVR_WrapperPlaybackStatus_t *)params, userdata);
         break;
         case DVR_PLAYBACK_EVENT_REACHED_END:
            /**< File player's EOF*/
           PLAY_EVT("EOF (%s)\n", (char *)userdata);
         break;
         default:
           PLAY_EVT("Unhandled event 0x%x from (%s)\n", event, (char *)userdata);
         break;
      }
   }
   return DVR_SUCCESS;
}


enum {
    PLAYBACK     = 0x01,
    RECORDING    = 0x02,
    TIMESHIFTING = PLAYBACK | RECORDING | 0x10,
};

#define has_playback(_m_)    ((_m_) & PLAYBACK)
#define has_recording(_m_)   ((_m_) & RECORDING)
#define is_playback(_m_)     ((_m_) == PLAYBACK)
#define is_recording(_m_)    ((_m_) == RECORDING)
#define is_timeshifting(_m_) ((_m_) == TIMESHIFTING)

static int start_recording()
{
    DVR_WrapperRecordOpenParams_t rec_open_params;
    DVR_WrapperRecordStartParams_t rec_start_params;
    DVR_WrapperPidsInfo_t *pids_info;
    char cmd[256];
    int error;

    sprintf(cmd, "echo ts%d > /sys/class/stb/demux%d_source", tssrc, DMX_DEV_DVR);
    //system(cmd);
    printf("set dmx source used api\r\n");
    dvb_set_demux_source(DMX_DEV_DVR, tssrc);
    memset(&rec_open_params, 0, sizeof(DVR_WrapperRecordOpenParams_t));

    rec_open_params.dmx_dev_id = DMX_DEV_DVR;
    rec_open_params.segment_size = 100 * 1024 * 1024;/*100MB*/
    rec_open_params.max_size = size;
    rec_open_params.max_time = duration;
    rec_open_params.event_fn = RecEventHandler;
    rec_open_params.event_userdata = "rec0";
    rec_open_params.flags = 0;
    if (is_timeshifting(mode))
        rec_open_params.flags |= DVR_RECORD_FLAG_ACCURATE;

    strncpy(rec_open_params.location, pfilename, sizeof(rec_open_params.location));

    rec_open_params.is_timeshift = (is_timeshifting(mode)) ? DVR_TRUE : DVR_FALSE;

    if (!(vpid > 0 && vpid < 0x1fff))
      rec_open_params.flush_size = 1024;

    error = dvr_wrapper_open_record(&recorder, &rec_open_params);
    if (error) {
      ERR( "recorder open fail = (0x%x)\n", error);
      return -1;
    }

    INF( "Starting %s recording %p [%ld secs/%llu bytes] [%s.ts]\n",
       (is_timeshifting(mode))? "timeshift" : "normal",
       recorder,
       rec_open_params.max_time,
       rec_open_params.max_size,
       rec_open_params.location);


    memset(&rec_start_params, 0, sizeof(rec_start_params));

    pids_info = &rec_start_params.pids_info;
    pids_info->nb_pids = 2;
    pids_info->pids[0].pid = vpid;
    pids_info->pids[1].pid = apid;
    pids_info->pids[0].type = DVR_STREAM_TYPE_VIDEO << 24 | vfmt;
    pids_info->pids[1].type = DVR_STREAM_TYPE_AUDIO << 24 | afmt;
    error = dvr_wrapper_start_record(recorder, &rec_start_params);
    if (error)
    {
      ERR( "recorder start fail = (0x%x)\n", error);
      dvr_wrapper_close_record(recorder);
      return -1;
    }

    return 0;
}

static int get_dvr_info(char *location, int *apid, int *afmt, int *vpid, int *vfmt)
{
    uint32_t segment_nb;
    uint64_t *p_segment_ids;
    DVR_RecordSegmentInfo_t seg_info;
    int error;
    int aid = 0x1fff, vid = 0x1fff;
    int aft = 0, vft = 0;

    error = dvr_segment_get_list(location, &segment_nb, &p_segment_ids);
    if (!error && segment_nb) {
        error = dvr_segment_get_info(location, p_segment_ids[0], &seg_info);
        free(p_segment_ids);
    }
    if (!error) {
        int i;
        for (i = 0; i < seg_info.nb_pids; i++) {
            switch (DVR_STREAM_TYPE_TO_TYPE(seg_info.pids[i].type))
            {
            case DVR_STREAM_TYPE_VIDEO:
                vid = seg_info.pids[i].pid;
                vft = DVR_STREAM_TYPE_TO_FMT(seg_info.pids[i].type);
                RESULT("type(0x%x)[video] pid(0x%x) fmt(%d)\n",
                    DVR_STREAM_TYPE_TO_TYPE(seg_info.pids[i].type),
                    seg_info.pids[i].pid,
                    DVR_STREAM_TYPE_TO_FMT(seg_info.pids[i].type)
                    );
            break;
            case DVR_STREAM_TYPE_AUDIO:
                aid = seg_info.pids[i].pid;
                aft = DVR_STREAM_TYPE_TO_FMT(seg_info.pids[i].type);
                RESULT("type(0x%x)[audio] pid(0x%x) fmt(%d)\n",
                    DVR_STREAM_TYPE_TO_TYPE(seg_info.pids[i].type),
                    seg_info.pids[i].pid,
                    DVR_STREAM_TYPE_TO_FMT(seg_info.pids[i].type)
                    );
            break;
            default:
                RESULT("type(0x%x) pid(0x%x) fmt(%d)\n",
                    DVR_STREAM_TYPE_TO_TYPE(seg_info.pids[i].type),
                    seg_info.pids[i].pid,
                    DVR_STREAM_TYPE_TO_FMT(seg_info.pids[i].type)
                    );
            break;
            }
        }
    }

    if (apid)
        *apid = aid;
    if (afmt)
        *afmt = aft;
    if (vpid)
        *vpid = vid;
    if (vfmt)
        *vfmt = vft;

    return 0;
}
static uint64_t dvr_time_getClock(void)
{
  struct timespec ts;
  uint64_t ms;

  clock_gettime(CLOCK_REALTIME, &ts);
  ms = ts.tv_sec*1000+ts.tv_nsec/1000000;
  INF("dvr_time_getClock:sec:%ld.\n", ts.tv_sec);
  return ms;
}

static int start_playback(int apid, int afmt, int vpid, int vfmt)
{
    DVR_WrapperPlaybackOpenParams_t play_params;
    int error;

    memset(&play_params, 0, sizeof(play_params));
    memset(&play_pids, 0, sizeof(play_pids));

    play_params.dmx_dev_id = DMX_DEV_AV;

    play_pids.video.type = DVR_STREAM_TYPE_VIDEO;
    play_pids.audio.type = DVR_STREAM_TYPE_AUDIO;

    if (is_timeshifting(mode)) {
        strncpy(play_params.location, pfilename, sizeof(play_params.location));
        play_params.is_timeshift = DVR_TRUE;

        play_pids.video.pid = vpid;
        play_pids.video.format = vfmt;
        play_pids.audio.pid = apid;
        play_pids.audio.format = afmt;
    } else {
        strncpy(play_params.location, pfilename, sizeof(play_params.location));
        play_params.is_timeshift = DVR_FALSE;
        {
            int vpid = 0x1fff, apid = 0x1fff, vfmt = 0, afmt = 0;
            get_dvr_info(pfilename, &apid, &afmt, &vpid, &vfmt);

            play_pids.video.pid = vpid;
            play_pids.video.format = vfmt;
            play_pids.audio.pid = apid;
            play_pids.audio.format = afmt;
        }
    }

    play_params.event_fn = PlayEventHandler;
    play_params.event_userdata = "play0";

     /*open TsPlayer*/
    {
       uint32_t versionM, versionL;
       am_tsplayer_init_params init_param =
       {
          .source = TS_MEMORY,
          .dmx_dev_id = DMX_DEV_AV,
          .event_mask = 0,
               /*AM_TSPLAYER_EVENT_TYPE_PTS_MASK
             | AM_TSPLAYER_EVENT_TYPE_DTV_SUBTITLE_MASK
             | AM_TSPLAYER_EVENT_TYPE_USERDATA_AFD_MASK
             | AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED_MASK
             | AM_TSPLAYER_EVENT_TYPE_AUDIO_CHANGED_MASK
             | AM_TSPLAYER_EVENT_TYPE_DATA_LOSS_MASK
             | AM_TSPLAYER_EVENT_TYPE_DATA_RESUME_MASK
             | AM_TSPLAYER_EVENT_TYPE_SCRAMBLING_MASK
             | AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME_MASK,*/
       };
       am_tsplayer_result result =
          AmTsPlayer_create(init_param, &tsplayer_handle);
       INF( "open TsPlayer %s, result(%d)\n", (result)? "FAIL" : "OK", result);

       result = AmTsPlayer_getVersion(&versionM, &versionL);
       INF( "TsPlayer verison(%d.%d) %s, result(%d)\n",
          versionM, versionL,
          (result)? "FAIL" : "OK",
          result);

       result = AmTsPlayer_registerCb(tsplayer_handle,
          tsplayer_callback,
          "tsp0");

       result = AmTsPlayer_setWorkMode(tsplayer_handle, TS_PLAYER_MODE_NORMAL);
       INF( " TsPlayer set Workmode NORMAL %s, result(%d)\n", (result)? "FAIL" : "OK", result);
       //result = AmTsPlayer_setSyncMode(tsplayer_handle, TS_SYNC_NOSYNC );
       //PLAY_DBG(" TsPlayer set Syncmode FREERUN %s, result(%d)", (result)? "FAIL" : "OK", result);
       result = AmTsPlayer_setSyncMode(tsplayer_handle, TS_SYNC_PCRMASTER );
       INF( " TsPlayer set Syncmode PCRMASTER %s, result(%d)\n", (result)? "FAIL" : "OK", result);

       play_params.playback_handle = (Playback_DeviceHandle_t)tsplayer_handle;
    }

    play_params.block_size = 188 * 1024;
    printf("set dmx source hiu used api\r\n");
    dvb_set_demux_source(DMX_DEV_DVR, DVB_DEMUX_SOURCE_DMA0);

    error = dvr_wrapper_open_playback(&player, &play_params);
    if (!error)
    {
       //DVR_PlaybackFlag_t play_flag = (is_timeshifting(mode))? DVR_PLAYBACK_STARTED_PAUSEDLIVE : 0;
       DVR_PlaybackFlag_t play_flag = (pause)? DVR_PLAYBACK_STARTED_PAUSEDLIVE : 0;

       INF( "Starting playback\n");
       int time = dvr_time_getClock();
       INF( "Starting playback time:%d\n", time);
	    //error = dvr_wrapper_setlimit_playback(player, time - 110*60*1000, 90*60*1000);
        error = dvr_wrapper_start_playback(player, play_flag, &play_pids);
       if (error)
       {
          ERR( "Start play failed, error %d\n", error);
       }
     }

    return 0;
}

static char *help_vfmt =
  "\n\t0:DVR_VIDEO_FORMAT_MPEG1" /**< MPEG1 video.*/
  "\n\t1:DVR_VIDEO_FORMAT_MPEG2" /**< MPEG2 video.*/
  "\n\t2:DVR_VIDEO_FORMAT_H264"  /**< H264.*/
  "\n\t3:DVR_VIDEO_FORMAT_HEVC"; /**< HEVC.*/

static char *help_afmt =
  "\n\t0:DVR_AUDIO_FORMAT_MPEG" /**< MPEG audio*/
  "\n\t1:DVR_AUDIO_FORMAT_AC3"  /**< AC3 audio.*/
  "\n\t2:DVR_AUDIO_FORMAT_EAC3" /**< EAC3 audio.*/
  "\n\t3:DVR_AUDIO_FORMAT_DTS"  /**< DTS audio.*/
  "\n\t4:DVR_AUDIO_FORMAT_AAC"  /**< AAC audio.*/
  "\n\t5:DVR_AUDIO_FORMAT_HEAAC"; /**<HE AAC audio.*/

static void usage(int argc, char *argv[])
{
  INF( "Usage: timeshifting: %s [v=pid:fmt] [a=pid:fmt] [tsin=n] [dur=s] [pause=n]\n", argv[0]);
  INF( "Usage: playback    : %s [file=x] [pause=n]\n", argv[0]);
  INF( "Usage: record      : %s [v=pid:fmt] [a=pid:fmt] [tsin=n] [dur=s] [recfile=x]\n", argv[0]);
  INF( "vfmt:%s\n", help_vfmt);
  INF( "afmt:%s\n", help_afmt);
  INF( "a particular parameter named \"helper\", which can be used to get the video layer shown by excuting some trivial operations.\n");
}


static void helper(int enable)
{
  if (enable) {
    INF("helper enable\n");
    system("echo 1 > /sys/class/graphics/fb0/osd_display_debug ;echo 1 > /sys/class/graphics/fb0/blank");
    system("echo 1 > /sys/class/video/video_global_output; echo 0 > /sys/class/video/disable_video");
  } else {
    system("echo 0 > /sys/class/graphics/fb0/osd_display_debug ;echo 0 > /sys/class/graphics/fb0/blank");
    INF("helper disable\n");
  }
}
static void enable_helper() { helper(1); }
static void disable_helper() { helper(0); }
void sig_handler(int signo)
{
  if (signo == SIGTERM)
    disable_helper();
  exit(0);
}

int main(int argc, char **argv)
{
    int error;
    int i;
    int helper = 0;

    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "v", 1))
            sscanf(argv[i], "v=%i:%i", &vpid, &vfmt);
        else if (!strncmp(argv[i], "a", 1))
            sscanf(argv[i], "a=%i:%i", &apid, &afmt);
        else if (!strncmp(argv[i], "dur", 3))
            sscanf(argv[i], "dur=%i", &duration);
        else if (!strncmp(argv[i], "size", 4))
            sscanf(argv[i], "size=%i", &size);
        else if (!strncmp(argv[i], "tsin", 4))
            sscanf(argv[i], "tsin=%i", &tssrc);
        else if (!strncmp(argv[i], "pause", 5))
            sscanf(argv[i], "pause=%i", &pause);
        else if (!strncmp(argv[i], "file", 4))
            sscanf(argv[i], "file=%s", (char *)(&filename));
        else if (!strncmp(argv[i], "recfile", 7))
            sscanf(argv[i], "recfile=%s", (char *)(&rec_filename));
        else if (!strncmp(argv[i], "helper", 6))
            sscanf(argv[i], "helper=%i", &helper);
        else if (!strncmp(argv[i], "help", 4)) {
            usage(argc, argv);
            exit(0);
        }
    }

    if (argc == 1) {
      usage(argc, argv);
      exit(0);
    }

    if (helper) {
        atexit(&disable_helper);
        signal(SIGTERM, &sig_handler);
        signal(SIGINT, &sig_handler);
        enable_helper();
    }

    INF( "mode:");

    if (strlen(filename)) {
        mode |= PLAYBACK;
        INF( " playback");
        pfilename = filename;
    }

    if (strlen(rec_filename)) {
        mode |= RECORDING;
        INF( " recording");
        pfilename = rec_filename;
    }

    if (!mode) {
        mode = TIMESHIFTING;
        INF( " timeshifting");
        pfilename = pfilename;
    }

    INF( " 0x%x\n", mode);

    if (has_recording(mode)) {
        INF( "video:%d:%d(pid/fmt) audio:%d:%d(pid/fmt)\n",
            vpid, vfmt, apid, afmt);
        INF( "duration:%d size:%d tsin:%d\n",
            duration, size, tssrc);
        if (!is_timeshifting(mode)) {
            INF( "recording file:%s\n",
                pfilename);
        }
    }

    if (has_playback(mode)) {
        if (!is_timeshifting(mode)) {
            INF( "playback file:%s\n",
                pfilename);
        }
        INF( "pause:%d\n",
            pause);
    }

    if (has_recording(mode))
        error = start_recording();

    if (is_playback(mode))
        error = start_playback(0, 0, 0, 0);

    if (is_timeshifting(mode))
        playback_pending = 1;

    //command loop
    start_test();

    if (has_recording(mode)) {
        error = dvr_wrapper_stop_record(recorder);
        INF("stop record = (0x%x)\n", error);
        error = dvr_wrapper_close_record(recorder);
        INF("close record = (0x%x)\n", error);
    }

    if (has_playback(mode)) {
        error = dvr_wrapper_stop_playback(player);
        INF("stop playback = (0x%x)\n", error);
        error = dvr_wrapper_close_playback(player);
        INF("stop playback = (0x%x)\n", error);

        if (!tsplayer_handle)
            AmTsPlayer_release(tsplayer_handle);
    }

    return 0;
}
