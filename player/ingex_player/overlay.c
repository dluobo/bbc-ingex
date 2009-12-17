/*
 * $Id: overlay.c,v 1.1 2009/12/17 15:57:40 john_f Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Based on YUV_lib code: Jim Easterbrook
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "overlay.h"
#include "logging.h"
#include "macros.h"



static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}


static int alloc_overlay_workspace(OverlayWorkspace *workspace, int width, int height)
{
    int required_size = width * height * 2;
    
    if (workspace->image && workspace->image_size < required_size) {
        free(workspace->image);
        workspace->image = NULL;
    }
    
    if (!workspace->image) {
        workspace->image = (unsigned short*)malloc(required_size * sizeof(unsigned short));
        workspace->image_size = required_size;
    }
    
    return workspace->image != NULL;
}

static void filterUV(overlay* ovly, const int ssx, const int ssy)
{
    BYTE*   srcLineA;
    BYTE*   srcLineB;
    BYTE*   dstLine;
    int     i, j;

    // make UV image by filtering Y image
    ovly->Cbuff = ovly->buff + (ovly->w * ovly->h);
    // horizontal
    if (ssx == 1)
    {
        // copy data
        memcpy(ovly->Cbuff, ovly->buff, ovly->w * ovly->h);
    }
    else if (ssx == 2)
    {
        int A, B, C;

        // 1/4, 1/2, 1/4 filter
        srcLineA = ovly->buff;
        dstLine = ovly->Cbuff;
        for (j = 0; j < ovly->h; j++)
        {
            A = 0;
            B = *srcLineA++;
            for (i = 0; i < ovly->w - 1; i++)
            {
                C = *srcLineA++;
                *dstLine++ = (A + (B * 2) + C) / 4;
                A = B;
                B = C;
            }
            *dstLine++ = (A + (B * 2)) / 4;
        }
    }
    // vertical
    if (ssy == 2)
    {
        // 1/2, 1/2 filter
        srcLineA = ovly->Cbuff;
        srcLineB = ovly->Cbuff + ovly->w;
        dstLine = ovly->Cbuff;
        for (j = 0; j < ovly->h - 1; j++)
        {
            for (i = 0; i < ovly->w; i++)
                *dstLine++ = (*srcLineA++ + *srcLineB++) / 2;
        }
        for (i = 0; i < ovly->w; i++)
            *dstLine++ = *srcLineA++ / 2;
    }
    ovly->ssx = ssx;
    ovly->ssy = ssy;
}

// Routine to unpack 16 bytes of 10-bit input to 12 unsigned shorts
static void unpack12(uint8_t* pIn, unsigned short* pOut)
{
    int     i;

    for (i = 0; i < 4; i++)
    {
        *pOut = *pIn++;
        *pOut++ += (*pIn & 0x03) << 8;
        *pOut = *pIn++ >> 2;
        *pOut++ += (*pIn & 0x0f) << 6;
        *pOut = *pIn++ >> 4;
        *pOut++ += (*pIn++ & 0x3f) << 4;
    }
}

// Routine to pack 12 unsigned shorts into 16 bytes of 10-bit output
static void pack12(unsigned short* pIn, uint8_t* pOut)
{
    int     i;

    for (i = 0; i < 4; i++)
    {
        *pOut++ = *pIn & 0xff;
        *pOut = (*pIn++ >> 8) & 0x03;
        *pOut++ += (*pIn & 0x3f) << 2;
        *pOut = (*pIn++ >> 6) & 0x0f;
        *pOut++ += (*pIn & 0x0f) << 4;
        *pOut++ = (*pIn++ >> 4) & 0x3f;
    }
}

static void unpack_10bit_image(unsigned short* pOutFrame, uint8_t* pInFrame,
                               const int StrideOut, const int StrideIn,
                               const int xLen, const int yLen)
{
    uint8_t*            pIn;
    unsigned short*     pOut;
    int                 x, y;

    for (y = 0; y < yLen; y++)
    {
        pIn = pInFrame;
        pOut = pOutFrame;
        for (x = 0; x < xLen; x += 6)
        {
            unpack12(pIn, pOut);
            pIn += 16;
            pOut += 12;
        }
        pInFrame += StrideIn;
        pOutFrame += StrideOut;
    }
}

void pack_10bit_image(uint8_t* pOutFrame, unsigned short* pInFrame,
                      const int StrideOut, const int StrideIn,
                      const int xLen, const int yLen)
{
    unsigned short*     pIn;
    uint8_t*            pOut;
    int                 x, y;

    for (y = 0; y < yLen; y++)
    {
        pIn = pInFrame;
        pOut = pOutFrame;
        for (x = 0; x < xLen; x += 6)
        {
            pack12(pIn, pOut);
            pIn += 12;
            pOut += 16;
        }
        pInFrame += StrideIn;
        pOutFrame += StrideOut;
    }
}

static int apply_overlay_10bit(overlay *ovly, unsigned char *image, int width, int height,
                               int x, int y, unsigned char colour_y, unsigned char colour_u, unsigned char colour_v,
                               int box, OverlayWorkspace *workspace)
{
    int i, j;
    int x0, y0, x1, y1;
    int ssx, ssy;
    int box_key;
    int txt_key;
    int bg;
    unsigned short colour_10bit_y = ((unsigned short)colour_y) << 2;
    unsigned short colour_10bit_u = ((unsigned short)colour_u) << 2;
    unsigned short colour_10bit_v = ((unsigned short)colour_v) << 2;
    unsigned short *image_upack_y;
    unsigned short *image_upack_u;
    unsigned short *image_upack_v;
    BYTE* srcLine;
    BYTE* srcPtr;
    unsigned short *dstLine;
    unsigned short *dstLine2;
    unsigned short *dstPtr;
    unsigned short *dstPtr2;
    int target_linestride_y, target_linestride_uv;
    int target_pixelstride_y, target_pixelstride_uv;
    int target_x0, target_y0, target_x1, target_y1;
    int target_width, target_height;
    unsigned char *source_image;
    
    
    // select target image area for applying the overlay
    
    target_x0 = (x / 12) * 12; // round down to 12 pixel pack
    target_y0 = y;
    target_x1 = ((x + ovly->w + 11) / 12) * 12; // round up to 12 pixel pack
    target_y1 = y + ovly->h;
    
    target_x0 = max(target_x0, 0);
    target_y0 = max(target_y0, 0);
    target_x1 = min(target_x1, width);
    target_y1 = min(target_y1, height);
    
    target_width = target_x1 - target_x0;
    target_height = target_y1 - target_y0;
    
    if (target_width <= 0 || target_height <= 0)
        return 1;
    
    target_linestride_y = target_width * 2;
    target_pixelstride_y = 2;
    target_linestride_uv = target_linestride_y;
    target_pixelstride_uv = 4;
    

    // unpack the target image area

    if (!alloc_overlay_workspace(workspace, target_width, target_height))
        return 0;
    
    source_image = image + (target_y0 * ((width + 5) / 6 * 16)) +
                           ((target_x0 + 5) / 6 * 16);
    
    unpack_10bit_image(workspace->image, source_image, target_width * 2, (width + 5) / 6 * 16,
                       target_width, target_height);
    image_upack_u = workspace->image;
    image_upack_y = workspace->image + 1;
    image_upack_v = workspace->image + 2;
    

    // apply overlay to target image area
    
    // set clipped start and end points
    x0 = max(x, 0);
    y0 = max(y, 0);
    x1 = min(x + ovly->w, width);
    y1 = min(y + ovly->h, height);
    // chroma subsampling
    ssx = 2;
    ssy = 1;
    if (ovly->Cbuff == NULL || ovly->ssx != ssx || ovly->ssy != ssy)
        // make UV image by filtering Y image
        filterUV(ovly, ssx, ssy);
    // normalise colour
    box_key = 1024 - (1024 * box / 100);
    // do Y
    srcLine = ovly->buff + ((y0 - y) * ovly->w) + (x0 - x);
    dstLine = image_upack_y + ((y0 - target_y0) * target_linestride_y) +
                              ((x0 - target_x0) * target_pixelstride_y);
    for (j = y0; j < y1; j++)
    {
        srcPtr = srcLine;
        dstPtr = dstLine;
        for (i = x0; i < x1; i++)
        {
            txt_key = (int)(*srcPtr) << 2;
            srcPtr++;
            // dim background level over entire box
            bg = 64 + ((box_key * (*dstPtr - 64)) / 1024);
            // key in foreground level
            *dstPtr = bg + ((txt_key * (colour_10bit_y - bg)) / 1024);
            // next pixel
            dstPtr += target_pixelstride_y;
        }
        srcLine += ovly->w;
        dstLine += target_linestride_y;
    }
    // do UV
    // adjust start position to cosited YUV samples
    i = x0 % ssx;
    if (i != 0)
        x0 += ssx - i;
    j = y0 % ssy;
    if (j != 0)
        y0 += ssy - j;
    srcLine = ovly->Cbuff + ((y0 - y) * ovly->w) + x0 - x;
    dstLine = image_upack_u + (((y0 - target_y0)/ssy) * target_linestride_uv) +
                              (((x0 - target_x0)/ssx) * target_pixelstride_uv);
    dstLine2 = image_upack_v + (((y0 - target_y0)/ssy) * target_linestride_uv) +
                               (((x0 - target_x0)/ssx) * target_pixelstride_uv);
    for (j = y0; j < y1; j += ssy)
    {
        srcPtr = srcLine;
        dstPtr = dstLine;
        dstPtr2 = dstLine2;
        for (i = x0; i < x1; i += ssx)
        {
            txt_key = ((int)*srcPtr) << 2;
            srcPtr += ssx;
            // dim background level over entire box
            bg = 512 + ((box_key * (*dstPtr - 512)) / 1024);
            // key in foreground level
            *dstPtr = bg + ((txt_key * (colour_10bit_u - bg)) / 1024);
            dstPtr += target_pixelstride_uv;
            // and again for V
            bg = 512 + ((box_key * (*dstPtr2 - 512)) / 1024);
            *dstPtr2 = bg + ((txt_key * (colour_10bit_v - bg)) / 1024);
            dstPtr2 += target_pixelstride_uv;
        }
        srcLine += (ssy * ovly->w);
        dstLine += target_linestride_uv;
        dstLine2 += target_linestride_uv;
    }
    

    // pack target area back to source
    
    pack_10bit_image(source_image, workspace->image, (width + 5) / 6 * 16, target_width * 2,
                     target_width, target_height);
    
    return 1;
}



void init_overlay_workspace(OverlayWorkspace *workspace)
{
    memset(workspace, 0, sizeof(*workspace));
}

void clear_overlay_workspace(OverlayWorkspace *workspace)
{
    if (workspace->image)
        free(workspace->image);
    
    init_overlay_workspace(workspace);
}

int apply_overlay(overlay *ovly, unsigned char *image, StreamFormat format, int width, int height,
                  int x, int y, unsigned char colour_y, unsigned char colour_u, unsigned char colour_v, int box,
                  OverlayWorkspace *workspace)
{
    if (format == UYVY_10BIT_FORMAT)
        return apply_overlay_10bit(ovly, image, width, height, x, y, colour_y, colour_u, colour_v, box, workspace);
    
    
    YUV_frame frame;
    
    switch (format)
    {
        case UYVY_FORMAT:
            CHK_ORET(YUV_frame_from_buffer(&frame, image, width, height, UYVY) == 1);
            break;
        case YUV422_FORMAT:
            CHK_ORET(YUV_frame_from_buffer(&frame, image, width, height, YV16) == 1);
            break;
        case YUV420_FORMAT:
            CHK_ORET(YUV_frame_from_buffer(&frame, image, width, height, I420) == 1);
            break;
        default:
            return 0;
    }
    
    CHK_ORET(add_overlay(ovly, &frame, x, y, colour_y, colour_u, colour_v, box) == YUV_OK);
    
    return 1;
}

int apply_timecode_overlay(timecode_data* tc_data, int hr, int mn, int sc, int fr, int is_pal,
                           unsigned char *image, StreamFormat format, int width, int height,
                           int x, int y, unsigned char colour_y, unsigned char colour_u, unsigned char colour_v, int box,
                           OverlayWorkspace *workspace)
{
    int c, n;
    int offset;
    char tc_str[16];

    if (is_pal)
        sprintf(tc_str, "%02d:%02d:%02d:%02d", hr, mn, sc, fr);
    else
        sprintf(tc_str, "%02d;%02d;%02d;%02d", hr, mn, sc, fr);
    
    // legitimise start point
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    offset = 0;
    for (n = 0; n < 11; n++)
    {
        c = tc_str[n] - '0';
        CHK_ORET(apply_overlay(&tc_data->tc_ovly[c], image, format, width, height,
                               x + offset, y, colour_y, colour_u, colour_v, box, workspace));
        offset += tc_data->tc_ovly[c].w;
    }
    
    return 1;
}

