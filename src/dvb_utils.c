#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "dvr_types.h"
#include "dvb_utils.h"
#include "dvr_utils.h"

#include <dmx.h>

static int ciplus_enable = 0;

/**
 * Enable/disable CIplus mode.
 * \param enable Enable/disable.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int dvb_enable_ciplus(int enable)
{
    int out;
    char buf[32];

    ciplus_enable = enable;

    if (dvr_check_dmx_isNew())
        return 0;

    if (enable)
    {
        int i;

        out = 0;

        for (i = 0; i < 3; i ++)
        {
            DVB_DemuxSource_t src;

            dvb_get_demux_source(i, &src);
            if (src != DVB_DEMUX_SOURCE_DMA0)
                out |= 1 << i;
        }
    }
    else
    {
        out = 8;
    }

    snprintf(buf, sizeof(buf), "%d", out);
    dvr_file_echo("/sys/class/dmx/ciplus_output_ctrl", buf);

    return 0;
}

/**
 * Set the demux's input source.
 * \param dmx_idx Demux device's index.
 * \param src The demux's input source.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int dvb_set_demux_source(int dmx_idx, DVB_DemuxSource_t src)
{
    char node[32];
    struct stat st;
    int r;

    snprintf(node, sizeof(node), "/sys/class/stb/demux%d_source", dmx_idx);

    r = stat(node, &st);
    if (r == -1)
    {
        int fd, source, input = 0;
        memset(node, 0, sizeof(node));
        snprintf(node, sizeof(node), "/dev/dvb0.demux%d", dmx_idx);
        fd = open(node, O_WRONLY);
        if (fd != -1)
        {
            if (src >= DVB_DEMUX_SOURCE_TS0 &&
                src <= DVB_DEMUX_SOURCE_TS7) {
                source = FRONTEND_TS0 + src - DVB_DEMUX_SOURCE_TS0;
                input = INPUT_DEMOD;
            } else if (src >= DVB_DEMUX_SOURCE_DMA0 &&
                src <= DVB_DEMUX_SOURCE_DMA7) {
                source = DMA_0 + src - DVB_DEMUX_SOURCE_DMA0;
                input = INPUT_LOCAL;
            } else if (src >= DVB_DEMUX_SECSOURCE_DMA0 &&
                src <= DVB_DEMUX_SECSOURCE_DMA7) {
                source = DMA_0 + src - DVB_DEMUX_SECSOURCE_DMA0;
                input = INPUT_LOCAL_SEC;
            } else if (src >= DVB_DEMUX_SOURCE_DMA0_1 &&
                src <= DVB_DEMUX_SOURCE_DMA7_1) {
                source = DMA_0_1 + src - DVB_DEMUX_SOURCE_DMA0_1;
                input = INPUT_LOCAL;
            } else if (src >= DVB_DEMUX_SECSOURCE_DMA0_1 &&
                src <= DVB_DEMUX_SECSOURCE_DMA7_1) {
                source = DMA_0_1 + src - DVB_DEMUX_SECSOURCE_DMA0_1;
                input = INPUT_LOCAL_SEC;
            } else if (src >= DVB_DEMUX_SOURCE_TS0_1 &&
                src <= DVB_DEMUX_SOURCE_TS7_1) {
                source = FRONTEND_TS0_1 + src - DVB_DEMUX_SOURCE_TS0_1;
                input = INPUT_DEMOD;
            } else {
                assert(0);
            }

            if (ioctl(fd, DMX_SET_INPUT, input) == -1)
            {
                 DVR_INFO("dvb_set_demux_source ioctl DMX_SET_INPUT:%d error:%d", input, errno);
                 r = -1;
            }
            else
            {
                 DVR_INFO("dvb_set_demux_source ioctl succeeded src:%d DMX_SET_INPUT:%d dmx_idx:%d", src, input, dmx_idx);
                 r = 0;
            }
            if (ioctl(fd, DMX_SET_HW_SOURCE, source) == -1)
            {
                DVR_INFO("dvb_set_demux_source ioctl DMX_SET_HW_SOURCE:%d error:%d", source, errno);
                r = -1;
            }
            else
            {
                DVR_INFO("dvb_set_demux_source ioctl succeeded src:%d DMX_SET_HW_SOURCE:%d dmx_idx:%d", src, source, dmx_idx);
                r = 0;
            }
            close(fd);
        }
        else
        {
            DVR_INFO("dvb_set_demux_source open \"%s\" failed, error:%d", node, errno);
        }
    }
    else
    {
        char *val;

        if (ciplus_enable)
        {
            char buf[32];
            int i, out;

            out = 0;

            for (i = 0; i < 3; i ++)
            {
                DVB_DemuxSource_t dmx_src;

                if (i == dmx_idx)
                    dmx_src = src;
                else
                    dvb_get_demux_source(i, &dmx_src);
                if (dmx_src != DVB_DEMUX_SOURCE_DMA0)
                    out |= 1 << i;
            }

            snprintf(buf, sizeof(buf), "%d", out);
            dvr_file_echo("/sys/class/dmx/ciplus_output_ctrl", buf);
        }

        switch (src)
        {
        case DVB_DEMUX_SOURCE_TS0:
            val = "ts0";
            break;
        case DVB_DEMUX_SOURCE_TS1:
            val = "ts1";
            break;
        case DVB_DEMUX_SOURCE_TS2:
            val = "ts2";
            break;
        case DVB_DEMUX_SOURCE_DMA0:
        case DVB_DEMUX_SOURCE_DMA1:
        case DVB_DEMUX_SOURCE_DMA2:
        case DVB_DEMUX_SOURCE_DMA3:
        case DVB_DEMUX_SOURCE_DMA4:
        case DVB_DEMUX_SOURCE_DMA5:
        case DVB_DEMUX_SOURCE_DMA6:
        case DVB_DEMUX_SOURCE_DMA7:
            val = "hiu";
            break;
        default:
            assert(0);
        }

        r = dvr_file_echo(node, val);
    }

    return r;
}

/**
 * Get the demux's input source.
 * \param dmx_idx Demux device's index.
 * \param point src that demux's input source.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int dvb_get_demux_source(int dmx_idx, DVB_DemuxSource_t *src)
{
    char node[32] = {0};
    char buf[32] = {0};
    struct stat st;
    int r, source_no;

    snprintf(node, sizeof(node), "/sys/class/stb/demux%d_source", dmx_idx);
    r = stat(node, &st);
    if (r == -1)
    {
        int fd, source;
        memset(node, 0, sizeof(node));
        snprintf(node, sizeof(node), "/dev/dvb0.demux%d", dmx_idx);
        fd = open(node, O_RDONLY);
        if (fd != -1)
        {
            if (ioctl(fd, DMX_GET_HW_SOURCE, &source) != -1)
            {
                switch (source)
                {
                case FRONTEND_TS0:
                    *src = DVB_DEMUX_SOURCE_TS0;
                    break;
                case FRONTEND_TS1:
                    *src = DVB_DEMUX_SOURCE_TS1;
                    break;
                case FRONTEND_TS2:
                    *src = DVB_DEMUX_SOURCE_TS2;
                    break;
                case FRONTEND_TS3:
                    *src = DVB_DEMUX_SOURCE_TS3;
                    break;
                case FRONTEND_TS4:
                    *src = DVB_DEMUX_SOURCE_TS4;
                    break;
                case FRONTEND_TS5:
                    *src = DVB_DEMUX_SOURCE_TS5;
                    break;
                case FRONTEND_TS6:
                    *src = DVB_DEMUX_SOURCE_TS6;
                    break;
                case FRONTEND_TS7:
                    *src = DVB_DEMUX_SOURCE_TS7;
                    break;
                case DMA_0:
                    *src = DVB_DEMUX_SOURCE_DMA0;
                    break;
                case DMA_1:
                    *src = DVB_DEMUX_SOURCE_DMA1;
                    break;
                case DMA_2:
                    *src = DVB_DEMUX_SOURCE_DMA2;
                    break;
                case DMA_3:
                    *src = DVB_DEMUX_SOURCE_DMA3;
                    break;
                case DMA_4:
                    *src = DVB_DEMUX_SOURCE_DMA4;
                    break;
                case DMA_5:
                    *src = DVB_DEMUX_SOURCE_DMA5;
                    break;
                case DMA_6:
                    *src = DVB_DEMUX_SOURCE_DMA6;
                    break;
                case DMA_7:
                    *src = DVB_DEMUX_SOURCE_DMA7;
                    break;
                case FRONTEND_TS0_1:
                    *src = DVB_DEMUX_SOURCE_TS0_1;
                    break;
                case FRONTEND_TS1_1:
                    *src = DVB_DEMUX_SOURCE_TS1_1;
                    break;
                case FRONTEND_TS2_1:
                    *src = DVB_DEMUX_SOURCE_TS2_1;
                    break;
                case FRONTEND_TS3_1:
                    *src = DVB_DEMUX_SOURCE_TS3_1;
                    break;
                case FRONTEND_TS4_1:
                    *src = DVB_DEMUX_SOURCE_TS4_1;
                    break;
                case FRONTEND_TS5_1:
                    *src = DVB_DEMUX_SOURCE_TS5_1;
                    break;
                case FRONTEND_TS6_1:
                    *src = DVB_DEMUX_SOURCE_TS6_1;
                    break;
                case FRONTEND_TS7_1:
                    *src = DVB_DEMUX_SOURCE_TS7_1;
                    break;
                default:
                    assert(0);
                }
            }
            else
            {
                DVR_INFO("ioctl DMX_GET_HW_SOURCE:%d error:%d", source, errno);
            }
            close(fd);
        }
        else
        {
            DVR_INFO("open \"%s\" failed, error:%d", node, errno);
        }
    }
    else
    {
        r = dvr_file_read(node, buf, sizeof(buf));
        if (r != -1)
        {
            if (strncmp(buf, "ts", 2) == 0 && strlen(buf) == 3)
            {
                sscanf(buf, "ts%d", &source_no);
                switch (source_no)
                {
                case 0:
                    *src = DVB_DEMUX_SOURCE_TS0;
                    break;
                case 1:
                    *src = DVB_DEMUX_SOURCE_TS1;
                    break;
                case 2:
                    *src = DVB_DEMUX_SOURCE_TS2;
                    break;
                default:
                    DVR_INFO("do not support demux source:%s", buf);
                    r = -1;
                    break;
                }
            }
            else if (strncmp(buf, "hiu", 3) == 0)
            {
                *src = DVB_DEMUX_SOURCE_DMA0;
            }
            else
            {
                r = -1;
            }
            DVR_INFO("dvb_get_demux_source \"%s\" :%s", node, buf);
        }
    }

    return r;
}
//check is device platform is used new dmx
int dvr_check_dmx_isNew(void)
{
    char node[32];
    struct stat st;
    int r;

    snprintf(node, sizeof(node), "/sys/class/stb/demux%d_source", 0);

    r = stat(node, &st);
    if (r == -1)
    {
        return 1;
    }
    return 0;
}
