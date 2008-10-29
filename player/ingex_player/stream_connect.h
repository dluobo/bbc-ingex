/*
 * $Id: stream_connect.h,v 1.2 2008/10/29 17:47:42 john_f Exp $
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

#ifndef __STREAM_CONNECT_H__
#define __STREAM_CONNECT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"
#include "media_sink.h"


typedef struct
{
    void* data; /* passed to functions */
    
    MediaSourceListener* (*get_source_listener)(void* data);
    
    /* complete all remaining tasks */
    int (*sync)(void* data);
    
    /* close and free resources */
    void (*close)(void* data);
} StreamConnect;


/* utility functions for calling StreamConnect functions */

MediaSourceListener* stc_get_source_listener(StreamConnect* connect);
int stc_sync(StreamConnect* connect);
void stc_close(StreamConnect* connect);



/* connector that passes data directly from source to sink */

int pass_through_accept(MediaSink* sink, const StreamInfo* streamInfo);
int create_pass_through_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, StreamConnect** connect);

    

#ifdef __cplusplus
}
#endif


#endif


