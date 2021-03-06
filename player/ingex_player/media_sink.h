/*
 * $Id: media_sink.h,v 1.6 2009/01/29 07:10:26 stuart_hc Exp $
 *
 *
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

#ifndef __MEDIA_SINK_H__
#define __MEDIA_SINK_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "media_sink_frame.h"
#include "on_screen_display.h"


typedef struct MediaSink MediaSink;
typedef struct VideoSwitchSink VideoSwitchSink;
typedef struct AudioSwitchSink AudioSwitchSink;
typedef struct HalfSplitSink HalfSplitSink;
typedef struct FrameSequenceSink FrameSequenceSink;

typedef int (*sink_read_frame_func)(void* data, struct MediaSink* sink);

typedef struct
{
    void* data; /* passed to functions */

    /* a new frame has been displayed */
    void (*frame_displayed)(void* data, const FrameInfo* frameInfo);

    /* a frame has been dropped */
    void (*frame_dropped)(void* data, const FrameInfo* frameInfo);

    /* the sink must be refreshed */
    void (*refresh_required)(void* data);

    /* the OSD screen has changed */
    void (*osd_screen_changed)(void* data, OSDScreen screen);
} MediaSinkListener;

struct MediaSink
{
    void* data; /* passed as parameter to functions */


    /* register a listener. Only 1 listener supported (TODO: support > 1) */
    int (*register_listener)(void* data, MediaSinkListener* listener);
    void (*unregister_listener)(void* data, MediaSinkListener* listener);

    /* return 1 if the stream type is accepted by the media sink */
    int (*accept_stream)(void* data, const StreamInfo* streamInfo);

    /* note: register stream will still fail even if accept_stream returned 1,
       eg. there is a limit on the number of streams */
    int (*register_stream)(void* data, int streamId, const StreamInfo* streamInfo);


    /* return 1 if the stream data is accepted by the media sink */
    int (*accept_stream_frame)(void* data, int streamId, const FrameInfo* frameInfo);

    /* return buffer for writing stream data to */
    /* note: this function can be called multiple times prior to receive_frame... */
    /* this function is not called when receive_stream_frame_const() is called below */
    int (*get_stream_buffer)(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer);
    /* signal that stream data is ready */
    int (*receive_stream_frame)(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize);
    /* signal that const stream data is ready. note: buffer could be != get_stream_buffer result */
    int (*receive_stream_frame_const)(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize);

    /* signal that frame is complete */
    int (*complete_frame)(void* data, const FrameInfo* frameInfo);
    /* signal that frame is cancelled */
    void (*cancel_frame)(void* data);

    /* try reset and return 1, otherwise close and return 2, otherwise return 0 */
    int (*reset_or_close)(void* data);

    /* for sinks that are buffered */
    int (*allocate_frame)(void* data, MediaSinkFrame** frame);
    int (*complete_sink_frame)(void* data, const MediaSinkFrame* frame);

    /* buffer sink */
    int (*get_buffer_state)(void* data, int* numBuffers, int* numBuffersFilled);

    void (*close)(void* data);


    /* on screen display */
    OnScreenDisplay* (*get_osd)(void* data);


    /* video switch */
    VideoSwitchSink* (*get_video_switch)(void* data);


    /* audio switch */
    AudioSwitchSink* (*get_audio_switch)(void* data);


    /* half-split */
    HalfSplitSink* (*get_half_split)(void* data);


    /* frame sequence */
    FrameSequenceSink* (*get_frame_sequence)(void* data);


    /* audio output sinks */
    int (*mute_audio)(void* data, int mute);
};



/* utility functions for calling MediaSinkListener functions */

void msl_frame_displayed(MediaSinkListener* listener, const FrameInfo* frameInfo);
void msl_frame_dropped(MediaSinkListener* listener, const FrameInfo* lastFrameInfo);
void msl_refresh_required(MediaSinkListener* listener);
void msl_osd_screen_changed(MediaSinkListener* listener, OSDScreen screen);


/* utility functions for calling MediaSink functions */

int msk_register_listener(MediaSink* sink, MediaSinkListener* listener);
void msk_unregister_listener(MediaSink* sink, MediaSinkListener* listener);

int msk_accept_stream(MediaSink* sink, const StreamInfo* streamInfo);
int msk_register_stream(MediaSink* sink, int streamId, const StreamInfo* streamInfo);

int msk_accept_stream_frame(MediaSink* sink, int streamId, const FrameInfo* frameInfo);
int msk_get_stream_buffer(MediaSink* sink, int streamId, unsigned int bufferSize, unsigned char** buffer);
int msk_receive_stream_frame(MediaSink* sink, int streamId, unsigned char* buffer, unsigned int bufferSize);
/* msk_receive_stream_frame_const will call msk_get_stream_buffer and msk_receive_stream_frame
if receive_stream_frame_const is not supported */
int msk_receive_stream_frame_const(MediaSink* sink, int streamId, const unsigned char* buffer, unsigned int bufferSize);
int msk_complete_frame(MediaSink* sink, const FrameInfo* frameInfo);
void msk_cancel_frame(MediaSink* sink);

void msk_start(MediaSink* sink, sink_read_frame_func readFrame, void* readData);
void msk_stop(MediaSink* sink);
int msk_allocate_frame(MediaSink* sink, MediaSinkFrame** frame);
void msk_set_osd_state(MediaSink* sink, OnScreenDisplayState* state);
int msk_complete_sink_frame(MediaSink* sink, const MediaSinkFrame* frame);

int msk_get_buffer_state(MediaSink* sink, int* numBuffers, int* numBuffersFilled);

int msk_reset_or_close(MediaSink* sink);
void msk_close(MediaSink* sink);

OnScreenDisplay* msk_get_osd(MediaSink* sink);

VideoSwitchSink* msk_get_video_switch(MediaSink* sink);

AudioSwitchSink* msk_get_audio_switch(MediaSink* sink);

HalfSplitSink* msk_get_half_split(MediaSink* sink);

FrameSequenceSink* msk_get_frame_sequence(MediaSink* sink);

int msk_mute_audio(MediaSink* sink, int mute);


#ifdef __cplusplus
}
#endif


#endif


