/*
 * $Id: nexus_multicast.c,v 1.5 2008/10/10 05:36:42 stuart_hc Exp $
 *
 * Utility to multicast video frames from dvs_sdi ring buffer to network
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <signal.h>
#include <unistd.h>	
#include <sys/ipc.h>
#include <sys/shm.h>

#include <unistd.h>
#include <sys/times.h>

#include "nexus_control.h"
#include "multicast_video.h"
#include "multicast_compressed.h"
#include "video_conversion.h"
#include "video_test_signals.h"


int verbose = 1;

static char *framesToStr(int tc, char *s)
{
	int frames = tc % 25;
	int hours = (int)(tc / (60 * 60 * 25));
	int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
	int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

	if (tc < 0 || frames < 0 || hours < 0 || minutes < 0 || seconds < 0
		|| hours > 59 || minutes > 59 || seconds > 59 || frames > 24)
		sprintf(s, "             ");
		//sprintf(s, "* INVALID *  ");
	else
		sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}

static void dvsaudio32_to_16bitmono(int channel, uint8_t *buf32, uint8_t *buf16)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    int channel_offset = 0;
    if (channel == 1)
        channel_offset = 4;

    // Skip every other channel, copying 16 most significant bits of 32 bits
    // from little-endian DVS format to little-endian 16bits
    for (i = channel_offset; i < 1920*4*2; i += 8) {
        *buf16++ = buf32[i+2];
        *buf16++ = buf32[i+3];
    }
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: nexus_multicast [options] address:port\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c channel    send video from specified SDI channel [default 0]\n");
    fprintf(stderr, "    -s WxH        size of scaled down image to transmit [default 240x192 (1/9 picture)]\n");
    fprintf(stderr, "                  (use 360x288 for 1/4-picture)\n");
    fprintf(stderr, "                  (use 180x144 for 1/16-picture)\n");
    fprintf(stderr, "    -t            send compressed MPEG-TS stream suitable for VLC playback\n");
    fprintf(stderr, "    -b kps        MPEG-2 video bitrate to use for compressed MPEG-TS [default 3500 kbps]\n");
    fprintf(stderr, "    -q            quiet operation (fewer messages)\n");
	exit(1);
}

extern int main(int argc, char *argv[])
{
	int				shm_id, control_id;
	uint8_t			*ring[MAX_CHANNELS];
	NexusControl	*pctl = NULL;
	int				channelnum = 0;
	int				bitrate = 3500;
	int				opt_size = 0;
	int				mpegts = 0;
	int				out_width = 240, out_height = 192;
	char			*address = NULL;
	int				fd = -1;

	int n;
	for (n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "-q") == 0)
		{
			verbose = 0;
		}
		else if (strcmp(argv[n], "-c") == 0)
		{
			if (n+1 >= argc ||
				sscanf(argv[n+1], "%d", &channelnum) != 1 ||
				channelnum > 7 || channelnum < 0)
			{
				fprintf(stderr, "-c requires integer channel number {0...7}\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-s") == 0)
		{
			if (n+1 >= argc ||
				sscanf(argv[n+1], "%dx%d", &out_width, &out_height) != 2)
			{
				fprintf(stderr, "-s requires size in the form WxH\n");
				return 1;
			}
			opt_size = 1;
			n++;
		}
		else if (strcmp(argv[n], "-b") == 0)
		{
			if (n+1 >= argc ||
				sscanf(argv[n+1], "%d", &bitrate) != 1)
			{
				fprintf(stderr, "-b requires bitrate in bps\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-t") == 0)
		{
			mpegts = 1;
		}
		else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
		{
			usage_exit();
		}
		else {
			address = argv[n];
		}
	}

	if (address == NULL) {
		usage_exit();
	}

	if (mpegts && opt_size == 0) {
		// default MPEG-TS to fullsize picture
		out_width = 720;
		out_height = 576;
	}

	/* Parse address - udp://@224.1.0.50:1234 or 224.1.0.50:1234 forms */
	int port;
	char remote[FILENAME_MAX], *p;
	if ((p = strchr(address, '@')) != NULL)
		strcpy(remote, p+1);				/* skip 'udp://@' portion */
	else
		if ((p = strrchr(address, '/')) != NULL)
			strcpy(remote, p+1);			/* skip 'udp://' portion */
		else
			strcpy(remote, address);

	if ((p = strchr(remote, ':')) == NULL) {	/* get the port number */
		port = 2000;		/* default port to 1234 */
	}
	else {
		port = atol(p+1);
		*p = '\0';
	}

	// If shared memory not found, sleep and try again
	if (verbose) {
		printf("Waiting for shared memory... ");
		fflush(stdout);
	}

	while (1)
	{
		control_id = shmget(9, sizeof(*pctl), 0444);
		if (control_id != -1)
			break;
		usleep(20 * 1000);
	}

	pctl = (NexusControl*)shmat(control_id, NULL, SHM_RDONLY);
	if (verbose)
		printf("connected to pctl\n");

	if (verbose)
		printf("  channels=%d elementsize=%d ringlen=%d\n",
				pctl->channels,
				pctl->elementsize,
				pctl->ringlen);

	if (channelnum+1 > pctl->channels)
	{
		printf("  channelnum not available\n");
		return 1;
	}

	int i;
	for (i = 0; i < pctl->channels; i++)
	{
		while (1)
		{
			shm_id = shmget(10 + i, pctl->elementsize, 0444);
			if (shm_id != -1)
				break;
			usleep(20 * 1000);
		}
		ring[i] = (uint8_t*)shmat(shm_id, NULL, SHM_RDONLY);
		if (verbose)
			printf("  attached to channel[%d]\n", i);
	}

	NexusBufCtl *pc;
	pc = &pctl->channel[channelnum];
	int tc, ltc, signal_ok;
	int last_saved = -1;
	int width = pctl->width;
	int height = pctl->height;

	// get the alternative video frame (4:2:0 or 4:2:2 planar)
	int video_offset = pctl->sec_video_offset;
	int video_422yuv = 0;
	if (pctl->sec_video_format == Format422PlanarYUV || pctl->sec_video_format == Format422PlanarYUVShifted)
		video_422yuv = 1;
	printf("Using secondary video buffer with format %s\n", nexus_capture_format_name(pctl->sec_video_format));

	uint8_t *scaled_frame = (uint8_t *)malloc(out_width * out_height * 3/2);

	// setup blank video and audio buffers
	uint8_t *blank_video_uyvy = (uint8_t *)malloc(out_width * out_height * 2);
	uyvy_no_video_frame(out_width, out_height, blank_video_uyvy);
	uint8_t *blank_video = (uint8_t *)malloc(out_width * out_height * 3/2);
	uyvy_to_yuv420_nommx(out_width, out_height, 0, blank_video_uyvy, blank_video);
	free(blank_video_uyvy);
	uint8_t blank_audio[1920*2*2];
	memset(blank_audio, 0, sizeof(blank_audio));

	mpegts_encoder_t *ts = NULL;
	if (mpegts) {
		char url[64];
		// create string suitable for ffmpeg url_fopen() e.g. udp://239.255.1.1:2000
		sprintf(url, "udp://%s:%d", remote, port);

		// setup MPEG-TS encoder
		if ((ts = mpegts_encoder_init(url, out_width, out_height, bitrate, 4)) == NULL) {
			return 1;
		}
	}
	else {
		// open socket for uncompressed multicast
		if ((fd = open_socket_for_streaming(remote, port)) == -1) {
			exit(1);
		}
	}

	while (1)
	{
		if (last_saved == pc->lastframe) {
			usleep(2 * 1000);		// 0.020 seconds = 50 times a sec
			continue;
		}

		int diff_to_last = pc->lastframe - last_saved;
		if (diff_to_last != 1) {
			printf("\ndiff_to_last = %d\n", diff_to_last);
		}

		tc = *(int*)(ring[channelnum] + pctl->elementsize *
									(pc->lastframe % pctl->ringlen)
							+ pctl->vitc_offset);
		ltc = *(int*)(ring[channelnum] + pctl->elementsize *
									(pc->lastframe % pctl->ringlen)
							+ pctl->ltc_offset);
		signal_ok = *(int*)(ring[channelnum] + pctl->elementsize *
									(pc->lastframe % pctl->ringlen)
							+ pctl->signal_ok_offset);

		// get video and audio pointers
		uint8_t *video_frame = ring[channelnum] + video_offset +
								pctl->elementsize * (pc->lastframe % pctl->ringlen);
		uint8_t *audio_dvs_fmt = ring[channelnum] + pctl->audio12_offset +
								pctl->elementsize * (pc->lastframe % pctl->ringlen);

		uint8_t audio[1920*2*2];
		uint8_t *p_video, *p_audio;

		if (signal_ok) {
			if (width != out_width || height != out_height) {
				// scale down video suitable for multicast
				if (video_422yuv)
					scale_video422_for_multicast(width, height, out_width, out_height, video_frame, scaled_frame);
				else
					scale_video420_for_multicast(width, height, out_width, out_height, video_frame, scaled_frame);
				p_video = scaled_frame;
			}
			else {
				p_video = video_frame;
			}

			// reformat audio to two mono channels one after the other
			// i.e. 1920 samples of channel 0, followed by 1920 samples of channel 1
			dvsaudio32_to_16bitmono(0, audio_dvs_fmt, audio);
			dvsaudio32_to_16bitmono(1, audio_dvs_fmt, audio + 1920*2);
			p_audio = audio;
		}
		else {
			p_video = blank_video;
			p_audio = blank_audio;
		}

		// Send the frame.
		if (ts) {
			mpegts_encoder_encode(ts,
									p_video,
									(int16_t*)p_audio,
									pc->lastframe);
		}
		else {
			send_audio_video(fd, out_width, out_height, 2,
									p_video,
									p_audio,
									pc->lastframe, tc, ltc, pc->source_name);
		}

		if (verbose) {
			char tcstr[32], ltcstr[32];
			printf("\rcam%d lastframe=%d %s  tc=%10d  %s   ltc=%11d  %s ",
					channelnum, pc->lastframe, signal_ok ? "ok" : "--",
					tc, framesToStr(tc, tcstr), ltc, framesToStr(ltc, ltcstr));
			fflush(stdout);
		}

		last_saved = pc->lastframe;
	}

	return 0;
}
