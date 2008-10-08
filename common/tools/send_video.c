/*
* send_video.c - Send uncompressed video as multicast UDP
*
* Copyright (C) 2003 Stuart Cunningham <stuart_hc@users.sourceforge.net>
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
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>			/* for getopt */
#include <sys/time.h>

#include "multicast_video.h"
#include "YUV_scale_pic.h"



/* Compile with
	gcc -g -W -Wall -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -o send_video send_video.c
*/

static struct timeval	start_time;

static double tv_diff_microsecs(const struct timeval* a, const struct timeval* b)
{
	int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
	return (double)diff;
}


static void usage(void)
{
	fprintf(stderr, "Usage:\n\tsend_video [options] address[:port]\n\n");
	fprintf(stderr, "\tE.g. send_video 239.255.1.1:2000\n\n");
	fprintf(stderr, "\t-b    benchmark multicast transmission (no video input file required)\n");
	fprintf(stderr, "\t-i    specify a different input video file [default out.yuv]\n");
	fprintf(stderr, "\t-t    specify a pre-prepared MPEG-TS input file\n");
	fprintf(stderr, "\t-a    specify a 48kHz stereo audio wav input file\n");
	fprintf(stderr, "\t-r n  limit transmit rate to n kbps\n");
	fprintf(stderr, "\t-h    input size is HD 1080i [default input is 720x576]\n");
	fprintf(stderr, "\t-j    input is 4:2:2 YUV [default input is 4:2:0 YUV]\n");
	fprintf(stderr, "\t-s x  scale down streamed video by e.g. 2 for 360x288, 3 for 240x192\n");
	fprintf(stderr, "\t      [default is x1 i.e. 720x576]\n");
	fprintf(stderr, "\t-d    turn on debugging\n");
	fprintf(stderr, "\t-l    limit number of video frames sent\n");
	exit(1);
}

extern int main(int argc, char *argv[])
{
	char				remote[4096], *p,
						*input_file = "out.yuv",
						*audio_input_file = NULL,
						*source_name = "send_video source";
	FILE				*input = NULL, *fp_audio = NULL;
	int					c, fd, port,
						use_limit = 0,
						mpegts_input = 0,
						verbose = 0, benchmark = 0;
	uint64_t			limit = 0;
	int					rate_kbps_limit = 0;
	int					scale = 1;
	int					debug = 0;
	int					input_422yuv = 0;
	int					in_width = 720, in_height = 576;

	// letters followed by ':' means the option requires an argument
	while ((c = getopt(argc, argv, ":dbvi:t:a:r:l:s:hj")) != -1)
	{
		switch(c) {
		case 'd':
			debug = 1;
			break;
		case 'b':
			benchmark = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'i':
			input_file = optarg;
			break;
		case 't':
			input_file = optarg;
			mpegts_input = 1;
			break;
		case 'a':
			audio_input_file = optarg;
			break;
		case 'r':
			rate_kbps_limit = strtoll(optarg, NULL, 10);
			break;
		case 'l':
			limit = strtoll(optarg, NULL, 10);
			use_limit = 1;
			break;
		case 's':
			scale = strtoll(optarg, NULL, 10);
			break;
		case 'h':
			in_width = 1920;
			in_height = 1080;
			break;
		case 'j':
			input_422yuv = 1;
			break;
		case ':':
			printf("-%c requires an argument\n", optopt);
			usage();
			break;
		default:
		case '?':
			printf("unknown option %c\n", optopt);
			usage();
			break;
		}
	}

	if (argc - optind != 1)
		usage();

	/* Parse address - udp://@224.1.0.50:1234 or 224.1.0.50:1234 forms */
	if ((p = strchr(argv[optind], '@')) != NULL)
		strcpy(remote, p+1);				/* skip 'udp://@' portion */
	else
		if ((p = strrchr(argv[optind], '/')) != NULL)
			strcpy(remote, p+1);			/* skip 'udp://' portion */
		else
			strcpy(remote, argv[optind]);

	if ((p = strchr(remote, ':')) == NULL) {	/* get the port number */
		port = 2000;		/* default port to 1234 */
	}
	else {
		port = atol(p+1);
		*p = '\0';
	}

	/* Setup input data */
	int buf_size = in_width * in_height * 3/2;		// input video size at 4:2:0
	if (input_422yuv)
		buf_size = in_width * in_height * 2;		// input video size at 4:2:2
	int audio_buf_size = 1920*2 * 2;				// 48kHz, 16bit, 2 channels
	int out_width = in_width;
	int out_height = in_height;
	if (scale > 1) {
		out_width /= scale;
		out_height /= scale;
	}
	int video_buf_size = out_width * out_height * 3/2;
	if (mpegts_input) {
		buf_size = 188;
		video_buf_size = 188;
		if (scale != 1 || audio_input_file != NULL) {
			printf("options incompatible with MPEG-TS transmission\n");
			exit(1);
		}
	}

	unsigned char *buf = malloc(buf_size);
	unsigned char *audio = malloc(audio_buf_size);

	if (!mpegts_input) {
		printf("Scaling input video %dx%d to %dx%d\n", in_width, in_height, out_width, out_height);
	}

	// video input
	if (benchmark) {
		int i;
		for (i = 0; i < buf_size; i++) {
			buf[i] = i % 256;
			if (i % 188 == 0)
				buf[i] = 0x47;
		}
	}
	else {
		if ( (input = fopen(input_file, "rb")) == NULL) {
			perror("fopen");
			exit(1);
		}
	}

	// audio input
	if (!benchmark && audio_input_file) {
		if ( (fp_audio = fopen(audio_input_file, "rb")) == NULL) {
			perror("fopen");
			exit(1);
		}
		// skip over WAV header
		if (fseek(fp_audio, 44, SEEK_SET) == -1) {
			perror("fseek");
			exit(1);
		}
	}
	else {
		// dummy audio
		int i;
		for (i = 0; i < audio_buf_size; i+=2) {
			audio[i+0] = i % 256;
			audio[i+1] = i % 256;
		}
	}

	/*** network setup ***/
	if ((fd = open_socket_for_streaming(remote, port)) == -1) {
		exit(1);
	}

	// debug: save scaled video to debug.yuv
	FILE *output = NULL;
	if (debug) {
		if ((output = fopen("debug.yuv", "wb")) == NULL) {
			perror("fopen");
			exit(1);
		}
	}

	// start rate clock
	gettimeofday(&start_time, NULL);
	
	uint64_t packets = 0;
	uint64_t total_written = 0;
	uint8_t *p_video = malloc(video_buf_size);

	while (1)
	{
		if (benchmark) {
			// TODO: increment counter
			unsigned short tmp = (unsigned short)packets;
			memcpy(&buf[1], &tmp, sizeof(tmp));
		}
		else {
			read_video_frame:
			if (fread(buf, buf_size, 1, input) != 1) {
				if (feof(input)) {
					rewind(input);		// loop over input
					goto read_video_frame;
				}
				perror("fread");
				exit(1);
			}

			if (fp_audio) {
				uint8_t tmp[audio_buf_size];
				int i;
				read_audio_frame:
				if (fread(tmp, audio_buf_size, 1, fp_audio) != 1) {
					if (feof(fp_audio)) {
						fseek(fp_audio, 44, SEEK_SET);	// seek to start of audio samples
						goto read_audio_frame;
					}
					perror("fread");
					exit(1);
				}
				// de-interleave stereo audio into a mono channel followed by second mono channel
				for (i = 0; i < audio_buf_size; i+=4) {
					audio[i/2 + 0] = tmp[i + 0];
					audio[i/2 + 1] = tmp[i + 1];
					audio[audio_buf_size/2 + i/2 +0] = tmp[i + 2];
					audio[audio_buf_size/2 + i/2 +1] = tmp[i + 3];
				}
			}
		}

		if (scale > 1) {
			if (input_422yuv)
				scale_video422_for_multicast(in_width, in_height, out_width, out_height, buf, p_video);
			else
				scale_video420_for_multicast(in_width, in_height, out_width, out_height, buf, p_video);
		}
		else {
			p_video = buf;
		}

		if (debug) {
			if (fwrite(p_video, video_buf_size, 1, output) != 1) {
				perror("fwrite");
				exit(1);
			}
		}

		ssize_t bytes_written;
		if (mpegts_input) {
			bytes_written = send(fd, p_video, buf_size, 0);
		}
		else {
			bytes_written = send_audio_video(fd, out_width, out_height, 2,
									p_video, audio,
									(int)packets,					// frame number
									(int)packets, (int)packets, source_name);
		}

		packets++;
		total_written += bytes_written;

		// Get times for logging and rate limiting
		struct timeval now_time;
		gettimeofday(&now_time, NULL);
		double diff = tv_diff_microsecs(&start_time, &now_time);

		if (packets % 5 == 0)
		{
			double rate = total_written / diff;				// megabytes per sec

			printf("\rtotal_written = %12llu, av rate=%10.3fMbps (%10.3fMBps)", total_written, rate * 8, rate);
			fflush(stdout);
		}

		double needed_time;
		if (rate_kbps_limit) {
			needed_time = total_written * 8.0 / (rate_kbps_limit * 1000) * 1000000;
		}
		else {
			// limit sending rate to 25fps
			needed_time = packets / 25.0 * 1000000;		// times are all in microseconds
		}
		double sleep_time = needed_time - diff;
		if (sleep_time > 0)
			usleep(sleep_time);

		// optional limit on total number of "packets"
		if (limit == packets)
			break;
	}

	free(buf);
	free(audio);
	free(p_video);

	return 0;
}
