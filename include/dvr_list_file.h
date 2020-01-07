#ifndef _DVR_LIST_FILE_H_
#define _DVR_LIST_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DVR_SegmentListInfo_s {
} DVR_SegmentListInfo;

int dvr_segment_list_file_store(const char *path, DVR_SegmentListInfo *p_info);

int dvr_segment_list_file_load(const char *path, DVR_SegmentListInfo *info);

#ifdef __cplusplus
}
#endif

#endif /*END _DVR_LIST_FILE_H_*/
