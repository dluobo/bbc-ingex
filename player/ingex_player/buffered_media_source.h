/*
 * $Id: buffered_media_source.h,v 1.2 2008/10/29 17:47:41 john_f Exp $
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

#ifndef __BUFFERED_MEDIA_SOURCE_H__
#define __BUFFERED_MEDIA_SOURCE_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"
#include "media_player.h"


typedef struct BufferedMediaSource BufferedMediaSource;


int bmsrc_create(MediaSource* targetSource, int size, int blocking, float byteRateLimit,
    BufferedMediaSource** bufSource);
MediaSource* bmsrc_get_source(BufferedMediaSource* bufSource);
void bmsrc_set_media_player(BufferedMediaSource* bufSource, MediaPlayer* player);



#ifdef __cplusplus
}
#endif


#endif


