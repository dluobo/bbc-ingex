/*
 * $Id: YUV_to_rgb.c,v 1.1 2009/12/17 15:43:29 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
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
#include "YUV_to_rgb.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Super sample a line with (1,2,1)/4 filtering
static void h_up_2_121(BYTE* srcLine, BYTE* dstLine,
                       const int inStride, const int w)
{
    int     acc;
    int     i;
    BYTE    in_0;
    BYTE    in_1;

    in_0 = *srcLine;
    srcLine += inStride;
    for (i = (w/2) - 1; i != 0; i--)
    {
        *dstLine++ = in_0;
        in_1 = *srcLine;
        srcLine += inStride;
        acc = in_0;
        acc += in_1;
        *dstLine++ = (acc + 1) / 2;
        in_0 = in_1;
    }
    *dstLine++ = in_0;
    // repeat last edge sample
    *dstLine = in_0;
}

int to_RGB(YUV_frame* in_frame, BYTE* out_R, BYTE* out_G, BYTE* out_B,
           const int RGBpixelStride, const int RGBlineStride,
           void* workSpace)
{
    int     ssx, ssy;
    BYTE*   Y_line;
    BYTE*   U_line;
    BYTE*   V_line;
    BYTE*   Y_p;
    BYTE*   U_p;
    BYTE*   V_p;
    BYTE*   R_line;
    BYTE*   G_line;
    BYTE*   B_line;
    BYTE*   R_p;
    BYTE*   G_p;
    BYTE*   B_p;
    int     i, j;
    int     U_inc, V_inc;
    int     Y, U, V, R, G, B;

    // check in_frame format
    ssx = in_frame->Y.w / in_frame->U.w;
    ssy = in_frame->Y.h / in_frame->U.h;
    if ((ssx > 2) || (ssy != 1))
        return YUV_Fail;    // can only convert from 4:4:4 or 4:2:2 so far
    // set up intermediate arrays
    if (ssx == 2)
    {
        U_inc = 1;
        V_inc = 1;
    }
    else
    {
        U_inc = in_frame->U.pixelStride;
        V_inc = in_frame->V.pixelStride;
    }
    // do it
    Y_line = in_frame->Y.buff;
    U_line = in_frame->U.buff;
    V_line = in_frame->V.buff;
    R_line = out_R;
    G_line = out_G;
    B_line = out_B;
    for (j = in_frame->Y.h; j != 0; j--)
    {
        Y_p = Y_line;
        if (ssx == 2)
        {
            U_p = workSpace;
            V_p = U_p + in_frame->Y.w;
            // up convert chrominance
            h_up_2_121(U_line, U_p, in_frame->U.pixelStride, in_frame->Y.w);
            h_up_2_121(V_line, V_p, in_frame->V.pixelStride, in_frame->Y.w);
        }
        else
        {
            U_p = U_line;
            V_p = V_line;
        }
        R_p = R_line;
        G_p = G_line;
        B_p = B_line;
        for (i = in_frame->Y.w; i != 0; i--)
        {
            Y = *Y_p;   Y_p += in_frame->Y.pixelStride;
            U = *U_p;   U_p += U_inc;
            V = *V_p;   V_p += V_inc;
            U = U - 128;
            V = V - 128;
            R = Y + (V * 89831 / 65536);
            G = Y - (((V * 45757) + (U * 22050)) / 65536);
            B = Y + (U * 113538 / 65536);
            *R_p = min(max(R, 0), 255);
            *G_p = min(max(G, 0), 255);
            *B_p = min(max(B, 0), 255);
            R_p += RGBpixelStride;
            G_p += RGBpixelStride;
            B_p += RGBpixelStride;
        }
        Y_line += in_frame->Y.lineStride;
        U_line += in_frame->U.lineStride;
        V_line += in_frame->V.lineStride;
        R_line += RGBlineStride;
        G_line += RGBlineStride;
        B_line += RGBlineStride;
    }
    return YUV_OK;
}
