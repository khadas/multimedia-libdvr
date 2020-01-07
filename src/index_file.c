#include <stdio.h>
#include "index_file.h"

int index_file_open(Index_FileHandle_t *p_handle, Index_FileOpenParams_t *p_params)
{
  return 0;
}

int index_file_close(Index_FileHandle_t handle)
{
  return 0;
}

int index_file_write(Index_FileHandle_t handle, uint64_t pcr, loff_t offset)
{
  return 0;
}

loff_t index_file_lookup_by_time(Index_FileHandle_t handle, time_t time)
{
  return 0;
}
