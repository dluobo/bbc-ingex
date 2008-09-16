/*
 * $Id: audio_utils.c,v 1.4 2008/09/16 09:37:59 stuart_hc Exp $
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

#include <math.h>

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


extern int writeWavHeader(FILE *fp, int bits_per_sample, int num_ch)
{
	uint8_t		riff_fmt[12 + 24] =	"RIFF....WAVEfmt ";
	uint8_t		data_hdr[8] =		"data";

	uint32_t	rate = 48000;
	uint32_t	_bitsPerSample = bits_per_sample;
	uint16_t	_bytesPerFrame = (_bitsPerSample+7)/8 * num_ch;
	uint32_t	avgBPS = rate * _bitsPerSample / 8;

	// RIFF and fmt chunk
	storeUInt32_LE(&riff_fmt[4], 0);					// size 0 - updated on file close
	storeUInt32_LE(&riff_fmt[12+4], 16);				// size is always 16 for PCM
	storeUInt16_LE(&riff_fmt[12+8], 0x0001);			// wFormatTag WAVE_FORMAT_PCM
	storeUInt16_LE(&riff_fmt[12+10], num_ch);			// nchannels
	storeUInt32_LE(&riff_fmt[12+12], rate);				// nSamplesPerSec
	storeUInt32_LE(&riff_fmt[12+16], avgBPS);			// nAvgBytesPerSec
	storeUInt16_LE(&riff_fmt[12+20], _bytesPerFrame);	// nBlockAlign
	storeUInt16_LE(&riff_fmt[12+22], _bitsPerSample);	// nBitsPerSample

	// data header
	storeUInt32_LE(&data_hdr[4], 0);					// size 0 - updated on file close

	if (fwrite(riff_fmt, sizeof(riff_fmt), 1, fp) != 1)
		return 1;
	if (fwrite(data_hdr, sizeof(data_hdr), 1, fp) != 1)
		return 1;

	return 1;
}

extern void update_WAV_header(FILE *fp)
{
	uint8_t		buf[4];

	// Assumes file pointer is currently at end of file
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
//          16 -> 16 (no conversion)
// NB. max num_samples 1920*2 if conversion needed.
extern void write_audio(FILE *fp, uint8_t *p, int num_samples, int bits_per_input_sample, int bits_per_output_sample)
{
	uint8_t		buf24[1920*2*3];		// requires messy use of 3 * 8bit types to give 24bits
	uint16_t	buf16[1920*2];			// use array of 16bit type
	uint32_t	*p32 = (uint32_t *)p;	// treats audio buffer as array of 32bit ints
	int			i;

	if (bits_per_output_sample == 32) {
		fwrite(p, num_samples * 4, 1, fp);
		return;
	}

	if (bits_per_output_sample == 16) {
        if (bits_per_input_sample == 32) {
            for (i = 0; i < num_samples; i++) {
                buf16[i] = p32[i] >> 16;		// discard (truncate) lowest bits
            }
            fwrite(buf16, num_samples * 2, 1, fp);
        }
        else {
            fwrite(p, num_samples * 2, 1, fp);
        }
		return;
	}

	if (bits_per_output_sample == 24) {
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

extern void dvsaudio32_to_16bitmono(int channel, const uint8_t *buf32, uint8_t *buf16)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    int channel_offset = 0;
    if (channel == 1)
        channel_offset = 4;

    // Skip every other channel, copying 16 most significant bits of 32 bits
    // from little-endian DVS format to little-endian 16bits
    for (i = channel_offset; i < 1920*4*2; i += 8) {
        *buf16++ = buf32[i+2];
        *buf16++ = buf32[i+3];
    }
}

extern double calc_audio_peak_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power)
{
    int i;
    int32_t sample;
    int32_t maxSample = 0;
    double power;

    switch (byte_alignment)
    {
        case 1:
            for (i = 0; i < num_samples; i++) 
            {
                sample = (int32_t)((uint32_t)p_samples[i] << 24);
                
                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 2:
            for (i = 0; i < num_samples; i++) 
            {
                sample = (int32_t)(
                    (((uint32_t)p_samples[i * 2]) << 16) | 
                    (((uint32_t)p_samples[i * 2 + 1]) << 24));

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 3:
            for (i = 0; i < num_samples; i++) 
            {
                sample = (int32_t)(
                    (((uint32_t)p_samples[i * 3]) << 8) | 
                    (((uint32_t)p_samples[i * 3 + 1]) << 16) | 
                    (((uint32_t)p_samples[i * 3 + 2]) << 24)); 
                    
                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 4:
            for (i = 0; i < num_samples; i++) 
            {
                sample = (int32_t)(
                    (uint32_t)p_samples[i * 4] | 
                    (((uint32_t)p_samples[i * 4 + 1]) << 8) | 
                    (((uint32_t)p_samples[i * 4 + 2]) << 16) | 
                    (((uint32_t)p_samples[i * 4 + 3]) << 24));
                    
                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        default:
            /* not supported */
            break;
    }
    
    /* return min_power - 1 if no audio is present */
    if (maxSample == 0)
    {
        return min_power - 1;
    }
    
    /* dBFS - decibel full scale */
    power = 20 * log10(maxSample / (2.0*1024*1024*1024));
    
    /* always return min_power if there is audio */
    if (power < min_power)
    {
        return min_power;
    }
    
    return power;
}

extern void deinterleave_32to16(int32_t * src, int16_t * dest0, int16_t * dest1, int count)
{
    int i;
    for(i = 0; i < count; ++i)
    {
        *dest0++ = *src++ >> 16;
        *dest1++ = *src++ >> 16;
    }
}


