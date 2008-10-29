/*
 * $Id: YUV_small_pic.c,v 1.2 2008/10/29 17:47:41 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Jim Easterbrook, <easter@users.sourceforge.net>
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

#include "YUV_frame.h"
#include "YUV_small_pic.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Sub sample a line with no filtering
static void h_sub_2_alias(BYTE* srcLine, BYTE* dstLine,
                          const int inStride, const int outStride,
                          const int w)
{
    int		i;

    for (i = 0; i < w; i++)
    {
        *dstLine = *srcLine;
        dstLine += outStride;
        srcLine += inStride * 2;
    }
}

static void h_sub_3_alias(BYTE* srcLine, BYTE* dstLine,
                          const int inStride, const int outStride,
                          const int w)
{
    int		i;

    for (i = 0; i < w; i++)
    {
        *dstLine = *srcLine;
        dstLine += outStride;
        srcLine += inStride * 3;
    }
}

// Sub sample a line with (1,2,1)/4 filtering
static void h_sub_2_121(BYTE* srcLine, BYTE* dstLine,
                        const int inStride, const int outStride, const int w)
{
    int		acc;
    int		i;

    for (i = 0; i < w - 1; i++)
    {
        acc = *srcLine;
        srcLine += inStride;
        acc += *srcLine * 2;
        srcLine += inStride;
        acc += *srcLine;
        *dstLine = acc / 4;
        dstLine += outStride;
    }
    // repeat last edge sample
    acc = *srcLine;
    srcLine += inStride;
    acc += *srcLine * 3;
    *dstLine = acc / 4;
}

// Sub sample a line with (1,1)/2 filtering
static void h_sub_2_11(BYTE* srcLine, BYTE* dstLine,
                       const int inStride, const int outStride, const int w)
{
    int		acc;
    int		i;

    for (i = 0; i < w; i++)
    {
        acc = *srcLine;
        srcLine += inStride;
        acc += *srcLine;
        srcLine += inStride;
        *dstLine = acc / 2;
        dstLine += outStride;
    }
}

// Sub sample a line with (1,1,1)/3 filtering
static void h_sub_3_111(BYTE* srcLine, BYTE* dstLine,
                        const int inStride, const int outStride, const int w)
{
    int		acc;
    int		i;

    // repeat first edge sample
    acc = *srcLine;
    for (i = 0; i < w; i++)
    {
        acc += *srcLine;
        srcLine += inStride;
        acc += *srcLine;
        srcLine += inStride;
        *dstLine = acc / 3;
        dstLine += outStride;
        acc = *srcLine;
        srcLine += inStride;
    }
}

// Do (1,2,1)/4 vertical filtering
static void v_fil_121(BYTE* inBuffA, BYTE* inBuffB,
                      BYTE* inBuffC, BYTE* outBuff,
                      const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + (*inBuffB++ * 2) + *inBuffC++) / 4;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + (*inBuffB * 2) + *inBuffC) / 4;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
            inBuffC += inStride;
        }
}

// Do (1,1)/2 vertical filtering
static void v_fil_11(BYTE* inBuffA, BYTE* inBuffB, BYTE* outBuff,
                     const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + *inBuffB++) / 2;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + *inBuffB) / 2;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
        }
}

// Do (1,1,1)/3 vertical filtering
static void v_fil_111(BYTE* inBuffA, BYTE* inBuffB,
                      BYTE* inBuffC, BYTE* outBuff,
                      const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + *inBuffB++ + *inBuffC++) / 3;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + *inBuffB + *inBuffC) / 3;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
            inBuffC += inStride;
        }
}

typedef void sub_line_proc(BYTE*, BYTE*, const int, const int, const int);

// Make a half height copy of inBuff in outBuff
static void comp_2(component* inFrame, component* outFrame,
                   const int vfil, const int yoff,
                   sub_line_proc* do_h_sub, int hsub, BYTE** work)
{
    BYTE*	inBuff = inFrame->buff;
    BYTE*	outBuff = outFrame->buff;
    int		j;

    if (vfil <= 0)
    {
        j = (yoff + 50) / 100;
        inBuff += inFrame->lineStride * j;
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, outBuff,
                     inFrame->pixelStride, outFrame->pixelStride, outFrame->w);
            inBuff += inFrame->lineStride * 2;
            outBuff += outFrame->lineStride;
        }
    }
    else if (vfil >= 2 && ((yoff % 100) < 25 || (yoff % 100) >= 75))
    {
        int A, B, C;
        // repeat first edge line
        A = 1;
        B = 1;
        C = 2;
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, work[B], inFrame->pixelStride, 1, outFrame->w);
            inBuff += inFrame->lineStride;
            do_h_sub(inBuff, work[C], inFrame->pixelStride, 1, outFrame->w);
            inBuff += inFrame->lineStride;
            v_fil_121(work[A], work[B], work[C], outBuff,
                      1, outFrame->pixelStride, outFrame->w);
            outBuff += outFrame->lineStride;
            A = C;
            C = 2 - A;
        }
    }
    else
    {
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, work[0], inFrame->pixelStride, 1, outFrame->w);
            inBuff += inFrame->lineStride;
            do_h_sub(inBuff, work[1], inFrame->pixelStride, 1, outFrame->w);
            inBuff += inFrame->lineStride;
            v_fil_11(work[0], work[1], outBuff, 1, outFrame->pixelStride,
                     outFrame->w);
            outBuff += outFrame->lineStride;
        }
    }
}

// Make a third height copy of inBuff in outBuff
static void comp_3(component* inFrame, component* outFrame,
                   const int vfil, const int yoff,
                   sub_line_proc* do_h_sub, int hsub, BYTE** work)
{
    BYTE*	inBuff = inFrame->buff;
    BYTE*	outBuff = outFrame->buff;
    int		j;

    if (vfil <= 0)
    {
        j = (yoff + 25) / 50;
        inBuff += inFrame->lineStride * j;
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, outBuff,
                     inFrame->pixelStride, outFrame->pixelStride, outFrame->w);
            inBuff += inFrame->lineStride * 3;
            outBuff += outFrame->lineStride;
        }
    }
    else
    {
        j = (yoff + 25) / 50;
        if (j > 1)
            inBuff += inFrame->lineStride * (j - 1);
        // repeat first edge line
        do_h_sub(inBuff, work[0], inFrame->pixelStride, 1, outFrame->w);
        if (j > 0)
            inBuff += inFrame->lineStride;
        for (j = 0; j < outFrame->h - 1; j++)
        {
            do_h_sub(inBuff, work[1], inFrame->pixelStride, 1, outFrame->w);
            inBuff += inFrame->lineStride;
            do_h_sub(inBuff, work[2], inFrame->pixelStride, 1, outFrame->w);
            inBuff += inFrame->lineStride;
            v_fil_111(work[0], work[1], work[2], outBuff,
                      1, outFrame->pixelStride, outFrame->w);
            outBuff += outFrame->lineStride;
            do_h_sub(inBuff, work[0], inFrame->pixelStride, 1, outFrame->w);
            inBuff += inFrame->lineStride;
        }
        do_h_sub(inBuff, work[1], inFrame->pixelStride, 1, outFrame->w);
        inBuff += inFrame->lineStride;
        do_h_sub(inBuff, work[2], inFrame->pixelStride, 1, outFrame->w);
        v_fil_111(work[0], work[1], work[2], outBuff,
                  1, outFrame->pixelStride, outFrame->w);
    }
}

int small_pic(YUV_frame* in_frame, YUV_frame* out_frame,
              int x, int y, int hsub, int vsub,
              int intlc, int hfil, int vfil, void* workSpace)
{
    sub_line_proc*	sub_line_Y;
    sub_line_proc*	sub_line_UV;
    BYTE*		work[3];
    int			ssx, ssy;
    YUV_frame		sub_frame;

    ssx = in_frame->Y.w / in_frame->U.w;
    ssy = in_frame->Y.h / in_frame->U.h;
    // adjust position, if required
    x = max(x, 0);
    y = max(y, 0);
    x -= x % ssx;
    y -= y % ssy;
    if (intlc)
        y -= y % 2;
    // create a sub frame representing the portion of out_frame to be written
    sub_frame = *out_frame;
    sub_frame.Y.buff += (y * sub_frame.Y.lineStride) +
                        (x * sub_frame.Y.pixelStride);
    sub_frame.U.buff += ((y/ssy) * sub_frame.U.lineStride) +
                        ((x/ssx) * sub_frame.U.pixelStride);
    sub_frame.V.buff += ((y/ssy) * sub_frame.V.lineStride) +
                        ((x/ssx) * sub_frame.V.pixelStride);
    sub_frame.Y.w = min(in_frame->Y.w / hsub, out_frame->Y.w - x);
    sub_frame.Y.h = min(in_frame->Y.h / vsub, out_frame->Y.h - y);
    sub_frame.U.w = min(in_frame->U.w / hsub, out_frame->U.w - (x/ssx));
    sub_frame.U.h = min(in_frame->U.h / vsub, out_frame->U.h - (y/ssy));
    sub_frame.V.w = min(in_frame->V.w / hsub, out_frame->V.w - (x/ssx));
    sub_frame.V.h = min(in_frame->V.h / vsub, out_frame->V.h - (y/ssy));
    // select horizontal sub sampling routine
    switch (hsub)
    {
        case 2:
            if (hfil <= 0)
            {
                sub_line_Y = &h_sub_2_alias;
                sub_line_UV = &h_sub_2_alias;
            }
            else
            {
                if (ssx == 2)
                    sub_line_Y = &h_sub_2_121;
                else
                    sub_line_Y = &h_sub_2_11;
                sub_line_UV = &h_sub_2_11;
            }
            break;
        case 3:
            if (hfil <= 0)
            {
                sub_line_Y = &h_sub_3_alias;
                sub_line_UV = &h_sub_3_alias;
            }
            else
            {
                sub_line_Y = &h_sub_3_111;
                sub_line_UV = &h_sub_3_111;
            }
            break;
        default:
            return -1;
    }
    work[0] = workSpace;
    work[1] = work[0] + in_frame->Y.w;
    work[2] = work[1] + in_frame->Y.w;
    if (intlc)
    {
        YUV_frame	in_field;
        YUV_frame	out_field;
        switch (vsub)
        {
            case 2:
                // "first" field i.e. "even" lines"
                extract_YUV_field(in_frame, &in_field, 0);
                extract_YUV_field(&sub_frame, &out_field, 0);
                comp_2(&in_field.Y, &out_field.Y, vfil, 0,
                       sub_line_Y, hsub, work);
                comp_2(&in_field.U, &out_field.U, vfil, 0,
                       sub_line_UV, hsub, work);
                comp_2(&in_field.V, &out_field.V, vfil, 0,
                       sub_line_UV, hsub, work);
                // "second" field i.e. "odd" lines"
                extract_YUV_field(in_frame, &in_field, 1);
                extract_YUV_field(&sub_frame, &out_field, 1);
                comp_2(&in_field.Y, &out_field.Y, vfil, 50,
                       sub_line_Y, hsub, work);
                comp_2(&in_field.U, &out_field.U, vfil, 50,
                       sub_line_UV, hsub, work);
                comp_2(&in_field.V, &out_field.V, vfil, 50,
                       sub_line_UV, hsub, work);
                break;
            case 3:
                // "first" field i.e. "even" lines"
                extract_YUV_field(in_frame, &in_field, 0);
                extract_YUV_field(&sub_frame, &out_field, 0);
                comp_3(&in_field.Y, &out_field.Y, vfil, 0,
                       sub_line_Y, hsub, work);
                comp_3(&in_field.U, &out_field.U, vfil, 0,
                       sub_line_UV, hsub, work);
                comp_3(&in_field.V, &out_field.V, vfil, 0,
                       sub_line_UV, hsub, work);
                // "second" field i.e. "odd" lines"
                extract_YUV_field(in_frame, &in_field, 1);
                extract_YUV_field(&sub_frame, &out_field, 1);
                comp_3(&in_field.Y, &out_field.Y, vfil, 50,
                       sub_line_Y, hsub, work);
                comp_3(&in_field.U, &out_field.U, vfil, 50,
                       sub_line_UV, hsub, work);
                comp_3(&in_field.V, &out_field.V, vfil, 50,
                       sub_line_UV, hsub, work);
                break;
            default:
                return -1;
        }
    }
    else // not interlaced
    {
        switch (vsub)
        {
            case 2:
                comp_2(&in_frame->Y, &sub_frame.Y, vfil, 0, sub_line_Y,
                       hsub, work);
                comp_2(&in_frame->U, &sub_frame.U, vfil, 0, sub_line_UV,
                       hsub, work);
                comp_2(&in_frame->V, &sub_frame.V, vfil, 0, sub_line_UV,
                       hsub, work);
                break;
            case 3:
                comp_3(&in_frame->Y, &sub_frame.Y, vfil, 0, sub_line_Y,
                       hsub, work);
                comp_3(&in_frame->U, &sub_frame.U, vfil, 0, sub_line_UV,
                       hsub, work);
                comp_3(&in_frame->V, &sub_frame.V, vfil, 0, sub_line_UV,
                       hsub, work);
                break;
            default:
                return -1;
        }
    }
    return 0;
}
