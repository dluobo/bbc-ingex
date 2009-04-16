#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_test_signals.h"
#include "video_conversion_10bits.h"
#include "video_conversion.h"
#include "video_burn_in_timecode.h"
#include "psnr.h"

static void usage_exit(void)
{
	fprintf(stderr, "unittests\n");
	fprintf(stderr, "\n");
	exit(1);
}

static void read_sample(const char *input_name, size_t frame_size, uint8_t *frame)
{
	FILE *fp_input = NULL;

	if ((fp_input = fopen(input_name, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", input_name);
		exit(1);
	}

	size_t r;
	if ((r = fread(frame, 1, frame_size, fp_input)) != frame_size) {
		fprintf(stderr, "Expected %zu, read %zu\n", frame_size, r);
		perror("fread");
		exit(1);
	}

	fclose(fp_input);
}

static void write_sample(const char *output_name, size_t frame_size, const uint8_t *frame)
{
	FILE *fp_output = NULL;

	if ((fp_output = fopen(output_name, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", output_name);
		exit(1);
	}

	if (fwrite(frame, 1, frame_size, fp_output) != frame_size) {
		perror("fwrite");
		exit(1);
	}

	fclose(fp_output);
}

int main(int argc, char *argv[])
{
	int width = 720;
	int height = 576;
	int result = 0;
	size_t frame_size = width * height * 2;
	size_t frame_size_10bit = frame_size * 4 / 3;
	char *output_name = "test.uyvy";
	char *input_name = "sport10bit";

	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage_exit();
		}
		fprintf(stderr, "Unrecognied argument %s\n", argv[i]);
		usage_exit();
	}

	uint8_t *frame = (uint8_t*)malloc(frame_size);
	uint8_t *frame2 = (uint8_t*)malloc(frame_size);
	uint8_t *frame10bit = (uint8_t*)malloc(frame_size_10bit);
	uint8_t *frame10bit2 = (uint8_t*)malloc(frame_size_10bit);

	// colourbars and timecode burning
	uyvy_color_bars(width, height, frame);
	burn_mask_uyvy(111222, 90, 90, 720, 576, frame);
	write_sample("c.uyvy", frame_size, frame);
	uyvy_to_yuv422_nommx(width, height, 0, frame, frame2);
	write_sample("c.uyvy.yuv", frame_size, frame2);

	uyvy_to_yuv420_nommx(width, height, 0, frame, frame2);
	write_sample("c.yuv", 720*576*3/2, frame2);

	burn_mask_yuv420(111222, 90, 90, 720, 576, frame2);
	write_sample("cb.yuv", 720*576*3/2, frame2);

	uyvy_color_bars(width, height, frame);
	uyvy_to_yuv422_nommx(width, height, 0, frame, frame2);
	burn_mask_yuv422(111222, 90, 90, 720, 576, frame2);
	write_sample("c422.yuv", 720*576*2, frame2);

	// Test creation of "NO VIDEO" uyvy caption frame
	uyvy_no_video_frame(width, height, frame);
	write_sample(output_name, frame_size, frame);

	// Test 10bit conversions
	read_sample(input_name, frame_size_10bit, frame10bit);

	ConvertFrame10to8(frame, frame10bit, width*2, width*2*4/3, width, height);
	DitherFrame(frame2, frame10bit, width*2, width*2*4/3, width, height);
	double y, u, v;
	psnr_uyvy(frame, frame2, width, height, &y, &u, &v);
	printf("frame(10to8) frame2(Dither 10to8) PSNR: y=%.2f u=%.2f v=%.2f\n", y,u,v);

	write_sample(output_name, frame_size, frame);

	ConvertFrame8to10(frame10bit2, frame, width*2*4/3, width*2, width, height);
	ConvertFrame10to8(frame2, frame10bit2, width*2, width*2*4/3, width, height);

	write_sample("frame2.uyvy", frame_size, frame2);

	if ( memcmp(frame, frame2, frame_size) != 0 ) {
		double y, u, v;
		psnr_uyvy(frame, frame2, width, height, &y, &u, &v);
		printf("frame frame2 mismatch PSNR: y=%.2f u=%.2f v=%.2f\n", y,u,v);
		result = 1;
	}

	// Testing random frame generation
	uyvy_random_frame(width, height, frame);
	for (i = 0; i < 50; i++) {
		uyvy_random_frame(width, height, frame2);
		psnr_uyvy(frame, frame2, width, height, &y, &u, &v);
		// PSNR should be < 9.5 for 720x576 images (based on experiment - the theory is left as an exercise)
		if (y > 9.5 || u > 8.0 || v > 8.0)
			return 1;
	}
	
	// cleanup
	free(frame);
	free(frame2);
	free(frame10bit);
	free(frame10bit2);

	return result;
}
