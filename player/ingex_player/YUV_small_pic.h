/*
 * $Id: YUV_small_pic.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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

#ifndef __YUVLIB_SMALL_PIC__
#define __YUVLIB_SMALL_PIC__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Make a reduced size copy of in_frame in out_frame at position x,y.
 * The reduction ratio is set by hsub and vsub, which should be 2 or 3.
 * The trade off between quality and speed is set by hfil & vfil. Fastest
 * mode is simple subsampling, selected with value 0. Simple area averaging
 * filtering is selected with value 1.
 * If the video is interlaced, setting intlc to 1 will have each field
 * processed separately.
 * 2:1 vertical reduction with interlace has a filter mode 2 which gets
 * the two fields correctly positioned.
 * workSpace is allocated by the caller, and must be large enough to store
 * three lines of one component, i.e. in_frame->Y.w * 3 bytes.
 * Return value is 0 for success, <0 for failure (e.g. invalid hsub or vsub)
 */
int small_pic(YUV_frame* in_frame, YUV_frame* out_frame,
              int x, int y, int hsub, int vsub,
              int intlc, int hfil, int vfil, void* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_SMALL_PIC__
