#include <stdio.h>
#include "dvr_index_file.h"

int dvr_index_file_open(DVR_IndexFileHandle *p_handle, DVR_IndexFileOpenParams *p_params)
{
  return 0;
}

int dvr_index_file_close(DVR_IndexFileHandle handle)
{
  return 0;
}

int dvr_index_file_write(DVR_IndexFileHandle handle, uint64_t pcr, loff_t offset)
{
  return 0;
}

loff_t dvr_index_file_lookup_by_time(DVR_IndexFileHandle handle, time_t time)
{
  return 0;
}
