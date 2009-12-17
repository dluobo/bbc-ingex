/*
 * $Id: overlay.h,v 1.1 2009/12/17 15:57:40 john_f Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#ifndef __OVERLAY_H__
#define __OVERLAY_H__

#include <YUV_frame.h>
#include <YUV_text_overlay.h>

#include "frame_info.h"


typedef struct
{
    unsigned short *image;
    int image_size;
}  OverlayWorkspace;


void init_overlay_workspace(OverlayWorkspace *workspace);
void clear_overlay_workspace(OverlayWorkspace *workspace);


int apply_overlay(overlay *ovly, unsigned char *image, StreamFormat format, int width, int height,
                  int x, int y, unsigned char colour_y, unsigned char colour_u, unsigned char colour_v, int box,
                  OverlayWorkspace *workspace);


int apply_timecode_overlay(timecode_data *tc_data, int hr, int mn, int sc, int fr, int is_pal,
                           unsigned char *image, StreamFormat format, int width, int height,
                           int x, int y, unsigned char colour_y, unsigned char colour_u, unsigned char colour_v, int box,
                           OverlayWorkspace *workspace);

#endif

