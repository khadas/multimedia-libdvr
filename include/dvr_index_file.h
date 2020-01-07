#ifndef _DVR_INDEX_FILE_H_
#define _DVR_INDEX_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DVR_INDEX_FILE_MAX_PATH_LENGTH 256
typedef uint32_t DVR_IndexFileHandle;

typedef enum {
  DVR_INDEX_RECORD_MODE,
  DVR_INDEX_PLAYBACK_MODE,
  DVR_INDEX_UNKNOW_MODE,
} DVR_IndexFileOpenMode;

typedef struct DVR_IndexFileOpenParams_s {
  char path[DVR_INDEX_FILE_MAX_PATH_LENGTH];
  DVR_IndexFileOpenMode mode;
} DVR_IndexFileOpenParams;

int dvr_index_file_open(DVR_IndexFileHandle *p_handle, DVR_IndexFileOpenParams *p_params);

int dvr_index_file_close(DVR_IndexFileHandle handle);

int dvr_index_file_write(DVR_IndexFileHandle handle, uint64_t pts, loff_t offset);

loff_t dvr_index_file_lookup_by_time(DVR_IndexFileHandle handle, time_t time);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_INDEX_FILE_H_*/
