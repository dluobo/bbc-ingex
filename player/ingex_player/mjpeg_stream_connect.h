/*
 * $Id: mjpeg_stream_connect.h,v 1.2 2008/10/29 17:47:42 john_f Exp $
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

#ifndef __MJPEG_STREAM_CONNECT_H__
#define __MJPEG_STREAM_CONNECT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "stream_connect.h"


/* connector that decodes DV */

int mjpeg_connect_accept(MediaSink* sink, const StreamInfo* streamInfo);
int create_mjpeg_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, 
    StreamConnect** connect);


/* use MJPEG decoders as a static and reusable resource */

int init_mjpeg_decoder_resources();
void free_mjpeg_decoder_resources();


#ifdef __cplusplus
}
#endif


#endif


