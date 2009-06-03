/*
 * $Id: video_conversion.h,v 1.4 2009/06/03 09:25:47 john_f Exp $
 *
 * MMX optimised video format conversion functions
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __VIDEO_CONVERSION_H__
#define __VIDEO_CONVERSION_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

// MMX optimised video format conversion functions

void uyvy_to_yuv422(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output);
void yuv422_to_uyvy(int width, int height, int shift_picture_up, uint8_t *input, uint8_t *output);

// UYVY to YUV 4:2:0 conversion
// The _nommx function is provided for testing and to workaround problem with odd sizes in mmx version
void uyvy_to_yuv420(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output);
void uyvy_to_yuv420_nommx(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output);
void uyvy_to_yuv422_nommx(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output);

void yuv444_to_uyvy(int width, int height, uint8_t *input, uint8_t *output);

void yuv422_to_yuv420_DV_sampling(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output);
void uyvy_to_yuv420_DV_sampling(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif
