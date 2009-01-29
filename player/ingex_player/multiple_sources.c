/*
 * $Id: multiple_sources.c,v 1.6 2009/01/29 07:10:26 stuart_hc Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "multiple_sources.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


/* is true if all streams in a source have been disabled */
#define SOURCE_IS_DISABLED(ele) \
    (ele->numStreams == ele->disabledStreamCount)


typedef struct MediaSourceElement
{
    struct MediaSourceElement* next;

    MediaSource* source;
    int numStreams;
    int disabledStreamCount;

    int isComplete;
    int postComplete;
} MediaSourceElement;

typedef MediaSourceElement MediaSourceList;

typedef struct
{
    MediaSourceListener sourceListener;
    int startStreamId;

    MediaSourceListener* targetListener;
} CollectiveListener;

struct MultipleMediaSources
{
    Rational aspectRatio;
    int64_t maxLength;
    Rational maxLengthFrameRate;

    MediaSource collectiveSource;
    MediaSourceList sources;

    int64_t syncPosition; /* all sources must report that they are at this position before
                             a file command is executed */
};



static int mls_accept_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    CollectiveListener* cListener = (CollectiveListener*)data;

    return sdl_accept_frame(cListener->targetListener, cListener->startStreamId + streamId, frameInfo);
}

static int mls_allocate_buffer(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    CollectiveListener* cListener = (CollectiveListener*)data;

    return sdl_allocate_buffer(cListener->targetListener, cListener->startStreamId + streamId,
        buffer, bufferSize);
}

static void mls_deallocate_buffer(void* data, int streamId, unsigned char** buffer)
{
    CollectiveListener* cListener = (CollectiveListener*)data;

    sdl_deallocate_buffer(cListener->targetListener, cListener->startStreamId + streamId, buffer);
}

static int mls_receive_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    CollectiveListener* cListener = (CollectiveListener*)data;

    return sdl_receive_frame(cListener->targetListener, cListener->startStreamId + streamId,
        buffer, bufferSize);
}


static int sync_sources(MultipleMediaSources* multSource)
{
    MediaSourceElement* ele = &multSource->sources;
    int64_t position;
    int syncResult = 0;

    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            if (!msc_get_position(ele->source, &position))
            {
                syncResult = -1;
                break;
            }

            if (position != multSource->syncPosition)
            {
                /* seek back to the sync position */
                syncResult = msc_seek(ele->source, multSource->syncPosition);
                if (syncResult != 0)
                {
                    break;
                }
            }
        }

        ele = ele->next;
    }

    return syncResult;
}

static int append_source(MediaSourceList* sourceList, MediaSource** source)
{
    MediaSourceElement* ele = sourceList;
    MediaSourceElement* newEle = NULL;
    int i;

    /* handle first appended source */
    if (ele->source == NULL)
    {
        ele->source = *source;
        ele->numStreams = msc_get_num_streams(*source);
        *source = NULL;
        return 1;
    }

    /* move to end */
    while (ele->next != NULL)
    {
        ele = ele->next;
    }

    /* create element */
    if ((newEle = (MediaSourceElement*)malloc(sizeof(MediaSourceElement))) == NULL)
    {
        ml_log_error("Failed to allocate memory\n");
        return 0;
    }
    memset(newEle, 0, sizeof(MediaSourceElement));

    /* append source and take ownership */
    ele->next = newEle;
    newEle->source = *source;

    /* get num streams and disabled count */
    newEle->numStreams = msc_get_num_streams(*source);
    newEle->disabledStreamCount = 0;
    for (i = 0; i < newEle->numStreams; i++)
    {
        if (msc_stream_is_disabled(newEle->source, i))
        {
            newEle->disabledStreamCount++;
        }
    }

    *source = NULL;
    return 1;
}


static int mls_is_complete(void* data)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int isComplete = 1;

    while (ele != NULL && ele->source != NULL)
    {
        if (!ele->isComplete)
        {
            ele->isComplete = msc_is_complete(ele->source);
            isComplete &= ele->isComplete;
        }
        ele = ele->next;
    }

    return isComplete;
}

static int mls_post_complete(void* data, MediaSource* rootSource, MediaControl* mediaControl)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int result = 1;

    while (ele != NULL && ele->source != NULL)
    {
        if (ele->isComplete)
        {
            ele->postComplete = msc_post_complete(ele->source, rootSource, mediaControl);
        }
        result &= ele->isComplete && ele->postComplete;
        ele = ele->next;
    }

    return result;
}

static int mls_get_num_streams(void* data)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int total = 0;

    while (ele != NULL && ele->source != NULL)
    {
        total += ele->numStreams;

        ele = ele->next;
    }

    return total;
}

static int mls_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int maxIndex = 0;
    int minIndex = 0;
    int result;

    while (ele != NULL && ele->source != NULL)
    {
        maxIndex += ele->numStreams;
        if (streamIndex < maxIndex)
        {
            result = msc_get_stream_info(ele->source, streamIndex - minIndex, streamInfo);

            /* force picture aspect ratio if neccessary */
            if (result &&
                (*streamInfo)->type == PICTURE_STREAM_TYPE &&
                (multSource->aspectRatio.num > 0 && multSource->aspectRatio.den > 0))
            {
                ((StreamInfo*)(*streamInfo))->aspectRatio = multSource->aspectRatio;
            }
            return result;
        }
        minIndex = maxIndex;

        ele = ele->next;
    }

    return 0;
}

static void mls_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int i;

    while (ele != NULL && ele->source != NULL)
    {
        msc_set_frame_rate_or_disable(ele->source, frameRate);

        /* recalculate disabledStreamCount */
        ele->disabledStreamCount = 0;
        for (i = 0; i < ele->numStreams; i++)
        {
            if (msc_stream_is_disabled(ele->source, i))
            {
                ele->disabledStreamCount++;
            }
        }

        ele = ele->next;
    }

    multSource->maxLength = convert_length(multSource->maxLength, &multSource->maxLengthFrameRate, frameRate);
    multSource->maxLengthFrameRate = *frameRate;
}

static int mls_disable_stream(void* data, int streamIndex)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int maxIndex = 0;
    int minIndex = 0;

    while (ele != NULL && ele->source != NULL)
    {
        maxIndex += ele->numStreams;
        if (streamIndex < maxIndex)
        {
            if (!msc_disable_stream(ele->source, streamIndex - minIndex))
            {
                return 0;
            }
            ele->disabledStreamCount++;
            return 1;
        }
        minIndex = maxIndex;

        ele = ele->next;
    }

    return 0;
}

static void mls_disable_audio(void* data)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;

    while (ele != NULL && ele->source != NULL)
    {
        msc_disable_audio(ele->source);

        ele = ele->next;
    }
}

static int mls_stream_is_disabled(void* data, int streamIndex)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int maxIndex = 0;
    int minIndex = 0;

    while (ele != NULL && ele->source != NULL)
    {
        maxIndex += ele->numStreams;
        if (streamIndex < maxIndex)
        {
            return msc_stream_is_disabled(ele->source, streamIndex - minIndex);
        }
        minIndex = maxIndex;

        ele = ele->next;
    }

    return 0;
}

static int mls_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    CollectiveListener collectiveListener;
    int readResult = 0;
    int startStreamId = 0;
    int syncResult;

    /* make sure the sources are in sync */
    if ((syncResult = sync_sources(multSource)) != 0)
    {
        return syncResult;
    }

    memset(&collectiveListener, 0, sizeof(collectiveListener));

    collectiveListener.sourceListener.data = &collectiveListener;
    collectiveListener.sourceListener.accept_frame = mls_accept_frame;
    collectiveListener.sourceListener.allocate_buffer = mls_allocate_buffer;
    collectiveListener.sourceListener.deallocate_buffer = mls_deallocate_buffer;
    collectiveListener.sourceListener.receive_frame = mls_receive_frame;

    collectiveListener.targetListener = listener;

    while (ele != NULL && ele->source != NULL)
    {
        collectiveListener.startStreamId = startStreamId;

        if (!SOURCE_IS_DISABLED(ele))
        {
            readResult = msc_read_frame(ele->source, frameInfo, &collectiveListener.sourceListener);
            if (readResult != 0)
            {
                break;
            }
        }

        startStreamId += ele->numStreams;

        ele = ele->next;
    }

    if (readResult == 0)
    {
        /* update sync position */
        multSource->syncPosition += 1;
    }

    return readResult;
}

static int mls_is_seekable(void* data)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int isSeekable = 1;

    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            if (!msc_is_seekable(ele->source))
            {
                isSeekable = 0;
                break;
            }
        }

        ele = ele->next;
    }

    return isSeekable;
}

static int mls_seek(void* data, int64_t position)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int seekResult = 0;
    int syncResult;

    /* make sure the sources are in sync */
    if ((syncResult = sync_sources(multSource)) != 0)
    {
        return syncResult;
    }

    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            seekResult = msc_seek(ele->source, position);
            if (seekResult != 0 )
            {
                break;
            }
        }

        ele = ele->next;
    }

    if (seekResult == 0)
    {
        /* update sync position */
        multSource->syncPosition = position;
    }

    return seekResult;
}

static int mls_seek_timecode(void* data, const Timecode* timecode, TimecodeType type, TimecodeSubType subType)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int seekResult = 0;
    int haveATimedOutResult = 0;
    int failed = 0;
    MediaSourceElement* passedEle = NULL;
    int64_t position;
    int syncResult;

    /* make sure the sources are in sync */
    if ((syncResult = sync_sources(multSource)) != 0)
    {
        return syncResult;
    }

    /* find the source that supports and successfully seeks to the timecode */
    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            seekResult = msc_seek_timecode(ele->source, timecode, type, subType);
            if (seekResult == 0)
            {
                passedEle = ele;
                if (!msc_get_position(ele->source, &position))
                {
                    ml_log_error("Failed to get position at seek to timecode\n");
                    return -1;
                }
                break;
            }
            else if (seekResult == -2)
            {
                haveATimedOutResult = 1;
            }
        }

        ele = ele->next;
    }
    if (passedEle == NULL)
    {
        /* none of the sources could be seeked to the timecode */
        if (haveATimedOutResult)
        {
            /* assume that the timed out source could have seeked to the timecode */
            return -2;
        }
        return -1;
    }

    /* try seek to the position corresponding to the timecode for the sources that failed */
    ele = &multSource->sources;
    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            if (ele != passedEle && msc_seek(ele->source, position) != 0)
            {
                failed = 1;
                break;
            }
        }

        ele = ele->next;
    }

    if (!failed)
    {
        /* update sync position */
        multSource->syncPosition = position;
    }

    return !failed ? 0 : -1;
}

static int mls_get_length(void* data, int64_t* length)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int failed = 0;
    int64_t srcLength;
    int64_t minLength = -1;

    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            if (!msc_get_length(ele->source, &srcLength))
            {
                failed = 1;
                break;
            }
            minLength = (minLength < 0 || srcLength < minLength) ? srcLength : minLength;
        }

        ele = ele->next;
    }

    if (failed)
    {
        return 0;
    }

    if (multSource->maxLength > 0 && minLength > multSource->maxLength)
    {
        minLength = multSource->maxLength;
    }

    *length = minLength;
    return 1;
}

static int mls_get_position(void* data, int64_t* position)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;

    *position = multSource->syncPosition;
    return 1;
}

static int mls_get_available_length(void* data, int64_t* length)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int64_t srcLength;
    int64_t minSrcLength = -1;

    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            if (!msc_get_available_length(ele->source, &srcLength))
            {
                minSrcLength = -1;
                break;
            }
            if (minSrcLength == -1 || srcLength < minSrcLength)
            {
                minSrcLength = srcLength;
            }
        }

        ele = ele->next;
    }

    if (minSrcLength == -1)
    {
        return 0;
    }

    if (multSource->maxLength > 0 && minSrcLength > multSource->maxLength)
    {
        minSrcLength = multSource->maxLength;
    }

    *length = minSrcLength;
    return 1;
}

static int mls_eof(void* data)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int eof = 0;
    int syncResult;

    /* make sure the sources are in sync */
    if ((syncResult = sync_sources(multSource)) != 0)
    {
        /* failure result is 0 and not the sync result */
        return 0;
    }

    while (ele != NULL && ele->source != NULL)
    {
        if (!SOURCE_IS_DISABLED(ele))
        {
            eof |= msc_eof(ele->source);
        }

        ele = ele->next;
    }

    return eof;
}

static void mls_set_source_name(void* data, const char* name)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    while (ele != NULL && ele->source != NULL)
    {
        msc_set_source_name(ele->source, name);

        ele = ele->next;
    }
}

static void mls_set_clip_id(void* data, const char* id)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    while (ele != NULL && ele->source != NULL)
    {
        msc_set_clip_id(ele->source, id);

        ele = ele->next;
    }
}

static void mls_close(void* data)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    MediaSourceElement* nextEle;

    if (data == NULL)
    {
        return;
    }

    while (ele != NULL)
    {
        nextEle = ele->next;

        msc_close(ele->source);
        if (ele != &multSource->sources) /* first element is static */
        {
            SAFE_FREE(&ele);
        }

        ele = nextEle;
    }
    SAFE_FREE(&multSource);
}

static int64_t mls_convert_position(void* data, int64_t position, MediaSource* childSource)
{
    MultipleMediaSources* multSource = (MultipleMediaSources*)data;
    MediaSourceElement* ele = &multSource->sources;
    int64_t returnPosition = position;
    int64_t childPosition;

    if (childSource == &multSource->collectiveSource)
    {
        return position;
    }

    /* we take the position of any child source that deviates from the given position */
    while (ele != NULL && ele->source != NULL)
    {
        childPosition = msc_convert_position(ele->source, position, childSource);
        if (childPosition != position)
        {
            returnPosition = childPosition;
            break;
        }

        ele = ele->next;
    }

    return returnPosition;
}


int mls_create(const Rational* aspectRatio, int64_t maxLength, const Rational* maxLengthFrameRate, MultipleMediaSources** multSource)
{
    MultipleMediaSources* newMultSource;

    CALLOC_ORET(newMultSource, MultipleMediaSources, 1);

    if (aspectRatio != NULL)
    {
        newMultSource->aspectRatio = *aspectRatio;
    }
    newMultSource->maxLength = maxLength;
    newMultSource->maxLengthFrameRate = *maxLengthFrameRate;

    newMultSource->collectiveSource.data = newMultSource;
    newMultSource->collectiveSource.is_complete = mls_is_complete;
    newMultSource->collectiveSource.post_complete = mls_post_complete;
    newMultSource->collectiveSource.get_num_streams = mls_get_num_streams;
    newMultSource->collectiveSource.get_stream_info = mls_get_stream_info;
    newMultSource->collectiveSource.set_frame_rate_or_disable = mls_set_frame_rate_or_disable;
    newMultSource->collectiveSource.disable_stream = mls_disable_stream;
    newMultSource->collectiveSource.disable_audio = mls_disable_audio;
    newMultSource->collectiveSource.stream_is_disabled = mls_stream_is_disabled;
    newMultSource->collectiveSource.read_frame = mls_read_frame;
    newMultSource->collectiveSource.is_seekable = mls_is_seekable;
    newMultSource->collectiveSource.seek = mls_seek;
    newMultSource->collectiveSource.seek_timecode = mls_seek_timecode;
    newMultSource->collectiveSource.get_length = mls_get_length;
    newMultSource->collectiveSource.get_position = mls_get_position;
    newMultSource->collectiveSource.get_available_length = mls_get_available_length;
    newMultSource->collectiveSource.eof = mls_eof;
    newMultSource->collectiveSource.set_source_name = mls_set_source_name;
    newMultSource->collectiveSource.set_clip_id = mls_set_clip_id;
    newMultSource->collectiveSource.close = mls_close;
    newMultSource->collectiveSource.convert_position = mls_convert_position;

    *multSource = newMultSource;
    return 1;
}

int mls_assign_source(MultipleMediaSources* multSource, MediaSource** source)
{
    return append_source(&multSource->sources, source);
}

int mls_finalise_blank_sources(MultipleMediaSources* multSource)
{
    MediaSourceElement* ele = &multSource->sources;
    StreamInfo videoStreamInfo;
    int i;
    const StreamInfo* streamInfo;
    int haveVideoStreamInfo = 0;

    while (ele != NULL && ele->source != NULL)
    {
        int numStreams = msc_get_num_streams(ele->source);
        for (i = 0; i < numStreams; i++)
        {
            CHK_ORET(msc_get_stream_info(ele->source, i, &streamInfo));
            if (streamInfo->type == PICTURE_STREAM_TYPE &&
                streamInfo->format != UNKNOWN_FORMAT)
            {
                videoStreamInfo = *streamInfo;
                haveVideoStreamInfo = 1;
                break;
            }
        }
        ele = ele->next;
    }
    if (!haveVideoStreamInfo)
    {
        /* use default settings if no video source with known format is present */
        videoStreamInfo.type = PICTURE_STREAM_TYPE;
        videoStreamInfo.format = UYVY_FORMAT;
        videoStreamInfo.width = 720;
        videoStreamInfo.height = 576;
        videoStreamInfo.frameRate = g_palFrameRate;
        videoStreamInfo.aspectRatio.num = 4;
        videoStreamInfo.aspectRatio.den = 3;
    }

    ele = &multSource->sources;
    while (ele != NULL && ele->source != NULL)
    {
        CHK_ORET(msc_finalise_blank_source(ele->source, &videoStreamInfo));
        ele = ele->next;
    }

    return 1;
}

MediaSource* mls_get_media_source(MultipleMediaSources* multSource)
{
    return &multSource->collectiveSource;
}


