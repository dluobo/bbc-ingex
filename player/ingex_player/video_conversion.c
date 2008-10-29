/*
 * $Id: video_conversion.c,v 1.4 2008/10/29 17:47:42 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham, <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <assert.h>

#ifdef __MMX__
#include <mmintrin.h>
#endif

#include "video_conversion.h"


#ifdef __MMX__

#if 0
void uyvy_to_yuv422(int width, int height, uint8_t *input, uint8_t *output)
{
	__m64 chroma_mask = _mm_set_pi8(255, 0, 255, 0, 255, 0, 255, 0);
	__m64 luma_mask = _mm_set_pi8(0, 255, 0, 255, 0, 255, 0, 255);
	uint8_t *y_comp = output;
	uint8_t *u_comp = output + width * height;
	uint8_t *v_comp = u_comp + (int)((width * height)/2);	// 4:2:2
	int i, j;

	/* Do the y component */
	 uint8_t *tmp = input;
	for (j = 0; j < height; ++j)
	{
		// Consume 16 bytes of UYVY data per iteration (8 pixels worth)
		for (i = 0; i < width*2;  i += 16)
		{
			//__m64 m1 = _mm_and_si64 (*(__m64 *)input, luma_mask);
			//__m64 m2 = _mm_and_si64 (*(__m64 *)(input+8), luma_mask);
			//__m64 m2 = _mm_set_pi8 (0, 0, 0, 0, 0, 0, 0, 0);
			//*(__m64 *)y_comp = _mm_packs_pu16 (m2, m1);
			__m64 m0 = *(__m64 *)input;
			__m64 m2 = _mm_srli_si64(m0, 8);
			__m64 m3 = _mm_slli_si64(m0, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m0 = m2;
			__m64 m1 = *(__m64 *)(input+8);
			m2 = _mm_srli_si64(m1, 8);
			m3 = _mm_slli_si64(m1, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m1 = m2;
			*(__m64 *)y_comp = _mm_packs_pu16 (m0, m1);

			y_comp += 8;
			input += 16;
		}
	}
	/* Do the chroma components */
	input = tmp;
	for (j = 0; j < height; ++j)
	{
		/* Process every line for yuv 4:2:2 */
		for (i = 0; i < width*2;  i += 16)
		{
			__m64 m1 = _mm_unpacklo_pi8 (*(__m64 *)input, *(__m64 *)(input+8));
			__m64 m2 = _mm_unpackhi_pi8 (*(__m64 *)input, *(__m64 *)(input+8));

			__m64 m3 = _mm_unpacklo_pi8 (m1, m2);
			__m64 m4 = _mm_unpackhi_pi8 (m1, m2);
			//*(__m64 *)u_comp = _mm_unpacklo_pi8 (m1, m2);
			//*(__m64 *)v_comp = _mm_unpackhi_pi8 (m1, m2);
			memcpy (u_comp, &m3, 4);
			memcpy (v_comp, &m4, 4);
			u_comp += 4;
			v_comp += 4;
			input += 16;
		}
	}
    
    _mm_empty();        // Clear aliased fp register state
}

#endif

void uyvy_to_yuv420(int width, int height, uint8_t *input, uint8_t *output)
{
	__m64 chroma_mask = _mm_set_pi8(255, 0, 255, 0, 255, 0, 255, 0);
	__m64 luma_mask = _mm_set_pi8(0, 255, 0, 255, 0, 255, 0, 255);
	uint8_t *y_comp = output;
	uint8_t *u_comp = output + width * height;
	uint8_t *v_comp = u_comp + (int)((width * height)/4);	// 4:2:0
	int i, j;

	/* Do the y component */
	 uint8_t *tmp = input;
	for (j = 0; j < height; ++j)
	{
		// Consume 16 bytes of UYVY data per iteration (8 pixels worth)
		for (i = 0; i < width*2;  i += 16)
		{
			//__m64 m1 = _mm_and_si64 (*(__m64 *)input, luma_mask);
			//__m64 m2 = _mm_and_si64 (*(__m64 *)(input+8), luma_mask);
			//__m64 m2 = _mm_set_pi8 (0, 0, 0, 0, 0, 0, 0, 0);
			//*(__m64 *)y_comp = _mm_packs_pu16 (m2, m1);
			__m64 m0 = *(__m64 *)input;
			__m64 m2 = _mm_srli_si64(m0, 8);
			__m64 m3 = _mm_slli_si64(m0, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m0 = m2;
			__m64 m1 = *(__m64 *)(input+8);
			m2 = _mm_srli_si64(m1, 8);
			m3 = _mm_slli_si64(m1, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m1 = m2;
			*(__m64 *)y_comp = _mm_packs_pu16 (m0, m1);

			y_comp += 8;
			input += 16;
		}
	}
	/* Do the chroma components */
	input = tmp;
	for (j = 0; j < height; ++j)
	{
		/* Skip every odd line to subsample to yuv 4:2:0 */
		if (j %2)
		{
			input += width*2;
			continue;
		}
		for (i = 0; i < width*2;  i += 16)
		{
			__m64 m1 = _mm_unpacklo_pi8 (*(__m64 *)input, *(__m64 *)(input+8));
			__m64 m2 = _mm_unpackhi_pi8 (*(__m64 *)input, *(__m64 *)(input+8));

			__m64 m3 = _mm_unpacklo_pi8 (m1, m2);
			__m64 m4 = _mm_unpackhi_pi8 (m1, m2);
			//*(__m64 *)u_comp = _mm_unpacklo_pi8 (m1, m2);
			//*(__m64 *)v_comp = _mm_unpackhi_pi8 (m1, m2);
			memcpy (u_comp, &m3, 4);
			memcpy (v_comp, &m4, 4);
			u_comp += 4;
			v_comp += 4;
			input += 16;
		}
	}

    _mm_empty();        // Clear aliased fp register state
}
#else
// Convert UYVY -> Planar YUV 4:2:0 (fourcc=I420 or YV12)
// U0 Y0 V0 Y1   U2 Y2 V2 Y3   ->   Y0 Y1 Y2... U0 U2... V0 V2...
// but U and V are skipped every second line
void uyvy_to_yuv420(int width, int height, uint8_t *input, uint8_t *output)
{
	int i;

	// Copy Y plane as is
	for (i = 0; i < width*height; i++)
	{
		output[i] = input[ i*2 + 1 ];
	}

	// Copy U & V planes, downsampling in vertical direction
	// by simply skipping every second line.
	// Each U or V plane is 1/4 the size of the Y plane.
	int i_macropixel = 0;
	for (i = 0; i < width*height / 4; i++)
	{
		output[width*height + i] = input[ i_macropixel*4 ];			// U
		output[width*height*5/4 + i] = input[ i_macropixel*4 + 2 ];	// V

		// skip every second line
		if (i_macropixel % (width) == (width - 1))
			i_macropixel += width/2;

		i_macropixel++;
	}
}
#endif

#if defined(HAVE_FFMPEG)

#ifdef __MMX__
void yuv422_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output)
{
	int i, j;
	uint8_t *y, *u, *v;
	int start_line = 0;

	// Shifting picture up one line is necessary when decoding PAL DV50
	if (shift_picture_up) 
    {
		// Skip one line of input picture and start one line lower
		start_line = 1;
	}

	// Convert to UYVY
	for (j = start_line; j < height; j++)
	{
		y = input->data[0] + j * input->linesize[0];
		u = input->data[1] + j * input->linesize[1];
		v = input->data[2] + j * input->linesize[2];

		for (i = 0; i < width*2; i += 32)
		{
            if (i + 32 > width*2)
            {
                // can no longer process in 32 byte chunks - revert to basic method below
                break;
            }
            
			__m64 m0 = *(__m64 *)u;			// load UUUU UUUU(0)
			__m64 m1 = *(__m64 *)v;			// load VVVV VVVV(0)
			__m64 m2 = m0;					// copy U for mix op
			m0 = _mm_unpacklo_pi8(m0, m1);	// mix -> UVUV UVUV(0) (in m0)
			m2 = _mm_unpackhi_pi8(m2, m1);	// mix -> UVUV UVUV(8) (in m2)

			__m64 m3 = *(__m64 *)y;			// load YYYY YYYY(0)
			__m64 m5 = *(__m64 *)(y+8);		// load YYYY YYYY(8)
			__m64 m4 = m0;
			__m64 m6 = m2;
			m0 = _mm_unpacklo_pi8(m0, m3);	// mix to UYVY UYVY(0)
			m4 = _mm_unpackhi_pi8(m4, m3);	// mix to UYVY UYVY(8)
			m2 = _mm_unpacklo_pi8(m2, m5);	// mix to UYVY UYVY(16)
			m6 = _mm_unpackhi_pi8(m6, m5);	// mix to UYVY UYVY(24)

			*(__m64 *)(output+0) = m0;
			*(__m64 *)(output+8) = m4;
			*(__m64 *)(output+16) = m2;
			*(__m64 *)(output+24) = m6;

			u += 8;
			v += 8;
			y += 16;
			output += 32;
		}
        
        // process the rest of the line
        for (; i < width*2; i += 2)
        {
			if (i % 4 == 0)
            {
				*output++ = *u++;
            }
			else
            {
				*output++ = *v++;
            }
			*output++ = *y++;
        }
	}
    _mm_empty();        // Clear aliased fp register state
    
	if (shift_picture_up) {
		// Fill bottom line with one line of black
		for (i = 0; i < width*2; i += 4) {
			*output++ = 0x80;
			*output++ = 0x10;
			*output++ = 0x80;
			*output++ = 0x10;
		}
	}
}

#else

void yuv422_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output)
{
	int i, j;
	uint8_t *y, *u, *v;
	int start_line = 0;

	// Shifting picture up one line is necessary when decoding PAL DV50
	if (shift_picture_up) 
    {
		// Skip one line of input picture and start one line lower
		start_line = 1;
	}

	// Convert to UYVY
	for (j = start_line; j < height; j++) {
		y = input->data[0] + j * input->linesize[0];
		u = input->data[1] + j * input->linesize[1];
		v = input->data[2] + j * input->linesize[2];

		for (i = 0; i < width; i++)
		{
			if (i % 2 == 0)
				*output++ = *u++;
			else
				*output++ = *v++;
			*output++ = *y++;
		}
	}

	if (shift_picture_up) {
		// Fill bottom line with one line of black
		for (i = 0; i < width*2; i += 4) {
			*output++ = 0x80;
			*output++ = 0x10;
			*output++ = 0x80;
			*output++ = 0x10;
		}
	}
}

#endif

/* TODO: an MMX version and improved calculation */
void yuv4xx_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output)
{
	int i, j;
	uint8_t *y, *u, *v;
	int start_line = 0;

	// Shifting picture up one line is necessary when decoding PAL DV25
	if (shift_picture_up) 
    {
		// Skip one line of input picture and start one line lower
		start_line = 1;
	}


    /* Convert to UYVY */
    if (input->linesize[1] == input->linesize[0] / 2)
    {
        /* YUV420P */
        for (j = start_line; j < height; j++) {
            y = input->data[0] + j * input->linesize[0];
            u = input->data[1] + (j / 2) * input->linesize[1];
            v = input->data[2] + (j / 2) * input->linesize[2];
    
            for (i = 0; i < width; i++)
            {
                if (i % 2 == 0)
                {
                    if (j % 2 == 0)
                    {
                        *output++ = *u++;
                    }
                    else
                    {
                        *output++ = (*u + *(input->linesize[1] + u)) / 2; /* take average of prev and next line */
                        u++;
                    }
                }
                else
                {
                    if (j % 2 == 0)
                    {
                        *output++ = *v++;
                    }
                    else
                    {
                        *output++ = (*v + *(input->linesize[1] + v)) / 2; /* take average of prev and next line */
                        v++;
                    }
                }
                *output++ = *y++;
            }
        }

        if (shift_picture_up) {
            // Fill bottom line with one line of black
            for (i = 0; i < width*2; i += 4) {
                *output++ = 0x80;
                *output++ = 0x10;
                *output++ = 0x80;
                *output++ = 0x10;
            }
        }
    }
    else
    {
        /* YUV411P */
        for (j = start_line; j < height; j++) {
            y = input->data[0] + j * input->linesize[0];
            u = input->data[1] + j * input->linesize[1];
            v = input->data[2] + j * input->linesize[2];
    
            for (i = 0; i < width; i++)
            {
                if (i % 4 == 0)
                    *output++ = *u++;
                else if (i % 4 == 1)
                    *output++ = *v++;
                else if (i % 4 == 2)
                    *output++ = (*u + *(u-1)) / 2; /* take average of prev and next sample */ 
                else
                    *output++ = (*v + *(v-1)) / 2; /* take average of prev and next sample */
                    
                *output++ = *y++;
            }
        }

        if (shift_picture_up) {
            // Fill bottom line with one line of black
            for (i = 0; i < width*2; i += 4) {
                *output++ = 0x80;
                *output++ = 0x10;
                *output++ = 0x80;
                *output++ = 0x10;
            }
        }
    }
}


void yuv422_to_yuv422(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output)
{
	int j;
	uint8_t *y, *u, *v;
    uint8_t *yOut, *uOut, *vOut; 
	int start_line = 0;

	// Shifting picture up one line is necessary when decoding PAL DV50
	if (shift_picture_up) 
    {
		// Skip one line of input picture and start one line lower
		start_line = 1;
	}

    yOut = output;
    uOut = output + width * height;
    vOut = uOut + width * height / 2;
    
	/* copy every line, where width <= linesize */
	for (j = start_line; j < height; j++) {
		y = input->data[0] + j * input->linesize[0];
		u = input->data[1] + j * input->linesize[1];
		v = input->data[2] + j * input->linesize[2];

        memcpy(yOut, y, width);
        memcpy(uOut, u, width / 2);
        memcpy(vOut, v, width / 2);

        yOut += width;
        uOut += width / 2;
        vOut += width / 2;
	}

	if (shift_picture_up) {
		// Fill bottom line with one line of black
        memset(yOut, 0x10, width);
        memset(uOut, 0x80, width / 2);
        memset(vOut, 0x80, width / 2);
	}
}

void yuv4xx_to_yuv4xx(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output)
{
	int j;
	uint8_t *y, *u, *v;
    uint8_t *yOut, *uOut, *vOut; 
	int start_line = 0;

	// Shifting picture up one line is necessary when decoding PAL DV25
	if (shift_picture_up) 
    {
		// Skip one line of input picture and start one line lower
		start_line = 1;
	}


	/* copy every line, where width <= linesize */
    if (input->linesize[1] == input->linesize[0] / 2)
    {
        /* YUV420P */
        yOut = output;
        uOut = output + height * width;
        vOut = output + height * width * 5 / 4;
        
        for (j = start_line; j < height; j++) 
        {
            y = input->data[0] + j * input->linesize[0];
            
            memcpy(yOut, y, width);
            yOut += width;
            
            if (j % 2 == 0)
            {
                u = input->data[1] + (j / 2) * input->linesize[1];
                v = input->data[2] + (j / 2) * input->linesize[2];

                memcpy(uOut, u, width / 2);
                memcpy(vOut, v, width / 2);

                uOut += width / 2;
                vOut += width / 2;
            }
        }

        if (shift_picture_up) {
            // Fill bottom line with one line of black
            memset(yOut, 0x10, width);
            memset(uOut, 0x80, width / 2);
            memset(vOut, 0x80, width / 2);
        }
    }
    else
    {
        /* YUV411P */
        yOut = output;
        uOut = output + width * height;
        vOut = output + width * height * 5 / 4;
        
        for (j = start_line; j < height; j++) 
        {
            y = input->data[0] + j * input->linesize[0];
            u = input->data[1] + j * input->linesize[1];
            v = input->data[2] + j * input->linesize[2];
    
            memcpy(yOut, y, width);
            memcpy(uOut, u, width / 4);
            memcpy(vOut, v, width / 4);
    
            yOut += width;
            uOut += width / 4;
            vOut += width / 4;
        }

        if (shift_picture_up) {
            // Fill bottom line with one line of black
            memset(yOut, 0x10, width);
            memset(uOut, 0x80, width / 4);
            memset(vOut, 0x80, width / 4);
        }
    }
}

#endif /* defined(HAVE_FFMPEG) */

#ifdef __MMX__
void yuv422_to_uyvy_2(int width, int height, int shift_picture_up, uint8_t *input, uint8_t *output)
{
	int i, j, start_line;
	uint8_t *y, *u, *v;

	y = input;
	u = input + width*height;
	v = input + width*height * 3/2;
	start_line = 0;

	// Shifting picture up one line is necessary when decoding PAL DV50
	if (shift_picture_up) {
		// Skip one line of input picture and start one line lower
		y += width;
		u += width / 2;
		v += width / 2;
		start_line = 1;
	}

	// Convert to UYVY
	for (j = start_line; j < height; j++)
	{
		for (i = 0; i < width*2; i += 32)
		{
            if (i + 32 > width*2)
            {
                // can no longer process in 32 byte chunks - revert to basic method below
                break;
            }
            
			__m64 m0 = *(__m64 *)u;			// load UUUU UUUU(0)
			__m64 m1 = *(__m64 *)v;			// load VVVV VVVV(0)
			__m64 m2 = m0;					// copy U for mix op
			m0 = _mm_unpacklo_pi8(m0, m1);	// mix -> UVUV UVUV(0) (in m0)
			m2 = _mm_unpackhi_pi8(m2, m1);	// mix -> UVUV UVUV(8) (in m2)

			__m64 m3 = *(__m64 *)y;			// load YYYY YYYY(0)
			__m64 m5 = *(__m64 *)(y+8);		// load YYYY YYYY(8)
			__m64 m4 = m0;
			__m64 m6 = m2;
			m0 = _mm_unpacklo_pi8(m0, m3);	// mix to UYVY UYVY(0)
			m4 = _mm_unpackhi_pi8(m4, m3);	// mix to UYVY UYVY(8)
			m2 = _mm_unpacklo_pi8(m2, m5);	// mix to UYVY UYVY(16)
			m6 = _mm_unpackhi_pi8(m6, m5);	// mix to UYVY UYVY(24)

			*(__m64 *)(output+0) = m0;
			*(__m64 *)(output+8) = m4;
			*(__m64 *)(output+16) = m2;
			*(__m64 *)(output+24) = m6;

			u += 8;
			v += 8;
			y += 16;
			output += 32;
		}
        
        // process the rest of the line
        for (; i < width*2; i += 2)
        {
			if (i % 4 == 0)
            {
				*output++ = *u++;
            }
			else
            {
				*output++ = *v++;
            }
			*output++ = *y++;
        }
	}
    _mm_empty();        // Clear aliased fp register state

	if (shift_picture_up) {
		// Fill bottom line with one line of black UYVY
		for (i = 0; i < width*2; i += 4) {
			*output++ = 0x80;
			*output++ = 0x10;
			*output++ = 0x80;
			*output++ = 0x10;
		}
	}
}

#else 

void yuv422_to_uyvy_2(int width, int height, int shift_picture_up, uint8_t *input, uint8_t *output)
{
	int i, j;
	uint8_t *y, *u, *v;
	int start_line = 0;

	y = input;
	u = input + width*height;
	v = input + width*height * 3 / 2;
    
	// Shifting picture up one line is necessary when decoding PAL DV50
	if (shift_picture_up) 
    {
		// Skip one line of input picture and start one line lower
		start_line = 1;
        y += width;
        u += width / 2;
        v += width / 2;
	}

	// Convert to UYVY
	for (j = start_line; j < height; j++) 
    {
		for (i = 0; i < width; i++)
		{
			if (i % 2 == 0)
				*output++ = *u++;
			else
				*output++ = *v++;
			*output++ = *y++;
		}
	}

	if (shift_picture_up) {
		// Fill bottom line with one line of black
		for (i = 0; i < width*2; i += 4) {
			*output++ = 0x80;
			*output++ = 0x10;
			*output++ = 0x80;
			*output++ = 0x10;
		}
	}
}

#endif




// Convert YUV444 (planar) to UYVY (packed 4:2:2) using naive conversion where
// alternate UV samples are simply discarded.
// Used for displaying rawfiles of YUV444 (unlikely to be used with an AVFrame input
// since few codecs output YUV444).
void yuv444_to_uyvy(int width, int height, uint8_t *input, uint8_t *output)
{
	int i, j;
	uint8_t *y, *u, *v;
	y = input;
	u = y + width * height;
	v = y + width * height * 2;

	// Convert to UYVY
	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++)
		{
			if (i % 2 == 0) {		// even pixels, copy U, Y, V
				*output++ = *u++;
				*output++ = *y++;
				*output++ = *v++;
			}
			else {					// odd pixels, copy just Y
				*output++ = *y++;
				u++;
				v++;
			}
		}
	}
}


void fill_black(StreamFormat format, int width, int height, unsigned char* image)
{
    if (format == UYVY_FORMAT)
    {
        int i;
        int size = width * height * 2;
        
        for (i = 0; i < size; i += 4)
        {
            image[i + 0] = 0x80;
            image[i + 1] = 0x10;
            image[i + 2] = 0x80;
            image[i + 3] = 0x10;
        }
    }
    else if (format == YUV422_FORMAT)
    {
        int i;
        int size = width * height;
        unsigned char* y = image;
        unsigned char* u = &image[width * height];
        unsigned char* v = &image[width * height * 3 / 2];
        
        for (i = 0; i < size; i ++)
        {
            *y++ = 0x10;
            if (i % 2 == 0)
            {
                *u++ = 0x80;
                *v++ = 0x80;
            }
        }
    }
    else if (format == YUV420_FORMAT)
    {
        int i;
        int size = width * height;
        unsigned char* y = image;
        unsigned char* u = &image[width * height];
        unsigned char* v = &image[width * height * 5 / 4];
        
        for (i = 0; i < size; i ++)
        {
            *y++ = 0x10;
            if (i % 4 == 0)
            {
                *u++ = 0x80;
                *v++ = 0x80;
            }
        }
    }
    else
    {
        assert(0);
    }
}


