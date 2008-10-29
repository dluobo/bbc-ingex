/*
 * $Id: null_sink.c,v 1.2 2008/10/29 17:47:42 john_f Exp $
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


#include "null_sink.h"
#include "on_screen_display.h"
#include "logging.h"
#include "macros.h"


/* a large number that we don't axpect to exceed */
#define MAX_STREAMS         256


typedef struct
{
    int streamId;
    unsigned char* buffer;
    unsigned int bufferSize; 
    int isPresent;
} NullStream;

typedef struct
{
    /* media sink interface */
    MediaSink mediaSink;
    
    /* listener */
    MediaSinkListener* listener;
    
    /* streams */
    NullStream streams[MAX_STREAMS];
    int numStreams;
    
    /* osd */
    OnScreenDisplay* osd;
    OSDListener osdListener;
    int osdInitialised;
} NullSink;



static void reset_streams(NullSink* sink)
{
    int i;

    for (i = 0; i < sink->numStreams; i++)
    {
        sink->streams[i].isPresent = 0;
    }
}

static NullStream* get_null_stream(NullSink* sink, int streamId)
{
    int i;
    
    for (i = 0; i < sink->numStreams; i++)
    {
        if (streamId == sink->streams[i].streamId)
        {
            return &sink->streams[i];
        }
    }
    return NULL;
}


static int nms_register_listener(void* data, MediaSinkListener* listener)
{
    NullSink* sink = (NullSink*)data;
    
    sink->listener = listener;
    
    return 1;
}

static void nms_unregister_listener(void* data, MediaSinkListener* listener)
{
    NullSink* sink = (NullSink*)data;
    
    if (sink->listener == listener)
    {
        sink->listener = NULL;
    }
}

static int nms_accept_stream(void* data, const StreamInfo* streamInfo)
{
    /* TODO: add option to set what the null sink should accept */
    
    /* make sure that the picture is UYVY */
    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        streamInfo->format != UYVY_FORMAT)
    {
        return 0;
    }
    /* make sure the sound is PCM */
    else if (streamInfo->type == SOUND_STREAM_TYPE &&
        streamInfo->format != PCM_FORMAT)
    {
        return 0;
    }
    return 1;
}

static int nms_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    NullSink* sink = (NullSink*)data;

    /* this should've been checked, but we do it here anyway */
    if (!nms_accept_stream(data, streamInfo))
    {
        ml_log_error("Can't register a stream %d that is not accepted\n", streamId);
        return 0;
    }

    if (sink->numStreams + 1 >= MAX_STREAMS)
    {
        ml_log_warn("Number of streams exceeds hard coded maximum %d\n", MAX_STREAMS);
        return 0;
    }

    if (streamInfo->type == PICTURE_STREAM_TYPE && 
        sink->osd != NULL && !sink->osdInitialised)
    {
        CHK_ORET(osd_initialise(sink->osd, streamInfo, &streamInfo->aspectRatio));
        sink->osdInitialised = 1;
    }
    
    sink->streams[sink->numStreams].streamId = streamId;
    sink->numStreams++;
    
    return 1;
}

static int nms_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    return 1;
}

static int nms_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    NullSink* sink = (NullSink*)data;
    NullStream* nullStream;
    
    if ((nullStream = get_null_stream(sink, streamId)) == NULL)
    {
        ml_log_error("Unknown stream %d for null output\n", streamId);
        return 0;
    }
    
    if (nullStream->bufferSize != bufferSize)
    {
        if ((nullStream->buffer = realloc(nullStream->buffer, bufferSize)) == NULL)
        {
            ml_log_error("Failed to realloc memory for raw stream buffer\n");
            return 0;
        }
        nullStream->bufferSize = bufferSize;
    }
    
    *buffer = nullStream->buffer;
    
    return 1;
}

static int nms_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    NullSink* sink = (NullSink*)data;
    NullStream* nullStream;
    
    if ((nullStream = get_null_stream(sink, streamId)) == NULL)
    {
        ml_log_error("Unknown stream %d for null output\n", streamId);
        return 0;
    }
    if (nullStream->bufferSize != bufferSize)
    {
        ml_log_error("Buffer size (%d) != raw stream buffer size (%d)\n", bufferSize, nullStream->bufferSize);
        return 0;
    }
        
    nullStream->isPresent = 1;

    return 1;
}

static int nms_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    NullSink* sink = (NullSink*)data;
    NullStream* nullStream;
    
    if ((nullStream = get_null_stream(sink, streamId)) == NULL)
    {
        ml_log_error("Unknown stream %d for null output\n", streamId);
        return 0;
    }
    
    /* we don't allocate a buffer like when nms_get_stream_buffer is called */
        
    nullStream->isPresent = 1;

    return 1;
}

static int nms_complete_frame(void* data, const FrameInfo* frameInfo)
{
    NullSink* sink = (NullSink*)data;

    msl_frame_displayed(sink->listener, frameInfo);

    reset_streams(sink);
    
    return 1;    
}

static void nms_cancel_frame(void* data)
{
    NullSink* sink = (NullSink*)data;

    reset_streams(sink);
}

static OnScreenDisplay* nms_get_osd(void* data)
{
    NullSink* sink = (NullSink*)data;

    return sink->osd;
}

static void nms_close(void* data)
{
    NullSink* sink = (NullSink*)data;
    int i;
    
    if (data == NULL)
    {
        return;
    }

    for (i = 0; i < sink->numStreams; i++)
    {
        SAFE_FREE(&sink->streams[i].buffer);
    }
    
    if (sink->osd != NULL)
    {
        osd_free(sink->osd);
    }
    
    SAFE_FREE(&sink);
}

static int nms_reset_or_close(void* data)
{
    NullSink* sink = (NullSink*)data;
    int i;
    
    for (i = 0; i < sink->numStreams; i++)
    {
        SAFE_FREE(&sink->streams[i].buffer);
    }
    sink->numStreams = 0;
    
    return 1;
}


static void nms_refresh_required(void* data)
{
    NullSink* sink = (NullSink*)data;
    
    msl_refresh_required(sink->listener);
}

static void nms_osd_screen_changed(void* data, OSDScreen screen)
{
    NullSink* sink = (NullSink*)data;
    
    msl_osd_screen_changed(sink->listener, screen);
}


int nms_open(MediaSink** sink)
{
    NullSink* newSink = NULL;
    
    CALLOC_ORET(newSink, NullSink, 1);

    
    newSink->mediaSink.data = newSink;
    newSink->mediaSink.register_listener = nms_register_listener;
    newSink->mediaSink.unregister_listener = nms_unregister_listener;
    newSink->mediaSink.accept_stream = nms_accept_stream;
    newSink->mediaSink.register_stream = nms_register_stream;
    newSink->mediaSink.accept_stream_frame = nms_accept_stream_frame;
    newSink->mediaSink.get_stream_buffer = nms_get_stream_buffer;
    newSink->mediaSink.receive_stream_frame = nms_receive_stream_frame;
    newSink->mediaSink.receive_stream_frame_const = nms_receive_stream_frame_const;
    newSink->mediaSink.complete_frame = nms_complete_frame;
    newSink->mediaSink.cancel_frame = nms_cancel_frame;
    newSink->mediaSink.get_osd = nms_get_osd;
    newSink->mediaSink.reset_or_close = nms_reset_or_close;
    newSink->mediaSink.close = nms_close;
    
    CHK_OFAIL(osdd_create(&newSink->osd));
    newSink->osdListener.data = newSink;
    newSink->osdListener.refresh_required = nms_refresh_required;
    newSink->osdListener.osd_screen_changed = nms_osd_screen_changed;
    osd_set_listener(newSink->osd, &newSink->osdListener);
    
    *sink = &newSink->mediaSink;
    return 1;
    
fail:
    nms_close(newSink);
    return 0;
}



