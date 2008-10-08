/*
 * $Id: nexus_save.c,v 1.4 2008/10/08 10:16:06 john_f Exp $
 *
 * Utility to store video frames from dvs_sdi ring buffer to disk files
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

// compile with:
// gcc -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -O3 -o save_mem save_mem.c
//
//
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

#ifdef USE_FFMPEG
#include "../common/ffmpeg_encoder.h"
#endif


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
	else
		sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: save_mem [options] videofile [audiofile]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c channel save video on specified channel [default 0]\n");
#ifdef USE_FFMPEG
    fprintf(stderr, "    -r res     encoder resolution (JPEG,DV25,DV50,IMX30,IMX40,IMX50,\n");
	fprintf(stderr, "               DNX36p,DNX120p,DNX185p,DNX120i,DNX185i,DMIH264) [default is uncompressed]\n");
#endif
    fprintf(stderr, "    -s         save video from the secondary buffer (either 4:2:2 or 4:2:0)\n");
    fprintf(stderr, "    -q         quiet operation (fewer messages)\n");
	exit(1);
}

extern int main(int argc, char *argv[])
{
	int				shm_id, control_id;
	uint8_t			*ring[MAX_CHANNELS];
	NexusControl	*pctl = NULL;
	int				channelnum = 0, sec_video = 0;
	char			*video_file = NULL, *audio_file = NULL;
	FILE			*outfp = NULL, *audiofp = NULL;
#ifdef USE_FFMPEG
	ffmpeg_encoder_resolution_t		res = (ffmpeg_encoder_resolution_t)-1;
#endif

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
#ifdef USE_FFMPEG
		else if (strcmp(argv[n], "-r") == 0)
		{
			if (strcmp(argv[n+1], "JPEG") == 0)
				res = FF_ENCODER_RESOLUTION_JPEG;
			if (strcmp(argv[n+1], "DV25") == 0)
				res = FF_ENCODER_RESOLUTION_DV25;
			if (strcmp(argv[n+1], "DV50") == 0)
				res = FF_ENCODER_RESOLUTION_DV50;
			if (strcmp(argv[n+1], "IMX30") == 0)
				res = FF_ENCODER_RESOLUTION_IMX30;
			if (strcmp(argv[n+1], "IMX40") == 0)
				res = FF_ENCODER_RESOLUTION_IMX40;
			if (strcmp(argv[n+1], "IMX50") == 0)
				res = FF_ENCODER_RESOLUTION_IMX50;
			if (strcmp(argv[n+1], "DNX36p") == 0)
				res = FF_ENCODER_RESOLUTION_DNX36p;
			if (strcmp(argv[n+1], "DNX120p") == 0)
				res = FF_ENCODER_RESOLUTION_DNX120p;
			if (strcmp(argv[n+1], "DNX185p") == 0)
				res = FF_ENCODER_RESOLUTION_DNX185p;
			if (strcmp(argv[n+1], "DNX120i") == 0)
				res = FF_ENCODER_RESOLUTION_DNX120i;
			if (strcmp(argv[n+1], "DNX185i") == 0)
				res = FF_ENCODER_RESOLUTION_DNX185i;
			if (strcmp(argv[n+1], "DMIH264") == 0)
				res = FF_ENCODER_RESOLUTION_DMIH264;
			if (res == -1)
				usage_exit();
			n++;
		}
#endif
		else if (strcmp(argv[n], "-s") == 0) {
			sec_video = 1;
		}
		else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0) {
			usage_exit();
		}
		else {
			if (video_file == NULL) {
				video_file = argv[n];
			}
			else if (audio_file == NULL) {
				audio_file = argv[n];
			}
			else {
				usage_exit();
			}
		}
	}

	if (video_file == NULL) {
		usage_exit();
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
			printf("  attached to channel[%d], pri=%s, sec=%s\n", i,
						nexus_capture_format_name(pctl->pri_video_format),
						nexus_capture_format_name(pctl->sec_video_format));
	}

	NexusBufCtl *pc;
	pc = &pctl->channel[channelnum];
	int tc, ltc;
	int last_saved = -1;
	int width = pctl->width;
	int height = pctl->height;

	// Default to primary 4:2:2 video
 	int frame_size = width*height*2;
	int video_offset = 0;
	int audio_offset = pctl->audio12_offset;
	int audio_size = pctl->audio_size;

	if (sec_video) {
		if (pctl->sec_video_format == FormatNone) {
			printf("sec_video_format is FormatNone, cannot save secondary buffer\n");
			return 1;
		}

		// get the alternative video frame (4:2:0 or 4:2:2 planar)
		if (pctl->sec_video_format == Format422PlanarYUV || pctl->sec_video_format == Format422PlanarYUVShifted)
			frame_size = width*height*2;
		else
			frame_size = width*height*3/2;
		video_offset = pctl->sec_video_offset;
	}

	if ((outfp = fopen(video_file, "wb")) == NULL) {
		perror("fopen for write of video file");
		return 1;
	}
	if (audio_file)
		if ((audiofp = fopen(audio_file, "wb")) == NULL) {
			perror("fopen for write of audio file");
			return 1;
		}

#ifdef USE_FFMPEG
	ffmpeg_encoder_t *ffmpeg_encoder = NULL;
	uint8_t *out = NULL;
	if (res != -1) {
		// Initialise ffmpeg encoder
		if ((ffmpeg_encoder = ffmpeg_encoder_init(res)) == NULL) {
			fprintf(stderr, "ffmpeg encoder init failed\n");
			return 1;
		}

		out = (uint8_t *)malloc(frame_size);	// worst case compressed size
	}

	// Check that video buffer is compatible
	switch (res) {
	case FF_ENCODER_RESOLUTION_JPEG:
	case FF_ENCODER_RESOLUTION_DV50:
	case FF_ENCODER_RESOLUTION_IMX30:
	case FF_ENCODER_RESOLUTION_IMX40:
	case FF_ENCODER_RESOLUTION_IMX50:
	case FF_ENCODER_RESOLUTION_DNX36p:
	case FF_ENCODER_RESOLUTION_DNX120p:
	case FF_ENCODER_RESOLUTION_DNX185p:
	case FF_ENCODER_RESOLUTION_DNX120i:
	case FF_ENCODER_RESOLUTION_DNX185i:
		if (pctl->pri_video_format != Format422PlanarYUV) {
			fprintf(stderr, "specified encoder resolution requires primary format of Format422PlanarYUV\n");
			return 1;
		}
		break;
	case FF_ENCODER_RESOLUTION_DMIH264:
		if (pctl->sec_video_format != Format420PlanarYUV) {
			fprintf(stderr, "specified encoder resolution requires secondary format of Format420PlanarYUV\n");
			return 1;
		}
	case FF_ENCODER_RESOLUTION_DV25:
		if (pctl->sec_video_format != Format420PlanarYUVShifted) {
			fprintf(stderr, "specified encoder resolution requires secondary format of Format420PlanarYUVShifted\n");
			return 1;
		}
		video_offset = pctl->sec_video_offset;
		break;
	}
#endif

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

		uint8_t *video_frame = ring[channelnum] + video_offset +
								pctl->elementsize * (pc->lastframe % pctl->ringlen);
		uint8_t *audio_frame = ring[channelnum] + audio_offset +
								pctl->elementsize * (pc->lastframe % pctl->ringlen);

#ifdef USE_FFMPEG
		if (ffmpeg_encoder) {
			int compressed_size = ffmpeg_encoder_encode(ffmpeg_encoder, video_frame, &out);
			if ( fwrite(out, compressed_size, 1, outfp) != 1 ) {
					perror("fwrite video");
					return(1);
			}
		}
		else {
			if (fwrite(video_frame, frame_size, 1, outfp) != 1) {
				perror("fwrite video");
				return 1;					
			}
			if (audiofp && fwrite(audio_frame, audio_size, 1, audiofp) != 1) {
				perror("fwrite audio");
				return 1;					
			}
		}
#else
		if (fwrite(video_frame, frame_size, 1, outfp) != 1) {
			perror("fwrite video");
			return 1;					
		}
		if (audiofp && fwrite(audio_frame, audio_size, 1, audiofp) != 1) {
			perror("fwrite audio");
			return 1;					
		}
#endif

		if (verbose) {
			char tcstr[32], ltcstr[32];
			printf("\rcam%d lastframe=%d  tc=%10d  %s   ltc=%11d  %s ",
					channelnum, pc->lastframe,
					tc, framesToStr(tc, tcstr), ltc, framesToStr(ltc, ltcstr));
			fflush(stdout);
		}

		last_saved = pc->lastframe;
	}

	return 0;
}
