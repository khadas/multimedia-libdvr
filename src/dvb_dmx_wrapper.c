/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *
 * @brief   linux dvb demux wrapper
 * @file    dvb_dmx_wrapper.c
 *
 * \author Chuanzhi Wang <chuanzhi.wang@amlogic.com>
 * \date 2020-07-16: create the document
 ***************************************************************************/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>

#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dmx.h"
#include "dvb_dmx_wrapper.h"

#define DMX_COUNT (3)
#define DMX_FILTER_COUNT (32*DMX_COUNT)
#define SEC_BUF_SIZE (4096)
#define DMX_POLL_TIMEOUT (200)

/*#define DEBUG_DEMUX_DATA*/
#define DMX_DBG(fmt, ...)       fprintf(stderr, "error:" fmt, ##__VA_ARGS__)

typedef struct
{
    int dev_no;
    int fd;
    int used;
    int enable;
    int need_free;
    AML_DMX_DataCb cb;
    void *user_data;
}dvb_dmx_filter_t;

typedef struct
{
    int dev_no;
    int running;
    pthread_t thread;
    pthread_mutex_t lock;

    dvb_dmx_filter_t filter[DMX_FILTER_COUNT];
}dvb_dmx_t;

static dvb_dmx_t dmx_devices[DMX_COUNT];

static inline BOOLEAN dmx_get_dev(int dev_no, dvb_dmx_t **dev)
{
	if ((dev_no < 0) || (dev_no >= DMX_COUNT))
	{
		DMX_DBG("invalid demux device number %d, must in(%d~%d)", dev_no, 0, DMX_COUNT-1);
		return FALSE;
	}

	*dev = &dmx_devices[dev_no];
	return TRUE;
}

static void* dmx_data_thread(void *arg)
{
    int i, fid;
    int ret;
    int cnt, len;
    uint32_t mask;
    uint8_t *sec_buf = NULL;
    int fids[DMX_FILTER_COUNT];
    struct pollfd fds[DMX_FILTER_COUNT];
    dvb_dmx_filter_t *filter = NULL;
    dvb_dmx_t *dmx = (dvb_dmx_t *)arg;

    sec_buf = (uint8_t *)malloc(SEC_BUF_SIZE);
    prctl(PR_SET_NAME, "dmx_data_thread");
    while (dmx->running)
    {
        cnt = 0;
        mask = 0;

        pthread_mutex_lock(&dmx->lock);
        for (fid = 0; fid < DMX_FILTER_COUNT; fid++)
        {
           if (dmx->filter[fid].need_free)
           {
               filter = &dmx->filter[fid];
               close(filter->fd);
               filter->used = 0;
               filter->need_free = 0;
               filter->cb = NULL;
           }

           if (dmx->filter[fid].used)
           {
               fds[cnt].events = POLLIN | POLLERR;
               fds[cnt].fd = dmx->filter[fid].fd;
               fids[cnt] = fid;
               cnt++;
           }
        }

    	pthread_mutex_unlock(&dmx->lock);

    	if (!cnt)
        {
            usleep(20*1000);
    	    continue;
        }

    	ret = poll(fds, cnt, DMX_POLL_TIMEOUT);
    	if (ret <= 0)
    	{
    	    continue;
    	}

    	for (i = 0; i < cnt; i++)
    	{
    	    if (fds[i].revents & (POLLIN | POLLERR))
    	    {
        		pthread_mutex_lock(&dmx->lock);
        		filter = &dmx->filter[fids[i]];
        		if (!filter->enable || !filter->used || filter->need_free)
        		{
        		    DMX_DBG("ch[%d] not used, not read", fids[i], len);
        		    len = 0;
        		}
        		else
        		{
                     len = read(filter->fd, sec_buf, SEC_BUF_SIZE);
                     if (len <= 0)
                     {
                         DMX_DBG("read demux filter[%d] failed (%s) %d", fids[i], strerror(errno), errno);
                     }
        		}
        		pthread_mutex_unlock(&dmx->lock);
#ifdef DEBUG_DEMUX_DATA
        		if (len)
        		    DMX_DBG("tid[%#x] ch[%d] %#x bytes", sec_buf[0], fids[i], len);
#endif
        		if (len > 0 && filter->cb)
        		{
        		    filter->cb(filter->dev_no, fids[i], sec_buf, len, filter->user_data);
        		}
    	    }
    	}
    }

    if (sec_buf)
    {
	    free(sec_buf);
    }

    return NULL;
}

static dvb_dmx_filter_t* dmx_get_filter(dvb_dmx_t * dev, int fhandle)
{
    if (fhandle >= DMX_FILTER_COUNT)
    {
    	DMX_DBG("wrong filter no");
    	return NULL;
    }

    if (!dev->filter[fhandle].used)
    {
    	DMX_DBG("filter %d not allocated", fhandle);
    	return NULL;
    }
    return &dev->filter[fhandle];
}

BOOLEAN AML_DMX_Open(int dev_no)
{
    dvb_dmx_t *dev = NULL;

    if (!dmx_get_dev(dev_no, &dev))
        return FALSE;

    if (dev->running)
    {
	    DMX_DBG("dmx already initialized");
	    return FALSE;
    }

	dev->dev_no = dev_no;

    pthread_mutex_init(&dev->lock, NULL);
    dev->running = 1;
    pthread_create(&dev->thread, NULL, dmx_data_thread, dev);

    return TRUE;
}

BOOLEAN AML_DMX_AllocateFilter(int dev_no, int *fhandle)
{
    int fd;
    int fid;
    dvb_dmx_filter_t *filter = NULL;
    char dev_name[32];

    dvb_dmx_t *dev = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("demux allocate failed, wrong dmx device no %d", dev_no);
        return FALSE;
    }

    pthread_mutex_lock(&dev->lock);
    filter = &dev->filter[0];
    for (fid = 0; fid < DMX_FILTER_COUNT; fid++)
    {
    	if (!filter[fid].used)
    	{
    	    break;
    	}
    }

    if (fid >= DMX_FILTER_COUNT)
    {
        DMX_DBG("filter id:%d, have no filter to alloc", fid);
        pthread_mutex_unlock(&dev->lock);
        return FALSE;
    }

    memset(dev_name, 0, sizeof(dev_name));
    sprintf(dev_name, "/dev/dvb0.demux%d", dev_no);
    fd = open(dev_name, O_RDWR);
    if (fd == -1)
    {
        DMX_DBG("cannot open \"%s\" (%s)", dev_name, strerror(errno));
        pthread_mutex_unlock(&dev->lock);
        return FALSE;
    }

    memset(&filter[fid], 0, sizeof(dvb_dmx_filter_t));
    filter[fid].dev_no = dev_no;
    filter[fid].fd = fd;
    filter[fid].used = 1;
    *fhandle = fid;

    pthread_mutex_unlock(&dev->lock);

    //DMX_DBG("fhandle = %d", fid);
    return TRUE;
}

BOOLEAN AML_DMX_SetSecFilter(int dev_no, int fhandle, const struct dmx_sct_filter_params *params)
{
    BOOLEAN ret = TRUE;
    dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("Wrong dmx device no %d", dev_no);
        return FALSE;
    }

    if (!params)
        return FALSE;

    pthread_mutex_lock(&dev->lock);

    filter = dmx_get_filter(dev, fhandle);
    if (filter)
    {
	   if (ioctl(filter->fd, DMX_STOP, 0) < 0)
	   {
		   DMX_DBG("dmx stop filter failed error:%s", strerror(errno));
		   ret = FALSE;
	   }
	   else if (ioctl(filter->fd, DMX_SET_FILTER, params) < 0)
	   {
	   	  DMX_DBG("set filter failed error:%s", strerror(errno));
		  ret = FALSE;
	   }
    }

    pthread_mutex_unlock(&dev->lock);
    //DMX_DBG("pid = %#x", params->pid);
    return ret;
}

BOOLEAN AML_DMX_SetPesFilter(int dev_no, int fhandle, const struct dmx_pes_filter_params *params)
{
    BOOLEAN ret = TRUE;
    dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("wrong dmx device no %d", dev_no);
        return FALSE;
    }

    if (!params)
        return FALSE;

    pthread_mutex_lock(&dev->lock);

    filter = dmx_get_filter(dev, fhandle);
    if (filter)
    {
	   if (ioctl(filter->fd, DMX_STOP, 0) < 0)
	   {
               DMX_DBG("dmx stop filter failed error:%s", strerror(errno));
               ret = FALSE;
	   }
	   else
	   {
	      fcntl(filter->fd, F_SETFL, O_NONBLOCK);
	      if (ioctl(filter->fd, DMX_SET_PES_FILTER, params) < 0)
	      {
                  DMX_DBG("set filter failed error:%s", strerror(errno));
                  ret = FALSE;
              }
	   }
    }

    pthread_mutex_unlock(&dev->lock);
    //DMX_DBG("pid = %#x", params->pid);
    return ret;
}


BOOLEAN AML_DMX_SetBufferSize(int dev_no, int fhandle, int size)
{
    BOOLEAN ret = TRUE;
    dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("wrong dmx device no %d", dev_no);
        return FALSE;
    }

    pthread_mutex_lock(&dev->lock);

    filter = dmx_get_filter(dev, fhandle);
    if (filter)
    {
	    if (ioctl(filter->fd, DMX_SET_BUFFER_SIZE, size) < 0)
	    {
 	   	  DMX_DBG("set buf size failed error:%s", strerror(errno));
		  ret = FALSE;
		}
    }

    pthread_mutex_unlock(&dev->lock);
    return ret;
}

BOOLEAN AML_DMX_FreeFilter(int dev_no, int fhandle)
{
    dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("wrong dmx device no %d", dev_no);
        return FALSE;
    }

    pthread_mutex_lock(&dev->lock);

    filter = dmx_get_filter(dev, fhandle);
    if (filter)
    {
	    filter->need_free = 1;
    }

    pthread_mutex_unlock(&dev->lock);

    //DMX_DBG("fhandle = %d", fhandle);
    return TRUE;
}

BOOLEAN AML_DMX_StartFilter(int dev_no, int fhandle)
{
    BOOLEAN ret = TRUE;
    dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("wrong dmx device no %d", dev_no);
        return FALSE;
    }

    pthread_mutex_lock(&dev->lock);

    filter = dmx_get_filter(dev, fhandle);
    if (filter && !filter->enable)
    {
        if (ioctl(filter->fd, DMX_START, 0) < 0)
        {
            DMX_DBG("dmx start filter failed error:%s", strerror(errno));
            ret = FALSE;
        }
        else
        {
            filter->enable = 1;
        }
    }

    pthread_mutex_unlock(&dev->lock);
    //DMX_DBG("ret = %d", ret);
    return ret;
}

BOOLEAN AML_DMX_StopFilter(int dev_no, int fhandle)
{
    BOOLEAN ret = TRUE;
    dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("wrong dmx device no %d", dev_no);
        return FALSE;
    }

    pthread_mutex_lock(&dev->lock);

    filter = dmx_get_filter(dev, fhandle);
    if (filter && filter->enable)
    {
        if (ioctl(filter->fd, DMX_STOP, 0) < 0)
        {
            DMX_DBG("dmx stop filter failed error:%s", strerror(errno));
            ret = FALSE;
        }
        else
        {
            filter->enable = 0;
        }
    }

    pthread_mutex_unlock(&dev->lock);
    //DMX_DBG("ret = %d", ret);
    return ret;
}

BOOLEAN AML_DMX_SetSource(int dev_no, AML_DMX_Source_t src)
{
	char buf[32];
	char *cmd;

	snprintf(buf, sizeof(buf), "/sys/class/stb/demux%d_source", dev_no);
	switch(src)
	{
		case AML_DMX_SRC_TS0:
			cmd = "ts0";
		break;
		case AML_DMX_SRC_TS1:
			cmd = "ts1";
		break;
		case AML_DMX_SRC_TS2:
			cmd = "ts2";
		break;
		case AML_DMX_SRC_TS3:
			cmd = "ts3";
		break;
		case AML_DMX_SRC_HIU:
			cmd = "hiu";
		break;
		case AML_DMX_SRC_HIU1:
			cmd = "hiu1";
		break;
		default:
			DMX_DBG("do not support demux source %d", src);
		return FALSE;
	}

	return AML_DMX_FileEcho(buf, cmd);
}


BOOLEAN AML_DMX_SetCallback(int dev_no, int fhandle, AML_DMX_DataCb cb, void *user_data)
{
    BOOLEAN ret = TRUE;
    dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("wrong dmx device no %d", dev_no);
        return FALSE;
    }

    pthread_mutex_lock(&dev->lock);
    filter = dmx_get_filter(dev, fhandle);
    if (filter)
    {
        filter->cb = cb;
        filter->user_data = user_data;
    }
    else
    {
        ret = FALSE;
    }

    pthread_mutex_unlock(&dev->lock);

    //DMX_DBG("ret = %d", ret);
    return ret;
}

BOOLEAN AML_DMX_Close(int dev_no)
{
    int i;
    int open_count = 0;
	dvb_dmx_t *dev = NULL;
    dvb_dmx_filter_t *filter = NULL;

    if (!dmx_get_dev(dev_no, &dev))
    {
        DMX_DBG("wrong dmx device no %d", dev_no);
        return FALSE;
    }

    pthread_mutex_lock(&dev->lock);

    for (i = 0; i < DMX_FILTER_COUNT; i++)
    {
    	filter = &dev->filter[i];
    	if (filter->used && filter->dev_no == dev_no)
    	{
    	    if (filter->enable)
    	    {
                ioctl(filter->fd, DMX_STOP, 0);
    	    }
    	    close(filter->fd);
    	}
	else if (filter->used)
    	{
    	    open_count++;
    	}
    }

    if (open_count == 0)
    {
       dev->running = 0;
       pthread_join(dev->thread, NULL);
       pthread_mutex_destroy(&dev->lock);
    }

    pthread_mutex_unlock(&dev->lock);
    return TRUE;
}

BOOLEAN AML_DMX_FileEcho(const char *name, const char *cmd)
{
	int fd, len, ret;

	if (!name || !cmd)
		return FALSE;

	fd = open(name, O_WRONLY);
	if (fd == -1)
	{
		DMX_DBG("cannot open file \"%s\"", name);
		return FALSE;
	}

	len = strlen(cmd);
	ret = write(fd, cmd, len);
	if (ret != len)
	{
		DMX_DBG("write failed file:\"%s\" cmd:\"%s\" error:\"%s\"", name, cmd, strerror(errno));
		close(fd);
		return FALSE;
	}

	close(fd);
	return TRUE;
}
