// $Id: disk_rw_benchmark.c,v 1.2 2009/01/29 06:58:46 stuart_hc Exp $

// fseeko() requires _LARGEFILE_SOURCE defined before including stdio.h
#if ! defined(_LARGEFILE_SOURCE) && ! defined(_FILE_OFFSET_BITS)
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

// gcc -Wall -W -g -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE disk_rw_benchmark.c -o disk_rw_benchmark

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>

#include <signal.h>

static struct timeval	start_time;
static size_t			block_size = 1024*1024;
static uint64_t			total_read = 0, total_written = 0;

static double tv_diff_microsecs(const struct timeval* a, const struct timeval* b)
{
	int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
	return (double)diff;
}

static double tv_diff_now_microsecs(const struct timeval* last)
{
	struct timeval *now, now_buf;
	now = &now_buf;
	gettimeofday(now, NULL);

	int64_t diff = (now->tv_sec - last->tv_sec) * 1000000 + now->tv_usec - last->tv_usec;
	return (double)diff;
}

static void handle_terminate_signals(int sig)
{
	uint64_t total = 0;
	if (total_read > 0)
		total = total_read;
	if (total_written > 0)
		total = total_written;

	double diff = tv_diff_now_microsecs(&start_time);

	printf("\n");
	printf("total time = %f sec, total bytes = %llu, rate = %f\n", diff / 1000000.0, total, total / diff);
   	exit(0);
}

static void usage_exit(void)
{
	fprintf(stderr, "disk_rw_benchmark [-o output] [input]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -l <MByte/s_rate>      limit read/write rate to MByte/s rate\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Before read testing you may wish to execute the following to flush caches\n");
	fprintf(stderr, "  sync\n");
	fprintf(stderr, "  echo 3 > /proc/sys/vm/drop_caches\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	char			*input_file = NULL, *output_file = NULL;
	FILE			*fp_input, *fp_output;
	size_t			num_blocks = 100000;
	size_t			n;
	double			rate_limit = 0.0;
	struct timeval	last_time = {0, 0};
	uint64_t		ignore_initial_burst = 0;

	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage_exit();
		}
		else if (strcmp(argv[i], "-o") == 0) {
			if (i < argc - 1) {
				output_file = argv[i+1];
				i++;
				continue;
			}
			usage_exit();
		}
		else if (strcmp(argv[i], "-i") == 0) {
			if (i < argc - 1) {
				ignore_initial_burst = strtoll(argv[i+1], NULL, 0); // accept hex or decimal
				i++;
				continue;
			}
			usage_exit();
		}
		else if (strcmp(argv[i], "-l") == 0) {
			if (i < argc - 1) {
				rate_limit = strtod(argv[i+1], NULL);
				i++;
				continue;
			}
			usage_exit();
		}
		if (input_file == NULL) {
			input_file = argv[i];
			continue;
		}
		usage_exit();
	}

	if (!input_file && !output_file)
		usage_exit();

	// install signal handlers
	if (signal(SIGINT, handle_terminate_signals) == SIG_ERR ||
		signal(SIGTERM, handle_terminate_signals) == SIG_ERR) {
		perror("installing signals with signal(SIGINT) or signal(SIGTERM)");
		return 1;
	}

	uint8_t *block = malloc(block_size);

	// write mode
	if (output_file) {

		// setup random block of data using values from 0 to 255
		srandom(0);		// start with same seed to aid debugging
		for (n = 0; n < block_size; n++) {
			block[n] = (int) (256.0 * (random() / (RAND_MAX + 1.0)));
		}

		if ((fp_output = fopen(output_file, "w")) == NULL) {
			perror("fopen");
			return 1;
		}

		gettimeofday(&start_time, NULL);

		for (n = 0; n < num_blocks; n++) {
			size_t written;
			if ((written = fwrite(block, 1, block_size, fp_output)) != block_size) {
				perror("fwrite");
				return 1;
			}
			
			total_written += written;

			struct timeval now_time;
			gettimeofday(&now_time, NULL);

			double diff = tv_diff_microsecs(&start_time, &now_time);
			double rate = total_written / diff * 1000000.0 / 1000000.0;	// megabytes per sec

			double diff_last = tv_diff_microsecs(&last_time, &now_time);
			double rate_inst = written / diff_last * 1000000.0 / 1000000.0;	// megabytes per sec

			printf("written %12llu, av rate=%10.3f, inst rate=%10.3f MByte/s\n", total_written, rate, rate_inst);
			last_time = now_time;

			if (rate_limit) {		// throttle write rate
				double needed_time = total_written / rate_limit;
				double sleep_time = needed_time - diff;
				if (sleep_time > 0)
					usleep(sleep_time);
			}

			if (ignore_initial_burst && total_written > ignore_initial_burst) {
				gettimeofday(&start_time, NULL);
				total_written = 0;
				ignore_initial_burst = 0;
			}
		}
	}
	else {
		// read mode

		if ((fp_input = fopen(input_file, "r")) == NULL) {
			perror("fopen");
			return 1;
		}

		gettimeofday(&start_time, NULL);

		for (n = 0; n < num_blocks; n++) {
			size_t read;
			if ((read = fread(block, 1, block_size, fp_input)) != block_size) {
				if (feof(fp_input)) {
					fclose(fp_input);
					break;
				}
				perror("fread");
				return 1;
			}
			total_read += read;

			struct timeval now_time;
			gettimeofday(&now_time, NULL);

			double diff = tv_diff_microsecs(&start_time, &now_time);
			double rate = total_read / diff * 1000000.0 / 1000000.0;	// megabytes per sec

			double diff_last = tv_diff_microsecs(&last_time, &now_time);
			double rate_inst = read / diff_last * 1000000.0 / 1000000.0;	// megabytes per sec

			printf("read %12llu, av rate=%10.3f, inst rate=%10.3f MByte/s\n", total_read, rate, rate_inst);
			last_time = now_time;

			if (rate_limit) {		// throttle reading rate
				double needed_time = total_read / rate_limit;
				double sleep_time = needed_time - diff;
				if (sleep_time > 0)
					usleep(sleep_time);
			}
		}
	}

	printf("\n");
	return 0;
}
