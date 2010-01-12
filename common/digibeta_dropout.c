/*
 * $Id: digibeta_dropout.c,v 1.1 2010/01/12 16:02:07 john_f Exp $
 *
 * DigiBeta dropout detector
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Jim Easterbrook
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

#include <stdlib.h>     // for qsort
#include <string.h>     // for memset

#include "YUV_frame.h"
#include "digibeta_dropout.h"

#define WIDTH   720
#define HEIGHT  576

#define BL_W    (WIDTH / 8)
#define BL_H    (HEIGHT / 8)
#define BL_H2   ((HEIGHT + 32) / 8)
//
// Added Jonathan Dec 09
//
#define MAXIMUM(a,b) ((a) < (b) ? (b) : (a))
#define MINIMUM(a,b) ((a) < (b) ? (a) : (b))


static int median_3(int A, int B, int C)
{
    if (A > B)
    {
        if (B >= C)
            return B;
        // B is the smallest
        if (A <= C)
            return A;
        return C;
    }
    else // A <= B
    {
        if (B <= C)
            return B;
        // B is the largest
        if (A >= C)
            return A;
        return C;
    }
}

// horizontally high pass filter a line
static void hpf_line(BYTE* inBuff, int* outBuff,
                     const int inStride, const int w)
{
    int     i;
    BYTE*   inPtr;
    int*    outPtr;
    int     A, B, C;

    inPtr = inBuff;
    outPtr = outBuff + 1;
    B = *inPtr;
    inPtr += inStride;
    C = *inPtr;
    inPtr += inStride;
    for (i = 1; i < w - 1; i++)
    {
        A = B;
        B = C;
        C = *inPtr;
        inPtr += inStride;
        *outPtr++ = abs(B - median_3(A, B, C));
    }
}

static int block_average(int* inFrame, int* outFrame)
{
    int     acc[BL_W];
    int     i, j, y;
    int*    inLine;
    int*    inPtr;
    int*    outLine;
    int*    outPtr;

    inLine = inFrame;
    outLine = outFrame;
    for (j = 0; j < BL_H; j++)
    {
        for (i = 0; i < BL_W; i++)
            acc[i] = 0;
        for (y = 0; y < 4; y++)
        {
            inPtr = inLine;
            for (i = 0; i < BL_W; i++)
            {
                acc[i] += *inPtr++;
                acc[i] += *inPtr++;
                acc[i] += *inPtr++;
                acc[i] += *inPtr++;
                acc[i] += *inPtr++;
                acc[i] += *inPtr++;
                acc[i] += *inPtr++;
                acc[i] += *inPtr++;
            }
            inLine += WIDTH;
        }
        outPtr = outLine;
        for (i = 0; i < BL_W; i++)
            *outPtr++ = acc[i];
        outLine += BL_W;
    }
    return 0;
}

// Subtract MAX of neighbouring pixels to remove areas and leave spikes
static int spatial_hpf(int* inFrame, int* outFrame)
{
    int     i, j;
    int     x, y;
    int*    inLine;
    int*    inPtr;
    int*    outLine;
    int*    outPtr;

    outLine = outFrame;
    for (y = 0; y < BL_H; y++)
    {
        outPtr = outLine;
        for (x = 0; x < BL_W; x++)
            *outPtr++ = 0;
        outLine += BL_W;
    }
    for (j = -1; j <= 1; j++)
    {
        for (i = -1; i <= 1; i++)
        {
            if (i != 0 || j != 0)
            {
                inLine = inFrame;
                outLine = outFrame;
                if (i < 0)
                    outLine -= i;
                else
                    inLine += i;
                if (j < 0)
                    outLine -= j * BL_W;
                else
                    inLine += j * BL_W;
                for (y = 0; y < BL_H - abs(j); y++)
                {
                    inPtr = inLine;
                    outPtr = outLine;
                    for (x = 0; x < BL_W - abs(i); x++)
                    {
                        *outPtr = MAXIMUM(*outPtr, *inPtr);
                        // max is a macro, so don't increment pointers within it!
                        inPtr++;
                        outPtr++;
                    }
                    inLine += BL_W;
                    outLine += BL_W;
                }
            }
        }
    }
    // now subtract 4 * max(neighbours) from main value
    inLine = inFrame;
    outLine = outFrame;
    for (y = 0; y < BL_H; y++)
    {
        inPtr = inLine;
        outPtr = outLine;
        for (x = 0; x < BL_W; x++)
        {
            *outPtr = *inPtr++ - (4 * *outPtr);
            outPtr++;
        }
        inLine += BL_W;
        outLine += BL_W;
    }
    return 0;
}

static int compare(const void* A, const void* B)
{
    if (*(int*)A > *(int*)B)
        return -1;
    if (*(int*)A < *(int*)B)
        return 1;
    return 0;
}

static dropout_result detect_dropout(int* inFrame)
{
    int             block, seq, sixth;  // loop counters
    int             idx;                // buffer index
    int             x0, y0;             // coords of input sample
    int             acc;
    int             buff[20];
    dropout_result  result;
    int*            inPtr;

    result.strength = -1000;
    result.sixth = -1;
    result.seq = -1;
    for (sixth = 0; sixth < 6; sixth++)
    {
        // accumulate input data, selecting values according to
        // presumed DigiBeta shuffling pattern
        x0 =  9 + (sixth / 2);
        y0 = 62 + (sixth % 2);
        for (seq = 0; seq < 57; seq++)
        {
            idx = 0;
            for (block = 0; block < 20; block++)
            {
                // bring pointers within range
                if (x0 < 0)
                {
                    x0 += BL_W;
                    y0 -= 2;
                }
                if (y0 >= BL_H2)
                {
                    y0 -= BL_H2;
                }
                // copy data
                if (y0 < BL_H)
                {
                    inPtr = inFrame + (y0 * BL_W) + x0;
                    buff[idx] = *inPtr;
                    idx += 1;
                }
                // increment pointers
                x0 -= 21;
                y0 += 4;
            }
            y0 += 40;
            // sort data and average largest two
            qsort(buff, idx, sizeof(buff[0]), compare);
            acc = ((buff[0] * 15) + (buff[1] * 100) + (buff[2] * 65)) / (15 + 100 + 65);
            // result is largest of these
            if (result.strength < acc)
            {
                result.strength = acc;
                result.sixth = sixth;
                result.seq = seq;
            }
        }
    }
    return result;
}

dropout_result digibeta_dropout(YUV_frame* in_frame, int xOffset, int yOffset,
                                int* workSpace)
{
    int     h, w, y;
    BYTE*   inLine;
    int*    outLine;
    int*    work[2];

    work[0] = workSpace;
    work[1] = work[0] + (WIDTH * HEIGHT / 2);
    // high pass filter input to workspace
    memset(work[0], 0, sizeof(int) * WIDTH * HEIGHT / 2);
    inLine = in_frame->Y.buff;
    outLine = work[0];
    if (xOffset >= 0)
    {
        inLine += xOffset * in_frame->Y.pixelStride;
        w = MINIMUM(in_frame->Y.w - xOffset, WIDTH);
    }
    else
    {
        outLine -= xOffset;
        w = MINIMUM(in_frame->Y.w, WIDTH + xOffset);
    }
    if (yOffset >= 0)
    {
        inLine += yOffset * in_frame->Y.lineStride;
        h = MINIMUM(in_frame->Y.h - yOffset, HEIGHT);
    }
    else
    {
        h = in_frame->Y.h;
        if ((yOffset % 2) != 0)
        {
            inLine += in_frame->Y.lineStride;
            yOffset -= 1;
            h -= 1;
        }
        outLine -= (yOffset / 2) * WIDTH;
        h = MINIMUM(h, HEIGHT + yOffset);
    }
    for (y = 0; y < (h + 1) / 2; y++)
    {
        hpf_line(inLine, outLine, in_frame->Y.pixelStride, w);
        inLine += 2 * in_frame->Y.lineStride;
        outLine += WIDTH;
    }
    // block average high pass filtered input
    block_average(work[0], work[1]);
    // high pass filter it to isolate dropout spikes
    spatial_hpf(work[1], work[0]);
    // process data in workspace to find dropouts
    return detect_dropout(work[0]);
}

