/*
 * $Id: fixwav.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Command line utility to update WAV header based on length of data.
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

#include "audio_utils.h"


extern int main(int argc, char *argv[])
{
	FILE		*fp;

	if (argc != 2) {
		printf("Usage: fixwav input.wav\n");
		return 1;
	}

	if ((fp = fopen(argv[1], "r+b")) == NULL)
	{
		printf("Could not open %s\n", argv[1]);
		return 1;
	}

	// seek to end of wav file
	//off_t filesize = ftello(fp);
	fseeko(fp, 0, SEEK_END);

	update_WAV_header(fp);		// also closes file	

	return 0;
}
