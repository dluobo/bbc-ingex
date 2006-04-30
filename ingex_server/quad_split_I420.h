/*
 * $Id: quad_split_I420.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Down-sample multiple video inputs to create quad-split view.
 *
 * Copyright (C) 2005  Jim Easterbrook <easter@users.sourceforge.net>
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

#ifndef QUAD_SPLIT_I420
#define QUAD_SPLIT_I420

typedef struct
{
    unsigned char*	cbuff;	// complete image buffer
    int			size;	// bytes allocated
    unsigned char*	Ybuff;	// start of Y plane
    unsigned char*	Ubuff;	// start of U plane
    unsigned char*	Vbuff;	// start of V plane
    int			w;	// image width in Y samples
    int			h;	// image height in picture lines
} I420_frame;

int alloc_frame_I420(I420_frame* frame, const int image_w, const int image_h,
                     unsigned char* buffer);

void clear_frame_I420(I420_frame* frame);

void quarter_frame_I420(I420_frame* in_frame, I420_frame* out_frame,
                        int x, int y, int intlc, int hfil, int vfil);

#endif
