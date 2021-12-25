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

#include "../../include/dvb_utils.h"
#include "../../include/amci.h"

typedef struct
{
    int dev_no;
    int running;
    pthread_t thread;
    pthread_mutex_t lock;
	int   fd;

}dvb_ci_t;

#define CI_COUNT 1

static dvb_ci_t CI_devices[CI_COUNT];

DVB_RESULT AM_CI_Detect(int dev_no, int *dec);

static inline DVB_RESULT ci_get_dev(int dev_no, dvb_ci_t **dev)
{
	if ((dev_no < 0) || (dev_no >= CI_COUNT))
	{
		DVB_DEBUG(1, "invalid CI device number %d, must in(%d~%d)", dev_no, 0, CI_COUNT-1);
		return DVB_FAILURE;
	}

	*dev = &CI_devices[dev_no];
	return DVB_SUCCESS;
}

static void* ci_data_thread(void *arg)
{
	dvb_ci_t *dev = NULL;
    if (ci_get_dev(0, &dev))
    {
        DVB_DEBUG(1, "Wrong dmx device no %d", 0);
        return NULL;
    }
	int dec = 0;
    while (dev->running)
	{
		AM_CI_Detect(0, &dec);
		usleep(2000*2000);
	}
	return NULL;
}


DVB_RESULT AM_CI_Open(int dev_no)
{
	dvb_ci_t *dev = NULL;

    if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong dmx device no %d", dev_no);
        return DVB_FAILURE;
    }
	char dev_name[32];
    if (dev->running)
    {
	    DVB_DEBUG(1, "CI DEV already initialized");
	    return DVB_FAILURE;
    }

	dev->dev_no = dev_no;

    pthread_mutex_init(&dev->lock, NULL);
    dev->running = 1;
	memset(dev_name, 0, sizeof(dev_name));
    sprintf(dev_name, "%s", "/dev/rawci");
    dev->fd = open(dev_name, O_RDWR);
    if (dev->fd == -1)
    {
        DVB_DEBUG(1, "cannot open \"%s\" (%s)", dev_name, strerror(errno));
        pthread_mutex_unlock(&dev->lock);
        return DVB_FAILURE;
    }
    pthread_create(&dev->thread, NULL, ci_data_thread, dev);

    return DVB_SUCCESS;
}

DVB_RESULT AM_CI_Detect(int dev_no, int *dec)
{
	dvb_ci_t *dev = NULL;

    if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
        return DVB_FAILURE;
    }
	int detect = 0;
	if (dev->fd < 0) {
		DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
		return DVB_FAILURE;
	}

	if (ioctl(dev->fd, AMCI_IOC_GET_DETECT, &detect) < 0) {
		DVB_DEBUG(1, "get detect info errot ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	DVB_DEBUG(1, "detect info ci device no %d, detect[%d]", dev_no, detect);
	*dec = detect;
	return DVB_SUCCESS;
}

DVB_RESULT AM_CI_Power(int dev_no, int power)
{
	dvb_ci_t *dev = NULL;

    if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
        return DVB_FAILURE;
    }
	int detect = 0;
	if (dev->fd < 0) {
		DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
		return DVB_FAILURE;
	}

	if (ioctl(dev->fd, AMCI_IOC_SET_POWER, &power) < 0) {
		DVB_DEBUG(1, "set power error ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	DVB_DEBUG(1, "set ci device no %d, power[%d]", dev_no, power);
	return DVB_SUCCESS;
}

DVB_RESULT AM_CI_Reset(int dev_no)
{
	dvb_ci_t *dev = NULL;

    if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
        return DVB_FAILURE;
    }
	int detect = 0;
	if (dev->fd < 0) {
		DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
		return DVB_FAILURE;
	}

	if (ioctl(dev->fd, AMCI_IOC_RESET, 0) < 0) {
		DVB_DEBUG(1, "set reset error ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	DVB_DEBUG(1, "reset ci device no %d", dev_no);
	return DVB_SUCCESS;
}

u_int8_t AM_CI_Ior(int dev_no, int addr)
{
	dvb_ci_t *dev = NULL;
	struct ci_rw_param param;

	if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
        return DVB_FAILURE;
    }
	if (dev->fd < 0) {
		DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	param.mode = AM_CI_IOR;
	param.addr = addr;
	//param.value ; return value
	if (ioctl(dev->fd, AMCI_IOC_IO, &param) < 0) {
		DVB_DEBUG(1, "IOR error ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	return param.value;
}

DVB_RESULT AM_CI_Iow(int dev_no, int addr, uint8_t value)
{
	dvb_ci_t *dev = NULL;
	struct ci_rw_param param;

	if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
        return DVB_FAILURE;
    }
	if (dev->fd < 0) {
		DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	param.mode = AM_CI_IOW;
	param.addr = addr;
	param.value = value;//
	if (ioctl(dev->fd, AMCI_IOC_IO, &param) < 0) {
		DVB_DEBUG(1, "IOW error ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	return DVB_SUCCESS;
}


u_int8_t AM_CI_Memr(int dev_no, int addr)
{
	dvb_ci_t *dev = NULL;
	struct ci_rw_param param;

	if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
        return DVB_FAILURE;
    }
	if (dev->fd < 0) {
		DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	param.mode = AM_CI_MEMR;
	param.addr = addr;
	//param.value ; return value
	if (ioctl(dev->fd, AMCI_IOC_IO, &param) < 0) {
		DVB_DEBUG(1, "MEMR error ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	return param.value;
}

DVB_RESULT AM_CI_Memw(int dev_no, int addr, uint8_t value)
{
	dvb_ci_t *dev = NULL;
	struct ci_rw_param param;

	if (ci_get_dev(dev_no, &dev))
    {
        DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
        return DVB_FAILURE;
    }
	if (dev->fd < 0) {
		DVB_DEBUG(1, "Wrong ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	param.mode = AM_CI_MEMW;
	param.addr = addr;
	param.value = value;//
	if (ioctl(dev->fd, AMCI_IOC_IO, &param) < 0) {
		DVB_DEBUG(1, "MEMW error ci device no %d", dev_no);
		return DVB_FAILURE;
	}
	return DVB_SUCCESS;
}

int main(int argc, char **argv)
{
	return 0;
}
