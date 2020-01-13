#ifndef _DVR_COMMON_H_
#define _DVR_COMMON_H_

#include <android/log.h>
#include "dvr_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#include <android/log.h>

#define DVR_MAX_RECORD_PIDS_COUNT 8
#define DVR_MAX_LOCATION_SIZE 512


#ifndef TAG_EXT
#define TAG_EXT
#endif

#define dvr_log_print(...) __android_log_print(ANDROID_LOG_INFO, "DVR_DEBUG" TAG_EXT, __VA_ARGS__)
#define DVR_DEBUG(_level,_fmt...) \
	do {\
	{\
		dvr_log_print(_fmt);\
	}\
	} while(0)


#define DVR_ASSERT(expr) \
  do {\
    if (!(expr)) {\
      DVR_DEBUG(1, "%s-%d failed", __func__, __LINE__);\
      return DVR_FAILURE;\
    }\
  } while (0)

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
} DVR_PidType_t;

typedef struct DVR_PidInfo_s {
  uint16_t           pid;
  DVR_PidType_t     type;
} DVR_PidInfo_t;

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_COMMON_H_*/
