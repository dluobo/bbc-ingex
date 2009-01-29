/*
 * $Id: half_split_sink.c,v 1.6 2009/01/29 07:10:26 stuart_hc Exp $
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
#include <assert.h>

#include "half_split_sink.h"
#include "video_switch_sink.h"
#include "video_conversion.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define HALF_SPLIT_FORMAT() \
    split->firstInputStream->streamInfo.format

#define HALF_SPLIT_WIDTH() \
    split->firstInputStream->streamInfo.width

#define HALF_SPLIT_HEIGHT() \
    split->firstInputStream->streamInfo.height


typedef enum
{
    SPLIT_TOP,
    SPLIT_BOTTOM,
    SPLIT_LEFT,
    SPLIT_RIGHT
} SplitSegment;

typedef struct VideoStreamElement
{
    struct VideoStreamElement* next;
    struct VideoStreamElement* prev;

    int index; /* used by half split to limit half split streams to 2 */
    int streamId;
    StreamInfo streamInfo;
} VideoStreamElement;

typedef struct
{
    int verticalSplit;
    HalfSplitType type;
    int showSplitDivide;

    int halfSplitHorizPos;
    int halfSplitVertPos;
    int panHorizPos;
    int panVertPos;

    VideoStreamElement* currentStream;
} HalfSplitState;

struct HalfSplitSink
{
    MediaSink* targetSink;

    VideoSwitchSink switchSink;
    MediaSink sink;

    /* targetSinkListener listens to target sink and forwards to the switch sink listener */
    MediaSinkListener targetSinkListener;
    MediaSinkListener* switchListener;

    VideoStreamElement streams;
    VideoStreamElement* firstInputStream;
    VideoStreamElement* halfSplitStream;

    unsigned char* halfSplitInputBuffer1;
    unsigned char* halfSplitInputBuffer2;
    unsigned int halfSplitInputBufferSize;
    unsigned char* halfSplitOutputBuffer;

    pthread_mutex_t stateMutex;
    HalfSplitState state;
    HalfSplitState nextState;
    int updateState;

    int horizStep;
    int vertStep;
    int horizDividerSize;
    int vertDividerSize;

    int haveCheckedFirstInputStream;
    int haveAcceptedFirstInputStream;
    int haveHalfSplitOutputBuffer;
};


/* TODO: half split with semi-transparent overlay; eg seen YUV_text_overlay.c, add_overlay() function */

/* TODO: pan past end if divider used */

static void half_split_uyvy(SplitSegment segment, int showSplitDivide, int dividerSize, HalfSplitType type,
    int splitPosition, unsigned char* inputBuffer, int width, int height,
    unsigned char* outputBuffer)
{
    int i;
    int splitCopySize;
    unsigned char* target;
    unsigned char* source;

    switch (segment)
    {
        case SPLIT_TOP:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                splitCopySize = width * height;
                target = outputBuffer;
                source = inputBuffer + width * splitPosition * 2;
            }
            else
            {
                splitCopySize = width * splitPosition * 2;
                target = outputBuffer;
                source = inputBuffer;
            }

            if (showSplitDivide)
            {
                splitCopySize -= width * dividerSize;
            }

            if (splitCopySize > 0)
            {
                memcpy(target, source, splitCopySize);
            }
            break;

        case SPLIT_LEFT:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                splitCopySize = width;
                target = outputBuffer;
                source = inputBuffer + splitPosition * 2;
            }
            else
            {
                splitCopySize = splitPosition * 2;
                target = outputBuffer;
                source = inputBuffer;
            }

            if (showSplitDivide)
            {
                splitCopySize -= dividerSize;
            }

            if (splitCopySize > 0)
            {
                for (i = 0; i < height; i++)
                {
                    memcpy(target, source, splitCopySize);
                    target += width * 2;
                    source += width * 2;
                }
            }
            break;

        case SPLIT_BOTTOM:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                splitCopySize = width * height;
                target = outputBuffer + width * height;
                source = inputBuffer + width * splitPosition * 2;
            }
            else
            {
                splitCopySize = width * (height - splitPosition) * 2;
                target = outputBuffer + (width * height * 2 - splitCopySize);
                if (type == CONTINUOUS_SPLIT_TYPE)
                {
                    source = inputBuffer + width * splitPosition * 2;
                }
                else /* SINGLE_PAN_SPLIT_TYPE */
                {
                    source = inputBuffer;
                }
            }

            if (showSplitDivide)
            {
                splitCopySize -= width * dividerSize;
                source += width * dividerSize;
                target += width * dividerSize;
            }

            if (splitCopySize > 0)
            {
                memcpy(target, source, splitCopySize);
            }
            break;

        case SPLIT_RIGHT:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                splitCopySize = width;
                target = outputBuffer + width;
                source = inputBuffer + splitPosition * 2;
            }
            else
            {
                splitCopySize = (width - splitPosition) * 2;
                target = outputBuffer + splitPosition * 2;

                if (type == CONTINUOUS_SPLIT_TYPE)
                {
                    source = inputBuffer + splitPosition * 2;
                }
                else /* SINGLE_PAN_SPLIT_TYPE */
                {
                    source = inputBuffer;
                }
            }

            if (showSplitDivide)
            {
                splitCopySize -= dividerSize;
                target += dividerSize;
                source += dividerSize;
            }

            if (splitCopySize > 0)
            {
                for (i = 0; i < height; i++)
                {
                    memcpy(target, source, splitCopySize);
                    target += width * 2;
                    source += width * 2;
                }
            }
            break;

        default:
            /* shouldn't be here */
            assert(0);
    }
}

static void half_split_yuv420(SplitSegment segment, int showSplitDivide, int dividerSize, HalfSplitType type,
    int splitPosition, unsigned char* inputBuffer, int width, int height,
    unsigned char* outputBuffer)
{
    int i;
    int y_splitCopySize;
    int u_splitCopySize;
    int v_splitCopySize;
    unsigned char* y_target;
    unsigned char* u_target;
    unsigned char* v_target;
    unsigned char* y_source;
    unsigned char* u_source;
    unsigned char* v_source;

    switch (segment)
    {
        case SPLIT_TOP:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                y_splitCopySize = width * height / 2;
                u_splitCopySize = width * height / 8;
                v_splitCopySize = width * height / 8;
                y_target = outputBuffer;
                u_target = outputBuffer + width * height;
                v_target = outputBuffer + width * height * 5 / 4;
                y_source = inputBuffer + width * splitPosition;
                u_source = inputBuffer + width * height + width * splitPosition / 4;
                v_source = inputBuffer + width * height * 5 / 4 + width * splitPosition / 4;
            }
            else
            {
                y_splitCopySize = width * splitPosition;
                u_splitCopySize = width * splitPosition / 4;
                v_splitCopySize = width * splitPosition / 4;
                y_target = outputBuffer;
                u_target = outputBuffer + width * height;
                v_target = outputBuffer + width * height * 5 / 4;
                y_source = inputBuffer;
                u_source = inputBuffer + width * height;
                v_source = inputBuffer + width * height * 5 / 4;
            }

            if (showSplitDivide)
            {
                y_splitCopySize -= width * dividerSize / 2;
                u_splitCopySize -= width * dividerSize / 8;
                v_splitCopySize -= width * dividerSize / 8;
            }

            if (y_splitCopySize > 0)
            {
                memcpy(y_target, y_source, y_splitCopySize);
            }
            if (u_splitCopySize > 0)
            {
                memcpy(u_target, u_source, u_splitCopySize);
            }
            if (v_splitCopySize > 0)
            {
                memcpy(v_target, v_source, v_splitCopySize);
            }
            break;

        case SPLIT_LEFT:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                y_splitCopySize = width / 2;
                u_splitCopySize = width / 4;
                v_splitCopySize = width / 4;
                y_target = outputBuffer;
                u_target = outputBuffer + width * height;
                v_target = outputBuffer + width * height * 5 / 4;
                y_source = inputBuffer + splitPosition;
                u_source = inputBuffer + width * height + splitPosition / 2;
                v_source = inputBuffer + width * height * 5 / 4 + splitPosition / 2;
            }
            else
            {
                y_splitCopySize = splitPosition;
                u_splitCopySize = splitPosition / 2;
                v_splitCopySize = splitPosition / 2;
                y_target = outputBuffer;
                u_target = outputBuffer + width * height;
                v_target = outputBuffer + width * height * 5 / 4;
                y_source = inputBuffer;
                u_source = inputBuffer + width * height;
                v_source = inputBuffer + width * height * 5 / 4;
            }

            if (showSplitDivide)
            {
                y_splitCopySize -= dividerSize / 2;
                u_splitCopySize -= dividerSize / 4;
                v_splitCopySize -= dividerSize / 4;
            }

            if (y_splitCopySize > 0)
            {
                for (i = 0; i < height; i++)
                {
                    memcpy(y_target, y_source, y_splitCopySize);
                    y_target += width;
                    y_source += width;
                }
            }
            if (u_splitCopySize > 0)
            {
                for (i = 0; i < height / 2; i++)
                {
                    memcpy(u_target, u_source, u_splitCopySize);
                    u_target += width / 2;
                    u_source += width / 2;
                }
            }
            if (v_splitCopySize > 0)
            {
                for (i = 0; i < height / 2; i++)
                {
                    memcpy(v_target, v_source, v_splitCopySize);
                    v_target += width / 2;
                    v_source += width / 2;
                }
            }
            break;

        case SPLIT_BOTTOM:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                y_splitCopySize = width * height / 2;
                u_splitCopySize = width * height / 8;
                v_splitCopySize = width * height / 8;
                y_target = outputBuffer + width * height / 2;
                u_target = outputBuffer + width * height + width * height / 8;
                v_target = outputBuffer + width * height * 5 / 4 + width * height / 8;
                y_source = inputBuffer + width * splitPosition;
                u_source = inputBuffer + width * height + width * splitPosition / 4;
                v_source = inputBuffer + width * height * 5 / 4 + width * splitPosition / 4;
            }
            else
            {
                y_splitCopySize = width * (height - splitPosition);
                u_splitCopySize = width * (height - splitPosition) / 4;
                v_splitCopySize = width * (height - splitPosition) / 4;
                y_target = outputBuffer + (width * height - y_splitCopySize);
                u_target = outputBuffer + width * height + (width * height / 4 - u_splitCopySize);
                v_target = outputBuffer + width * height * 5 / 4 + (width * height / 4 - v_splitCopySize);

                if (type == CONTINUOUS_SPLIT_TYPE)
                {
                    y_source = inputBuffer + width * splitPosition;
                    u_source = inputBuffer + width * height + width * splitPosition / 4;
                    v_source = inputBuffer + width * height * 5 / 4 + width * splitPosition / 4;
                }
                else
                {
                    y_source = inputBuffer;
                    u_source = inputBuffer + width * height;
                    v_source = inputBuffer + width * height * 5 / 4;
                }
            }

            if (showSplitDivide)
            {
                y_splitCopySize -= width * dividerSize / 2;
                u_splitCopySize -= width * dividerSize / 8;
                v_splitCopySize -= width * dividerSize / 8;
                y_source += width * dividerSize / 2;
                u_source += width * dividerSize / 8;
                v_source += width * dividerSize / 8;
                y_target += width * dividerSize / 2;
                u_target += width * dividerSize / 8;
                v_target += width * dividerSize / 8;
            }

            if (y_splitCopySize > 0)
            {
                memcpy(y_target, y_source, y_splitCopySize);
            }
            if (u_splitCopySize > 0)
            {
                memcpy(u_target, u_source, u_splitCopySize);
            }
            if (v_splitCopySize > 0)
            {
                memcpy(v_target, v_source, v_splitCopySize);
            }
            break;

        case SPLIT_RIGHT:
            if (type == DUAL_PAN_SPLIT_TYPE)
            {
                y_splitCopySize = width / 2;
                u_splitCopySize = width / 4;
                v_splitCopySize = width / 4;
                y_target = outputBuffer + width / 2;
                u_target = outputBuffer + width * height + width / 4;
                v_target = outputBuffer + width * height * 5 / 4 + width / 4;
                y_source = inputBuffer + splitPosition;
                u_source = inputBuffer + width * height + splitPosition / 2;
                v_source = inputBuffer + width * height * 5 / 4 + splitPosition / 2;
            }
            else
            {
                y_splitCopySize = (width - splitPosition);
                u_splitCopySize = (width - splitPosition) / 2;
                v_splitCopySize = (width - splitPosition) / 2;
                y_target = outputBuffer + splitPosition;
                u_target = outputBuffer + width * height + splitPosition / 2;
                v_target = outputBuffer + width * height * 5 / 4 + splitPosition / 2;

                if (type == CONTINUOUS_SPLIT_TYPE)
                {
                    y_source = inputBuffer + splitPosition;
                    u_source = inputBuffer + width * height + splitPosition / 2;
                    v_source = inputBuffer + width * height * 5 / 4 + splitPosition / 2;
                }
                else
                {
                    y_source = inputBuffer;
                    u_source = inputBuffer + width * height;
                    v_source = inputBuffer + width * height * 5 / 4;
                }
            }

            if (showSplitDivide)
            {
                y_splitCopySize -= dividerSize / 2;
                u_splitCopySize -= dividerSize / 4;
                v_splitCopySize -= dividerSize / 4;
                y_target += dividerSize / 2;
                u_target += dividerSize / 4;
                v_target += dividerSize / 4;
                y_source += dividerSize / 2;
                u_source += dividerSize / 4;
                v_source += dividerSize / 4;
            }

            if (y_splitCopySize > 0)
            {
                for (i = 0; i < height; i++)
                {
                    memcpy(y_target, y_source, y_splitCopySize);
                    y_target += width;
                    y_source += width;
                }
            }
            if (u_splitCopySize > 0)
            {
                for (i = 0; i < height / 2; i++)
                {
                    memcpy(u_target, u_source, u_splitCopySize);
                    u_target += width / 2;
                    u_source += width / 2;
                }
            }
            if (v_splitCopySize > 0)
            {
                for (i = 0; i < height / 2; i++)
                {
                    memcpy(v_target, v_source, v_splitCopySize);
                    v_target += width / 2;
                    v_source += width / 2;
                }
            }
            break;

        default:
            /* shouldn't be here */
            assert(0);
    }
}

static void half_split(StreamFormat videoFormat,
    SplitSegment segment, int showSplitDivide, int dividerSize, HalfSplitType type,
    int splitPosition, unsigned char* inputBuffer, int width, int height,
    unsigned char* outputBuffer)
{
    if (videoFormat == UYVY_FORMAT)
    {
        half_split_uyvy(segment, showSplitDivide, dividerSize, type, splitPosition,
            inputBuffer, width, height, outputBuffer);
    }
    else /* YUV420_FORMAT */
    {
        half_split_yuv420(segment, showSplitDivide, dividerSize, type, splitPosition,
            inputBuffer, width, height, outputBuffer);
    }
}

static int add_stream(HalfSplitSink* split, int streamId, const StreamInfo* streamInfo)
{
    VideoStreamElement* ele = &split->streams;
    VideoStreamElement* newEle = NULL;

    /* move to end */
    while (ele->next != NULL)
    {
        ele = ele->next;
    }

    /* create element */
    CALLOC_ORET(newEle, VideoStreamElement, 1);

    /* append */
    newEle->prev = ele;
    newEle->index = ele->index + 1;
    newEle->streamId = streamId;
    newEle->streamInfo = *streamInfo;
    if (split->halfSplitStream == NULL)
    {

        /* allocate buffers, set splits half way and other settings dependent on YUV format */
        if (streamInfo->format == UYVY_FORMAT)
        {
            split->halfSplitInputBufferSize = streamInfo->width * streamInfo->height * 2;
            MALLOC_OFAIL(split->halfSplitInputBuffer1, unsigned char, split->halfSplitInputBufferSize);
            MALLOC_OFAIL(split->halfSplitInputBuffer2, unsigned char, split->halfSplitInputBufferSize);

            split->nextState.halfSplitHorizPos = (streamInfo->width / 4) * 2;
            split->nextState.halfSplitVertPos = streamInfo->height / 2;

            split->horizStep = 2;
            split->vertStep = 1;

            split->horizDividerSize = 4;
            split->vertDividerSize = 4;
        }
        else /* YUV420 */
        {
            split->halfSplitInputBufferSize = streamInfo->width * streamInfo->height * 3 / 2;
            MALLOC_OFAIL(split->halfSplitInputBuffer1, unsigned char, split->halfSplitInputBufferSize);
            MALLOC_OFAIL(split->halfSplitInputBuffer2, unsigned char, split->halfSplitInputBufferSize);

            split->nextState.halfSplitHorizPos = (streamInfo->width / 4) * 2;
            split->nextState.halfSplitVertPos = (streamInfo->height / 4) * 2;

            split->horizStep = 2;
            split->vertStep = 2;

            split->horizDividerSize = 4;
            split->vertDividerSize = 4;
        }
        split->state.halfSplitHorizPos = split->nextState.halfSplitHorizPos;
        split->state.halfSplitVertPos = split->nextState.halfSplitVertPos;

        /* half split stream is the list root */
        split->halfSplitStream = &split->streams;
        split->firstInputStream = newEle;
        split->nextState.currentStream = &split->streams;
        split->state.currentStream = split->nextState.currentStream;
    }
    ele->next = newEle;

    assert(split->firstInputStream != NULL);

    return 1;

fail:
    SAFE_FREE(&newEle);
    return 0;
}

static int is_switchable_stream(HalfSplitSink* split, int streamId)
{
    VideoStreamElement* ele = split->firstInputStream;
    while (ele != NULL)
    {
        if (ele->streamId == streamId)
        {
            return 1;
        }
        ele = ele->next;
    }
    return 0;
}

/* returns 1 or 2, or 0 if it is not part of the half split */
static int get_half_split_index(HalfSplitSink* split, int streamId)
{
    VideoStreamElement* ele = split->firstInputStream;
    while (ele != NULL)
    {
        if (ele->streamId == streamId)
        {
            if (ele->index >= 1 && ele->index <= 2)
            {
                return ele->index;
            }
            else
            {
                /* not part of the half split */
                return 0;
            }

        }
        ele = ele->next;
    }
    return 0;
}



static void hss_frame_displayed(void* data, const FrameInfo* frameInfo)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    msl_frame_displayed(split->switchListener, frameInfo);
}

static void hss_frame_dropped(void* data, const FrameInfo* lastFrameInfo)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    msl_frame_dropped(split->switchListener, lastFrameInfo);
}

static void hss_refresh_required(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    msl_refresh_required(split->switchListener);
}



static int hss_register_listener(void* data, MediaSinkListener* listener)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    split->switchListener = listener;

    return msk_register_listener(split->targetSink, &split->targetSinkListener);
}

static void hss_unregister_listener(void* data, MediaSinkListener* listener)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (split->switchListener == listener)
    {
        split->switchListener = NULL;
    }

    msk_unregister_listener(split->targetSink, &split->targetSinkListener);
}

static int hss_accept_stream(void* data, const StreamInfo* streamInfo)
{
    HalfSplitSink* split = (HalfSplitSink*)data;
    int result;

    result = msk_accept_stream(split->targetSink, streamInfo);
    if (result && streamInfo->type == PICTURE_STREAM_TYPE &&
        streamInfo->format != UYVY_FORMAT && streamInfo->format != YUV420_FORMAT)
    {
        return 0;
    }
    return result;
}

static int hss_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        (streamInfo->format == UYVY_FORMAT || streamInfo->format == YUV420_FORMAT))
    {
        if (split->firstInputStream == NULL)
        {
            /* if sink accepts this stream then it is the first input stream */
            if (msk_register_stream(split->targetSink, streamId, streamInfo))
            {
                return add_stream(split, streamId, streamInfo);
            }
        }
        else
        {
            /* if stream matches 1st stream then add to list */
            if (streamInfo->format == split->firstInputStream->streamInfo.format &&
                memcmp(&streamInfo->frameRate, &split->firstInputStream->streamInfo.frameRate, sizeof(Rational)) == 0 &&
                streamInfo->width == HALF_SPLIT_WIDTH() &&
                streamInfo->height == HALF_SPLIT_HEIGHT())
            {
                return add_stream(split, streamId, streamInfo);
            }
        }
        return 0;
    }
    else
    {
        return msk_register_stream(split->targetSink, streamId, streamInfo);
    }
}

static int hss_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (is_switchable_stream(split, streamId))
    {
        if (split->updateState)
        {
            PTHREAD_MUTEX_LOCK(&split->stateMutex);
            split->state = split->nextState;
            PTHREAD_MUTEX_UNLOCK(&split->stateMutex);
            split->updateState = 0;
        }

        if (split->state.currentStream == split->halfSplitStream)
        {
            if (!split->haveCheckedFirstInputStream)
            {
                split->haveAcceptedFirstInputStream = msk_accept_stream_frame(split->targetSink,
                    split->firstInputStream->streamId, frameInfo);
                split->haveCheckedFirstInputStream = 1;
            }
            return split->haveAcceptedFirstInputStream &&
                get_half_split_index(split, streamId) != 0;
        }
        else
        {
            if (!split->haveCheckedFirstInputStream)
            {
                split->haveAcceptedFirstInputStream = msk_accept_stream_frame(split->targetSink,
                    split->firstInputStream->streamId, frameInfo);
                split->haveCheckedFirstInputStream = 1;
            }
            return split->haveAcceptedFirstInputStream && split->state.currentStream->streamId == streamId;
        }
    }
    else
    {
        return msk_accept_stream_frame(split->targetSink, streamId, frameInfo);
    }
}

static int hss_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (is_switchable_stream(split, streamId))
    {
        if (split->state.currentStream == split->halfSplitStream)
        {
            /* allocate the output buffer if we haven't done so already */
            if (!split->haveHalfSplitOutputBuffer)
            {
                /* check the buffer size */
                if (split->halfSplitInputBufferSize != bufferSize)
                {
                    ml_log_error("Requested buffer size (%d) != half split buffer size (%d)\n",
                        bufferSize, split->halfSplitInputBufferSize);
                    return 0;
                }

                CHK_ORET(msk_get_stream_buffer(split->targetSink, split->firstInputStream->streamId,
                    bufferSize, &split->halfSplitOutputBuffer));

                /* always clear the frame, eg. to prevent video from non-half-split showing through
                after a switch or a disabled OSD */
                fill_black(HALF_SPLIT_FORMAT(), HALF_SPLIT_WIDTH(), HALF_SPLIT_HEIGHT(), split->halfSplitOutputBuffer);

                split->haveHalfSplitOutputBuffer = 1;
            }

            /* set the input buffer */
            switch (get_half_split_index(split, streamId))
            {
                /* left or top */
                case 1:
                    *buffer = split->halfSplitInputBuffer1;
                    break;
                /* right or bottom */
                case 2:
                    *buffer = split->halfSplitInputBuffer2;
                    break;
                default:
                    /* shouldn't be here if accept_stream_frame was called */
                    assert(0);
                    return 0;
            }

            return 1;
        }
        else if (split->state.currentStream->streamId == streamId)
        {
            return msk_get_stream_buffer(split->targetSink, split->firstInputStream->streamId, bufferSize, buffer);
        }

        /* shouldn't be here if hss_accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else
    {
        return msk_get_stream_buffer(split->targetSink, streamId, bufferSize, buffer);
    }
}

static int hss_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (is_switchable_stream(split, streamId))
    {
        if (split->state.currentStream == split->halfSplitStream)
        {
            /* check the buffer size */
            if (split->halfSplitInputBufferSize != bufferSize)
            {
                ml_log_error("Buffer size (%d) != half split data size (%d)\n", bufferSize, split->halfSplitInputBufferSize);
                return 0;
            }

            /* half split */
            switch (get_half_split_index(split, streamId))
            {
                /* left or top */
                case 1:
                    half_split(HALF_SPLIT_FORMAT(),
                        split->state.verticalSplit ? SPLIT_TOP : SPLIT_LEFT,
                        split->state.showSplitDivide,
                        split->state.verticalSplit ? split->vertDividerSize : split->horizDividerSize,
                        split->state.type,
                        split->state.verticalSplit ?
                            (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panVertPos : split->state.halfSplitVertPos)
                            : (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panHorizPos : split->state.halfSplitHorizPos),
                        split->halfSplitInputBuffer1,
                        HALF_SPLIT_WIDTH(), HALF_SPLIT_HEIGHT(),
                        split->halfSplitOutputBuffer);
                    break;
                /* right or bottom */
                case 2:
                    half_split(HALF_SPLIT_FORMAT(),
                        split->state.verticalSplit ? SPLIT_BOTTOM : SPLIT_RIGHT,
                        split->state.showSplitDivide,
                        split->state.verticalSplit ? split->vertDividerSize : split->horizDividerSize,
                        split->state.type,
                        split->state.verticalSplit ?
                            (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panVertPos : split->state.halfSplitVertPos)
                            : (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panHorizPos : split->state.halfSplitHorizPos),
                        split->halfSplitInputBuffer2,
                        HALF_SPLIT_WIDTH(), HALF_SPLIT_HEIGHT(),
                        split->halfSplitOutputBuffer);
                    break;
                default:
                    /* shouldn't be here if accept_stream_frame was called */
                    assert(0);
                    return 0;
            }

            /* we don't send the half split until complete_frame is called */
            return 1;
        }
        else if (split->state.currentStream->streamId == streamId)
        {
            return msk_receive_stream_frame(split->targetSink, split->firstInputStream->streamId, buffer, bufferSize);
        }

        /* shouldn't be here if accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else
    {
        return msk_receive_stream_frame(split->targetSink, streamId, buffer, bufferSize);
    }
}

static int hss_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (is_switchable_stream(split, streamId))
    {
        if (split->state.currentStream == split->halfSplitStream)
        {
            /* check the buffer size */
            if (split->halfSplitInputBufferSize != bufferSize)
            {
                ml_log_error("Buffer size (%d) != half split data size (%d)\n", bufferSize, split->halfSplitInputBufferSize);
                return 0;
            }

            /* allocate the output buffer if we haven't done so already */
            if (!split->haveHalfSplitOutputBuffer)
            {
                CHK_ORET(msk_get_stream_buffer(split->targetSink, split->firstInputStream->streamId,
                    bufferSize, &split->halfSplitOutputBuffer));

                /* always clear the frame, eg. to prevent video from non-half-split showing through
                after a switch or a disabled OSD */
                fill_black(HALF_SPLIT_FORMAT(), HALF_SPLIT_WIDTH(), HALF_SPLIT_HEIGHT(), split->halfSplitOutputBuffer);

                split->haveHalfSplitOutputBuffer = 1;
            }

            /* half split */
            switch (get_half_split_index(split, streamId))
            {
                /* left or top */
                case 1:
                    half_split(HALF_SPLIT_FORMAT(),
                        split->state.verticalSplit ? SPLIT_TOP : SPLIT_LEFT,
                        split->state.showSplitDivide,
                        split->state.verticalSplit ? split->vertDividerSize : split->horizDividerSize,
                        split->state.type,
                        split->state.verticalSplit ?
                            (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panVertPos : split->state.halfSplitVertPos)
                            : (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panHorizPos : split->state.halfSplitHorizPos),
                        split->halfSplitInputBuffer1,
                        HALF_SPLIT_WIDTH(), HALF_SPLIT_HEIGHT(),
                        split->halfSplitOutputBuffer);
                    break;
                /* right or bottom */
                case 2:
                    half_split(HALF_SPLIT_FORMAT(),
                        split->state.verticalSplit ? SPLIT_BOTTOM : SPLIT_RIGHT,
                        split->state.showSplitDivide,
                        split->state.verticalSplit ? split->vertDividerSize : split->horizDividerSize,
                        split->state.type,
                        split->state.verticalSplit ?
                            (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panVertPos : split->state.halfSplitVertPos)
                            : (split->state.type == DUAL_PAN_SPLIT_TYPE ?
                                split->state.panHorizPos : split->state.halfSplitHorizPos),
                        split->halfSplitInputBuffer2,
                        HALF_SPLIT_WIDTH(), HALF_SPLIT_HEIGHT(),
                        split->halfSplitOutputBuffer);
                    break;
                default:
                    /* shouldn't be here if accept_stream_frame was called */
                    assert(0);
                    return 0;
            }

            /* we don't send the half split until complete_frame is called */
            return 1;
        }
        else if (split->state.currentStream->streamId == streamId)
        {
            return msk_receive_stream_frame_const(split->targetSink, split->firstInputStream->streamId, buffer, bufferSize);
        }

        /* shouldn't be here if accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else
    {
        return msk_receive_stream_frame_const(split->targetSink, streamId, buffer, bufferSize);
    }
}

static int hss_complete_frame(void* data, const FrameInfo* frameInfo)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (split->firstInputStream != NULL)  /* only if there is a video stream */
    {
        /* resets */
        split->haveCheckedFirstInputStream = 0;
        split->haveAcceptedFirstInputStream = 0;
        split->haveHalfSplitOutputBuffer = 0;
        split->updateState = 1;

        if (split->state.currentStream != NULL && split->state.currentStream == split->halfSplitStream)
        {
            /* send the half split */
            if (!msk_receive_stream_frame(split->targetSink, split->firstInputStream->streamId,
                split->halfSplitOutputBuffer, split->halfSplitInputBufferSize))
            {
                ml_log_error("Failed to send half split to sink\n");
                return 0;
            }
        }
    }

    return msk_complete_frame(split->targetSink, frameInfo);
}

static void hss_cancel_frame(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    msk_cancel_frame(split->targetSink);
}

static OnScreenDisplay* hss_get_osd(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    return msk_get_osd(split->targetSink);
}

static VideoSwitchSink* hss_get_video_switch(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    return msk_get_video_switch(split->targetSink);
}

static AudioSwitchSink* hss_get_audio_switch(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    return msk_get_audio_switch(split->targetSink);
}

static HalfSplitSink* hss_get_half_split(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    return split;
}

static int hss_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    return msk_get_buffer_state(split->targetSink, numBuffers, numBuffersFilled);
}

static int hss_mute_audio(void* data, int mute)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    return msk_mute_audio(split->targetSink, mute);
}

static void hss_close(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;
    VideoStreamElement* ele;
    VideoStreamElement* nextEle;

    if (data == NULL)
    {
        return;
    }

    msk_close(split->targetSink);

    ele = split->streams.next;
    while (ele != NULL)
    {
        nextEle = ele->next;
        SAFE_FREE(&ele);
        ele = nextEle;
    }

    SAFE_FREE(&split->halfSplitInputBuffer1);
    SAFE_FREE(&split->halfSplitInputBuffer2);

    destroy_mutex(&split->stateMutex);

    SAFE_FREE(&split);
}

static int hss_reset_or_close(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;
    VideoStreamElement* ele;
    VideoStreamElement* nextEle;
    int result;

    ele = split->streams.next;
    while (ele != NULL)
    {
        nextEle = ele->next;
        SAFE_FREE(&ele);
        ele = nextEle;
    }
    memset(&split->streams, 0, sizeof(VideoStreamElement));

    split->firstInputStream = NULL;
    split->halfSplitStream = NULL;

    split->state.currentStream = NULL;
    split->nextState.currentStream = NULL;
    split->updateState = 1;

    SAFE_FREE(&split->halfSplitInputBuffer1);
    SAFE_FREE(&split->halfSplitInputBuffer2);
    split->halfSplitOutputBuffer = NULL;

    split->haveCheckedFirstInputStream = 0;
    split->haveAcceptedFirstInputStream = 0;
    split->haveHalfSplitOutputBuffer = 0;

    result = msk_reset_or_close(split->targetSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            split->targetSink = NULL;
        }
        goto fail;
    }

    return 1;

fail:
    hss_close(data);
    return 2;
}


static MediaSink* hvs_get_media_sink(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    return &split->sink;
}

static int hvs_switch_next_video(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (split->nextState.currentStream == NULL)
    {
        return 0;
    }

    PTHREAD_MUTEX_LOCK(&split->stateMutex);
    if (split->nextState.currentStream->next != NULL)
    {
        split->nextState.currentStream = split->nextState.currentStream->next;
    }
    else
    {
        split->nextState.currentStream = &split->streams;
    }
    PTHREAD_MUTEX_UNLOCK(&split->stateMutex);

    msl_refresh_required(split->switchListener);

    return 1;
}

static int hvs_switch_prev_video(void* data)
{
    HalfSplitSink* split = (HalfSplitSink*)data;

    if (split->nextState.currentStream == NULL)
    {
        return 0;
    }

    PTHREAD_MUTEX_LOCK(&split->stateMutex);
    if (split->nextState.currentStream->prev != NULL)
    {
        split->nextState.currentStream = split->nextState.currentStream->prev;
    }
    else
    {
        while (split->nextState.currentStream->next != NULL)
        {
            split->nextState.currentStream = split->nextState.currentStream->next;
        }
    }
    PTHREAD_MUTEX_UNLOCK(&split->stateMutex);

    msl_refresh_required(split->switchListener);

    return 1;
}

static int hvs_switch_video(void* data, int index)
{
    HalfSplitSink* split = (HalfSplitSink*)data;
    VideoStreamElement* ele;
    int haveSwitched = 0;

    if (split->nextState.currentStream == NULL)
    {
        return 0;
    }

    PTHREAD_MUTEX_LOCK(&split->stateMutex);
    ele = &split->streams;
    while (ele != NULL)
    {
        if (index == ele->index)
        {
            split->nextState.currentStream = ele;
            haveSwitched = 1;
            break;
        }
        ele = ele->next;
    }
    PTHREAD_MUTEX_UNLOCK(&split->stateMutex);

    if (haveSwitched)
    {
        msl_refresh_required(split->switchListener);
    }

    return haveSwitched;
}



int hss_create_half_split(MediaSink* sink, int verticalSplit, HalfSplitType type, int showSplitDivide, HalfSplitSink** split)
{
    HalfSplitSink* newSplit;

    CALLOC_ORET(newSplit, HalfSplitSink, 1);

    newSplit->nextState.verticalSplit = verticalSplit;
    newSplit->nextState.type = type;
    newSplit->nextState.showSplitDivide = showSplitDivide;
    newSplit->updateState = 1;

    newSplit->targetSink = sink;

    newSplit->switchSink.data = newSplit;
    newSplit->switchSink.get_media_sink = hvs_get_media_sink;
    newSplit->switchSink.switch_next_video = hvs_switch_next_video;
    newSplit->switchSink.switch_prev_video = hvs_switch_prev_video;
    newSplit->switchSink.switch_video = hvs_switch_video;

    newSplit->targetSinkListener.data = newSplit;
    newSplit->targetSinkListener.frame_displayed = hss_frame_displayed;
    newSplit->targetSinkListener.frame_dropped = hss_frame_dropped;
    newSplit->targetSinkListener.refresh_required = hss_refresh_required;

    newSplit->sink.data = newSplit;
    newSplit->sink.register_listener = hss_register_listener;
    newSplit->sink.unregister_listener = hss_unregister_listener;
    newSplit->sink.accept_stream = hss_accept_stream;
    newSplit->sink.register_stream = hss_register_stream;
    newSplit->sink.accept_stream_frame = hss_accept_stream_frame;
    newSplit->sink.get_stream_buffer = hss_get_stream_buffer;
    newSplit->sink.receive_stream_frame = hss_receive_stream_frame;
    newSplit->sink.receive_stream_frame_const = hss_receive_stream_frame_const;
    newSplit->sink.complete_frame = hss_complete_frame;
    newSplit->sink.cancel_frame = hss_cancel_frame;
    newSplit->sink.get_osd = hss_get_osd;
    newSplit->sink.get_video_switch = hss_get_video_switch;
    newSplit->sink.get_audio_switch = hss_get_audio_switch;
    newSplit->sink.get_half_split = hss_get_half_split;
    newSplit->sink.get_buffer_state = hss_get_buffer_state;
    newSplit->sink.mute_audio = hss_mute_audio;
    newSplit->sink.reset_or_close = hss_reset_or_close;
    newSplit->sink.close = hss_close;

    CHK_OFAIL(init_mutex(&newSplit->stateMutex));


    *split = newSplit;
    return 1;

fail:
    hss_close(newSplit);
    return 0;
}


MediaSink* hss_get_media_sink(HalfSplitSink* split)
{
    return &split->sink;
}

void hss_set_half_split_orientation(HalfSplitSink* split, int verticalSplit)
{
    int refresh = 0;

    if (split == NULL)
    {
        return;
    }

    PTHREAD_MUTEX_LOCK(&split->stateMutex);
    if (verticalSplit < 0)
    {
        /* toggle */
        refresh = 1;
        split->nextState.verticalSplit = !split->nextState.verticalSplit;
    }
    else
    {
        refresh = (split->nextState.verticalSplit != verticalSplit);
        split->nextState.verticalSplit = verticalSplit;
    }
    PTHREAD_MUTEX_UNLOCK(&split->stateMutex);

    if (refresh)
    {
        msl_refresh_required(split->switchListener);
    }
}

void hss_set_half_split_type(HalfSplitSink* split, int type)
{
    int refresh = 0;

    if (split == NULL)
    {
        return;
    }

    PTHREAD_MUTEX_LOCK(&split->stateMutex);
    if (type == CONTINUOUS_SPLIT_TYPE ||
        type == SINGLE_PAN_SPLIT_TYPE ||
        type == DUAL_PAN_SPLIT_TYPE)
    {
        refresh = ((int)split->nextState.type != type);
        split->nextState.type = type;
    }
    else
    {
        /* toggle */
        refresh = 1;
        split->nextState.type = (split->nextState.type + 1) % (DUAL_PAN_SPLIT_TYPE + 1);
    }
    PTHREAD_MUTEX_UNLOCK(&split->stateMutex);

    if (refresh)
    {
        msl_refresh_required(split->switchListener);
    }
}

void hss_show_half_split(HalfSplitSink* split, int showSplitDivide)
{
    int refresh = 0;

    if (split == NULL)
    {
        return;
    }

    PTHREAD_MUTEX_LOCK(&split->stateMutex);
    if (showSplitDivide < 0)
    {
        /* toggle */
        refresh = 1;
        split->nextState.showSplitDivide = !split->nextState.showSplitDivide;
    }
    else
    {
        refresh = (split->nextState.showSplitDivide != showSplitDivide);
        split->nextState.showSplitDivide = showSplitDivide;
    }
    PTHREAD_MUTEX_UNLOCK(&split->stateMutex);

    if (refresh)
    {
        msl_refresh_required(split->switchListener);
    }
}

void hss_move_half_split(HalfSplitSink* split, int rightOrDown, int speed)
{
    int prevVal;
    int legitSpeed;
    int step;
    const int steps[6] = {1, 2, 4, 8, 16, 32};
    int refresh = 0;

    if (split == NULL)
    {
        return;
    }

    if (split->firstInputStream == NULL)
    {
        /* ignore until we know the stream dimensions */
        return;
    }

    legitSpeed = (speed < 0) ? 0 : speed;
    legitSpeed = (legitSpeed > 5) ? 5 : legitSpeed;


    PTHREAD_MUTEX_LOCK(&split->stateMutex);
    if (split->nextState.verticalSplit)
    {
        step = steps[legitSpeed] * split->vertStep;

        if (split->nextState.type == CONTINUOUS_SPLIT_TYPE ||
            split->nextState.type == SINGLE_PAN_SPLIT_TYPE)
        {
            prevVal = split->nextState.halfSplitVertPos;

            if (rightOrDown)
            {
                split->nextState.halfSplitVertPos = (split->nextState.halfSplitVertPos - step <= 0) ?
                    0 : split->nextState.halfSplitVertPos - step;
            }
            else
            {
                split->nextState.halfSplitVertPos = (split->nextState.halfSplitVertPos + step >= HALF_SPLIT_HEIGHT()) ?
                    HALF_SPLIT_HEIGHT() - split->vertStep : split->nextState.halfSplitVertPos + step;
            }

            refresh = (split->nextState.halfSplitVertPos != prevVal);
        }
        else /* DUAL_PAN_SPLIT_TYPE */
        {
            prevVal = split->nextState.panVertPos;

            if (rightOrDown)
            {
                split->nextState.panVertPos = (split->nextState.panVertPos - step <= 0) ?
                    0 : split->nextState.panVertPos - step;
            }
            else
            {
                split->nextState.panVertPos = (split->nextState.panVertPos + step >= HALF_SPLIT_HEIGHT() / 2) ?
                    HALF_SPLIT_HEIGHT() / 2 - split->vertStep : split->nextState.panVertPos + step;
            }

            refresh = (split->nextState.halfSplitVertPos != prevVal);
        }
    }
    else
    {
        step = steps[legitSpeed] * split->horizStep;

        if (split->nextState.type == CONTINUOUS_SPLIT_TYPE ||
            split->nextState.type == SINGLE_PAN_SPLIT_TYPE)
        {
            prevVal = split->nextState.halfSplitHorizPos;

            if (rightOrDown)
            {
                split->nextState.halfSplitHorizPos = (split->nextState.halfSplitHorizPos + step >= HALF_SPLIT_WIDTH()) ?
                    HALF_SPLIT_WIDTH() - split->horizStep : split->nextState.halfSplitHorizPos + step;
            }
            else
            {
                split->nextState.halfSplitHorizPos = (split->nextState.halfSplitHorizPos - step <= 0) ?
                    0 : split->nextState.halfSplitHorizPos - step;
            }

            refresh = (split->nextState.halfSplitVertPos != prevVal);
        }
        else /* DUAL_PAN_SPLIT_TYPE */
        {
            prevVal = split->nextState.panHorizPos;

            if (rightOrDown)
            {
                split->nextState.panHorizPos = (split->nextState.panHorizPos + step >= HALF_SPLIT_WIDTH() / 2) ?
                    HALF_SPLIT_WIDTH() / 2 - split->horizStep : split->nextState.panHorizPos + step;
            }
            else
            {
                split->nextState.panHorizPos = (split->nextState.panHorizPos - step <= 0) ?
                    0 : split->nextState.panHorizPos - step;
            }

            refresh = (split->nextState.panHorizPos != prevVal);
        }
    }
    PTHREAD_MUTEX_UNLOCK(&split->stateMutex);

    if (refresh)
    {
        msl_refresh_required(split->switchListener);
    }
}


