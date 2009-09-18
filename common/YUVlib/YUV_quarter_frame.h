/*
 * $Id: YUV_quarter_frame.h,v 1.2 2009/09/18 15:07:24 philipn Exp $
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

#ifndef __YUVLIB_QUARTER_FRAME__
#define __YUVLIB_QUARTER_FRAME__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

// Make a 2:1 down coonverted copy of in_frame in out_frame at position x,y
void quarter_frame(YUV_frame* in_frame, YUV_frame* out_frame,
                   int x, int y, int intlc, int hfil, int vfil,
                   void* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_QUARTER_FRAME__
