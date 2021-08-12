#include <stdio.h> 
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stddef.h>
#include <unistd.h>

#define ONCE_WRITE_SIZE (256 * 1024)	
#define WRITE_TOTAL_SIZE (100*1024*1024)
 
/* Define NULL pointer value and the offset() macro */
#ifndef NULL
#define NULL ((void *)0)
#endif

static int get_diff_time(struct timeval start_tv, struct timeval end_tv)
{
  return end_tv.tv_sec * 1000 + end_tv.tv_usec / 1000 - start_tv.tv_sec * 1000 - start_tv.tv_usec / 1000;
}

int main(int argc, char **argv)
{
	char * Filename = "arecord.data";
	int ts_fd = -1;
	int ret = 0;

	struct timeval t1, t2;
	struct timeval beginTime, endTime;
	char data[ONCE_WRITE_SIZE] = {'1'} ;

	if (argc > 1)  {
		Filename = argv[1];
		printf("filename: %s \n",Filename);		
	}	

	ts_fd = open(Filename, O_CREAT | O_RDWR | O_TRUNC , 0644);	
        if (ts_fd < 0) {
		printf("open %s failed\n",Filename);
		return -1;
	}	
	
	int write_count  = WRITE_TOTAL_SIZE / ONCE_WRITE_SIZE;
	
	gettimeofday(& beginTime, NULL);
	for(int i = 0; i < write_count;  i++) {
		gettimeofday(& t1, NULL);
		ret = write(ts_fd,data, ONCE_WRITE_SIZE);
		fsync(ts_fd);
		gettimeofday(& t2, NULL);
		if (ret != 0) {
			printf("%d: write %d data cost : %dms Bitrate: %d m bps\n", i, ret,get_diff_time(t1,t2), ONCE_WRITE_SIZE*(8*1000)/1024/1024/get_diff_time(t1,t2));
		};
	}
	gettimeofday(& endTime, NULL);
	close(ts_fd);
	ts_fd = -1;
	printf("write over, check data now\n");
	printf("TotalSize %dm,cost total Time: %dms\n", WRITE_TOTAL_SIZE/1024/1024, get_diff_time(beginTime,endTime));
	
	printf("Bitrate %dm bps\n", WRITE_TOTAL_SIZE/1024/1024*8*1000/get_diff_time(beginTime,endTime));
	
	return 0;
	
}
