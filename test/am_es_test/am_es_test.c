#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 * \brief
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-06-07: create the document
 ***************************************************************************/
#include "stdio.h"
#include <string.h>
#include <stdlib.h>
#include "dvr_record.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <am_debug.h>
#include <stdio.h>
#include "dmx.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
typedef struct
{
    int id;
    int rfd;
    pthread_t thread;
    int running;
    int wfd;
}DVRData;

static DVRData data_threads[3];

static int dvr_data_write(int fd, uint8_t *buf, int size)
{
    int ret;
    int left = size;
    uint8_t *p = buf;

    while (left > 0)
    {
        printf("write start\n");
        ret = write(fd, p, left);
        printf("write end\n");
        if (ret == -1)
        {
            if (errno != EINTR)
            {
                printf("Write DVR data failed: %s", strerror(errno));
                break;
            }
            ret = 0;
        } else {
            printf("%s write cnt:%d\n",__FUNCTION__,ret);
        }

        left -= ret;
        p += ret;
    }

    return (size - left);
}


static int inject_running=0;
static int inject_loop=0;
static void* inject_entry(void *arg)
{
    DVRData *dvr_data = (DVRData *)arg;
    int rfd = dvr_data->rfd;
    int fd = dvr_data->wfd;
    uint8_t buf[100*188];
    int len, left=0, send, ret;

    printf("inject thread start\r\n");
    while (inject_running) {
            len = sizeof(buf) - left;
            ret = read(rfd, buf+left, len);
            if (ret == 0) {
                break;
            }
            send = ret;
            send = dvr_data_write(fd, buf, send);
            sleep(1);
    }

    printf("inject thread end\r\n");
    inject_running = -1;
    return NULL;
}

int inject_file(int dvr_no, char *inject_name)
{
    int loop = 0;
    int rfd;
    char dev_name[32];
    int fd;
    int ret;

    pthread_t th;

    printf("inject file name=%s to dvr%d\r\n", inject_name, dvr_no);

    snprintf(dev_name, sizeof(dev_name), "/dev/dvb0.dvr%d", dvr_no);

    fd = open(dev_name, O_WRONLY);
    if (fd == -1)
    {
        printf("cannot open \"%s\" (%s)\r\n", dev_name, strerror(errno));
        return -1;
    }

    ret = ioctl(fd, DMX_SET_INPUT, INPUT_LOCAL);

    data_threads[0].id =0;
    data_threads[0].wfd = fd;

    data_threads[0].running = 0;

    rfd = open(inject_name, O_RDONLY, S_IRUSR);
    if (rfd == -1) {
        printf("cannot open \"%s\" (%s)\r\n", inject_name, strerror(errno));
        return -1;
    }
    data_threads[0].rfd = rfd;

    inject_loop = loop;
    inject_running = 1;
    pthread_create(&th, NULL, inject_entry, (void*)(long)&data_threads[0]);

    return 0;
}
int open_es_dmx(int dmx_no, int pid, int is_audio) {
    char dev_name[512]={0};
    int ret;
    snprintf(dev_name, sizeof(dev_name), "/dev/dvb0.demux%d", dmx_no);
    int fd = open(dev_name, O_RDWR);
    struct dmx_pes_filter_params aparam;
    printf("pid:%d,is audio:%d\r\n", pid, is_audio);
    aparam.pid = pid;
    if (is_audio) {
        printf("pid:%d,is audio:%d set audio pes type\r\n", pid, is_audio);
        aparam.pes_type = DMX_PES_AUDIO0;
    }
    else {
        printf("pid:%d,is audio:%d set video type\r\n", pid, is_audio);
        aparam.pes_type = DMX_PES_VIDEO0;
    }
    aparam.input = DMX_IN_DVR;
    aparam.output = DMX_OUT_TAP;
    aparam.flags = 0;
    aparam.flags |= DMX_ES_OUTPUT;
    if (is_audio == 0)
    aparam.flags |= DMX_OUTPUT_RAW_MODE;
    fcntl(fd,F_SETFL,O_NONBLOCK);
    ret = ioctl(fd, DMX_SET_BUFFER_SIZE, 1024*1024);
    ret = ioctl(fd, DMX_SET_PES_FILTER, &aparam);
    ioctl(fd, DMX_START, 0);
    if (ret == -1) {
        printf("set pes filter failed (%s)\r\n", strerror(errno));
        return 0;
    }
    return 0;
}
int main(int argc, char **argv)
{
    printf("argc:%d,%s  %s \r\n", argc, argv[0], argv[1]);
    if (argc < 4) {
        printf("usage:\r\n");
        printf("am_es_test pid isaudio filepath\r\n");
        return 0;
    }
    open_es_dmx(0, atoi(argv[1]),atoi(argv[2]));
    inject_file(0,argv[3]);
    printf("start sleep 20s\r\n");
    sleep(20);
    printf("exit\r\n");
    return 0;
}