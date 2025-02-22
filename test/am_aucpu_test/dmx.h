/*
 * dmx.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _UAPI_DVBDMX_H_
#define _UAPI_DVBDMX_H_

#include <linux/types.h>
#ifndef __KERNEL__
#include <time.h>
#endif

#define CONFIG_AMLOGIC_DVB_COMPAT

#define DMX_FILTER_SIZE 16

enum dmx_output
{
	DMX_OUT_DECODER, /* Streaming directly to decoder. */
	DMX_OUT_TAP,     /* Output going to a memory buffer */
			 /* (to be retrieved via the read command).*/
	DMX_OUT_TS_TAP,  /* Output multiplexed into a new TS  */
			 /* (to be retrieved by reading from the */
			 /* logical DVR device).                 */
	DMX_OUT_TSDEMUX_TAP /* Like TS_TAP but retrieved from the DMX device */
};

typedef enum dmx_output dmx_output_t;

typedef enum dmx_input
{
	DMX_IN_FRONTEND, /* Input from a front-end device.  */
	DMX_IN_DVR       /* Input from the logical DVR device.  */
} dmx_input_t;


typedef enum dmx_ts_pes
{
	DMX_PES_AUDIO0,
	DMX_PES_VIDEO0,
	DMX_PES_TELETEXT0,
	DMX_PES_SUBTITLE0,
	DMX_PES_PCR0,

	DMX_PES_AUDIO1,
	DMX_PES_VIDEO1,
	DMX_PES_TELETEXT1,
	DMX_PES_SUBTITLE1,
	DMX_PES_PCR1,

	DMX_PES_AUDIO2,
	DMX_PES_VIDEO2,
	DMX_PES_TELETEXT2,
	DMX_PES_SUBTITLE2,
	DMX_PES_PCR2,

	DMX_PES_AUDIO3,
	DMX_PES_VIDEO3,
	DMX_PES_TELETEXT3,
	DMX_PES_SUBTITLE3,
	DMX_PES_PCR3,

	DMX_PES_OTHER
} dmx_pes_type_t;

#define DMX_PES_AUDIO    DMX_PES_AUDIO0
#define DMX_PES_VIDEO    DMX_PES_VIDEO0
#define DMX_PES_TELETEXT DMX_PES_TELETEXT0
#define DMX_PES_SUBTITLE DMX_PES_SUBTITLE0
#define DMX_PES_PCR      DMX_PES_PCR0


typedef struct dmx_filter
{
	__u8  filter[DMX_FILTER_SIZE];
	__u8  mask[DMX_FILTER_SIZE];
	__u8  mode[DMX_FILTER_SIZE];
} dmx_filter_t;


struct dmx_sct_filter_params
{
	__u16          pid;
	dmx_filter_t   filter;
	__u32          timeout;
	__u32          flags;
#define DMX_CHECK_CRC       1
#define DMX_ONESHOT         2
#define DMX_IMMEDIATE_START 4
#define DMX_KERNEL_CLIENT   0x8000
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_USE_SWFILTER    0x100

/*bit 8~15 for mem sec_level*/
#define DMX_MEM_SEC_LEVEL1   (1 << 10)
#define DMX_MEM_SEC_LEVEL2   (1 << 11)
#define DMX_MEM_SEC_LEVEL3   (1 << 12)
#endif
};

#ifdef CONFIG_AMLOGIC_DVB_COMPAT

enum dmx_input_source {
	INPUT_DEMOD,
	INPUT_LOCAL,
	INPUT_LOCAL_SEC
};

/**
 * struct dmx_non_sec_es_header - non-sec Elementary Stream (ES) Header
 *
 * @pts_dts_flag:[1:0], 01:pts valid, 10:dts valid
 * @pts:	pts value
 * @dts:	dts value
 * @len:	data len
 */
struct dmx_non_sec_es_header {
	__u8 pts_dts_flag;
	__u64 pts;
	__u64 dts;
	__u32 len;
};

/**
 * struct dmx_sec_es_data - sec Elementary Stream (ES)
 *
 * @pts_dts_flag:[1:0], 01:pts valid, 10:dts valid
 * @pts:	pts value
 * @dts:	dts value
 * @buf_start:	buf start addr
 * @buf_end:	buf end addr
 * @data_start: data start addr
 * @data_end: data end addr
 */
struct dmx_sec_es_data {
	__u8 pts_dts_flag;
	__u64 pts;
	__u64 dts;
	__u32 buf_start;
	__u32 buf_end;
	__u32 data_start;
	__u32 data_end;
};

struct dmx_sec_ts_data {
	__u32 buf_start;
	__u32 buf_end;
	__u32 data_start;
	__u32 data_end;
};

enum dmx_audio_format {
	AUDIO_UNKNOWN = 0,	/* unknown media */
	AUDIO_MPX = 1,		/* mpeg audio MP2/MP3 */
	AUDIO_AC3 = 2,		/* Dolby AC3/EAC3 */
	AUDIO_AAC_ADTS = 3,	/* AAC-ADTS */
	AUDIO_AAC_LOAS = 4,	/* AAC-LOAS */
	AUDIO_DTS = 5,		/* DTS */
	AUDIO_MAX
};

struct dmx_mem_info {
	__u32 dmx_total_size;
	__u32 dmx_buf_phy_start;
	__u32 dmx_free_size;
	__u32 dvb_core_total_size;
	__u32 dvb_core_free_size;
	__u32 wp_offset;
	__u64 newest_pts;
};

struct dmx_sec_mem {
	__u32 buff;
	__u32 size;
};
#endif

/**
 * struct dmx_pes_filter_params - Specifies Packetized Elementary Stream (PES)
 *	filter parameters.
 *
 * @pid:	PID to be filtered.
 * @input:	Demux input, as specified by &enum dmx_input.
 * @output:	Demux output, as specified by &enum dmx_output.
 * @pes_type:	Type of the pes filter, as specified by &enum dmx_pes_type.
 * @flags:	Demux PES flags.
 */
struct dmx_pes_filter_params {
	__u16           pid;
	dmx_input_t  input;
	dmx_output_t output;
	dmx_pes_type_t pes_type;
	__u32           flags;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
/*bit 8~15 for mem sec_level*/
#define DMX_MEM_SEC_LEVEL1   (1 << 10)
#define DMX_MEM_SEC_LEVEL2   (1 << 11)
#define DMX_MEM_SEC_LEVEL3   (1 << 12)

/*bit 16~23 for output */
#define DMX_ES_OUTPUT        (1 << 16)
/*set raw mode, it will send the struct dmx_sec_es_data, not es data*/
#define DMX_OUTPUT_RAW_MODE	 (1 << 17)

/*24~31 one byte for audio type, dmx_audio_format_t*/
#define DMX_AUDIO_FORMAT_BIT 24

#endif
};

typedef struct dmx_caps {
	__u32 caps;
	int num_decoders;
} dmx_caps_t;

typedef enum dmx_source {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3,

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	DMX_SOURCE_FRONT0_OFFSET = 100,
	DMX_SOURCE_FRONT1_OFFSET,
	DMX_SOURCE_FRONT2_OFFSET
#endif
} dmx_source_t;

struct dmx_stc {
	unsigned int num;	/* input : which STC? 0..N */
	unsigned int base;	/* output: divisor for stc to get 90 kHz clock */
	__u64 stc;		/* output: stc in 'base'*90 kHz units */
};

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
enum {
	DMA_0 = 0,
	DMA_1,
	DMA_2,
	DMA_3,
	DMA_4,
	DMA_5,
	DMA_6,
	DMA_7,
	FRONTEND_TS0 = 32,
	FRONTEND_TS1,
	FRONTEND_TS2,
	FRONTEND_TS3,
	FRONTEND_TS4,
	FRONTEND_TS5,
	FRONTEND_TS6,
	FRONTEND_TS7,
};

/*define filter mem_info type*/
enum {
	DMX_VIDEO_TYPE = 0,
	DMX_AUDIO_TYPE,
	DMX_SUBTITLE_TYPE,
	DMX_TELETEXT_TYPE,
	DMX_SECTION_TYPE,
};

struct filter_mem_info {
	__u32 type;
	__u32 pid;
	struct dmx_mem_info	filter_info;
};

struct dmx_filter_mem_info {
	__u32 filter_num;
	struct filter_mem_info info[40];
};
#endif

#define DMX_START                _IO('o', 41)
#define DMX_STOP                 _IO('o', 42)
#define DMX_SET_FILTER           _IOW('o', 43, struct dmx_sct_filter_params)
#define DMX_SET_PES_FILTER       _IOW('o', 44, struct dmx_pes_filter_params)
#define DMX_SET_BUFFER_SIZE      _IO('o', 45)
#define DMX_GET_PES_PIDS         _IOR('o', 47, __u16[5])
#define DMX_GET_CAPS             _IOR('o', 48, dmx_caps_t)
#define DMX_SET_SOURCE           _IOW('o', 49, dmx_source_t)
#define DMX_GET_STC              _IOWR('o', 50, struct dmx_stc)
#define DMX_ADD_PID              _IOW('o', 51, __u16)
#define DMX_REMOVE_PID           _IOW('o', 52, __u16)
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_SET_INPUT           _IO('o', 80)
#define DMX_GET_MEM_INFO        _IOR('o', 81, struct dmx_mem_info)
#define DMX_SET_HW_SOURCE       _IO('o', 82)
#define DMX_GET_HW_SOURCE       _IOR('o', 83, int)
#define DMX_GET_FILTER_MEM_INFO _IOR('o', 84, struct dmx_filter_mem_info)
/*just for dvr sec mem, please call before DMX_SET_PES_FILTER*/
#define DMX_SET_SEC_MEM			_IOW('o', 85, struct dmx_sec_mem)
#endif

#endif /* _UAPI_DVBDMX_H_ */
