/*
 * $Id: YUV_ffmpeg.c,v 1.3 2009/09/18 15:07:24 philipn Exp $
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

#ifdef HAVE_FFMPEG

#include "YUV_frame.h"
#include "YUV_ffmpeg.h"

int YUV_frame_from_AVFrame(YUV_frame* dest,
                           AVCodecContext *avctx, AVFrame* source)
{
    int		ssx, ssy;

    switch (avctx->pix_fmt)
    {
    case PIX_FMT_YUV420P:
        ssx = 2;
        ssy = 2;
        break;
    case PIX_FMT_YUYV422: case PIX_FMT_YUV422P:
        ssx = 2;
        ssy = 1;
        break;
    case PIX_FMT_YUV444P:
        ssx = 1;
        ssy = 1;
        break;
    default:
        return YUV_unknown_format;
    }
    if ((avctx->width < 1) || (avctx->height < 1))
        return YUV_size_error;
    if ((avctx->width % ssx != 0) || (avctx->height % ssy != 0))
        return YUV_size_error;
    dest->Y.w = avctx->width;
    dest->Y.h = avctx->height;
    dest->Y.lineStride = source->linesize[0];
    dest->Y.buff = source->data[0];
    dest->U.w = avctx->width / ssx;
    dest->U.h = avctx->height / ssy;
    dest->U.lineStride = source->linesize[1];
    dest->U.buff = source->data[1];
    switch (avctx->pix_fmt)
    {
    case PIX_FMT_YUV420P: case PIX_FMT_YUV422P: case PIX_FMT_YUV444P:
        dest->Y.pixelStride = 1;
        dest->U.pixelStride = 1;
        break;
    case PIX_FMT_YUYV422:
        dest->Y.pixelStride = 2;
        dest->U.pixelStride = 4;
        break;
    default:
        return YUV_unknown_format;
    }
    dest->V = dest->U;
    dest->V.lineStride = source->linesize[2];
    dest->V.buff = source->data[2];
    return YUV_OK;
}

#endif /* defined HAVE_FFMPEG */
