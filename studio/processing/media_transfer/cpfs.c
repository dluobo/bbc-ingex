/***************************************************************************
 *   Copyright (C) 2008 British Broadcasting Corporation                   *
 *   - all rights reserved.                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/* cpfs.c - "Copy Fast and Slow".  Copies one file to another at full speed or at an approximate limited speed, switchable with signals.  See the usage_exit function. */

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>

#define BUFSIZE 1024 * 100
#define ONE_SEC	1000000LL /* # of microseconds in a second */

void display();
void sigslow();
void sigfast();
long long timediff(struct timeval*, struct timeval*);
void usage_exit(const char *);

int slow;
unsigned long long total_written, written_since_sleep, written_this_mode, written_since_display = 0;
struct stat src_st;
struct timeval start_time, sleep_time, mode_change_time, display_time, now;

int main(int argc, char ** argv) {
	signal(SIGUSR1, sigslow);
	signal(SIGUSR2, sigfast);
	if (5 != argc) {
		usage_exit(argv[0]);
	}
	slow = atoi(argv[1]);
	unsigned long rate = atol(argv[2]);
	if (!rate) {
		printf("Invalid rate '%s'.\n", argv[2]);
		usage_exit(argv[0]);
	}
	rate *= 1024;
	int src;
	if (-1 == (src = open(argv[3], O_RDONLY, 0))) {
		printf("Couldn't open source file '%s' for reading.\n", argv[3]);
		exit(1);
	}
	if (-1 == fstat(src, &src_st)) {
		printf("Couldn't stat source file '%s'.\n", argv[3]);
		exit(1);
	}
	int dest;
	if (-1 == (dest = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC))) { /* setting the mode doesn't seem to work */
		printf("Couldn't open destination file '%s' for writing.\n", argv[4]);
		exit(1);
	}
	if (-1 == fchmod(dest, src_st.st_mode)) {
		printf("Couldn't change mode of destination file '%s'.\n", argv[4]);
		exit(1);
	}
	char buffer[BUFSIZE];
	unsigned long long bytes_read;
	long long elapsed;
	gettimeofday(&start_time, NULL);
	mode_change_time = start_time;
	display_time = start_time;
	while (bytes_read = read(src, &buffer, BUFSIZE)) {
		/* write the buffer */
		if (write(dest, buffer, bytes_read) != bytes_read) {
			printf("Couldn't write to destination file '%s'.\n", argv[4]);
			exit(1);
		}
/*		fsync(dest);*/
		written_this_mode += bytes_read;
		written_since_display += bytes_read;
		total_written += bytes_read;
		gettimeofday(&now, NULL);
		elapsed = timediff(&now, &display_time);
		/* display progress */
		if (elapsed > ONE_SEC || !total_written || total_written == src_st.st_size) {
			/* time to update display: start, end or regular intervals */
			display();
			fflush(stdout);
			display_time = now;
			written_since_display = 0;
		}
		/* bandwidth limit */
		if (slow) {
			written_since_sleep += bytes_read;
			elapsed = timediff(&now, &sleep_time);
			unsigned long min_time = (long) written_since_sleep * ONE_SEC / rate; /* how long (in microseconds) it should take to write what has been written, at the given rate */
			if (min_time - elapsed > 100000) { /* more than 0.1s ahead (to avoid inaccurate small waits) */
				/* sleep and reset measurements */
				usleep(min_time - elapsed);
				sleep_time = now;
				written_since_sleep = 0;
			}
			else if (elapsed > min_time) { /* copying more slowly than the bandwidth limit */
				/* reset measurements to prevent a bandwidth spike being allowed through after a long period of slow transfer */
				sleep_time = now;
				written_since_sleep = 0;
			}
		}
	}
	close(src);
	close(dest);
/*	printf("\n"); */
	return 0;
}

void display() {
	char speed[9] = "----- ";
	long long elapsed = timediff(&now, &display_time);
	if (elapsed) {
		sprintf(speed, "%6.2f", (float) written_since_display / (1024. * 1024.) / elapsed * ONE_SEC);
	}
	char ave_speed[9] = "----- ";
	elapsed = timediff(&now, &mode_change_time);
	if (elapsed) {
		sprintf(ave_speed, "%6.2f", (float) written_this_mode / (1024. * 1024.) / elapsed * ONE_SEC);
	}
	printf("\r%s %5dsec %10.2fMB %3d%% %sMB/sec; average: %sMB/sec",
		slow ? "SLOW" : "FAST", /* mode */
		now.tv_sec - start_time.tv_sec, /* elapsed time */
		(float) total_written / (1024 * 1024), /* total written in MB */
		(int) (total_written * 100 / src_st.st_size), /* % written */
		speed,
		ave_speed
	);
}

void sigslow() {
/*	signal(SIGUSR1, sigslow); */
	if (!slow) {
		printf("\n");
		slow = 1;
		gettimeofday(&mode_change_time, NULL);
		written_this_mode = 0;
		/* start measuring for bandwidth limit */
		sleep_time = now;
		written_since_sleep = 0;
	}
}

void sigfast() {
/*	signal(SIGUSR2, sigfast); */
	if (slow) {
		printf("\n");
		slow = 0;
		gettimeofday(&mode_change_time, NULL);
		written_this_mode = 0;
	}
}

long long timediff(struct timeval *time2, struct timeval *time1) {
	long long elapsed = ((long long) time2->tv_sec - time1->tv_sec) * ONE_SEC + (time2->tv_usec - time1->tv_usec);
	if (elapsed < 0) /* rolled over midnight */
		elapsed += ONE_SEC * 24LL * 60 * 60;
	return elapsed;
}

void usage_exit(const char * name) {
	printf("Usage: %s <mode> <rate> <source file> <dest file>\n", name);
	printf("<mode> is numerical and non-zero to start copying no faster than <rate> (kbytes/sec) rather than full speed.\n");
	printf("Send SIGUSR1 to slow down to <rate> and SIGUSR2 to copy at full speed again.\n");
	exit(1);
}
