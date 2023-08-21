#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "ca.h"
#include "am_ca.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

//#define DEV_NAME "/dev/dvb/adapter0/ca"
#define DEV_NAME "/dev/dvb0.ca"
#define MAX_DSC_DEV         2
#define DSC_CHANNEL_COUNT	8

/****************************************************************************
 * Static data
 ***************************************************************************/
typedef struct _dsc_channel {
	int used;
	int pid;
	int algo;
	int dsc_type;
} dsc_channel;

typedef struct _dsc_dev{
	int used;
	int fd;
	dsc_channel channels[DSC_CHANNEL_COUNT];
	int ref;
} dsc_dev;

static dsc_dev rec_dsc_dev[MAX_DSC_DEV];
static pthread_mutex_t devLock  = PTHREAD_MUTEX_INITIALIZER;

/****************************************************************************
 * API functions
 ***************************************************************************/
#define LOG_ALL			0
#define LOG_INFO		1
#define LOG_DEBUG		2
#define LOG_ERROR		3

static int print_level = LOG_ALL;

#define am_ca_pr(level, x...) \
	do { \
		if ((level) >= print_level) \
			printf(x); \
	} while (0)

/**
 * Open the CA(descrambler) device.
 * \param devno The CA device number.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_open (int devno)
{
	int fd;
	char buf[32];
	static int init_flag = 0;
	dsc_dev *pdev = NULL;
	int i = 0;

	am_ca_pr(LOG_DEBUG,"ca_open enter\n");
	if (devno >= MAX_DSC_DEV || devno < 0)
		return -1;
	pthread_mutex_lock(&devLock);
	if (!init_flag) {
		init_flag = 1;
		memset(&rec_dsc_dev, 0, sizeof(rec_dsc_dev));
		for (i = 0; i < MAX_DSC_DEV; i++)
			rec_dsc_dev[i].fd = -1;
	}
	pdev = &rec_dsc_dev[devno];
	if (pdev->used) {
		pdev->ref++;
		pthread_mutex_unlock(&devLock);
		return 0;
	}

	snprintf(buf, sizeof(buf), DEV_NAME"%d", devno);
	fd = open(buf, O_RDWR);
	if (fd == -1)
	{
		am_ca_pr(LOG_ERROR, "cannot open \"%s\" (%d:%s)", DEV_NAME, errno, strerror(errno));
		pthread_mutex_unlock(&devLock);
		return 0;
	}
	pdev->fd = fd;
	pdev->used = 1;
	pdev->ref++;
	pthread_mutex_unlock(&devLock);
	return 0;
}

/**
 * Allocate a descrambler channel from the device.
 * \param devno The CA device number.
 * \param pid The descrambled elementary stream's PID of this channel.
 * \param algo The descrambling algorithm.
 * This parameter is defined as "enum ca_sc2_algo_type".
 * \param dsc_type The descrambler hardware module type for T5W.
 * This parameter is defined as "enum ca_sc2_dsc_type".
 * This parameter is not used on T5D.
 * \return The allocated descrambler channel's index.
 * \retval -1 On error.
 */
int ca_alloc_chan (int devno, unsigned int pid, int algo, int dsc_type)
{
	int i = 0;
	dsc_dev *pdev = NULL;
	dsc_channel *pchan = NULL;
	struct ca_pid pi;

	am_ca_pr(LOG_DEBUG,"ca_alloc_chan enter\n");

	if (devno >= MAX_DSC_DEV || devno < 0) {
		am_ca_pr(LOG_ERROR,"dev no invalid\n");
		return -1;
	}
	if (algo != CA_ALGO_AES_CBC_CLR_END &&
		algo != CA_ALGO_DES_SCTE41 &&
		algo != CA_ALGO_DES_SCTE52) {
		am_ca_pr(LOG_ERROR,"algo invalid\n");
		return -1;
	}
	if (dsc_type != CA_DSC_COMMON_TYPE) {
		am_ca_pr(LOG_ERROR,"dsc_type use CA_DSC_COMMON_TYPE\n");
		return -1;
	}

	pthread_mutex_lock(&devLock);
	pdev = &rec_dsc_dev[devno];
	if (!pdev->used) {
		pthread_mutex_unlock(&devLock);
		am_ca_pr(LOG_ERROR,"please call open first\n");
		return -1;
	}

	for (i = 0; i < DSC_CHANNEL_COUNT; i++) {
		if (!pdev->channels[i].used) {
			pchan = &pdev->channels[i];
			break;
		}
	}

	if (i == DSC_CHANNEL_COUNT) {
		pthread_mutex_unlock(&devLock);
		am_ca_pr(LOG_ERROR,"ca chan is full\n");
		return -1;
	}
	pchan->used = 1;
	pchan->pid = pid;

	pi.index = i;
	pi.pid = pid;
	am_ca_pr(LOG_DEBUG,"call CA_SET_PID\n");
	if (ioctl(pdev->fd, CA_SET_PID, &pi) == -1) {
		am_ca_pr(LOG_ERROR,"set pid failed \"%s\"", strerror(errno));
		pchan->used = 0;
		pthread_mutex_unlock(&devLock);
		return -1;
	}

	pchan->algo = algo;
	pchan->dsc_type = dsc_type;
	pthread_mutex_unlock(&devLock);
	am_ca_pr(LOG_DEBUG,"ca_alloc_chan exit\n");
	return i + devno * DSC_CHANNEL_COUNT;
}

/**
 * Free an unused descrambler channel.
 * \param devno the CA device number.
 * \param chan_index The descrambler channel's index to be freed.
 * The index is allocated by the function ca_alloc_chan.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_free_chan (int devno, int chan_index)
{
	dsc_dev *pdev = NULL;
	int index = 0;
	struct ca_pid pi;

	am_ca_pr(LOG_DEBUG,"ca_free_chan enter\n");

	if (devno >= MAX_DSC_DEV || devno < 0) {
		am_ca_pr(LOG_ERROR,"ca_free chan dev invalid:%d\n", devno);
		return -1;
	}
	index = chan_index - devno * DSC_CHANNEL_COUNT;
	if (index < 0 || index > DSC_CHANNEL_COUNT) {
		am_ca_pr(LOG_ERROR,"ca_free chan index invalid:%d\n", chan_index);
		return -1;
	}

	pthread_mutex_lock(&devLock);
	pdev = &rec_dsc_dev[devno];
	if (!pdev->used) {
		pthread_mutex_unlock(&devLock);
		am_ca_pr(LOG_ERROR,"ca_free device no open\n");
		return -1;
	}
	if (!pdev->channels[index].used) {
		pthread_mutex_unlock(&devLock);
		am_ca_pr(LOG_ERROR, "ca_free have been closed\n");
		return -1;
	}
	pi.index = index;
	pi.pid = -1;
	if (ioctl(pdev->fd, CA_SET_PID, &pi) == -1) {
		am_ca_pr(LOG_ERROR,"free chan failed \"%s\"", strerror(errno));
		pthread_mutex_unlock(&devLock);
		return -1;
	}
	pdev->channels[index].used = 0;
	pthread_mutex_unlock(&devLock);
	am_ca_pr(LOG_DEBUG,"ca_free_chan exit\n");

	return 0;
}

/**
 * Set the key to the descrambler channel.
 * \param devno The CA device number.
 * \param chan_index The descrambler channel's index to be set.
 * The index is allocated by the function ca_alloc_chan.
 * \param parity The key's parity.
 * This parameter is defined as "enum ca_sc2_key_type".
 * \param key_handle The key's handle.
 * The key is allocated and set by the CAS/CI+ TA.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_set_key (int devno, int chan_index, int parity, int key_handle)
{
	dsc_dev *pdev = NULL;
	int index = 0;
	int fd;
	dsc_channel *pchan = NULL;
	struct ca_descr_ex param;
	int alg_type = 0;
	int mode = 0;

	(void)key_handle;

	am_ca_pr(LOG_DEBUG,"ca_set_key enter\n");

	if (devno >= MAX_DSC_DEV || devno < 0) {
		am_ca_pr(LOG_ERROR,"ca_set_key chan dev invalid:%d\n", devno);
		return -1;
	}

	index = chan_index - devno * DSC_CHANNEL_COUNT;
	if (index < 0 || index > DSC_CHANNEL_COUNT) {
		am_ca_pr(LOG_ERROR,"ca_set_key chan index invalid:%d\n", chan_index);
		return -1;
	}

	pthread_mutex_lock(&devLock);
	pdev = &rec_dsc_dev[devno];
	if (!pdev->used) {
		pthread_mutex_unlock(&devLock);
		am_ca_pr(LOG_ERROR, "ca_set_key device no open\n");
		return -1;
	}
	pchan = &pdev->channels[index];
	if (!pchan->used) {
		pthread_mutex_unlock(&devLock);
		am_ca_pr(LOG_ERROR, "ca_set_key chan invalid\n");
		return -1;
	}

	if (pchan->algo == CA_ALGO_AES_CBC_CLR_END) {
		mode = CA_DSC_CBC;
		if (parity ==  CA_KEY_EVEN_TYPE)
			alg_type = CA_CW_AES_EVEN;
		else if (parity ==  CA_KEY_ODD_TYPE)
			alg_type = CA_CW_AES_ODD;
		else if (parity == CA_KEY_EVEN_IV_TYPE)
			alg_type = CA_CW_AES_EVEN_IV;
		else if (parity == CA_KEY_ODD_IV_TYPE)
			alg_type = CA_CW_AES_ODD_IV;
		else
			goto error;
	} else {
		mode = CA_DSC_ECB;
		if (parity == CA_KEY_EVEN_TYPE)
			alg_type = CA_CW_DES_EVEN;
		else if (parity == CA_KEY_ODD_TYPE)
			alg_type = CA_CW_DES_ODD;
		else
			goto error;
	}

	fd = pdev->fd;
	memset(&param,0,sizeof(struct ca_descr_ex));
	param.index = index;
	param.flags = CA_CW_FROM_KL;
	param.type = alg_type;
	param.mode = mode;
	if (ioctl(fd, CA_SET_DESCR_EX, &param) == -1) {
		am_ca_pr(LOG_ERROR, "ca_set_key ioctl fail %s\n", strerror(errno));
		goto error;
	}
	pthread_mutex_unlock(&devLock);
	am_ca_pr(LOG_DEBUG,"ca_set_key exit\n");
	return 0;

error:
	am_ca_pr(LOG_ERROR, "ca_set_key fail\n");
	pthread_mutex_unlock(&devLock);
	return -1;
}

/**
 * Set the key to the descrambler channel.
 * \param devno The CA device number.
 * \param chan_index The descrambler channel's index to be set.
 * The index is allocated by the function ca_alloc_chan.
 * \param parity The key's parity.
 * \param key_len The key's length.
 * \param key The key's content.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_set_cw_key(int devno, int chan_index, int parity, int key_len, char *key)
{
	dsc_dev *pdev = NULL;
	int index = 0;
	int fd;
	dsc_channel *pchan = NULL;
	struct ca_descr_ex param;
	int alg_type = 0;
	int mode = 0;
	int max_key_len = 16;
	int i = 0;

	am_ca_pr(LOG_DEBUG,"ca_set_cw_key enter, key_len:%d\n", key_len);
	for (i = 0; i < key_len; i++)
		am_ca_pr(LOG_DEBUG,"0x%0x ", key[i]);
	am_ca_pr(LOG_DEBUG,"\n");

	if (devno >= MAX_DSC_DEV || devno < 0) {
		am_ca_pr(LOG_ERROR,"ca_set_cw_key chan dev invalid:%d\n", devno);
		return -1;
	}

	index = chan_index - devno * DSC_CHANNEL_COUNT;
	if (index < 0 || index > DSC_CHANNEL_COUNT) {
		am_ca_pr(LOG_ERROR,"ca_set_cw_key chan index invalid:%d\n", chan_index);
		return -1;
	}

	pthread_mutex_lock(&devLock);
	pdev = &rec_dsc_dev[devno];
	if (!pdev->used) {
		am_ca_pr(LOG_ERROR, "ca_set_cw_key device no open\n");
		pthread_mutex_unlock(&devLock);
		return -1;
	}
	pchan = &pdev->channels[index];
	if (!pchan->used) {
		am_ca_pr(LOG_ERROR, "ca_set_key chan invalid\n");
		pthread_mutex_unlock(&devLock);
		return -1;
	}
	if (pchan->algo == CA_ALGO_AES_CBC_CLR_END) {
		mode = CA_DSC_CBC;
		if (parity ==  CA_KEY_EVEN_TYPE)
			alg_type = CA_CW_AES_EVEN;
		else if (parity ==  CA_KEY_ODD_TYPE)
			alg_type = CA_CW_AES_ODD;
		else if (parity == CA_KEY_EVEN_IV_TYPE)
			alg_type = CA_CW_AES_EVEN_IV;
		else if (parity == CA_KEY_ODD_IV_TYPE)
			alg_type = CA_CW_AES_ODD_IV;
		else
			goto error;
	} else {
		mode = CA_DSC_ECB;
		if (parity == CA_KEY_EVEN_TYPE)
			alg_type = CA_CW_DES_EVEN;
		else if (parity == CA_KEY_ODD_TYPE)
			alg_type = CA_CW_DES_ODD;
		else
			goto error;
	}

	fd = pdev->fd;
	memset(&param,0,sizeof(struct ca_descr_ex));
	if (key_len < max_key_len)
		max_key_len = key_len;

	memcpy(param.cw, key, max_key_len);
	param.index = index;
	param.flags = 0;
	param.type = alg_type;
	param.mode = mode;
	if (ioctl(fd, CA_SET_DESCR_EX, &param) == -1) {
		am_ca_pr(LOG_ERROR, "ca_set_cw_key ioctl fail %s\n", strerror(errno));
		goto error;
	}
	pthread_mutex_unlock(&devLock);
	am_ca_pr(LOG_DEBUG,"ca_set_cw_key exit\n");
	return 0;

error:
	am_ca_pr(LOG_ERROR, "ca_set_cw_key ioctl fail\n");
	pthread_mutex_unlock(&devLock);
	return -1;
}

/**
 * Close the CA device.
 * \param devno The CA device number.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_close (int devno)
{
	int fd;
	dsc_dev *pdev = NULL;

	am_ca_pr(LOG_DEBUG,"ca_close enter\n");
	if (devno >= MAX_DSC_DEV || devno < 0)
		return -1;
	pthread_mutex_lock(&devLock);
	pdev = &rec_dsc_dev[devno];

	if (!pdev->used) {
		pthread_mutex_unlock(&devLock);
		am_ca_pr(LOG_ERROR, "ca_close %d not used\n", devno);
		return 0;
	}
	pdev->ref--;
	if (pdev->ref == 0) {
		fd = pdev->fd;
		if (fd != -1) {
			close(fd);
		}
		memset(pdev, 0, sizeof(dsc_dev));
		pdev->fd = -1;
	}
	pthread_mutex_unlock(&devLock);
	am_ca_pr(LOG_DEBUG,"ca_close exit\n");
	return 0;
}

