/*
 * $Id: audio_utils.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Write uncompressed audio in WAV format, and update WAV header.
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

#include "audio_utils.h"

static void storeUInt16_LE(uint8_t *p, uint16_t value)
{
	p[0] = (value & 0x00ff) >> 0;
	p[1] = (value & 0xff00) >> 8;
}

static void storeUInt32_LE(uint8_t *p, uint32_t value)
{
	p[0] = (uint8_t)((value & 0x000000ff) >> 0);
	p[1] = (uint8_t)((value & 0x0000ff00) >> 8);
	p[2] = (uint8_t)((value & 0x00ff0000) >> 16);
	p[3] = (uint8_t)((value & 0xff000000) >> 24);
}


extern void writeWavHeader(FILE *fp, int bits_per_sample)
{
	uint8_t		riff_fmt[12 + 24] =	"RIFF....WAVEfmt ";
	uint8_t		data_hdr[8] =		"data";

	uint32_t	rate = 48000;
	uint32_t	_numCh = 2;
	uint32_t	_bitsPerSample = bits_per_sample;
	uint16_t	_bytesPerFrame = (_bitsPerSample+7)/8 * _numCh;
	uint32_t	avgBPS = rate * _bitsPerSample / 8;

	// RIFF and fmt chunk
	storeUInt32_LE(&riff_fmt[4], 0);					// size 0 - updated on file close
	storeUInt32_LE(&riff_fmt[12+4], 16);				// size is always 16 for PCM
	storeUInt16_LE(&riff_fmt[12+8], 0x0001);			// wFormatTag WAVE_FORMAT_PCM
	storeUInt16_LE(&riff_fmt[12+10], _numCh);			// nchannels
	storeUInt32_LE(&riff_fmt[12+12], rate);				// nSamplesPerSec
	storeUInt32_LE(&riff_fmt[12+16], avgBPS);			// nAvgBytesPerSec
	storeUInt16_LE(&riff_fmt[12+20], _bytesPerFrame);	// nBlockAlign
	storeUInt16_LE(&riff_fmt[12+22], _bitsPerSample);	// nBitsPerSample

	// data header
	storeUInt32_LE(&data_hdr[4], 0);					// size 0 - updated on file close

	fwrite(riff_fmt, sizeof(riff_fmt), 1, fp);
	fwrite(data_hdr, sizeof(data_hdr), 1, fp);
}

extern void update_WAV_header(FILE *fp)
{
	uint8_t		buf[4];

	long pos = ftell(fp);

	storeUInt32_LE(buf, pos - 8);
	fseek(fp, 4, SEEK_SET);		// location of RIFF size
	fwrite(buf, 4, 1, fp);		// write RIFF size

	storeUInt32_LE(buf, pos - 44);
	fseek(fp, 40, SEEK_SET);	// location of DATA size
	fwrite(buf, 4, 1, fp);		// write DATA size

	fclose(fp);
}

// Write audio with bits_per_sample conversion
// supports:
//          32 -> 32 (no conversion)
//          32 -> 24 mask off unused bits
//          32 -> 16 truncate
extern void write_audio(FILE *fp, uint8_t *p, int num_samples, int bits_per_sample)
{
	uint8_t		buf24[1920*2*3];		// requires messy use of 3 * 8bit types to give 24bits
	uint16_t	buf16[1920*2];			// use array of 16bit type
	uint32_t	*p32 = (uint32_t *)p;	// treats audio buffer as array of 32bit ints
	int			i;

	if (bits_per_sample == 32) {
		fwrite(p, num_samples * 4, 1, fp);
		return;
	}

	if (bits_per_sample == 16) {
		for (i = 0; i < num_samples; i++) {
			buf16[i] = p32[i] >> 16;		// discard (truncate) lowest bits
		}
		fwrite(buf16, num_samples * 2, 1, fp);
		return;
	}

	if (bits_per_sample == 24) {
		for (i = 0; i < num_samples; i++) {
			// samples as stored as little endian 32bit integers
			// E.g.  integer 0xff34e000 -> 0xe0 0x34 0xff on disk
			buf24[i*3+0] = (p32[i] & 0x0000ff00) >> 8;
			buf24[i*3+1] = (p32[i] & 0x00ff0000) >> 16;
			buf24[i*3+2] = (p32[i] & 0xff000000) >> 24;
		}
		fwrite(buf24, num_samples * 3, 1, fp);
		return;
	}
}
