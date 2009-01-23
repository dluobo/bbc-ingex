/*
 * $Id: dvsoem_dummy.c,v 1.5 2009/01/23 20:10:51 john_f Exp $
 *
 * Implement a debug-only DVS hardware library for testing.
 *
 * Copyright (C) 2007  Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../../video_burn_in_timecode.h"
#include "../../video_test_signals.h"
#include "../../audio_utils.h"

#include "dummy_include/dvs_clib.h"
#include "dummy_include/dvs_fifo.h"

// The sv_handle structure looks like this
// typedef struct {
//   int	magic;
//   int	size;
//   int	version;
//   int	vgui;
//   int	prontovideo;
//   int	debug;
//   int	pad[10];
// } sv_handle;
//

// Represents whether LTC, VITC or both are being generated
typedef enum {
	DvsTcAll,
	DvsTcLTC,
	DvsTcVITC
} DvsTcQuality;

// The DvsCard structure stores hardware parameters
// used to simulate an hardware SDI I/O card
typedef struct {
	int card;
	int channel;
	int width;
	int height;
	int frame_size;
	int frame_count;
	int dropped;
	int source_input_frames;
	DvsTcQuality tc_quality;
	struct timeval start_time;
	sv_fifo_buffer fifo_buffer;
	unsigned char *source_dmabuf;
#ifdef DVSDUMMY_LOGGING
	FILE *log_fp;
#endif
} DvsCard;

static inline void write_bcd(unsigned char* target, int val)
{
    *target = ((val / 10) << 4) + (val % 10);
}
static int int_to_dvs_tc(int tc)
{
	// e.g. turn 1020219 into hex 0x11200819
	int fr = tc % 25;
	int hr = (int)(tc / (60*60*25));
	int mi = (int)((tc - (hr * 60*60*25)) / (60 * 25));
	int se = (int)((tc - (hr * 60*60*25) - (mi * 60*25)) / 25);

	unsigned char raw_tc[4];
	write_bcd(raw_tc + 0, fr);
	write_bcd(raw_tc + 1, se);
	write_bcd(raw_tc + 2, mi);
	write_bcd(raw_tc + 3, hr);
	int result = *(int *)(raw_tc);
	return result;
}

// Represent the colour and position of a colour bar
typedef struct {
	double          position;
	unsigned char   colour[4];
} bar_colour_t;

static int64_t tv_diff_microsecs(const struct timeval* a, const struct timeval* b)
{
	int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
	return diff;
}

// SV implementations
int sv_fifo_init(sv_handle * sv, sv_fifo ** ppfifo, int bInput, int bShared, int bDMA, int flagbase, int nframes)
{
	return SV_OK;
}
int sv_fifo_free(sv_handle * sv, sv_fifo * pfifo)
{
	return SV_OK;
}
int sv_fifo_start(sv_handle * sv, sv_fifo * pfifo)
{
	return SV_OK;
}
int sv_fifo_getbuffer(sv_handle * sv, sv_fifo * pfifo, sv_fifo_buffer ** pbuffer, sv_fifo_bufferinfo * bufferinfo, int flags)
{
	DvsCard *dvs = (DvsCard*)sv;
	*pbuffer = &dvs->fifo_buffer;

	return SV_OK;
}
int sv_fifo_putbuffer(sv_handle * sv, sv_fifo * pfifo, sv_fifo_buffer * pbuffer, sv_fifo_bufferinfo * bufferinfo)
{
	DvsCard *dvs = (DvsCard*)sv;

	// Initialise start_time for first call
	if (dvs->start_time.tv_sec == 0)
		gettimeofday(&dvs->start_time, NULL);

	// For record, copy video + audio to dma.addr
	int source_offset = dvs->frame_count % dvs->source_input_frames;
	memcpy(pbuffer->dma.addr, dvs->source_dmabuf + source_offset * dvs->frame_size, dvs->frame_size);

	// Update timecodes
	dvs->frame_count++;
	int dvs_tc = int_to_dvs_tc(dvs->frame_count);
	pbuffer->timecode.vitc_tc = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
	pbuffer->timecode.vitc_tc2 = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
	pbuffer->timecode.ltc_tc = (dvs->tc_quality == DvsTcVITC) ? 0 : dvs_tc;
	pbuffer->anctimecode.dvitc_tc[0] = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
	pbuffer->anctimecode.dvitc_tc[1] = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
	pbuffer->anctimecode.dltc_tc = (dvs->tc_quality == DvsTcVITC) ? 0 : dvs_tc;

	// Burn timecode in video at x,y offset 40,40
	burn_mask_uyvy(dvs->frame_count, 40, 40, dvs->width, dvs->height, (unsigned char *)pbuffer->dma.addr);

	// Update hardware values
	pbuffer->control.tick = dvs->frame_count * 2;
	struct timeval now_time;
	gettimeofday(&now_time, NULL);
	pbuffer->control.clock_high = now_time.tv_sec;
	pbuffer->control.clock_low = now_time.tv_usec;

	// Simulate typical behaviour of capture fifo by sleeping until next
	// expected frame boundary. The simulated hardware clock is the system
	// clock, with frames captured at every 40000 microsecond boundary since
	// the initialisation of dvs->start_time.
	int64_t diff = tv_diff_microsecs(&dvs->start_time, &now_time);
	int64_t expected_diff = 40000 * dvs->frame_count;
	if (expected_diff > diff)
		usleep(expected_diff - diff);

#ifdef DVSDUMMY_LOGGING
	if (dvs->log_fp)
		fprintf(dvs->log_fp, "sv_fifo_putbuffer() frame_count=%d diff=%lld expected_diff=%lld slept=%lld\n",
			dvs->frame_count, diff, expected_diff, (expected_diff > diff) ? expected_diff - diff : 0);
#endif

	return SV_OK;
}
int sv_fifo_stop(sv_handle * sv, sv_fifo * pfifo, int flags)
{
	return SV_OK;
}
int sv_fifo_status(sv_handle * sv, sv_fifo * pfifo, sv_fifo_info * pinfo)
{
	DvsCard *dvs = (DvsCard*)sv;

	// Clear structure
	memset(pinfo, 0, sizeof(sv_fifo_info));

	// Record dropped frames
	pinfo->dropped = dvs->dropped;

	return SV_OK;
}
int sv_fifo_reset(sv_handle * sv, sv_fifo * pfifo)
{
	return SV_OK;
}

int sv_query(sv_handle * sv, int cmd, int par, int *val)
{
	*val = SV_OK;
	return SV_OK;
}
int sv_option_set(sv_handle * sv, int code, int val)
{
	return SV_OK;
}
int sv_option_get(sv_handle * sv, int code, int *val)
{
	*val = SV_OK;
	return SV_OK;
}
int sv_currenttime(sv_handle * sv, int brecord, int *ptick, uint32 *pclockhigh, uint32 *pclocklow)
{
	return SV_OK;
}
int sv_version_status( sv_handle * sv, sv_version * version, int versionsize, int deviceid, int moduleid, int spare)
{
	sprintf(version->module, "driver");
	version->release.v.major = 3;
	version->release.v.minor = 2;
	version->release.v.patch = 1;
	version->release.v.fix = 0;
	return SV_OK;
}
int sv_videomode(sv_handle * sv, int videomode)
{
	return SV_OK;
}
int sv_sync(sv_handle * sv, int sync)
{
	return SV_OK;
}

int sv_openex(sv_handle ** psv, char * setup, int openprogram, int opentype, int timeout, int spare)
{
	int card = 0, channel = 0;

	// Parse card string
	if (sscanf(setup, "PCI,card=%d,channel=%d", &card, &channel) == 2) {
		if (card > 3)
			return SV_ERROR_DEVICENOTFOUND;
		if (channel > 1)
			return SV_ERROR_DEVICENOTFOUND;
	}
	else if (sscanf(setup, "PCI,card=%d", &card) == 1) {
		if (card > 3)
			return SV_ERROR_DEVICENOTFOUND;
	}
	else {
		return SV_ERROR_SVOPENSTRING;
	}

	// Allocate private DVS card structure and return it as if it were a sv_handle*
	DvsCard *dvs = malloc(sizeof(DvsCard));
	dvs->card = card;
	dvs->channel = channel;
	dvs->frame_count = 0;
	dvs->start_time.tv_sec = 0;
	dvs->start_time.tv_usec = 0;
	dvs->dropped = 0;
	dvs->tc_quality = DvsTcAll;
	*psv = (sv_handle*)dvs;

	// Default is PAL width & height
	dvs->width = 720;
	dvs->height = 576;

	// A ring buffer can be used for sample video and audio loaded from files
	FILE *fp_video = NULL, *fp_audio = NULL;
	int64_t vidfile_size = 0;
	dvs->source_input_frames = 1;

	// Check DVSDUMMY_PARAM environment variable and change defaults if necessary.
	// The value is a ':' separated list of parameters such as:
	//
	// video=1920x1080
	// good_tc=A/L/V
	// vidfile=filename.uyvy			// UYVY 4:2:2
	// audfile=filename.pcm				// 16bit stereo pair at 48kHz
	const char *p_env = getenv("DVSDUMMY_PARAM");
	if (p_env) {
		// parse ':' separated list
		const char *p;
		do {
			char param[FILENAME_MAX];

			p = strchr(p_env, ':');
			if (p == NULL) {
				strcpy(param, p_env);
			}
			else {
				int len = p - p_env;
				strncpy(param, p_env, len);
				param[len] = '\0';
			}

			printf("param[%s]\n", param);

			if (strncmp(param, "video=", strlen("video=")) == 0) {
				int tmpw, tmph;
				if (sscanf(param, "video=%dx%d", &tmpw, &tmph) == 2) {
					dvs->width = tmpw;
					dvs->height = tmph;
				}
			}
			else if (strncmp(param, "good_tc=", strlen("good_tc=")) == 0) {
				const char *p_qual = param + strlen("good_tc=");
				if (*p_qual == 'L')
					dvs->tc_quality = DvsTcLTC;
				if (*p_qual == 'V')
					dvs->tc_quality = DvsTcVITC;
			}
			else if (strncmp(param, "vidfile=", strlen("vidfile=")) == 0) {
				const char *vidfile = param + strlen("vidfile=");
				if ((fp_video = fopen(vidfile, "rb")) == NULL) {
					fprintf(stderr, "Could not open %s\n", vidfile);
					return SV_ERROR_SVOPENSTRING;
				}
				struct stat buf;
				if (stat(vidfile, &buf) != 0)
					return SV_ERROR_SVOPENSTRING;
				vidfile_size = buf.st_size;
			}
			else if (strncmp(param, "audfile=", strlen("audfile=")) == 0) {
				const char *audfile = param + strlen("audfile=");
				if ((fp_audio = fopen(audfile, "rb")) == NULL) {
					fprintf(stderr, "Could not open %s\n", audfile);
					return SV_ERROR_SVOPENSTRING;
				}
			}

			if (!p)
				break;
			p_env = p + 1;
		} while (*p_env != '\0');
	}

	int video_size = dvs->width * dvs->height * 2;

	// DVS dma transfer buffer is:
	// <video (UYVY)><audio ch 1+2 with fill to 0x4000><audio ch 3+4 with fill>
	dvs->frame_size = video_size + 0x4000 + 0x4000;

	if (fp_video && fp_audio && vidfile_size) {
		int max_memory = 103680000;			// 5 seconds of 720x576
		if (vidfile_size < max_memory)
			dvs->source_input_frames = vidfile_size / video_size;
		else
			dvs->source_input_frames = max_memory / video_size;
	}
	dvs->source_dmabuf = malloc(dvs->frame_size * dvs->source_input_frames);

	if (!fp_video) {
		unsigned char *video = dvs->source_dmabuf;
		unsigned char *audio12 = video + video_size;
		unsigned char *audio34 = audio12 + 0x4000;
	
		// setup colorbars
		uyvy_color_bars(dvs->width, dvs->height, video);
	
		// TODO: create audio tone or click
		int i;
		for (i = 0; i < 0x4000; i += 8) {
	        audio12[i+0] = 0x00;    audio34[i+0] = 0x80;
	        audio12[i+1] = 0x01;    audio34[i+1] = 0x81;
	        audio12[i+2] = 0x02;    audio34[i+2] = 0x82;
	        audio12[i+3] = 0x03;    audio34[i+3] = 0x83;
	        audio12[i+4] = 0x04;    audio34[i+4] = 0x84;
	        audio12[i+5] = 0x05;    audio34[i+5] = 0x85;
	        audio12[i+6] = 0x06;    audio34[i+6] = 0x86;
	        audio12[i+7] = 0x07;    audio34[i+7] = 0x87;
		}
	}
	else {
		int i;
		int audio_size = 1920*2*2;
		unsigned char *video = dvs->source_dmabuf;
		for (i = 0; i < dvs->source_input_frames; i++) {
			if (fread(video, video_size, 1, fp_video) != 1) {
				fprintf(stderr, "Could not read frame %d from video file\n", i);
				return SV_ERROR_SVOPENSTRING;
			}
			uint8_t tmp[audio_size];
			if (fread(tmp, audio_size, 1, fp_audio) != 1) {
				fprintf(stderr, "Could not read frame %d from audio file\n", i);
				return SV_ERROR_SVOPENSTRING;
			}
			pair16bit_to_dvsaudio32(1920, tmp, video + video_size);
			video += dvs->frame_size;
		}
	}

	// Setup fifo_buffer which contains offsets to audio buffers
	dvs->fifo_buffer.audio[0].addr[0] = (char*)video_size;
	dvs->fifo_buffer.audio[0].addr[1] = (char*)(video_size + 0x4000);

#ifdef DVSDUMMY_LOGGING
	// logging for debugging
	char log_path[FILENAME_MAX];
	sprintf(log_path, "/tmp/dvsdummy_card%d_chan%d", card, channel);
	dvs->log_fp = fopen(log_path, "w");
	if (dvs->log_fp == NULL)
		fprintf(stderr, "Warning, fopen(%s) failed\n", log_path);
#endif

	return SV_OK;
}

int sv_close(sv_handle * sv)
{
	DvsCard *dvs = (DvsCard*)sv;
	free(dvs->source_dmabuf);
	return SV_OK;
}


char * sv_geterrortext(int code)
{
	static char *str = "Error string not implemented";
	switch (code) {
		case SV_ERROR_DEVICENOTFOUND:
			return "SV_ERROR_DEVICENOTFOUND. The device could not be found.";
	}
	return str;
}


int sv_status(sv_handle * sv, sv_info * info)
{
	DvsCard *dvs = (DvsCard*)sv;
	info->xsize = dvs->width;
	info->ysize = dvs->height;
	return SV_OK;
}

int sv_storage_status(sv_handle *sv, int cookie, sv_storageinfo * psiin, sv_storageinfo * psiout, int psioutsize, int flags)
{
	return SV_OK;
}

int sv_stop(sv_handle * sv)
{
	return SV_OK;
}

void sv_errorprint(sv_handle * sv, int code)
{
	return;
}

int sv_timecode_feedback(sv_handle * sv, sv_timecode_info* input, sv_timecode_info* output)
{
	return SV_OK;
}
