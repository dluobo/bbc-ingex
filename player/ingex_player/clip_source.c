/*
 * $Id: clip_source.c,v 1.4 2008/10/29 17:47:41 john_f Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "clip_source.h"
#include "logging.h"
#include "macros.h"


struct ClipSource
{
    int64_t start;
    int64_t duration;
    
    MediaSource mediaSource;
    
    MediaSource* targetSource;
};



static int cps_is_complete(void* data)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_is_complete(clipSource->targetSource);
}

static int cps_post_complete(void* data, MediaSource* rootSource, MediaControl* mediaControl)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_post_complete(clipSource->targetSource, rootSource, mediaControl);
}

static int cps_finalise_blank_source(void* data, const StreamInfo* streamInfo)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_finalise_blank_source(clipSource->targetSource, streamInfo);
}

static int cps_get_num_streams(void* data)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    return msc_get_num_streams(clipSource->targetSource);
}

static int cps_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_get_stream_info(clipSource->targetSource, streamIndex, streamInfo);
}

static int cps_disable_stream(void* data, int streamIndex)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_disable_stream(clipSource->targetSource, streamIndex);
}

static void cps_disable_audio(void* data)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_disable_audio(clipSource->targetSource);
}

static int cps_stream_is_disabled(void* data, int streamIndex)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_stream_is_disabled(clipSource->targetSource, streamIndex);
}

static int cps_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    if (clipSource->duration >= 0)
    {
        /* check the position is within the clip boundaries */
        int64_t position;
        if (!msc_get_position(clipSource->targetSource, &position))
        {
            return -1;
        }
        if (position < clipSource->start ||
            position >= clipSource->start + clipSource->duration)
        {
            return -1;
        }
    }
    else if (clipSource->start > 0)
    {
        /* check the position is within the clip boundaries */
        int64_t position;
        if (!msc_get_position(clipSource->targetSource, &position))
        {
            return -1;
        }
        if (position < clipSource->start)
        {
            return -1;
        }
    }
    
    return msc_read_frame(clipSource->targetSource, frameInfo, listener);
}

static int cps_is_seekable(void* data)
{
    ClipSource* clipSource = (ClipSource*)data;

    return msc_is_seekable(clipSource->targetSource);
}

static int cps_seek(void* data, int64_t position)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    if (clipSource->duration >= 0)
    {
        /* check the position is within the clip boundaries */
        if (position > clipSource->duration)
        {
            return 0;
        }

        return msc_seek(clipSource->targetSource, clipSource->start + position);
    }
    else if (clipSource->start > 0)
    {
        return msc_seek(clipSource->targetSource, clipSource->start + position);
    }
    
    return msc_seek(clipSource->targetSource, position);
}

static int cps_seek_timecode(void* data, const Timecode* timecode, TimecodeType type, TimecodeSubType subType)
{
    ClipSource* clipSource = (ClipSource*)data;
    int64_t originalPosition = -1;
    int64_t position;

    /* get position so we can recover is seek is outside the clip boundaries */
    if (!msc_get_position(clipSource->targetSource, &originalPosition))
    {
        originalPosition = -1; /* unknown */
    }
    
    
    int result = msc_seek_timecode(clipSource->targetSource, timecode, type, subType);
    if (originalPosition < 0 || result != 0)
    {
        return result;
    }
    
    /* check the seek is within the clip boundaries */
    
    if (!msc_get_position(clipSource->targetSource, &position))
    {
        /* TODO: what now? */
        /* assume outside - go back to original position */
        msc_seek(clipSource->targetSource, originalPosition);
        
        return -1;
    }
    
    if (clipSource->duration >= 0)
    {
        if (position < clipSource->start || 
            position > clipSource->start + clipSource->duration)
        {
            /* outside - go back to original position */
            msc_seek(clipSource->targetSource, originalPosition);
            
            return -1;
        }
    }
    else if (clipSource->start > 0)
    {
        if (position < clipSource->start)
        {
            /* outside - go back to original position */
            msc_seek(clipSource->targetSource, originalPosition);
            
            return -1;
        }
    }
    
    return result;
}

static int cps_get_length(void* data, int64_t* length)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    if (clipSource->duration >= 0)
    {
        int64_t targetLength;
        if (!msc_get_length(clipSource->targetSource, &targetLength))
        {
            return 0;
        }
        
        *length = targetLength - clipSource->start;
        *length = (*length > clipSource->duration) ? clipSource->duration : *length;
        *length = (*length < 0) ? 0 : *length;
        return 1;
    }
    else if (clipSource->start > 0)
    {
        int64_t targetLength;
        if (!msc_get_length(clipSource->targetSource, &targetLength))
        {
            return 0;
        }
        
        *length = targetLength - clipSource->start;
        *length = (*length < 0) ? 0 : *length;
        return 1;
    }

    return msc_get_length(clipSource->targetSource, length);
}

static int cps_get_position(void* data, int64_t* position)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    if (clipSource->duration >= 0)
    {
        int64_t targetPosition;
        if (!msc_get_position(clipSource->targetSource, &targetPosition))
        {
            return 0;
        }
        
        *position = targetPosition - clipSource->start;
        *position = (*position > clipSource->duration) ? clipSource->duration : *position;
        *position = (*position < 0) ? 0 : *position;
        return 1;
    }
    else if (clipSource->start > 0)
    {
        int64_t targetPosition;
        if (!msc_get_position(clipSource->targetSource, &targetPosition))
        {
            return 0;
        }
        
        *position = targetPosition - clipSource->start;
        *position = (*position < 0) ? 0 : *position;
        return 1;
    }

    return msc_get_position(clipSource->targetSource, position);
}

static int cps_get_available_length(void* data, int64_t* length)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    if (clipSource->duration >= 0)
    {
        int64_t targetAvailableLength;
        if (!msc_get_available_length(clipSource->targetSource, &targetAvailableLength))
        {
            return 0;
        }
        
        *length = targetAvailableLength - clipSource->start; 
        *length = (*length > clipSource->duration) ? clipSource->duration : *length;
        *length = (*length < 0) ? 0 : *length;
        
        return 1;
    }
    else if (clipSource->start > 0)
    {
        int64_t targetAvailableLength;
        if (!msc_get_available_length(clipSource->targetSource, &targetAvailableLength))
        {
            return 0;
        }
        
        *length = targetAvailableLength - clipSource->start; 
        *length = (*length < 0) ? 0 : *length;
        
        return 1;
    }

    return msc_get_available_length(clipSource->targetSource, length);
}

static int cps_eof(void* data)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    if (clipSource->duration >= 0)
    {
        if (msc_eof(clipSource->targetSource))
        {
            return 1;
        }
        
        int64_t position;
        if (!msc_get_position(clipSource->targetSource, &position))
        {
            return 0;
        }
        
        return position >= clipSource->start + clipSource->duration;
    }
    
    return msc_eof(clipSource->targetSource);
}

static void cps_set_source_name(void* data, const char* name)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    msc_set_source_name(clipSource->targetSource, name);    
}

static void cps_set_clip_id(void* data, const char* id)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    msc_set_clip_id(clipSource->targetSource, id);
}

static void cps_close(void* data)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    SAFE_FREE(&clipSource);
}

static int cps_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    ClipSource* clipSource = (ClipSource*)data;
    
    return msc_get_buffer_state(clipSource->targetSource, numBuffers, numBuffersFilled);
}    

static int64_t cps_convert_position(void* data, int64_t position, MediaSource* childSource)
{
    ClipSource* clipSource = (ClipSource*)data;
    int64_t childPosition = position;
    
    if (childSource != &clipSource->mediaSource)
    {
        childPosition = msc_convert_position(clipSource->targetSource, position, childSource);
    }
    
    return childPosition - clipSource->start;
}    


int cps_create(MediaSource* targetSource, int64_t start, int64_t duration, ClipSource** clipSource)
{
    ClipSource* newClipSource;
    
    if (start > 0 && !msc_seek(targetSource, start))
    {
        ml_log_warn("Failed to seek to start of the clip\n"); 
    }
    
    CALLOC_ORET(newClipSource, ClipSource, 1);
    
    newClipSource->targetSource = targetSource;
    newClipSource->start = (start < 0) ? 0 : start;
    newClipSource->duration = duration;
    
    newClipSource->mediaSource.data = newClipSource;
    newClipSource->mediaSource.is_complete = cps_is_complete;
    newClipSource->mediaSource.post_complete = cps_post_complete;
    newClipSource->mediaSource.finalise_blank_source = cps_finalise_blank_source;
    newClipSource->mediaSource.get_num_streams = cps_get_num_streams;
    newClipSource->mediaSource.get_stream_info = cps_get_stream_info;
    newClipSource->mediaSource.disable_stream = cps_disable_stream;
    newClipSource->mediaSource.disable_audio = cps_disable_audio;
    newClipSource->mediaSource.stream_is_disabled = cps_stream_is_disabled;
    newClipSource->mediaSource.read_frame = cps_read_frame;
    newClipSource->mediaSource.is_seekable = cps_is_seekable;
    newClipSource->mediaSource.seek = cps_seek;
    newClipSource->mediaSource.seek_timecode = cps_seek_timecode;
    newClipSource->mediaSource.get_length = cps_get_length;
    newClipSource->mediaSource.get_position = cps_get_position;
    newClipSource->mediaSource.get_available_length = cps_get_available_length;
    newClipSource->mediaSource.eof = cps_eof;
    newClipSource->mediaSource.set_source_name = cps_set_source_name;
    newClipSource->mediaSource.set_clip_id = cps_set_clip_id;
    newClipSource->mediaSource.close = cps_close;
    newClipSource->mediaSource.get_buffer_state = cps_get_buffer_state;
    newClipSource->mediaSource.convert_position = cps_convert_position;
    
    *clipSource = newClipSource;
    return 1;
}

MediaSource* cps_get_media_source(ClipSource* clipSource)
{
    return &clipSource->mediaSource;
}


