/*
 * $Id: test_browse_encoder.c,v 1.1 2008/07/08 16:47:10 philipn Exp $
 *
 * Use to test the browse_encoder.
 *
 * Copyright (C) 2007 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include "browse_encoder.h"

#ifndef M_PI
#define M_PI 3.1415926535897931
#endif

float t, tincr, tincr2;
int16_t audio_samples[1920*2];

extern int main (int argc, char **argv)
{
	browse_encoder_t *dvd;
	int frame_num = 0;
	FILE *vfp;
	FILE *afp;
	int video_frame_size = 720 * 576 * 1.5;
	uint8_t video_samples[(int)(720*576*1.5)];
	int frames_read;
	int idx = 1;
    struct timeval now, prev;

	if (argc < 4)
	{
		fprintf (stderr, "Usage: %s [-notimecode] planar_yuv_420_file wav_file output_mpg_file\n", argv[0]);
		exit(-1);
	}
	if (strcmp(argv[1], "-notimecode")==0)
	{
		frame_num = -1;
		++idx;
		argc--;
	}
	if (argc != 4)
	{
		fprintf (stderr, "Usage: %s [-notimecode] planar_yuv_420_file wav_file output_mpg_file", argv[0]);
		exit(-1);
	}

	if ((vfp = fopen (argv[idx], "rb")) == NULL)
	{
		perror ("Error: ");
		exit(-1);
	}

	if ((afp = fopen (argv[idx+1], "rb")) == NULL)
	{
		perror ("Error: ");
		exit(-1);
	}

	/* Seek past first 44 bytes in wav file */
	if (fseek(afp, 44L, SEEK_SET) != 0)
	{
		perror ("Error: ");
		exit(-1);
	}
	dvd = browse_encoder_init (argv[idx+2], 2700, 4);
	if (dvd == NULL) {
		return 1;
	}
	
	ArchiveTimecode ltc = {10,0,0,0,0}, vitc = {10,0,0,0,0};

    gettimeofday(&prev, NULL);
	while ((frames_read = fread (video_samples,  video_frame_size, 1, vfp)) == 1)
	{
		if (fread (audio_samples, sizeof(audio_samples), 1, afp) != 1)
			break;
		if (browse_encoder_encode (dvd, video_samples, audio_samples, ltc, vitc, frame_num)!= 0)
			fprintf (stderr, "browse_encoder_encode failed\n");
		if (frame_num >= 0)
			++frame_num;

        if (frame_num % 25 == 0) {
            gettimeofday(&now, NULL);
            printf("fps=%.1f\n", 25.0/(((now.tv_sec - prev.tv_sec) * 1000000 + now.tv_usec - prev.tv_usec) / 1000000.0));
            prev = now;
        }
	}
	browse_encoder_close(dvd);
	fclose(afp);
	fclose(vfp);

	return 0;
}


