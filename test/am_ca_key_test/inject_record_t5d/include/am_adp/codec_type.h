/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


/**
* @file codec_type.h
* @brief  Definitions of codec type and structures
* 
* @version 1.0.0
* @date 2011-02-24
*/
/* Copyright (C) 2007-2011, Amlogic Inc.
* All right reserved
*
*/
#ifndef CODEC_TYPE_H_
#define CODEC_TYPE_H_

#include "amports/amstream.h"
#include "amports/vformat.h"
#include "amports/aformat.h"
//#include "ppmgr/ppmgr.h"
#include <stdlib.h>

typedef int CODEC_HANDLE;

typedef enum {
    STREAM_TYPE_UNKNOWN,
    STREAM_TYPE_ES_VIDEO,
    STREAM_TYPE_ES_AUDIO,
    STREAM_TYPE_ES_SUB,
    STREAM_TYPE_PS,
    STREAM_TYPE_TS,
    STREAM_TYPE_RM,
} stream_type_t;

typedef struct {
    unsigned int    format;  ///< video format, such as H264, MPEG2...
    unsigned int    width;   ///< video source width
    unsigned int    height;  ///< video source height
    unsigned int    rate;    ///< video source frame duration
    unsigned int    extra;   ///< extra data information of video stream
    unsigned int    status;  ///< status of video stream
    unsigned int    ratio;   ///< aspect ratio of video source
    void *          param;   ///< other parameters for video decoder
    unsigned long long    ratio64;   ///< aspect ratio of video source
} dec_sysinfo_t;

typedef struct {
    int valid;               ///< audio extradata valid(1) or invalid(0), set by dsp
    int sample_rate;         ///< audio stream sample rate
    int channels;            ///< audio stream channels
    int bitrate;             ///< audio stream bit rate
    int codec_id;            ///< codec format id
    int block_align;         ///< audio block align from ffmpeg
    int extradata_size;      ///< extra data size
    char extradata[AUDIO_EXTRA_DATA_SIZE];;   ///< extra data information for decoder
} audio_info_t;

typedef struct {
    int valid;               ///< audio extradata valid(1) or invalid(0), set by dsp
    int sample_rate;         ///< audio stream sample rate
    int channels;            ///< audio stream channels
    int bitrate;             ///< audio stream bit rate
    int codec_id;            ///< codec format id
    int block_align;         ///< audio block align from ffmpeg
    int extradata_size;      ///< extra data size
    char extradata[512];;   ///< extra data information for decoder
} Asf_audio_info_t;

typedef struct {
    CODEC_HANDLE handle;        ///< codec device handler
    CODEC_HANDLE cntl_handle;   ///< video control device handler
    CODEC_HANDLE sub_handle;    ///< subtile device handler
    CODEC_HANDLE audio_utils_handle;  ///< audio utils handler
    stream_type_t stream_type;  ///< stream type(es, ps, rm, ts)
unsigned int has_video:
    1;                          ///< stream has video(1) or not(0)
unsigned int  has_audio:
    1;                          ///< stream has audio(1) or not(0)
unsigned int has_sub:
    1;                          ///< stream has subtitle(1) or not(0)
unsigned int noblock:
    1;                          ///< codec device is NONBLOCK(1) or not(0)
unsigned int dv_enable:
	1;							///< videois dv data.

    int video_type;             ///< stream video type(H264, VC1...)
    int audio_type;             ///< stream audio type(PCM, WMA...)
    int sub_type;               ///< stream subtitle type(TXT, SSA...)
    int video_pid;              ///< stream video pid
    int audio_pid;              ///< stream audio pid
    int sub_pid;                ///< stream subtitle pid
    int audio_channels;         ///< stream audio channel number
    int audio_samplerate;       ///< stream audio sample rate
    int vbuf_size;              ///< video buffer size of codec device
    int abuf_size;              ///< audio buffer size of codec device
    dec_sysinfo_t am_sysinfo;   ///< system information for video
    audio_info_t audio_info;    ///< audio information pass to audiodsp
    int packet_size;            ///< data size per packet
    int avsync_threshold;    ///<for adec in ms>
    void * adec_priv;          ///<for adec>
    void * amsub_priv;          // <for amsub>
    int SessionID;
    int dspdec_not_supported;//check some profile that audiodsp decoder can not support,we switch to arm decoder
    int switch_audio_flag;      //<switch audio flag switching(1) else(0)
    int automute_flag;
    char *sub_filename;
    int associate_dec_supported;//support associate or not
    int mixing_level;
    unsigned int drmmode;
} codec_para_t;

typedef struct {
    signed char id;
    unsigned char width;
    unsigned char height;
    unsigned char type;
} subtitle_info_t;
#define MAX_SUB_NUM         (32)

#define IS_VALID_PID(t)     (t>=0 && t<=0x1fff)
#define IS_VALID_STREAM(t)  (t>0 && t<=0x1fff)
#define IS_VALID_ATYPE(t)   (t>=0 && t<AFORMAT_MAX)
#define IS_VALID_VTYPE(t)   (t>=0 && t<VFORMAT_MAX)

//pass to arm audio decoder
typedef struct {
    int sample_rate;         ///< audio stream sample rate
    int channels;            ///< audio stream channels
    int format;            ///< codec format id
    int bitrate;
    int block_align;
    int codec_id;         //original codecid corespingding to ffmepg
    int handle;        ///< codec device handler
    int extradata_size;      ///< extra data size
    char extradata[AUDIO_EXTRA_DATA_SIZE];
    int SessionID;
    int dspdec_not_supported;//check some profile that audiodsp decoder can not support,we switch to arm decoder
    int droppcm_flag;               // drop pcm flag, if switch audio (1)
    int automute;
    unsigned int has_video;
    int associate_dec_supported;//support associate or not
    int mixing_level;
} arm_audio_info;


typedef struct {
    int sub_type;
    int sub_pid;
    int stream_type;  // to judge sub how to get data
    char *sub_filename;
    unsigned int curr_timeMs; //for idx+sub
    unsigned int next_pts; //for idx+sub

    unsigned int pts;
    unsigned int m_delay;
    unsigned short sub_start_x;
    unsigned short sub_start_y;
    unsigned short sub_width;
    unsigned short sub_height;
    char * odata;    // point to decoder data
    unsigned buffer_size;
} amsub_info_t;

//audio decoder type, default arc
#define AUDIO_ARC_DECODER 0
#define AUDIO_ARM_DECODER 1
#define AUDIO_FFMPEG_DECODER 2
#define AUDIO_ARMWFD_DECODER  3

int codec_set_drmmode(codec_para_t *pcodec, unsigned int setval);
int codec_get_video_checkin_bitrate(codec_para_t *pcodec, int *bitrate);
int codec_get_audio_checkin_bitrate(codec_para_t *pcodec, int *bitrate);
int codec_set_skip_bytes(codec_para_t* pcodec, unsigned int bytes);
int codec_get_dsp_apts(codec_para_t* pcodec, unsigned int * apts);
int codec_get_pcm_level(codec_para_t* pcodec, unsigned int* level);
#endif
