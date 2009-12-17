/*
 * $Id: YUV_blend.c,v 1.1 2009/12/17 15:43:29 john_f Exp $
 *
 *
 * Copyright (C) 2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Jim Easterbrook
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

#include <string.h>

#include "YUV_frame.h"
#include "YUV_blend.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

static void blend_line(BYTE* srcLine1, const int inStride1,
                       BYTE* srcLine2, const int inStride2,
                       BYTE* dstLine, const int outStride,
                       const int alpha, const int invert, const int w)
{
    int  i;
    int  in1, in2, out;
    int beta;
    if (invert)
    {
        beta = alpha - 256;
        for (i = w; i != 0; i--)
        {
            in1 = *srcLine1;
            srcLine1 += inStride1;
            in2 = *srcLine2;
            srcLine2 += inStride2;
            out = (alpha * in1) + (beta * in2);
            out = abs(out) + (128 * 256);
            *dstLine = min(out / 256, 255);
            dstLine += outStride;
        }
    }
    else
    {
        beta = 256 - alpha;
        for (i = w; i != 0; i--)
        {
            in1 = *srcLine1;
            srcLine1 += inStride1;
            in2 = *srcLine2;
            srcLine2 += inStride2;
            out = (alpha * in1) + (beta * in2);
            *dstLine = out / 256;
            dstLine += outStride;
        }
    }
}

int blend(YUV_frame* in_frame1, YUV_frame* in_frame2, YUV_frame* out_frame,
          const double alpha, const int invert)
{
    int     ssx1, ssy1, ssx2, ssy2;
    BYTE*   Y_in1;
    BYTE*   U_in1;
    BYTE*   V_in1;
    BYTE*   Y_in2;
    BYTE*   U_in2;
    BYTE*   V_in2;
    BYTE*   Y_out;
    BYTE*   U_out;
    BYTE*   V_out;
    int     w, h, j;
    int     i_alpha;

    // check input formats
    ssx1 = in_frame1->Y.w / in_frame1->U.w;
    ssy1 = in_frame1->Y.h / in_frame1->U.h;
    ssx2 = in_frame2->Y.w / in_frame2->U.w;
    ssy2 = in_frame2->Y.h / in_frame2->U.h;
    if ((ssx1 != ssx2) || (ssy1 != ssy2))
        return YUV_Fail;
    // get working dimensions
    w = min(in_frame1->Y.w, in_frame2->Y.w);
    h = min(in_frame1->Y.h, in_frame2->Y.h);
    // do it
    i_alpha = (alpha * 256.0) + 0.5;
    Y_in1 = in_frame1->Y.buff;
    Y_in2 = in_frame2->Y.buff;
    Y_out = out_frame->Y.buff;
    for (j = h; j != 0; j--)
    {
        blend_line(Y_in1, in_frame1->Y.pixelStride,
                   Y_in2, in_frame2->Y.pixelStride,
                   Y_out, out_frame->Y.pixelStride,
                   i_alpha, invert, w);
        Y_in1 += in_frame1->Y.lineStride;
        Y_in2 += in_frame2->Y.lineStride;
        Y_out += out_frame->Y.lineStride;
    }
    w = w / ssx1;
    h = h / ssy1;
    U_in1 = in_frame1->U.buff;
    V_in1 = in_frame1->V.buff;
    U_in2 = in_frame2->U.buff;
    V_in2 = in_frame2->V.buff;
    U_out = out_frame->U.buff;
    V_out = out_frame->V.buff;
    for (j = h; j != 0; j--)
    {
        blend_line(U_in1, in_frame1->U.pixelStride,
                   U_in2, in_frame2->U.pixelStride,
                   U_out, out_frame->U.pixelStride,
                   i_alpha, invert, w);
        blend_line(V_in1, in_frame1->V.pixelStride,
                   V_in2, in_frame2->V.pixelStride,
                   V_out, out_frame->V.pixelStride,
                   i_alpha, invert, w);
        U_in1 += in_frame1->U.lineStride;
        V_in1 += in_frame1->V.lineStride;
        U_in2 += in_frame2->U.lineStride;
        V_in2 += in_frame2->V.lineStride;
        U_out += out_frame->U.lineStride;
        V_out += out_frame->V.lineStride;
    }
    return YUV_OK;
}
