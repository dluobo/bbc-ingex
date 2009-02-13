#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <sys/time.h>

#include "../common/ffmpeg_encoder.h"

static int usage(char *argv[])
{
	fprintf(stderr, "Usage: %s [-r res] input.{uyvy,yuv} output.raw\n\n", argv[0]);
	fprintf(stderr, "    -b n_frames    read one frame then loop n_frames\n");
	fprintf(stderr, "    -r <res>       encoder resolution (JPEG,DV25,DV50,IMX30,IMX40,IMX50,\n");
	fprintf(stderr, "                   DNX36p,DNX120p,DNX185p,DNX120i,DNX185i)\n");
	return 1;
}

static double tv_diff_now_us(const struct timeval* last)
{
	struct timeval *now, now_buf;
	now = &now_buf;
	gettimeofday(now, NULL);

	uint64_t diff = (now->tv_sec - last->tv_sec) * 1000000 + now->tv_usec - last->tv_usec;
	return (double)diff;
}

extern int main(int argc, char *argv[])
{
	FILE			*input_fp, *output_fp;
	uint8_t			*in, *out;
	int				width = 720, height = 576;
	int				benchmark_encode = 0;
	int				n, res = -1;

	n = 1;
	while (n < argc)
	{
		if (strcmp(argv[n], "-b") == 0)
		{
			benchmark_encode = atoi(argv[n+1]);
			if (benchmark_encode < 1)
				return usage(argv);
			n += 2;
		}
		if (strcmp(argv[n], "-r") == 0)
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
			if (strcmp(argv[n+1], "DNX36p") == 0) {
				res = FF_ENCODER_RESOLUTION_DNX36p;
				width = 1920; height = 1080;
			}
			if (strcmp(argv[n+1], "DNX120p") == 0) {
				res = FF_ENCODER_RESOLUTION_DNX120p;
				width = 1920; height = 1080;
			}
			if (strcmp(argv[n+1], "DNX185p") == 0) {
				res = FF_ENCODER_RESOLUTION_DNX185p;
				width = 1920; height = 1080;
			}
			if (strcmp(argv[n+1], "DNX120i") == 0) {
				res = FF_ENCODER_RESOLUTION_DNX120i;
				width = 1920; height = 1080;
			}
			if (strcmp(argv[n+1], "DNX185i") == 0) {
				res = FF_ENCODER_RESOLUTION_DNX185i;
				width = 1920; height = 1080;
			}
			if (strcmp(argv[n+1], "DMIH264") == 0)
				res = FF_ENCODER_RESOLUTION_DMIH264;
			if (res == -1)
				return usage(argv);
			n += 2;
			continue;
		}
		break;
	}

	if (benchmark_encode) {
		if ((argc - n) != 1)
			return usage(argv);
	}
	else {
		if ((argc - n) != 2)
			return usage(argv);
	}

	if (res == -1)
		return usage(argv);

	// Open input file
	if ( (input_fp = fopen(argv[n], "rb")) == NULL) {
		perror(argv[n]);
		return 1;
	}

	// Initialise ffmpeg encoder
    ffmpeg_encoder_t *ffmpeg_encoder = ffmpeg_encoder_init(res, THREADS_USE_BUILTIN_TUNING);
	if (!ffmpeg_encoder) {
		fprintf(stderr, "ffmpeg encoder init failed\n");
		return 1;
	}

	int unc_frame_size = width*height*2;
	if (res == FF_ENCODER_RESOLUTION_DV25)
		unc_frame_size = width*height*3/2;

	in = (uint8_t *)malloc(unc_frame_size);
	out = (uint8_t *)malloc(unc_frame_size);	// worst case compressed size

	// Open output file
	if (! benchmark_encode)
		if ( (output_fp = fopen(argv[n+1], "wb")) == NULL) {
			perror(argv[n+1]);
			return 1;
		}

	int frames_encoded = 0;
	uint64_t total_size = 0;
	struct timeval start_time;
	gettimeofday(&start_time, NULL);

	while (1)
	{
		if (benchmark_encode && frames_encoded != 0) {
			if (frames_encoded == benchmark_encode)
				break;
		}
		else {
			if ( fread(in, unc_frame_size, 1, input_fp) != 1 ) {
				if (feof(input_fp))
					break;
	
				perror(argv[n]);
				return(1);
			}
		}

		int compressed_size = ffmpeg_encoder_encode(ffmpeg_encoder, in, &out);

		if (! benchmark_encode)
			if ( fwrite(out, compressed_size, 1, output_fp) != 1 ) {
				perror(argv[n+1]);
				return(1);
			}
		frames_encoded++;
		total_size += compressed_size;
	}

	printf("frames encoded = %d (%.2f sec), compressed size = %"PRIu64" (%.2fMbps), time = %.2f\n",
			frames_encoded, frames_encoded / 25.0,
			total_size, total_size / frames_encoded * 25.0 * 8 / 1000,
			tv_diff_now_us(&start_time) / 1000000.0);

	fclose(input_fp);
	if (! benchmark_encode)
		fclose(output_fp);
	free(in);
	free(out);

	return 0;
}
