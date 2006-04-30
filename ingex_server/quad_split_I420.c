/*
 * $Id: quad_split_I420.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Down-sample multiple video inputs to create quad-split view.
 *
 * Copyright (C) 2005  Jim Easterbrook <easter@users.sourceforge.net>
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

#include <stdlib.h>

#include "quad_split_I420.h"

int alloc_frame_I420(I420_frame* frame, const int image_w, const int image_h,
                     unsigned char* buffer)
{
    frame->w = image_w;
    frame->h = image_h;
    frame->size = frame->w * frame->h * 3 / 2;
    if (buffer == NULL)
        frame->cbuff = (unsigned char *)malloc(frame->size);
    else
        frame->cbuff = buffer;
    frame->Ybuff = frame->cbuff;
    frame->Ubuff = frame->Ybuff + (frame->w * frame->h);
    frame->Vbuff = frame->Ubuff + (frame->w * frame->h / 4);
    if ((frame->cbuff == NULL) || (image_w % 2 != 0) || (image_h % 2 != 0))
        return 0;
    return 1;
}

void clear_frame_I420(I420_frame* frame)
{
    unsigned char*	ptr;
    int			i, j;

    ptr = frame->Ybuff;
    for (j = 0; j < frame->h; j++)
        for (i = 0; i < frame->w; i++)
            *ptr++ = 16;
    ptr = frame->Ubuff;
    for (j = 0; j < frame->h / 2; j++)
        for (i = 0; i < frame->w / 2; i++)
            *ptr++ = 128;
    ptr = frame->Vbuff;
    for (j = 0; j < frame->h / 2; j++)
        for (i = 0; i < frame->w / 2; i++)
            *ptr++ = 128;
}

// Sub sample a line with no filtering
static void half_line_alias(unsigned char* srcLine, unsigned char* dstLine,
                            const int w)
{
    unsigned char*	src;
    unsigned char*	dst;
    int			i;

    src = srcLine;
    dst = dstLine;
    for (i = 0; i < w / 2; i++)
    {
        *dst++ = *src++;
        src++;
    }
}

// Sub sample a line with (1,2,1)/4 Y filtering and (1,1)/2 UV filtering
static void half_line_121(unsigned char* srcLine, unsigned char* dstLine,
                          const int w)
{
    unsigned char*	src0;
    unsigned char*	src1;
    unsigned char*	dst;
    int			i;
    unsigned char	held;

    // Work backwards to allow for awkward end of line stuff
    src1 = srcLine + w - 1;
    dst = dstLine + (w / 2) - 1;
    held = *src1;	// pre-load with edge value
    for (i = 0; i < w / 2; i++)
    {
        src0 = src1 - 1;
        *dst-- = (*src0 + (*src1 * 2) + held) / 4;
        held = *src0;
        src1 = src0 - 1;
    }
}

// Sub sample a line with (1,1)/2 filtering
static void half_line_11(unsigned char* srcLine, unsigned char* dstLine,
                         const int w)
{
    unsigned char*	src0;
    unsigned char*	src1;
    unsigned char*	dst;
    int			i;

    src0 = srcLine;
    dst = dstLine;
    for (i = 0; i < w / 2; i++)
    {
        src1 = src0 + 1;
        *dst++ = (*src0 + *src1) / 2;
        src0 = src1 + 1;
    }
}

// Do (1,2,1)/4 vertical filtering
static void v_fil_121(unsigned char* srcLineA, unsigned char* srcLineB,
                      unsigned char* srcLineC, unsigned char* dstLine,
                      const int w)
{
    int			i;
    unsigned char*	srcA;
    unsigned char*	srcB;
    unsigned char*	srcC;
    unsigned char*	dst;

    srcA = srcLineA;
    srcB = srcLineB;
    srcC = srcLineC;
    dst = dstLine;
    for (i = 0; i < w; i++)
        *dst++ = (*srcA++ + (*srcB++ * 2) + *srcC++) / 4;
}

// Do (1,1)/2 vertical filtering
static void v_fil_11(unsigned char* srcLineA, unsigned char* srcLineB,
                     unsigned char* dstLine, const int w)
{
    int			i;
    unsigned char*	srcA;
    unsigned char*	srcB;
    unsigned char*	dst;

    srcA = srcLineA;
    srcB = srcLineB;
    dst = dstLine;
    for (i = 0; i < w; i++)
        *dst++ = (*srcA++ + *srcB++) / 2;
}

typedef void half_line_proc(unsigned char*, unsigned char*, const int);
static half_line_proc* half_line_Y;
static half_line_proc* half_line_UV;

static unsigned char*	work[3] = {NULL, NULL, NULL};

// Make a quarter size copy of in_buff in out_buff, top left corner at x, y
static void quarter_121_Y(I420_frame* in_frame, I420_frame* out_frame,
                          unsigned char* srcLine, unsigned char* dstLine,
                          const int v_fil, const int step)
{
    int		j;
    int		Nm1, N, Np1;

    Nm1 = 0;		// use first line twice (edge padding)
    N   = 0;
    Np1 = 1;
    for (j = 0; j < in_frame->h / (step * 2); j++)
    {
        if (v_fil)
        {
            half_line_Y(srcLine, work[N], in_frame->w);
            srcLine += in_frame->w * step;
            half_line_Y(srcLine, work[Np1], in_frame->w);
            srcLine += in_frame->w * step;
            v_fil_121(work[Nm1], work[N], work[Np1], dstLine, in_frame->w / 2);
            Nm1 =  Np1;
            N   = (Nm1 + 1) % 3;
            Np1 = (N   + 1) % 3;
        }
        else	// no v_fil
        {
            half_line_Y(srcLine, dstLine, in_frame->w);
            srcLine += in_frame->w * step * 2;
        }
        dstLine += out_frame->w * step;
    }
}

static void quarter_121_UV(I420_frame* in_frame, I420_frame* out_frame,
                           unsigned char* srcLine, unsigned char* dstLine,
                           const int v_fil, const int step)
{
    int		j;
    int		Nm1, N, Np1;

    Nm1 = 0;		// use first line twice (edge padding)
    N   = 0;
    Np1 = 1;
    for (j = 0; j < in_frame->h / (step * 4); j++)
    {
        if (v_fil)
        {
            half_line_UV(srcLine, work[N], in_frame->w / 2);
            srcLine += in_frame->w * step / 2;
            half_line_UV(srcLine, work[Np1], in_frame->w / 2);
            srcLine += in_frame->w * step / 2;
            v_fil_121(work[Nm1], work[N], work[Np1], dstLine, in_frame->w / 4);
            Nm1 =  Np1;
            N   = (Nm1 + 1) % 3;
            Np1 = (N   + 1) % 3;
        }
        else	// no v_fil
        {
            half_line_UV(srcLine, dstLine, in_frame->w / 2);
            srcLine += in_frame->w * step;
        }
        dstLine += out_frame->w * step / 2;
    }
}

static void quarter_11_Y(I420_frame* in_frame, I420_frame* out_frame,
                         unsigned char* srcLine, unsigned char* dstLine,
                         const int v_fil, const int step)
{
    int			j;

    for (j = 0; j < in_frame->h / (step * 2); j++)
    {
        if (v_fil)
        {
            half_line_Y(srcLine, work[0], in_frame->w);
            srcLine += in_frame->w * step;
            half_line_Y(srcLine, work[1], in_frame->w);
            srcLine += in_frame->w * step;
            v_fil_11(work[0], work[1], dstLine, in_frame->w / 2);
        }
        else	// no v_fil
        {
            half_line_Y(srcLine, dstLine, in_frame->w);
            srcLine += in_frame->w * step * 2;
        }
        dstLine += out_frame->w * step;
    }
}

static void quarter_11_UV(I420_frame* in_frame, I420_frame* out_frame,
                          unsigned char* srcLine, unsigned char* dstLine,
                          const int v_fil, const int step)
{
    int			j;

    for (j = 0; j < in_frame->h / (step * 4); j++)
    {
        if (v_fil)
        {
            half_line_UV(srcLine, work[0], in_frame->w / 2);
            srcLine += in_frame->w * step / 2;
            half_line_UV(srcLine, work[1], in_frame->w / 2);
            srcLine += in_frame->w * step / 2;
            v_fil_11(work[0], work[1], dstLine, in_frame->w / 4);
        }
        else	// no v_fil
        {
            half_line_UV(srcLine, dstLine, in_frame->w / 2);
            srcLine += in_frame->w * step;
        }
        dstLine += out_frame->w * step / 2;
    }
}

void quarter_frame_I420(I420_frame* in_frame, I420_frame* out_frame,
                        int x, int y, int intlc, int hfil, int vfil)
{
    if (work[0] == NULL)
    {
        work[0] = (unsigned char*)malloc(out_frame->w * 3);
        work[1] = work[0] + out_frame->w;
        work[2] = work[1] + out_frame->w;
    }
    if (hfil)
    {
        half_line_Y = &half_line_121;
        half_line_UV = &half_line_11;
    }
    else
    {
        half_line_Y = &half_line_alias;
        half_line_UV = &half_line_alias;
    }
    if (intlc)
    {
        // "first" field i.e. "even" lines"
        quarter_121_Y(in_frame, out_frame, in_frame->Ybuff,
                      out_frame->Ybuff + (y * out_frame->w) + x,
                      vfil, 2);
        quarter_121_UV(in_frame, out_frame, in_frame->Ubuff,
                       out_frame->Ubuff + ((y/2) * out_frame->w / 2) + (x/2),
                       vfil, 2);
        quarter_121_UV(in_frame, out_frame, in_frame->Vbuff,
                       out_frame->Vbuff + ((y/2) * out_frame->w / 2) + (x/2),
                       vfil, 2);
        // "second" field i.e. "odd" lines"
        quarter_11_Y(in_frame, out_frame,
                     in_frame->Ybuff + in_frame->w,
                     out_frame->Ybuff + ((y+1) * out_frame->w) + x,
                     vfil, 2);
        quarter_11_UV(in_frame, out_frame,
                      in_frame->Ubuff + (in_frame->w / 2),
                      out_frame->Ubuff + (((y/2)+1) * out_frame->w / 2) + (x/2),
                      vfil, 2);
        quarter_11_UV(in_frame, out_frame,
                      in_frame->Vbuff + (in_frame->w / 2),
                      out_frame->Vbuff + (((y/2)+1) * out_frame->w / 2) + (x/2),
                      vfil, 2);
    }
    else
        quarter_121_Y(in_frame, out_frame, in_frame->Ybuff,
                      out_frame->Ybuff + (y * out_frame->w) + x,
                      vfil, 1);
        quarter_121_UV(in_frame, out_frame, in_frame->Ubuff,
                       out_frame->Ubuff + ((y/2) * out_frame->w / 2) + (x/2),
                       vfil, 1);
        quarter_121_UV(in_frame, out_frame, in_frame->Vbuff,
                       out_frame->Vbuff + ((y/2) * out_frame->w / 2) + (x/2),
                       vfil, 1);
}
