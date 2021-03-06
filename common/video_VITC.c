/*
 * $Id: video_VITC.c,v 1.2 2007/10/26 15:04:12 john_f Exp $
 *
 * Logging and debugging utility functions.
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
#include <time.h>
#include <sys/time.h>
#include "video_VITC.h"

extern char *framesToStr(int tc, char *s)
{
	int frames = tc % 25;
	int hours = (int)(tc / (60 * 60 * 25));
	int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
	int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

	sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}

// Table to quickly convert DVS style timecodes to integer timecodes
static int dvs_to_int[128] = {
 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0, 
10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0, 
20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0, 
30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0, 
40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0, 
50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
60, 61, 62, 63, 64, 65, 66, 67, 68, 69,  0,  0,  0,  0,  0,  0,
70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  0,  0,  0,  0,  0,  0
};

extern int dvs_tc_to_int(int tc)
{
	// E.g. 0x09595924
	// E.g. 0x10000000
	int hours = dvs_to_int[(tc & 0x3f000000) >> 24];
	int mins  = dvs_to_int[(tc & 0x007f0000) >> 16];
	int secs  = dvs_to_int[(tc & 0x00007f00) >> 8];
	int frames= dvs_to_int[(tc & 0x0000003f)];

	int tc_as_total =	hours * (25*60*60) +
						mins * (25*60) +
						secs * (25) +
						frames;

	return tc_as_total;
}

extern int tc_to_int(unsigned hh, unsigned mm, unsigned ss, unsigned ff)
{
	int tc_as_total =	hh * (25*60*60) +
						mm * (25*60) +
						ss * (25) +
						ff;
	return tc_as_total;
}


typedef struct {
	const unsigned char *line;
	int bit0_centre;
	float bit_width;
} VITCinfo;

// Input must be one line of UYVY video (720 pixels, 720*2 bytes)
extern unsigned VBIbit(VITCinfo *pinfo, int bit)
{
	// Bits are 7.5 luma samples wide (see SMPTE-266)
	// Experiment showed bit 0 is centered on luma sample
	// 28 for DVS
	// 30 for Rubidium
	// 27 for D3 at left of picture
	//int bit_pos = (int)(bit * 7.5 + 0.5) + 28;
	int bit_pos = (int)(bit * pinfo->bit_width + 0.5) + pinfo->bit0_centre;

	// use the bit_pos index into the UYVY packing
	unsigned char Y = pinfo->line[bit_pos * 2 + 1];

	// If the luma value is greater than 0x7F then it is
	// likely to be a "1" bit value.  SMPTE-266M says the
	// state of 1 is indicated by 0xC0.
	return (Y > 0x7F);
}

extern unsigned readVBIbits(VITCinfo *pinfo, int bit, int width)
{
	unsigned value = 0;
	int i;

	for (i=0; i < width; i++) {
		value |= (VBIbit(pinfo, bit + i) << i);
	}
	return value;
}

extern int readVITC(const unsigned char *line,
					unsigned *hh, unsigned *mm, unsigned *ss, unsigned *ff)
{
	unsigned CRC = 0;
	int i;
	VITCinfo info;
	info.line = line;

	// First, scan from left to find first sync pair (black->white->black).
	// Start and end pixels (18 to 42) are very conservative (VITC would
	// need to be very wrong to be close to these limits).
	int bit0_start = -1;
	int bit0_end = -1;
	int lastY, last2Y;
	lastY = 0xFF;
	last2Y = 0xFF;
	for (i = 18; i < 42; i++) {
		unsigned char Y = line[i*2+1];
		//printf("i=%d offset=%d Y=%x\n", i, i*2+1, Y);
		if (bit0_start == -1) {
			if (Y > 0xB0)
				bit0_start = i;
			continue;
		}
		// bit 0 start found, look for bit 0 end
		if (Y <= 0xB0 && lastY <= 0xB0 && Y < lastY && lastY < last2Y) {
			bit0_end = i - 2;
			break;
		}
		last2Y = lastY;
		lastY = Y;
	}
	//printf("bit0_st=%d bit0_en=%d width=%d\n", bit0_start, bit0_end, bit0_end - bit0_start + 1);
	if (bit0_start == -1 || bit0_end == -1)		// No sync at bits 0 to 1
		return 0;

	// The centre of bit zero is set to be 2 pixels before the end since the spec
	// shows the width of the peak to be 4 pixels of 0xC0 values
	info.bit0_centre = bit0_end - 2;

	// Now, find the transition from bit 80 to bit 81 (last sync pair).
	// We can't reliably tell when bit 80 started since the previous
	// bit (bit 79) is part of BG6 and may have been set high (1).
	int bit80_end = -1;

	// The centre of bit80 should be 630 = 30 + 7.5 * 80 (SMPTE 266M shows 30 as bit0 centre)
	int scan_start = 630;

	// If bit0 is not quite right (should be 30), start a little further along.
	if (info.bit0_centre > 32)
		scan_start = 634;

	lastY = 0xFF;
	last2Y = 0xFF;
	for (i = scan_start; i < scan_start + 7.5*2; i++) {
		unsigned char Y = line[i*2+1];
		//printf("i=%d offset=%d Y=%x\n", i, i*2+1, Y);
		if (Y <= 0xB0 && lastY <= 0xB0 && Y < lastY && lastY < last2Y) {
			bit80_end = i - 2;
			break;
		}
		last2Y = lastY;
		lastY = Y;
	}
	//printf("bit80_end=%d\n", bit80_end);
	//printf("bit0_centre=%d, bit_width=%.2f\n", bit0_end - 2, (bit80_end-bit0_end)/80.0);

	if (bit80_end == -1)		// No sync bit at bit 80
		return 0;

	info.bit_width = (bit80_end-bit0_end)/80.0;

	for (i = 0; i < 10; i++)
		CRC ^= readVBIbits(&info, i*8, 8);
	CRC = (((CRC & 3) ^ 1) << 6) | (CRC >> 2);
	//unsigned film_CRC = CRC ^ 0xFF;
	//unsigned prod_CRC = CRC ^ 0xF0;
	unsigned crc_read = readVBIbits(&info, 82, 8);
	//printf("crc_read=%d CRC=%u fCRC=%u pCRC=%u\n", crc_read,CRC,film_CRC,prod_CRC);
	if (crc_read != CRC)
		return 0;

	// Note that tens of frames are two bits - flags bits are ignored
	*ff = readVBIbits(&info, 2, 4) + readVBIbits(&info, 12, 2) * 10;
	*ss = readVBIbits(&info, 22, 4) + readVBIbits(&info, 32, 3) * 10;
	*mm = readVBIbits(&info, 42, 4) + readVBIbits(&info, 52, 3) * 10;
	*hh = readVBIbits(&info, 62, 4) + readVBIbits(&info, 72, 2) * 10;
	return 1;
}

// Return true if the line contains only black or grey (< 0x80)
// luminance values and no colour difference.
// Useful for detecting when there is no VITC at all on a line.
extern int black_or_grey_line(const unsigned char *line, int width)
{
	int i;
	for (i = 0; i < width; i++) {
		unsigned char UV = line[i*2];
		unsigned char Y = line[i*2+1];

		if (abs(0x80 - UV) > 1) {
			// contains colour, therefore not a grey or black line
			return 0;
		}

		if (Y >= 0x80) {
			// contains a reasonably bright pixel
			return 0;
		}
	}
	return 1;
}
