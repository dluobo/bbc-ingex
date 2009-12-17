/*
 * $Id: YUV_to_rgb.h,v 1.1 2009/12/17 15:43:29 john_f Exp $
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

#ifndef __YUVLIB_TO_RGB__
#define __YUVLIB_TO_RGB__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Convert YUV to RGB.
 * Input frame can be any 4:4:4 or 4:2:2 sampled YUVlib format.
 * The output frame is assumed to comprise three BYTE arrays, but these
 * will typically be interleaved. The RGBpixelStride value is the address
 * increment used to get from one R, G or B value to the next R, G or B
 * value. This is most likely to be 3, but may sometimes be 4.
 * The RGBlineStride value is the address increment to get from one R, G
 * or B value to the R, G or B value immediately below. It is probably the
 * image width multiplied by RGBpixelStride, but might be rounded up to get
 * a particular alignment in some systems.
 * The minimum size of workSpace is in_frame->Y.w * 2.
 */
int to_RGB(YUV_frame* in_frame, BYTE* out_R, BYTE* out_G, BYTE* out_B,
           const int RGBpixelStride, const int RGBlineStride,
           void* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_TO_RGB__
