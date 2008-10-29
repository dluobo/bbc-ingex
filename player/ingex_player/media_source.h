/*
 * $Id: media_source.h,v 1.5 2008/10/29 17:47:42 john_f Exp $
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

#ifndef __MEDIA_SOURCE_H__
#define __MEDIA_SOURCE_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "frame_info.h"
#include "media_control.h"


typedef struct
{
    void* data; /* passed to functions */
    
    /* return 1 if accept data */
    int (*accept_frame)(void* data, int streamId, const FrameInfo* frameInfo);
    
    /* allocate buffer must return 0 if the data is not required */
    /* note: this function can be called multiple times prior to receive_frame... */
    int (*allocate_buffer)(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize);
    
    /* deallocate is called when an error occurs after allocate_buffer and before receive_frame */
    void (*deallocate_buffer)(void* data, int streamId, unsigned char** buffer);
    
    /* receive the stream data */
    int (*receive_frame)(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize);

    /* receive the const stream data. note: buffer could be != allocate_buffer result  */
    int (*receive_frame_const)(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize);
} MediaSourceListener;

typedef struct MediaSource
{
    void* data; /* passed to functions */

    /* returns 1 if the source is complete and ready for post_complete */
    int (*is_complete)(void* data);
    
    /* return 1 if post_complete successful */
    int (*post_complete)(void* data, struct MediaSource* rootSource, MediaControl* mediaControl);
    
    /* complete the blank video source stream info */
    int (*finalise_blank_source)(void* data, const StreamInfo* streamInfo);
    
    /* returns number of streams coming from source */
    int (*get_num_streams)(void* data);
    /* returns in streamInfo the information about the stream */
    int (*get_stream_info)(void* data, int streamIndex, const StreamInfo** streamInfo);
    
    /* disabling a stream means data is not sent to the MediaSourceListener in read_frame */
    int (*disable_stream)(void* data, int streamIndex);
    void (*disable_audio)(void* data);
    int (*stream_is_disabled)(void* data, int streamIndex);
    
    /* read a frame and send stream data to listener */
    /* 0 means success, -1 if failed, -2 if timed out */ 
    int (*read_frame)(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener);
    
    /* general source control functions */
    int (*is_seekable)(void* data);
    /* 0 means success, -1 if failed, -2 if timed out */ 
    int (*seek)(void* data, int64_t position);
    /* 0 means success, -1 if failed, -2 if timed out */ 
    int (*seek_timecode)(void* data, const Timecode* timecode, TimecodeType type, TimecodeSubType subType);
    int (*get_length)(void* data, int64_t* length);
    int (*get_position)(void* data, int64_t* position);
    int (*get_available_length)(void* data, int64_t* length);
    int (*eof)(void* data);
    void (*close)(void* data);
    
    /* buffered source */
    int (*get_buffer_state)(void* data, int* numBuffers, int* numBuffersFilled);
    
    /* convert a position from a child source */
    int64_t (*convert_position)(void* data, int64_t position, struct MediaSource* childSource);
    
    /* set the source name */
    void (*set_source_name)(void* data, const char* name);
    
    /* set the clip identifier */
    void (*set_clip_id)(void* data, const char* id);
    
} MediaSource;


/* utility functions for calling MediaSourceListener functions */

int sdl_accept_frame(MediaSourceListener* listener, int streamId, const FrameInfo* frameInfo);
int sdl_allocate_buffer(MediaSourceListener* listener, int streamId, unsigned char** buffer, unsigned int bufferSize);
void sdl_deallocate_buffer(MediaSourceListener* listener, int streamId, unsigned char** buffer);
int sdl_receive_frame(MediaSourceListener* listener, int streamId, unsigned char* buffer, unsigned int bufferSize);
/* sdl_receive_frame_const will call sdl_allocate_buffer and sdl_receive_frame if sdl_receive_frame_const 
is not supported */
int sdl_receive_frame_const(MediaSourceListener* listener, int streamId, const unsigned char* buffer, unsigned int bufferSize);


/* utility functions for calling MediaSource functions */

int msc_is_complete(MediaSource* source);
int msc_post_complete(MediaSource* source, MediaSource* rootSource, MediaControl* mediaControl);
int msc_finalise_blank_source(MediaSource* source, const StreamInfo* streamInfo);
int msc_get_num_streams(MediaSource* source);
int msc_get_stream_info(MediaSource* source, int streamIndex, const StreamInfo** streamInfo);
int msc_disable_stream(MediaSource* source, int streamIndex);
void msc_disable_audio(MediaSource* source);
int msc_stream_is_disabled(MediaSource* source, int streamIndex);
int msc_read_frame(MediaSource* source, const FrameInfo* frameInfo, MediaSourceListener* listener);
int msc_is_seekable(MediaSource* source);
int msc_seek(MediaSource* source, int64_t position);
int msc_seek_timecode(MediaSource* source, const Timecode* timecode, TimecodeType type, TimecodeSubType subType);
int msc_get_length(MediaSource* source, int64_t* length);
int msc_get_position(MediaSource* source, int64_t* position);
int msc_get_available_length(MediaSource* source, int64_t* length);
int msc_eof(MediaSource* source);
void msc_close(MediaSource* source);
int msc_get_buffer_state(MediaSource* source, int* numBuffers, int* numBuffersFilled);
int64_t msc_convert_position(MediaSource* source, int64_t position, MediaSource* childSource);
void msc_set_source_name(MediaSource* source, const char* name);
void msc_set_clip_id(MediaSource* source, const char* id);


/* create a new unique id for the source */
int msc_create_id();



/* maps stream ids to source ids */
typedef struct
{
    int streamId;
    int sourceId;
} MediaSourceStream;

typedef struct 
{
    MediaSourceStream streams[64];
    int numStreams;
} MediaSourceStreamMap;


void msc_init_stream_map(MediaSourceStreamMap* map);
int msc_add_stream_to_map(MediaSourceStreamMap* map, int streamId, int sourceId);
int msc_get_source_id(MediaSourceStreamMap* map, int streamId, int* sourceId);


#ifdef __cplusplus
}
#endif


#endif



