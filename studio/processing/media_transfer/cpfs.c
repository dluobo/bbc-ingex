/***************************************************************************
 *   $Id: cpfs.c,v 1.7 2009/04/16 18:19:50 john_f Exp $                  *
 *                                                                         *
 *   Copyright (C) 2008-2009 British Broadcasting Corporation              *
 *   - all rights reserved.                                                *
 *   Author: Matthew Marks                                                 *
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

/* cpfs.c - "Copy Fast and Slow".  Copies one file to another at "high" (or unlimited) or "low" (or zero) approximate speeds, switchable with signals.  See the usage_exit function. */

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include <utime.h>
#include <string.h>

#define BUFSIZE 1024 * 1024 /* if this is too big then the buffer will have to be created on the heap or the program will segfault immediately */
#define ONE_SEC	1000000LL /* # of microseconds in a second */

void update(const int);
void handler(const int);
unsigned long long timediff(struct timeval*, struct timeval*);
void usage_exit(const char *);

int slow;
unsigned long slow_rate;
int new_mode;
int signalled = 0; /* used to flag when a signal has been received */
int alreadyPrinted = 0; /* prevents it leaving an "Opening destination file..." message behind if signalled soon after starting */
unsigned long long total_written, written_since_sleep, written_this_mode, written_since_display = 0;
struct stat src_st;
struct timeval start_time, sleep_time, mode_change_time, display_time;

int main(int argc, char ** argv) {
	struct sigaction action = {{0}};
	action.sa_handler = handler;
	if (-1 == sigaction(SIGUSR1, &action, NULL)
	 || -1 == sigaction(SIGUSR2, &action, NULL)) {
		printf("sigaction call failed\n");
		exit(1);
	}
	if (6 != argc) {
		usage_exit(argv[0]);
	}
	slow = atoi(argv[1]);
	unsigned long fast_rate = atol(argv[2]);
	fast_rate *= 1024;
	slow_rate = atol(argv[3]);
	slow_rate *= 1024;
	int src;
	if (-1 == (src = open(argv[4], O_RDONLY, 0))) {
		printf("Couldn't open source file '%s' for reading.\n", argv[4]);
		exit(1);
	}
	if (-1 == fstat(src, &src_st)) {
		printf("Couldn't stat source file '%s'.\n", argv[4]);
		exit(1);
	}
	int dest;
	printf("Opening destination file... "); /* may take a while if it's overwriting an old one */
	fflush(stdout);
	if (-1 == (dest = open(argv[5], O_WRONLY | O_CREAT | O_TRUNC))) { /* setting the mode doesn't seem to work */
		printf("Couldn't open destination file '%s' for writing.\n", argv[5]);
		exit(1);
	}
	if (-1 == fchmod(dest, src_st.st_mode)) {
		printf("Couldn't change mode of destination file '%s'.\n", argv[5]);
		exit(1);
	}
	char buffer[BUFSIZE];
	unsigned long long bytes_read = 0;
	unsigned long long elapsed;
	gettimeofday(&start_time, NULL);
	mode_change_time = start_time;
	display_time = start_time;
	sleep_time = start_time;
	update(0);
	struct timeval now;
	do {
		while (slow && !slow_rate && src_st.st_size) { /* copying is stopped */
			/* wait for a signal */
			sleep(1);
			update(0);
		}
		/* write the buffer */
		if (write(dest, buffer, bytes_read) != bytes_read) {
			printf("Couldn't write to destination file '%s'.\n", argv[5]);
			exit(1);
		}
		/* update counters */
		written_this_mode += bytes_read;
		written_since_display += bytes_read;
		total_written += bytes_read;
		gettimeofday(&now, NULL);
		elapsed = timediff(&now, &display_time);
		/* display progress and check for signals */
		if (elapsed > ONE_SEC || total_written == src_st.st_size) {
			/* time to update: end or at regular intervals */
			update(0);
		}
		/* bandwidth limit */
		if ((slow && slow_rate) || fast_rate) { /* copying at a limited speed */
			written_since_sleep += bytes_read;
			elapsed = timediff(&now, &sleep_time);
			unsigned long long min_time = (unsigned long long) written_since_sleep * ONE_SEC / (slow ? slow_rate : fast_rate); /* how long (in microseconds) it should take to write what has been written, at the given rate */
			if (min_time > 100000 + elapsed) { /* more than 0.1s ahead (to avoid inaccurate small waits) */
				/* sleep and reset measurements */
				sleep_time = now;
				written_since_sleep = 0;
				while (min_time > ONE_SEC + 100000 + elapsed) { /* keep updating during long waits */
					usleep(ONE_SEC);
					update(0);
					min_time -= ONE_SEC;
				}
				usleep(min_time - elapsed);
			}
			else if (elapsed > min_time) { /* copying more slowly than the bandwidth limit */
				/* reset measurements to prevent a bandwidth spike being allowed through after a long period of slow transfer */
				sleep_time = now;
				written_since_sleep = 0;
			}
		}
	} while ((bytes_read = read(src, &buffer, BUFSIZE)));
	printf(".  Closing files...");
	fflush(stdout);
	close(src);
	close(dest);
	update(1);
	printf("                                           \n");
	/* conserve modification time */
	struct utimbuf dest_times;
	dest_times.modtime = src_st.st_mtime;
	dest_times.actime = src_st.st_mtime;
	if (-1 == utime(argv[5] , &dest_times)) {
		printf("\nCouldn't set destination file times.\n");
	}
	return 0;
}

void update(const int nospeed) {
	/* check for signals */
	if (signalled) {
		signalled = 0;
		if (new_mode != slow) {
			slow = new_mode;
			if (alreadyPrinted) {
				printf("\n"); /* to preserve the previous mode's report */
			}
			gettimeofday(&mode_change_time, NULL);
			written_this_mode = 0;
			/* start measuring for bandwidth limit */
			sleep_time = mode_change_time;
			written_since_sleep = 0;
		}
	}
	/* update display */
	struct timeval now;
	gettimeofday(&now, NULL);
	char speed[10] = "------ ";
	unsigned long long elapsed = timediff(&now, &display_time);
	if (nospeed) {
		strcpy(speed, "");
	}
	else if (elapsed) {
		sprintf(speed, "%7.2f", (float) written_since_display / (1024. * 1024.) / elapsed * ONE_SEC);
	}
	char ave_speed[9] = "----- ";
	elapsed = timediff(&now, &mode_change_time);
	if (elapsed) {
		sprintf(ave_speed, "%6.2f", (float) written_this_mode / (1024. * 1024.) / elapsed * ONE_SEC);
	}
	printf("\r%s %5dsec %10.2fMB %s%3d%% %s%s%sMB/sec",
		slow ? (slow_rate ? "SLOW" : "STOP") : "FAST", /* mode */
		(int) (now.tv_sec - start_time.tv_sec), /* elapsed time */
		(float) total_written / (1024 * 1024), /* total written in MB */
		src_st.st_size ? "" : "(empty file) ", /* empty file or not */
		src_st.st_size ? (int) (total_written * 100 / src_st.st_size) : 100, /* % written */
		speed,
		nospeed ? "" : "MB/sec; average: ",
		ave_speed
	);
	fflush(stdout);
	display_time = now;
	written_since_display = 0;
	alreadyPrinted = 1;
}

void handler(const int signum) {
	new_mode = signum == SIGUSR1 ? 1 : 0;
	signalled = 1;
}

unsigned long long timediff(struct timeval *time2, struct timeval *time1) {
	long long elapsed = ((long long) time2->tv_sec - time1->tv_sec) * ONE_SEC + (time2->tv_usec - time1->tv_usec);
	if (elapsed < 0) /* rolled over midnight */
		elapsed += ONE_SEC * 24LL * 60 * 60;
	return elapsed;
}

void usage_exit(const char * name) {
	printf("Usage: %s <mode> <fast rate> <slow rate> <source file> <dest file>\n", name);
	printf("<mode> is numerical and non-zero to start copying no faster than <slow rate> (kbytes/sec) rather than <fast rate> (kbytes/sec).\n");
	printf("Set <fast rate> to zero to place no restriction on copying speed.\n");
	printf("<slow rate> can be zero.\n");
	printf("Send SIGUSR1 to copy at <slow rate> and SIGUSR2 to copy at <fast rate>.\n");
	exit(1);
}
