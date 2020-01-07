#ifndef _DVR_LIST_FILE_H_
#define _DVR_LIST_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Segment_ListInfo_s {
} Segment_ListInfo;

int segment_list_file_store(const char *path, Segment_ListInfo *p_info);

int segment_list_file_load(const char *path, Segment_ListInfo *info);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_LIST_FILE_H_*/
