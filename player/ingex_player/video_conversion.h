/*
 * $Id: video_conversion.h,v 1.5 2009/01/29 07:10:27 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Stuart Cunningham
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

#ifndef __VIDEO_CONVERSION_H__
#define __VIDEO_CONVERSION_H__


#ifdef __cplusplus
extern "C"
{
#endif

#if defined(HAVE_FFMPEG)

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#else
#include <libavcodec/avcodec.h>
#endif

#endif

#include "frame_info.h"


void uyvy_to_yuv420(int width, int height, uint8_t *input, uint8_t *output);

#if defined(HAVE_FFMPEG)

void yuv4xx_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv422_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv4xx_to_yuv4xx(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv422_to_yuv422(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

#endif /* defined(HAVE_FFMPEG) */

void yuv444_to_uyvy(int width, int height, uint8_t *input, uint8_t *output);

void yuv422_to_uyvy_2(int width, int height, int shift_picture_up, uint8_t *input, uint8_t *output);

void fill_black(StreamFormat format, int width, int height, unsigned char* image);

#ifdef __cplusplus
}
#endif


#endif

