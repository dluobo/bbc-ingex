#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "media_source.h"
#include "logging.h"


int sdl_accept_frame(MediaSourceListener* listener, int streamId, const FrameInfo* frameInfo)
{
    if (listener && listener->accept_frame)
    {
        return listener->accept_frame(listener->data, streamId, frameInfo);
    }
    return 0;
}

int sdl_allocate_buffer(MediaSourceListener* listener, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    if (listener && listener->allocate_buffer)
    {
        return listener->allocate_buffer(listener->data, streamId, buffer, bufferSize);
    }
    return 0;
}

void sdl_deallocate_buffer(MediaSourceListener* listener, int streamId, unsigned char** buffer)
{
    if (listener && listener->deallocate_buffer)
    {
        listener->deallocate_buffer(listener->data, streamId, buffer);
    }
}

int sdl_receive_frame(MediaSourceListener* listener, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    if (listener && listener->receive_frame)
    {
        return listener->receive_frame(listener->data, streamId, buffer, bufferSize);
    }
    return 0;
}

int sdl_receive_frame_const(MediaSourceListener* listener, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    unsigned char* nonconstBuffer = NULL;
    int result;
    
    if (listener && listener->receive_frame_const)
    {
        return listener->receive_frame_const(listener->data, streamId, buffer, bufferSize);
    }
    else
    {
        result = sdl_allocate_buffer(listener, streamId, &nonconstBuffer, bufferSize);
        if (result)
        {
            memcpy(nonconstBuffer, buffer, bufferSize);
            result = sdl_receive_frame(listener, streamId, nonconstBuffer, bufferSize);
        }
        return result;
    }
}


int msc_is_complete(MediaSource* source)
{
    if (source && source->is_complete)
    {
        return source->is_complete(source->data);
    }
    return 1; /* source if complete by default */
}

int msc_post_complete(MediaSource* source, MediaSource* rootSource, MediaControl* mediaControl)
{
    if (source && source->post_complete)
    {
        return source->post_complete(source->data, rootSource, mediaControl);
    }
    return 1; /* post complete is successful by default */
}

int msc_finalise_blank_source(MediaSource* source, const StreamInfo* streamInfo)
{
    if (source && source->finalise_blank_source)
    {
        return source->finalise_blank_source(source->data, streamInfo);
    }
    return 1; /* is successful by default */
}

int msc_get_num_streams(MediaSource* source)
{
    if (source && source->get_num_streams)
    {
        return source->get_num_streams(source->data);
    }
    return 0;
}

int msc_get_stream_info(MediaSource* source, int streamIndex, const StreamInfo** streamInfo)
{
    if (source && source->get_stream_info)
    {
        return source->get_stream_info(source->data, streamIndex, streamInfo);
    }
    return 0;
}

int msc_disable_stream(MediaSource* source, int streamIndex)
{
    if (source && source->disable_stream)
    {
        return source->disable_stream(source->data, streamIndex);
    }
    return 0;
}

void msc_disable_audio(MediaSource* source)
{
    if (source && source->disable_audio)
    {
        source->disable_audio(source->data);
    }
}

int msc_stream_is_disabled(MediaSource* source, int streamIndex)
{
    if (source && source->stream_is_disabled)
    {
        return source->stream_is_disabled(source->data, streamIndex);
    }
    return 0;
}

int msc_read_frame(MediaSource* source, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    if (source && source->read_frame)
    {
        return source->read_frame(source->data, frameInfo, listener);
    }
    return 0;
}

int msc_is_seekable(MediaSource* source)
{
    if (source && source->is_seekable)
    {
        return source->is_seekable(source->data);
    }
    return 0;
}

int msc_seek(MediaSource* source, int64_t position)
{
    if (source && source->seek)
    {
        return source->seek(source->data, position);
    }
    return 0;
}

int msc_seek_timecode(MediaSource* source, const Timecode* timecode, TimecodeType type, TimecodeSubType subType)
{
    if (source && source->seek_timecode)
    {
        return source->seek_timecode(source->data, timecode, type, subType);
    }
    return 0;
}

int msc_get_length(MediaSource* source, int64_t* length)
{
    if (source && source->get_length)
    {
        return source->get_length(source->data, length);
    }
    return 0;
}

int msc_get_position(MediaSource* source, int64_t* position)
{
    if (source && source->get_position)
    {
        return source->get_position(source->data, position);
    }
    return 0;
}

int msc_get_available_length(MediaSource* source, int64_t* length)
{
    if (source && source->get_available_length)
    {
        return source->get_available_length(source->data, length);
    }
    return 0;
}

int msc_eof(MediaSource* source)
{
    if (source && source->eof)
    {
        return source->eof(source->data);
    }
    return 0;
}

void msc_close(MediaSource* source)
{
    if (source && source->close)
    {
        source->close(source->data);
    }
}

int msc_get_buffer_state(MediaSource* source, int* numBuffers, int* numBuffersFilled)
{
    if (source && source->get_buffer_state)
    {
        return source->get_buffer_state(source->data, numBuffers, numBuffersFilled);
    }
    return 0;
}

int64_t msc_convert_position(MediaSource* source, int64_t position, MediaSource* childSource)
{
    if (source && source->convert_position)
    {
        return source->convert_position(source->data, position, childSource);
    }
    return position;
}

void msc_set_source_name(MediaSource* source, const char* name)
{
    if (source && source->set_source_name)
    {
        source->set_source_name(source->data, name);
    }
}

void msc_set_clip_id(MediaSource* source, const char* id)
{
    if (source && source->set_clip_id)
    {
        source->set_clip_id(source->data, id);
    }
}


int msc_create_id()
{
    static int id = 0;
    
    return id++;
}


void msc_init_stream_map(MediaSourceStreamMap* map)
{
    memset(map, 0, sizeof(*map));
}

int msc_add_stream_to_map(MediaSourceStreamMap* map, int streamId, int sourceId)
{
    int i;
    
    for (i = 0; i < map->numStreams; i++)
    {
        if (map->streams[i].streamId == streamId)
        {
            map->streams[i].sourceId = sourceId;
            return 1;
        }
    }
    
    if (map->numStreams + 1 < (int)(sizeof(map->streams) / sizeof(int)))
    {
        map->streams[map->numStreams].sourceId = sourceId;
        map->streams[map->numStreams].streamId = streamId;
        map->numStreams++;
    }
    else
    {
        ml_log_error("Number streams exceeds maximum (%d) expected\n", sizeof(map->streams) / sizeof(int));
        return 0;
    }
    
    return 1;
}

int msc_get_source_id(MediaSourceStreamMap* map, int streamId, int* sourceId)
{
    int i;
    
    for (i = 0; i < map->numStreams; i++)
    {
        if (map->streams[i].streamId == streamId)
        {
            *sourceId = map->streams[i].sourceId;
            return 1;
        }
    }
    
    return 0;
}



