/*
* receive_video.c - Receive uncompressed video over multicast
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "multicast_video.h"
#include <audio_utils.h>

/* Compile with
	gcc -g -W -Wall -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -o receive_video receive_video.c
*/

static void usage(void)
{
	fprintf(stderr, "Usage:\n\treceive_video [options] address[:port]\n\n");
	fprintf(stderr, "\tE.g. receive_video 239.255.1.1:2000\n\n");
	fprintf(stderr, "\t-o    specify a different video output file [default rec.yuv]\n");
	fprintf(stderr, "\t      playback with ... -rawvideo size=69120:w=240:h=192\n");
	fprintf(stderr, "\t                        -rawvideo size=155520:w=360:h=288\n");
	fprintf(stderr, "\t                        -rawvideo size=622080:w=720:h=576\n");
	fprintf(stderr, "\t-a    specify a different audio output file [default rec.wav]\n");
	fprintf(stderr, "\t-t    timeout in decimal seconds for frame read [default 0.045]\n");
	fprintf(stderr, "\t-l    limit in bytes of the saved video file before program exits\n");
	fprintf(stderr, "\t-u    update frequency in terms of packets [default 50]\n");
	fprintf(stderr, "\t-r    read raw packets without interpreting them\n");
	fprintf(stderr, "\t-q    quiet\n");
	fprintf(stderr, "\t-v    increase verbosity [default 1]\n");
	exit(1);
}

extern int main(int argc, char *argv[])
{
	char				remote[4096], *p, *video_file = "rec.yuv", *audio_file = "rec.wav";
	FILE				*fp_video = NULL, *fp_audio = NULL;
	int					c, fd, port, verbose = 1, raw_read = 0;
	uint64_t			video_file_limit = UINT64_MAX;
	double				timeout = 0.045;		// 1/2 frame

	while ((c = getopt(argc, argv, ":qvro:a:t:l:h")) != -1)
	{
		switch(c) {
		case 'q':
			verbose = 0;
			break;
		case 'v':
			verbose++;
			break;
		case 'r':
			raw_read = 1;
			break;
		case 'o':
			video_file = optarg;
			break;
		case 'a':
			audio_file = optarg;
			break;
		case 't':
			timeout = strtod(optarg, NULL);
			break;
		case 'l':
			video_file_limit = strtoll(optarg, NULL, 10);
			break;
		case ':':
			printf("-%c requires an argument\n", optopt);
			usage();
			break;
		default:
		case 'h':
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
		port =1234;		/* default port to 1234 */
	}
	else {
		port = atol(p+1);
		*p = '\0';
	}

	/*** network setup ***/
	if ((fd = connect_to_multicast_address(remote, port)) == -1) {
		exit(1);
	}

	// Raw read from socket, ignoring any structure
	if (raw_read) {
		if ((fp_video = fopen(video_file, "wb")) == NULL) {
			perror("fopen video");
			exit(1);
		}
		while (raw_read) {
			uint8_t buf[5000];
			ssize_t bytes_read = recv(fd, buf, PACKET_SIZE, 0);
			printf("\rRead %zd bytes", bytes_read);

			if (fwrite(buf, bytes_read, 1, fp_video) != 1) {
				perror("fwrite");
				exit(1);
			}
		}
	}

	// Read video parameters from multicast stream.
	// This allows us to allocate enough space for video and audio buffers.
	IngexNetworkHeader header;
	if (udp_read_frame_header(fd, &header) == -1) {
		printf("Could not read video parameters from stream\n");
		exit(1);
	}

	if ((fp_video = fopen(video_file, "wb")) == NULL) {
		perror("fopen video");
		exit(1);
	}
	if ((fp_audio = fopen(audio_file, "wb")) == NULL) {
		perror("fopen audio");
		exit(1);
	}
	// write 16bits per sample, 2 channel WAV header
	if (! writeWavHeader(fp_audio, 16, 2)) {
		printf("Could not write WAV header\n");
		exit(1);
	}

	int video_size = header.width * header.height * 3/2;
	int audio_size = header.audio_size;
	uint8_t *video = malloc(video_size);
	uint8_t *audio = malloc(audio_size);
	uint8_t *tmp_audio = malloc(audio_size);

	// Clear video and audio buffers since network reads may return
	// incomplete frames
	memset(video, 0, video_size);
	memset(audio, 0, audio_size);

#ifndef MULTICAST_SINGLE_THREAD
	udp_reader_thread_t udp_reader;
	udp_reader.fd = fd;
	if (! udp_init_reader(header.width, header.height, &udp_reader)) {
		return 1;
	}
#endif

	int packet_count = 0;
	uint64_t video_file_size = 0;
	while (1)
	{
		int packets_read = 0;
		int i;

#ifdef MULTICAST_SINGLE_THREAD
		int res = udp_read_frame_audio_video(fd, timeout, &header, video, audio, &packets_read);
#else
		int res = udp_read_next_frame(&udp_reader, timeout, &header, video, audio, &packets_read);
#endif
		if (res == -1) {
			printf("timeout reading frame\n");
			continue;
		}

		// write video to file
		if (fwrite(video, video_size, 1, fp_video) != 1) {
			perror("fwrite video");
			exit(1);
		}
		video_file_size += video_size;

		// reformat audio into interleaved 16bit sample stereo for WAV file
		for (i = 0; i < audio_size; i+=4) {
			tmp_audio[i + 0] = audio[i/2 + 0];
			tmp_audio[i + 1] = audio[i/2 + 1];
			tmp_audio[i + 2] = audio[audio_size/2 + i/2 +0];
			tmp_audio[i + 3] = audio[audio_size/2 + i/2 +1];
		}

		// write audio samples to file
		if (fwrite(tmp_audio, audio_size, 1, fp_audio) != 1) {
			perror("fwrite audio");
			exit(1);
		}

		if (verbose) {
			if (verbose > 1)
				printf("packet=%5d frame_number=%d packets_read=%d video_file_size=%"PRIu64"\n", packet_count, header.frame_number, packets_read, video_file_size);
			else {
				printf("\rpacket=%5d frame_number=%d packets_read=%d video_file_size=%"PRIu64"", packet_count, header.frame_number, packets_read, video_file_size);
				fflush(stdout);
			}
		}

		// Check optional video file limit
		if (video_file_size >= video_file_limit) {
			if (verbose)
				printf("\nvideo file limit reached\n");
			break;
		}

		packet_count++;
	}

	free(video);
	free(audio);
	free(tmp_audio);

#ifndef MULTICAST_SINGLE_THREAD
	udp_shutdown_reader(&udp_reader);
#endif

	return 0;
}
