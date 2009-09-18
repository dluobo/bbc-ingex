/*
 * $Id: YUV_ffmpeg.h,v 1.3 2009/09/18 15:07:24 philipn Exp $
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

#ifndef __YUVLIB_FFMPEG__
#define __YUVLIB_FFMPEG__

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avformat.h>
#else
#include <libavformat/avformat.h>
#endif

#include "YUV_frame.h"

/* Set a YUV_frame to point at the contents of an ffmpeg AVFrame.
 */
extern int YUV_frame_from_AVFrame(YUV_frame* dest,
                                  AVCodecContext *avctx, AVFrame* source);

#endif // __YUVLIB_FFMPEG__

