/*
 * $Id: media_sink.c,v 1.4 2008/10/29 17:47:42 john_f Exp $
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


#include "media_sink.h"


void msl_frame_displayed(MediaSinkListener* listener, const FrameInfo* frameInfo)
{
    if (listener && listener->frame_displayed)
    {
        listener->frame_displayed(listener->data, frameInfo);
    }
}

void msl_frame_dropped(MediaSinkListener* listener, const FrameInfo* lastFrameInfo)
{
    if (listener && listener->frame_dropped)
    {
        listener->frame_dropped(listener->data, lastFrameInfo);
    }
}

void msl_refresh_required(MediaSinkListener* listener)
{
    if (listener && listener->refresh_required)
    {
        listener->refresh_required(listener->data);
    }
}

void msl_osd_screen_changed(MediaSinkListener* listener, OSDScreen screen)
{
    if (listener && listener->osd_screen_changed)
    {
        listener->osd_screen_changed(listener->data, screen);
    }
}


int msk_register_listener(MediaSink* sink, MediaSinkListener* listener)
{
    if (sink && sink->register_listener)
    {
        return sink->register_listener(sink->data, listener);
    }
    return 0;
}

void msk_unregister_listener(MediaSink* sink, MediaSinkListener* listener)
{
    if (sink && sink->unregister_listener)
    {
        sink->unregister_listener(sink->data, listener);
    }
}

int msk_accept_stream(MediaSink* sink, const StreamInfo* streamInfo)
{
    if (sink && sink->accept_stream)
    {
        return sink->accept_stream(sink->data, streamInfo);
    }
    return 0;
}

int msk_register_stream(MediaSink* sink, int streamId, const StreamInfo* streamInfo)
{
    if (sink && sink->register_stream)
    {
        return sink->register_stream(sink->data, streamId, streamInfo);
    }
    return 0;
}

int msk_accept_stream_frame(MediaSink* sink, int streamId, const FrameInfo* frameInfo)
{
    if (sink && sink->accept_stream_frame)
    {
        return sink->accept_stream_frame(sink->data, streamId, frameInfo);
    }
    return 0;
}

int msk_get_stream_buffer(MediaSink* sink, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    if (sink && sink->get_stream_buffer)
    {
        return sink->get_stream_buffer(sink->data, streamId, bufferSize, buffer);
    }
    return 0;
}

int msk_receive_stream_frame(MediaSink* sink, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    if (sink && sink->receive_stream_frame)
    {
        return sink->receive_stream_frame(sink->data, streamId, buffer, bufferSize);
    }
    return 0;
}

int msk_receive_stream_frame_const(MediaSink* sink, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    unsigned char* nonconstBuffer;
    int result;
    
    if (sink && sink->receive_stream_frame_const)
    {
        return sink->receive_stream_frame_const(sink->data, streamId, buffer, bufferSize);
    }
    else 
    {
        /* sink doesn't accept const buffer - take the non-const option */
        result = msk_get_stream_buffer(sink, streamId, bufferSize, &nonconstBuffer);
        if (result)
        {
            memcpy(nonconstBuffer, buffer, bufferSize);
            result = msk_receive_stream_frame(sink, streamId, nonconstBuffer, bufferSize);
        }
        return result;
    }
}

int msk_complete_frame(MediaSink* sink, const FrameInfo* frameInfo)
{
    if (sink && sink->complete_frame)
    {
        return sink->complete_frame(sink->data, frameInfo);
    }
    return 0;
}

void msk_cancel_frame(MediaSink* sink)
{
    if (sink && sink->cancel_frame)
    {
        sink->cancel_frame(sink->data);
    }
}


int msk_reset_or_close(MediaSink* sink)
{
    if (sink && sink->reset_or_close)
    {
        return sink->reset_or_close(sink->data);
    }
    return 0;
}

void msk_close(MediaSink* sink)
{
    if (sink && sink->close)
    {
        sink->close(sink->data);
    }
}

OnScreenDisplay* msk_get_osd(MediaSink* sink)
{
    if (sink && sink->get_osd)
    {
        return sink->get_osd(sink->data);
    }
    return NULL;
}
    
VideoSwitchSink* msk_get_video_switch(MediaSink* sink)
{
    if (sink && sink->get_video_switch)
    {
        return sink->get_video_switch(sink->data);
    }
    return NULL;
}
    
AudioSwitchSink* msk_get_audio_switch(MediaSink* sink)
{
    if (sink && sink->get_audio_switch)
    {
        return sink->get_audio_switch(sink->data);
    }
    return NULL;
}
    
HalfSplitSink* msk_get_half_split(MediaSink* sink)
{
    if (sink && sink->get_half_split)
    {
        return sink->get_half_split(sink->data);
    }
    return NULL;
}
    
FrameSequenceSink* msk_get_frame_sequence(MediaSink* sink)
{
    if (sink && sink->get_frame_sequence)
    {
        return sink->get_frame_sequence(sink->data);
    }
    return NULL;
}
    
int msk_allocate_frame(MediaSink* sink, MediaSinkFrame** frame)
{
    if (sink && sink->allocate_frame)
    {
        return sink->allocate_frame(sink->data, frame);
    }
    return 0;
}

int msk_complete_sink_frame(MediaSink* sink, const MediaSinkFrame* frame)
{
    if (sink && sink->complete_sink_frame)
    {
        return sink->complete_sink_frame(sink->data, frame);
    }
    return 0;
}

int msk_get_buffer_state(MediaSink* sink, int* numBuffers, int* numBuffersFilled)
{
    if (sink && sink->get_buffer_state)
    {
        return sink->get_buffer_state(sink->data, numBuffers, numBuffersFilled);
    }
    return 0;
}


