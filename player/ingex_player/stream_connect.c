/*
 * $Id: stream_connect.c,v 1.2 2008/10/29 17:47:42 john_f Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "stream_connect.h"
#include "logging.h"
#include "macros.h"



MediaSourceListener* stc_get_source_listener(StreamConnect* connect)
{
    if (connect && connect->get_source_listener)
    {
        return connect->get_source_listener(connect->data);
    }
    return NULL;
}

int stc_sync(StreamConnect* connect)
{
    if (connect && connect->sync)
    {
        return connect->sync(connect->data);
    }
    /* if the stream connect doesn't implement sync then we assume it is sync'ed */
    return 1;
}

void stc_close(StreamConnect* connect)
{
    if (connect && connect->close)
    {
        connect->close(connect->data);
    }
}


typedef struct
{
    int sourceStreamId;
    int sinkStreamId;
    StreamConnect streamConnect;
    MediaSourceListener sourceListener;
    MediaSink* sink;
} CopyStreamConnect;


static MediaSourceListener* cyc_get_source_listener(void* data)
{
    CopyStreamConnect* connect = (CopyStreamConnect*)data;
    
    return &connect->sourceListener;
}

static void cyc_close(void* data)
{
    CopyStreamConnect* connect = (CopyStreamConnect*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    SAFE_FREE(&connect);
}


static int cyc_accept_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    CopyStreamConnect* connect = (CopyStreamConnect*)data;
    
    return connect->sourceStreamId == streamId && msk_accept_stream_frame(connect->sink, streamId, frameInfo);
}
    
static int cyc_allocate_buffer(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    CopyStreamConnect* connect = (CopyStreamConnect*)data;
    
    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Buffer allocation request for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    return msk_get_stream_buffer(connect->sink, connect->sinkStreamId, bufferSize, buffer);
}

static void cyc_deallocate_buffer(void* data, int streamId, unsigned char** buffer)
{
    /* do nothing because buffer is reused */
}

static int cyc_receive_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    CopyStreamConnect* connect = (CopyStreamConnect*)data;

    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Received frame for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    if (!msk_receive_stream_frame(connect->sink, connect->sinkStreamId, buffer, bufferSize))
    {
        ml_log_error("failed to write frame to media sink\n");
        return 0;
    }
    
    return 1;
    
}

static int cyc_receive_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    CopyStreamConnect* connect = (CopyStreamConnect*)data;

    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Received frame for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    if (!msk_receive_stream_frame_const(connect->sink, connect->sinkStreamId, buffer, bufferSize))
    {
        ml_log_error("failed to write frame to media sink\n");
        return 0;
    }
    
    return 1;
    
}


int pass_through_accept(MediaSink* sink, const StreamInfo* streamInfo)
{
    /* accept streams where frames can be passed directly to sink */
    return msk_accept_stream(sink, streamInfo);
}

int create_pass_through_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, StreamConnect** connect)
{
    CopyStreamConnect* newConnect;
    
    if (!msk_register_stream(sink, sinkStreamId, streamInfo))
    {
        /* could have failed if max streams exceeded for example */
        return 0;
    }
    
    CALLOC_ORET(newConnect, CopyStreamConnect, 1);
    
    newConnect->sink = sink;
    newConnect->sourceStreamId = sourceStreamId;
    newConnect->sinkStreamId = sinkStreamId;
    
    newConnect->streamConnect.data = newConnect;
    newConnect->streamConnect.get_source_listener = cyc_get_source_listener;
    newConnect->streamConnect.close = cyc_close;

    newConnect->sourceListener.data = newConnect;
    newConnect->sourceListener.accept_frame = cyc_accept_frame;
    newConnect->sourceListener.allocate_buffer = cyc_allocate_buffer;
    newConnect->sourceListener.deallocate_buffer = cyc_deallocate_buffer;
    newConnect->sourceListener.receive_frame = cyc_receive_frame;
    newConnect->sourceListener.receive_frame_const = cyc_receive_frame_const;
    
    
    *connect = &newConnect->streamConnect;
    return 1;
}


