/*
 * $Id: YUV_scale_pic.c,v 1.3 2009/09/18 15:07:24 philipn Exp $
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

#include <string.h> // for memset
#include <stdint.h> // for uint32_t
#include <stdio.h>

#include "YUV_frame.h"
#include "YUV_scale_pic.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Copy a line, scaling down by 256
static void h_scale_copy(uint32_t* srcLine, BYTE* dstLine,
                         const int outStride, const int w)
{
    int     i;

    for (i = 0; i < w; i++)
    {
        *dstLine = *srcLine++ / 256;
        dstLine += outStride;
    }
}

// Scale a line with no filtering
static void h_sub_alias(BYTE* srcLine, BYTE* dstLine,
                        const int inStride, const int outStride,
                        const int xup, const int xdown, const int w,
                        uint32_t* work)
{
    int     i;
    int     dx_int;
    int     dx_frac;
    int     err;

    dx_int = xdown / xup; // whole number of input samples per output sample
    dx_frac = xdown - (dx_int * xup); // fractional part of same
    err = xup / 2; // start at centre of first input sample group
    if (err + (w * dx_frac) <= xup)
    {
        for (i = 0; i < w; i++)
        {
            *dstLine = *srcLine;
            dstLine += outStride;
            srcLine += inStride * dx_int;
        }
        return;
    }
    for (i = 0; i < w; i++)
    {
        if (err >= xup)
        {
            err -= xup;
            srcLine += inStride;
        }
        *dstLine = *srcLine;
        dstLine += outStride;
        srcLine += inStride * dx_int;
        err += dx_frac;
    }
}

// Same as above, but scales input by 256
static void h_sub_scale_alias(BYTE* srcLine, uint32_t* dstLine,
                              const int inStride,
                              int xup, int xdown, const int w)
{
    int     i;
    int     dx_int;
    int     dx_frac;
    int     err;

    dx_int = xdown / xup; // whole number of input samples per output sample
    dx_frac = xdown - (dx_int * xup); // fractional part of same
    err = xup / 2; // start at centre of first input sample group
    if (err + (w * dx_frac) <= xup)
    {
        for (i = 0; i < w; i++)
        {
            *dstLine++ = *srcLine * 256;
            srcLine += inStride * dx_int;
        }
        return;
    }
    for (i = 0; i < w; i++)
    {
        if (err >= xup)
        {
            err -= xup;
            srcLine += inStride;
        }
        *dstLine++ = *srcLine * 256;
        srcLine += inStride * dx_int;
        err += dx_frac;
    }
}

// Scale a line with interpolation
static void h_sub_scale_interp(BYTE* srcLine, uint32_t* dstLine,
                               const int inStride,
                               int xup, int xdown, const int w)
{
    int     i;
    int     err;
    uint32_t    alpha;
    uint32_t    beta;
    uint32_t    inpA, inpB;

    // first input and output samples are cosited
    err = xup;
    // get first input
    inpB = *srcLine;
    srcLine += inStride;
    inpA = inpB;
    for (i = 0; i < w; i++)
    {
        if (err > xup)
        {
            // get next input
            inpA = inpB;
            inpB = *srcLine;
            srcLine += inStride;
            err -= xup;
        }
        alpha = (xup - err) * 256;
        beta =         err  * 256;
        *dstLine++ = ((alpha * inpA) +
                       (beta * inpB)) / xup;
        err += xdown;
    }
}

// Scale a line with averaging
static void h_sub_scale_ave(BYTE* srcLine, uint32_t* dstLine,
                            const int inStride,
                            int xup, int xdown, const int w)
{
    int     i;
    int     err;
    uint32_t    part;
    uint32_t    whole;
    uint32_t    acc;

    if ((xup * 2) == xdown)
    {
        // reduces to simple 1/4, 1/2, 1/4 filter (i.e. 64, 128, 64)
        // edge padding applied to first sample
        acc = (64 + 128) * *srcLine;
        srcLine += inStride;
        part = 64 * *srcLine;
        srcLine += inStride;
        *dstLine++ = acc + part;
        // rest of samples are easier
        for (i = 1; i < w; i++)
        {
            acc = part + (128 * *srcLine);
            srcLine += inStride;
            part = 64 * *srcLine;
            srcLine += inStride;
            *dstLine++ = acc + part;
        }
        return;
    }
    if ((xup * 3) == xdown)
    {
        // reduces to simple 1/3, 1/3, 1/3 filter (i.e. 85, 86, 85)
        // edge padding applied to first sample
        acc = (85 + 86) * *srcLine;
        srcLine += inStride;
        acc += 85 * *srcLine;
        srcLine += inStride;
        *dstLine++ = acc;
        // rest of samples are easier
        for (i = 1; i < w; i++)
        {
            acc = 85 * *srcLine;
            srcLine += inStride;
            acc += 86 * *srcLine;
            srcLine += inStride;
            acc += 85 * *srcLine;
            srcLine += inStride;
            *dstLine++ = acc;
        }
        return;
    }
    // start at centre of first input sample group
    err = (xdown - xup) / 2;
    // initialise accumulated output with edge padding
    acc = (xdown - err) * *srcLine;
    srcLine += inStride;
    for (i = 0; i < w; i++)
    {
        while (err > xup)
        {
            // add next input to accumulated input
            acc += xup * *srcLine;
            srcLine += inStride;
            err -= xup;
        }
        // divide next sample between acumulated input and next output and save
        // this output
        whole = *srcLine;
        part = whole * err;
        srcLine += inStride;
        err -= xup;
        *dstLine++ = ((acc + part) * 256) / xdown;
        acc = (whole * xup) - part; // residue of input sample
        err += xdown;
    }
}

static void h_sub_interp(BYTE* srcLine, BYTE* dstLine,
                         const int inStride, const int outStride,
                         const int xup, const int xdown, const int w,
                         uint32_t* work)
{
    h_sub_scale_interp(srcLine, work, inStride, xup, xdown, w);
    h_scale_copy(work, dstLine, outStride, w);
}

static void h_sub_ave(BYTE* srcLine, BYTE* dstLine,
                      const int inStride, const int outStride,
                      const int xup, const int xdown, const int w,
                      uint32_t* work)
{
    h_sub_scale_ave(srcLine, work, inStride, xup, xdown, w);
    h_scale_copy(work, dstLine, outStride, w);
}

typedef void sub_line_proc2(BYTE*, BYTE*, const int, const int,
                            int, int, const int, uint32_t*);

static void v_sub_alias(component* inFrame, component* outFrame,
                        const int hfil, int xup, int xdown,
                        int yup, int ydown, int yoff, uint32_t** work)
{
    BYTE*   inBuff = inFrame->buff;
    BYTE*   outBuff = outFrame->buff;
    int     j;
    int     dy_int;
    int     dy_frac;
    int     err;
    sub_line_proc2* h_sub;

    // select horizontal subsampling routine
    if (hfil <= 0 || xup == xdown)
        h_sub = &h_sub_alias;
    else if (xup > xdown)
        h_sub = &h_sub_interp;
    else
        h_sub = &h_sub_ave;
    // get whole and scaled fractional part of down:up ratio
    dy_int = ydown / yup;
    dy_frac = ydown - (dy_int * yup);
    // start at centre of first pixel
    err = yup / 2;
    // adjust for offset (percentage of output sample)
    err += ((ydown - yup) * yoff) / 100;
    for (j = 0; j < outFrame->h; j++)
    {
        while (err >= yup)
        {
            err -= yup;
            inBuff += inFrame->lineStride;
        }
        h_sub(inBuff, outBuff,
              inFrame->pixelStride, outFrame->pixelStride,
              xup, xdown, outFrame->w, work[0]);
        outBuff += outFrame->lineStride;
        inBuff += inFrame->lineStride * dy_int;
        err += dy_frac;
    }
}

static void add_line(uint32_t* acc, uint32_t* inp, const int w,
                     const uint32_t gain)
{
    int     i;

    for (i = 0; i < w; i++)
        *acc++ += *inp++ * gain;
}

static void scale_line(uint32_t* acc, uint32_t* inp, const int w,
                       const uint32_t gain)
{
    int     i;

    for (i = 0; i < w; i++)
        *acc++ = *inp++ * gain;
}

static void interp_line(uint32_t* inpA, uint32_t* inpB, BYTE* dstLine,
                        const int outStride, const int w,
                        const uint32_t alpha, const uint32_t beta,
                        const uint32_t gain)
{
    int     i;

    for (i = 0; i < w; i++)
    {
        *dstLine = ((alpha * *inpA++) +
                     (beta * *inpB++)) / gain;
        dstLine += outStride;
    }
}

static void split_line(uint32_t* acc, uint32_t* inp, BYTE* dstLine,
                       const int outStride, const int w,
                       const uint32_t alpha, const uint32_t beta,
                       const uint32_t gain)
{
    int     i;
    uint32_t    whole;

    if (beta == 0)
    {
        for (i = 0; i < w; i++)
        {
            *dstLine = (*acc + (*inp++ * alpha)) / gain;
            dstLine += outStride;
            *acc++ = 0;
        }
        return;
    }
    for (i = 0; i < w; i++)
    {
        whole = *inp++;
        *dstLine = (*acc + (whole * alpha)) / gain;
        dstLine += outStride;
        *acc++ = whole * beta; // residue of input sample
    }
}

typedef void sub_line_proc1(BYTE*, uint32_t*, const int, int, int, const int);

static void v_sub_interp(component* inFrame, component* outFrame,
                         const int hfil, int xup, int xdown,
                         int yup, int ydown, int yoff, uint32_t** work)
{
    BYTE*   inBuff = inFrame->buff;
    BYTE*   outBuff = outFrame->buff;
    int     j;
    int     err;
    int     A, B;
    sub_line_proc1* h_sub;

    // select horizontal subsampling routine
    if (hfil <= 0 || xup == xdown)
        h_sub = &h_sub_scale_alias;
    else if (xup > xdown)
        h_sub = &h_sub_scale_interp;
    else
        h_sub = &h_sub_scale_ave;
    // first input and output samples are cosited
    err = yup;
    // adjust for offset (percentage of output sample)
    err += ((ydown - yup) * yoff) / 100;
    // get first line of input
    B = 0;
    h_sub(inBuff, work[B],
          inFrame->pixelStride, xup, xdown, outFrame->w);
    inBuff += inFrame->lineStride;
    A = B;
    for (j = 0; j < outFrame->h; j++)
    {
        if (err > yup)
        {
            // get next line of input
            A = B;
            B = 1 - A;
            h_sub(inBuff, work[B],
                  inFrame->pixelStride, xup, xdown, outFrame->w);
            inBuff += inFrame->lineStride;
            err -= yup;
        }
        interp_line(work[A], work[B], outBuff,
                    outFrame->pixelStride, outFrame->w,
                    yup - err, err, 256 * yup);
        outBuff += outFrame->lineStride;
        err += ydown;
    }
}

static void v_sub_ave(component* inFrame, component* outFrame,
                      const int hfil, int xup, int xdown,
                      int yup, int ydown, int yoff, uint32_t** work)
{
    BYTE*   inBuff = inFrame->buff;
    BYTE*   outBuff = outFrame->buff;
    int     j;
    int     err;
    uint32_t    scale;
    sub_line_proc1* h_sub;

    // select horizontal subsampling routine
    if (hfil <= 0 || xup == xdown)
        h_sub = &h_sub_scale_alias;
    else if (xup > xdown)
        h_sub = &h_sub_scale_interp;
    else
        h_sub = &h_sub_scale_ave;
    // start at centre of first input sample group
    err = (ydown - yup) / 2;
    // adjust for offset (percentage of output sample)
    err += ((ydown - yup) * yoff) / 100;
    // get first input line
    h_sub(inBuff, work[1],
          inFrame->pixelStride, xup, xdown, outFrame->w);
    inBuff += inFrame->lineStride;
    // edge padding of first input line
    scale_line(work[0], work[1], outFrame->w, ydown - err);
    for (j = 0; j < outFrame->h; j++)
    {
        while (err > yup)
        {
            h_sub(inBuff, work[1],
                  inFrame->pixelStride, xup, xdown, outFrame->w);
            inBuff += inFrame->lineStride;
            err -= yup;
            // add input to this output
            add_line(work[0], work[1], outFrame->w, yup);
        }
        scale = err;
        h_sub(inBuff, work[1],
              inFrame->pixelStride, xup, xdown, outFrame->w);
        inBuff += inFrame->lineStride;
        err -= yup;
        // divide last line of input between this and next output line
        split_line(work[0], work[1], outBuff,
                   outFrame->pixelStride, outFrame->w,
                   scale, yup - scale, 256 * ydown);
        outBuff += outFrame->lineStride;
        err += ydown;
    }
}

static void scale_comp(component* inFrame, component* outFrame,
                       const int hfil, const int vfil,
                       int xup, int xdown, int yup, int ydown,
                       int yoff, uint32_t** work)
{
    if (vfil <= 0 || yup == ydown)
        v_sub_alias(inFrame, outFrame, hfil, xup, xdown,
                    yup, ydown, yoff, work);
    else if (yup > ydown)
        v_sub_interp(inFrame, outFrame, hfil, xup, xdown,
                     yup, ydown, yoff, work);
    else
        v_sub_ave(inFrame, outFrame, hfil, xup, xdown,
                  yup, ydown, yoff, work);
}

int resize_pic(YUV_frame* in_frame, YUV_frame* out_frame,
               int x, int y, int xup, int xdown, int yup, int ydown,
               int intlc, int hfil, int vfil, void* workSpace)
{
    int     ssx_in, ssx_out;
    int     ssy_in, ssy_out;
    YUV_frame   sub_frame;
    uint32_t*   work[3];

    // make up and down numbers even, for later convenience
    xup = xup * 2;
    xdown = xdown * 2;
    yup = yup * 2;
    ydown = ydown * 2;
    // get UV subsampling ratios
    ssx_in = in_frame->Y.w / in_frame->U.w;
    ssy_in = in_frame->Y.h / in_frame->U.h;
    ssx_out = out_frame->Y.w / out_frame->U.w;
    ssy_out = out_frame->Y.h / out_frame->U.h;
    // adjust position, if required
    x = max(x, 0);
    y = max(y, 0);
    x -= x % ssx_out;
    y -= y % ssy_out;
    if (intlc)
        y -= y % 2;
    // create a sub frame representing the portion of out_frame to be written
    sub_frame = *out_frame;
    sub_frame.Y.buff += (y * sub_frame.Y.lineStride) +
                        (x * sub_frame.Y.pixelStride);
    sub_frame.U.buff += ((y/ssy_out) * sub_frame.U.lineStride) +
                        ((x/ssx_out) * sub_frame.U.pixelStride);
    sub_frame.V.buff += ((y/ssy_out) * sub_frame.V.lineStride) +
                        ((x/ssx_out) * sub_frame.V.pixelStride);
    sub_frame.Y.w = min(in_frame->Y.w * xup / xdown, out_frame->Y.w - x);
    sub_frame.Y.h = min(in_frame->Y.h * yup / ydown, out_frame->Y.h - y);
    sub_frame.U.w = min(in_frame->U.w * ssx_in * xup / (xdown * ssx_out),
                        out_frame->U.w - (x/ssx_out));
    sub_frame.U.h = min(in_frame->U.h * ssy_in * yup / (ydown * ssy_out),
                        out_frame->U.h - (y/ssy_out));
    sub_frame.V.w = min(in_frame->V.w * ssx_in * xup / (xdown * ssx_out),
                        out_frame->V.w - (x/ssx_out));
    sub_frame.V.h = min(in_frame->V.h * ssy_in * yup / (ydown * ssy_out),
                        out_frame->V.h - (y/ssy_out));
    work[0] = workSpace;
    work[1] = work[0] + sub_frame.Y.w;
    work[2] = work[1] + sub_frame.Y.w;
    if (intlc)
    {
        component   in_field;
        component   out_field;
        int     f;
        for (f = 0; f < 2; f++)
        {
            extract_field(&in_frame->Y, &in_field, f);
            extract_field(&sub_frame.Y, &out_field, f);
            scale_comp(&in_field, &out_field, hfil, vfil,
                       xup, xdown, yup, ydown, f * 50, work);
            extract_field(&in_frame->U, &in_field, f);
            extract_field(&sub_frame.U, &out_field, f);
            scale_comp(&in_field, &out_field, hfil, vfil,
                       ssx_in * xup, xdown * ssx_out,
                       ssy_in * yup, ydown * ssy_out, f * 50, work);
            extract_field(&in_frame->V, &in_field, f);
            extract_field(&sub_frame.V, &out_field, f);
            scale_comp(&in_field, &out_field, hfil, vfil,
                       ssx_in * xup, xdown * ssx_out,
                       ssy_in * yup, ydown * ssy_out, f * 50, work);
        }
    }
    else // not interlaced
    {
        scale_comp(&in_frame->Y, &sub_frame.Y, hfil, vfil,
                   xup, xdown, yup, ydown, 0, work);
        scale_comp(&in_frame->U, &sub_frame.U, hfil, vfil,
                   ssx_in * xup, xdown * ssx_out,
                   ssy_in * yup, ydown * ssy_out, 0, work);
        scale_comp(&in_frame->V, &sub_frame.V, hfil, vfil,
                   ssx_in * xup, xdown * ssx_out,
                   ssy_in * yup, ydown * ssy_out, 0, work);
    }
    return YUV_OK;
}

int scale_pic(YUV_frame* in_frame, YUV_frame* out_frame,
              int x, int y, int w, int h,
              int intlc, int hfil, int vfil, void* workSpace)
{
    return resize_pic(in_frame, out_frame, x, y,
                      w, in_frame->Y.w, h, in_frame->Y.h,
                      intlc, hfil, vfil, workSpace);
}
