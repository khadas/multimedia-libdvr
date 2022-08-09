#ifndef _INDEX_FILE_H_
#define _INDEX_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define INDEX_FILE_MAX_PATH_LENGTH 256
typedef uint32_t Index_FileHandle_t;

typedef enum {
  INDEX_RECORD_MODE,
  INDEX_PLAYBACK_MODE,
  INDEX_UNKNOWN_MODE,
} Index_FileOpenMode_t;

typedef struct Index_FileOpenParams_s {
  char path[INDEX_FILE_MAX_PATH_LENGTH];
  Index_FileOpenMode_t mode;
} Index_FileOpenParams_t;

int index_file_open(Index_FileHandle_t *p_handle, Index_FileOpenParams_t *p_params);

int index_file_close(Index_FileHandle_t handle);

int index_file_write(Index_FileHandle_t handle, uint64_t pts, loff_t offset);

loff_t index_file_lookup_by_time(Index_FileHandle_t handle, time_t time);

#ifdef __cplusplus
}
#endif

#endif /*END _INDEX_FILE_H_*/
