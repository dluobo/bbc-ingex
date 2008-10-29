/*
 * $Id: half_split_sink.h,v 1.2 2008/10/29 17:47:41 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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

#ifndef __HALF_SPLIT_SINK_H__
#define __HALF_SPLIT_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"

typedef enum
{
    CONTINUOUS_SPLIT_TYPE = 0, /* second image continues from where the first left off */
    SINGLE_PAN_SPLIT_TYPE, /* second image shown starts from the top/left */
    DUAL_PAN_SPLIT_TYPE  /* split stays in middle and images pan */
} HalfSplitType;


int hss_create_half_split(MediaSink* sink, int vertical, HalfSplitType type, int showSplitDivide, HalfSplitSink** split);
MediaSink* hss_get_media_sink(HalfSplitSink* split);


void hss_set_half_split_orientation(HalfSplitSink* split, int vertical);
void hss_set_half_split_type(HalfSplitSink* split, int type);
void hss_show_half_split(HalfSplitSink* split, int showSplitDivide);
void hss_move_half_split(HalfSplitSink* split, int rightOrDown, int speed);


#ifdef __cplusplus
}
#endif


#endif


