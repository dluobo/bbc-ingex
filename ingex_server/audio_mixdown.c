/*
 * $Id: audio_mixdown.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Command line utility to mix 4 audio tracks into 2 audio tracks.
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
#include <limits.h>
#include <stdarg.h>

#include "AudioMixer.h"
#include "audio_utils.h"


extern int main(int argc, char *argv[])
{
	FILE		*fp_audio12;
	FILE		*fp_audio34;
	FILE		*fp_audiomix;
	int			frames_delay = 6;

	if (argc != 5) {
		printf("Usage: audio_mixdown {MAIN|ISO} input12.wav input34.wav output16bit.wav\n");
		return 1;
	}

	bool iso_mix = false;
	if (strcmp(argv[1], "ISO") == 0) {
		iso_mix = true;
	}
	const char *audio_name1 = argv[2];
	const char *audio_name2 = argv[3];
	const char *audio_mix = argv[4];

	if ((fp_audio12 = fopen(audio_name1, "rb")) == NULL)
	{
		printf("Could not open %s\n", audio_name1);
		return 1;
	}
	if ((fp_audio34 = fopen(audio_name2, "rb")) == NULL)
	{
		printf("Could not open %s\n", audio_name2);
		return 1;
	}
	fseek(fp_audio12, 44, SEEK_SET);
	fseek(fp_audio34, 44, SEEK_SET);

	if ((fp_audiomix = fopen(audio_mix, "wb")) == NULL)
	{
		printf("Could not open %s\n", audio_mix);
		return 1;
	}

	// Audio is 16-bit signed, little-endian, 48000, 2-channel
	writeWavHeader(fp_audiomix, 16);

	AudioMixer main_mixer(AudioMixer::MAIN);
	AudioMixer iso_mixer(AudioMixer::ISO);

	int		frames_read = 0;
	while (1)
	{
		uint8_t		buf12[1920 * 2 * 4];
		uint8_t		buf34[1920 * 2 * 4];

		if (fread(buf12, 1920*2*4, 1, fp_audio12) != 1) {
			if (feof(fp_audio12))
				break;
			perror("Failed to read from fp_audio12");
			break;
		}
		if (fread(buf34, 1920*2*4, 1, fp_audio34) != 1) {
			if (feof(fp_audio34))
				break;
			perror("Failed to read from fp_audio34");
			break;
		}

		frames_read++;
		if (frames_read <= frames_delay)
		{
			//printf("frames_read <= frames_delay (%d <= %d)\n", frames_read, frames_delay);
			continue;
		}
	
		const int32_t	*p_audio12 = (int32_t*) buf12;
		const int32_t	*p_audio34 = (int32_t*) buf34;
		int16_t			mixed[1920*2];	// stereo pair output

		// mix audio
		if (iso_mix)
			iso_mixer.Mix(p_audio12, p_audio34, mixed, 1920);
		else
			main_mixer.Mix(p_audio12, p_audio34, mixed, 1920);

		// save uncompressed audio
		if (fwrite(mixed, sizeof(mixed), 1, fp_audiomix) != 1) {
			perror("fwrite");
			return 1;
		}
	}

	if (frames_delay > 0) {
		int16_t		zero[1920*2] = {0};
		for (int i = 0; i < frames_delay; i++) {
			
			//printf("padding with zeros frame (i=%d)\n", i);
			if (fwrite(zero, sizeof(zero), 1, fp_audiomix) != 1) {
				perror("fwrite");
				return 1;
			}
		}
	}

	// update and close files
	fclose(fp_audio12);
	fclose(fp_audio34);
	update_WAV_header(fp_audiomix);		// also closes file	

	return 0;
}
