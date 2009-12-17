/*
 * $Id: YUV_blend.h,v 1.1 2009/12/17 15:43:29 john_f Exp $
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

#ifndef __YUVLIB_BLEND__
#define __YUVLIB_BLEND__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mix two YUV frames using a constant 'alpha'.
 * Output is (a * in1) + ((1 - a) * in2) i.e. the normal "compositing
 * equation".
 * The "invert" switch allows in2 to be inverted, making a difference signal.
 */
int blend(YUV_frame* in_frame1, YUV_frame* in_frame2, YUV_frame* out_frame,
          const double alpha, const int invert);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_BLEND__
