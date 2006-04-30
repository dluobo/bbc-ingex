/*
 * $Id: nexus_save.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Command line utility to save video & audio from shared mem buffers.
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
#include <limits.h>
#include <unistd.h>

#include "recorder.h"


static int read_opt_tc(const char *s, int *p)
{
	int tmp_tc, hr, mi, se, fr;
	if (sscanf(s, "%d:%02d:%02d:%02d", &hr, &mi, &se, &fr) == 4) {
		*p = hr*3600*25 + mi*60*25 + se*25 + fr;
		return 1;
	}
	if (sscanf(s, "%d", &tmp_tc) == 1) {
		*p = tmp_tc;
		return 1;
	}
	return 0;
}

static void usage_exit(char *argv[])
{
	fprintf(stderr, "Usage: %s [-c 0..3] [-n] outputfile\n\n", argv[0]);
	fprintf(stderr, "\t-c   select PCI card to record from 0..3\n");
	fprintf(stderr, "\t-n   skip disk I/O\n");
	exit(1);
}

extern int main(int argc, char *argv[])
{
	char			*output_stem = NULL;
	int				n;
	int				cardlimit = 3;
	int				verbose = 0;
	int				capture_length = INT_MAX;
	int				start_tc = -1, end_tc = -1;
	int				crash_record = 0, test_rapid_stop_start = 0;
	int				bits_per_sample = 0;
	bool			a_enable[4] = {true, true, true, true};

	// process command line options
	for (n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
		{
			usage_exit(argv);
		}
		else if (strcmp(argv[n], "-q") == 0)
		{
			verbose = 0;
		}
		else if (strcmp(argv[n], "-crash") == 0)
		{
			crash_record = 1;
		}
		else if (strcmp(argv[n], "-t") == 0)
		{
			test_rapid_stop_start = 1;
		}
		else if (strcmp(argv[n], "-e") == 0)
		{
			int a[4] = {1,1,1,1};
			if (sscanf(argv[n+1], "%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3]) != 4)
			{
				if (sscanf(argv[n+1], "%d,%d,%d", &a[0], &a[1], &a[2]) != 3)
				{
					fprintf(stderr, "-e requires list of flags e.g. 1,1,0[,1]\n");
					return 1;
				}
			}
			// non-zero means enabled
			a_enable[0] = (a[0] != 0);
			a_enable[1] = (a[1] != 0);
			a_enable[2] = (a[2] != 0);
			a_enable[3] = (a[3] != 0);
			n++;
		}
		else if (strcmp(argv[n], "-l") == 0)
		{
			if (sscanf(argv[n+1], "%d", &capture_length) != 1 ||
				capture_length < 1)
			{
				fprintf(stderr, "-l requires non-zero capture length\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-st") == 0)
		{
			if (! read_opt_tc(argv[n+1], &start_tc))
			{
				fprintf(stderr, "-st requires timecode as number of edit units\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-et") == 0)
		{
			if (! read_opt_tc(argv[n+1], &end_tc))
			{
				fprintf(stderr, "-et requires timecode as number of edit units\n");
				return 1;
			}
			capture_length = end_tc - start_tc + 1;
			n++;
		}
		else if (strcmp(argv[n], "-bps") == 0)
		{
			if (sscanf(argv[n+1], "%d", &bits_per_sample) != 1 ||
				(bits_per_sample != 16 && bits_per_sample != 24 && bits_per_sample != 32))
			{
				fprintf(stderr, "-bps must be 32, 24 or 16\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-c") == 0)
		{
			if (sscanf(argv[n+1], "%d", &cardlimit) != 1 ||
				cardlimit > 4 || cardlimit < 0)
			{
				fprintf(stderr, "-c requires integer card number <= 4\n");
				return 1;
			}
			n++;
		}
		else
		{
			if (!output_stem)
			{
				output_stem = argv[n];
				continue;
			}
		}
	}

	char tape1[] = "Tape 1";
	char tape2[] = "Tape 2";
	char tape3[] = "Tape 3";
	const char *a_tape[3];
	a_tape[0] = tape1;
	a_tape[1] = tape2;
	a_tape[2] = tape3;
	char descr[] = "sample description";

	if (! Recorder::recorder_init())
		return 1;

	Recorder *rec = new Recorder();

	if (! rec->recorder_prepare_start( start_tc,		// target timecode
							a_enable,			// array of enable flags
							a_tape,
							descr,
							output_stem,		// video path
							output_stem,		// dvd path
							5000,				// bitrate in kbps
							crash_record		// boolean crash record
							)) {
		return 1;
	}


	if (! rec->recorder_start())
		return 1;

	// sleep until half the capture length
	usleep((capture_length / 2) / 25 * 1000 * 1000);

	rec->recorder_stop(capture_length);

	if (test_rapid_stop_start) {
		if (! rec->recorder_prepare_start( start_tc,		// target timecode
							a_enable,			// array of enable flags
							a_tape,
							descr,
							output_stem,		// video path
							output_stem,		// dvd path
							5000,				// bitrate in kbps
							crash_record		// boolean crash record
							)) {
			return 1;
		}

		if (! rec->recorder_start())
			return 1;

		// sleep until half the capture length
		usleep((capture_length / 2) / 25 * 1000 * 1000);

		rec->recorder_stop(capture_length);
	}

	rec->recorder_wait_for_completion();

	return 0;
}
