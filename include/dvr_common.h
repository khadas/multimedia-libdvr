#ifndef _DVR_COMMON_H_
#define _DVR_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DVR_MAX_RECORD_PIDS 8

typedef enum
{
  DVR_PID_TYPE_AUDIO_MPEG1 = 0,
  DVR_PID_TYPE_AUDIO_MPEG2 = 1,
  DVR_PID_TYPE_AUDIO_AC3   = 2,
  DVR_PID_TYPE_VIDEO_MPEG2 = 3,
  DVR_PID_TYPE_SECTIONS    = 6,
  DVR_PID_TYPE_VIDEO_H264  = 9,
  DVR_PID_TYPE_AUDIO_AAC   = 10,
  DVR_PID_TYPE_AUDIO_HEAAC = 11,
  DVR_PID_TYPE_ECM         = 12,
  DVR_PID_TYPE_VIDEO_HEVC  = 13,
  DVR_PID_TYPE_AUDIO_EAC3  = 14,
  DVR_PID_TYPE_PCR         = 15,
  DVR_PID_TYPE_PES         = 16,
  DVR_PID_TYPE_AUDIO_AC4   = 17,
} DVR_PidType;

typedef struct DVR_PidInfo_s {
  uint16_t        pid;
  DVR_PidType     type;
} DVR_PidInfo;

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_COMMON_H_*/
