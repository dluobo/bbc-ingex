// Must set _LARGEFILE_SOURCE before including stdio.h
#if ! defined(_LARGEFILE_SOURCE) && ! defined(_FILE_OFFSET_BITS)
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/times.h>
#include "mjpeg_compress.h"
#include "../../../common/video_conversion.h"

#ifdef BUILD_USING_MJPEG_THREADED_API
#define mjpeg_compress_t mjpeg_compress_threaded_t
#define mjpeg_compress_init(a,b,c,d) mjpeg_compress_init_threaded(a,b,c,d)
#define mjpeg_compress_frame_yuv(a,b,c,d,e,f,g,h) mjpeg_compress_frame_yuv_threaded(a,b,c,d,e,f,g,h)
#define mjpeg_compress_free(a) mjpeg_compress_free_threaded(a)
#endif

static void usage_exit(void)
{
    fprintf(stderr, "Usage: yuv422_to_mjpeg [options] input.yuv output.mjpeg\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -l frame limit       number of frames to process\n");
    fprintf(stderr, "    -r <number>          Internal Avid resolution Id number:\n");
    fprintf(stderr, "                           76 - 2:1     77 - 3:1\n");
    fprintf(stderr, "                           75 - 10:1    78 - 15:1s\n");
    fprintf(stderr, "                           82 - 20:1   110 - 10:1m\n");
    fprintf(stderr, "                          111 - 4:1m\n");
    fprintf(stderr, "                           (default)\n");
    fprintf(stderr, "    -b benchmark-num     read first frame and compress it num times\n");
    fprintf(stderr, "    -u                   input video is UYVY (not YUV planar)\n");
	exit(1);
}

extern int main(int argc, char *argv[])
{
	char				*input_name = NULL, *output_name = NULL;
	FILE				*input_fp, *output_fp;
	unsigned int		width = 720, height = 576;
	int					n;
	int					resId = 82;			// 20:1 is default
	int					benchmark = 0;		// off by default
	int					frame_limit = 0;	// off by default
	int					input_is_uyvy = 0;	// flag to treat input as UYVY


	// process command-line args
	for (n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
		{
			usage_exit();
		}
		else if (strcmp(argv[n], "-r") == 0)			// resolution id
		{
			if (sscanf(argv[n+1], "%u", &resId) != 1)
				usage_exit();
			n++;
		}
		else if (strcmp(argv[n], "-l") == 0)
		{
			if (sscanf(argv[n+1], "%u", &frame_limit) != 1)
				usage_exit();
			n++;
		}
		else if (strcmp(argv[n], "-b") == 0)
		{
			if (sscanf(argv[n+1], "%u", &benchmark) != 1)
				usage_exit();
			frame_limit = 1;
			n++;
		}
		else if (strcmp(argv[n], "-u") == 0)
		{
			input_is_uyvy = 1;
		}
		else
		{
			if (!input_name)
			{
				input_name = argv[n];
				continue;
			}
			if (!output_name)
			{
				output_name = argv[n];
				continue;
			}
		}
	}
	if (!input_name || !output_name) {
		usage_exit();
	}

	unsigned frame_size = width*height*2;

	uint8_t *orig_frame = (uint8_t*)malloc(frame_size);
	uint8_t *frame = orig_frame;
	uint8_t *tmp_frame = NULL;
	if (input_is_uyvy) {
		tmp_frame = (uint8_t*)malloc(frame_size);
	}

	if ((input_fp = fopen(input_name, "rb")) == NULL) {
		fprintf(stderr, "Could not open %s\n", input_name);
		exit(1);
	}

	if ((output_fp = fopen(output_name, "wb")) == NULL) {
		fprintf(stderr, "Could not open %s\n", output_name);
		exit(1);
	}

	mjpeg_compress_t *mc = NULL;
	mjpeg_compress_init((MJPEGResolutionID)resId, width, height, &mc);

	int total_bytes = 0;
	int frame_num = 0;
	while (1) {
		if (input_is_uyvy) {
			frame = tmp_frame;
		}

		// Read a frame from input
		if (fread(frame, 1, frame_size, input_fp) < frame_size) {
			if (feof(input_fp))		// exhausted input
				break;
			fprintf(stderr, "fread failed to read frame from %s\n", input_name);
			return 1;
		}

		// convert video if in UYVY format
		if (input_is_uyvy) {
			uyvy_to_yuv422(width, height, 0, tmp_frame, orig_frame);
			frame = orig_frame;
		}

		// Compress frame to JPEG
		uint8_t *y = frame;
		uint8_t *u = frame + width * height;
		uint8_t *v = frame + width * height * 3/2;
		uint8_t *outputframe = NULL;
		unsigned outsize;

		// Compress full frame (or one field depending upon resId)
		int i = 0;
		do {
			//struct tms buf1, buf2;
			//times(&buf1);
			if ((outsize = mjpeg_compress_frame_yuv(mc, y, u, v, width, width / 2, width / 2, &outputframe)) == 0) {
				fprintf(stderr, "mjpeg_compress_frame_yuv failed\n");
				return 1;
			}
			//times(&buf2);
			if (benchmark > 0)
				printf("%d of %d\n", i+1, benchmark);
				//printf("%d of %d (%ld)\n", i, benchmark, buf2.tms_utime - buf1.tms_utime);
			i++;
		} while (i < benchmark);

		// Save compressed field to disk
		if (fwrite(outputframe, 1, outsize, output_fp) < outsize) {
			fprintf(stderr, "fwrite failed to write frame\n");
			return 1;
		}

		total_bytes += outsize;
		printf("Saved frame %d size=%u avg_bitrate=%.2fMbps\n", frame_num, outsize, (total_bytes * 8.0 / ((frame_num+1) * 0.04)) / 1000000.0 );

		frame_num++;
		if (frame_num == frame_limit) {
			printf("frame limit %d reached\n", frame_limit);
			break;
		}
	}

	fclose(input_fp);
	fclose(output_fp);

	mjpeg_compress_free(mc);

	free(orig_frame);
	if (input_is_uyvy)
		free(tmp_frame);

	printf("Total compressed size=%u, frames=%d (bytes per frame=%.2f)\n", total_bytes, frame_num, (double)total_bytes / (double)frame_num);
	return 0;
}
