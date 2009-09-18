/*
 * $Id: video_switch_sink.c,v 1.10 2009/09/18 16:16:24 philipn Exp $
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

#include "video_switch_sink.h"
#include "YUV_frame.h"
#include "YUV_small_pic.h"
#include "media_source.h"
#include "source_event.h"
#include "video_conversion.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define MAX_SPLITS          9



typedef struct VideoStreamElement
{
    struct VideoStreamElement* next;
    struct VideoStreamElement* prev;

    int index; /* used to limit number of streams in split view */
    int streamId;
    StreamInfo streamInfo;

    int sourceNameEventStreamIndex;
    char sourceName[64];
} VideoStreamElement;


typedef struct
{
    VideoSwitchSplit videoSwitchSplit;
    int splitCount;
    int applySplitFilter;
    int splitSelect;
    int prescaledSplit;
    int masterTimecodeIndex;
    int masterTimecodeType;
    int masterTimecodeSubType;

    int showSourceName;

    MediaSink* targetSink;

    VideoSwitchSink switchSink;
    MediaSink sink;

    /* targetSinkListener listens to target sink and forwards to the switch sink listener */
    MediaSinkListener targetSinkListener;
    MediaSinkListener* switchListener;

    VideoStreamElement streams;
    VideoStreamElement* firstInputStream;
    VideoStreamElement* splitStream;
    VideoStreamElement* currentStream;
    char currentSourceName[64];

    pthread_mutex_t nextCurrentStreamMutex;
    VideoStreamElement* nextCurrentStream;
    int disableSwitching;
    int showSplitSelect;
    int nextShowSplitSelect;
    int haveSwitched;

    unsigned char* splitWorkspace[MAX_SPLITS];
    unsigned char* splitInputBuffer[MAX_SPLITS];
    unsigned int splitInputBufferSize;
    YUV_frame splitFrameIn[MAX_SPLITS];
    unsigned char* splitOutputBuffer;
    unsigned int splitOutputBufferSize;
    YUV_frame splitFrameOut;
    formats yuvFormat;
    int splitOutputWidth;
    int splitOutputHeight;
    int borderWidth;
    int borderHeight;

    int haveCheckedFirstInputStream;
    int haveAcceptedFirstInputStream;
    int haveSplitOutputBuffer;

    MediaSourceStreamMap eventStreamMap;
    int eventStreams[64]; /* size should be == MediaSourceStreamMap::streams array */
    int numEventStreams;

    unsigned char* eventBuffer;
    unsigned int eventBufferSize;

    VideoSwitchDatabase* database;
} DefaultVideoSwitch;


static void set_source_name(VideoStreamElement* ele, const char* name)
{
    if (name != NULL)
    {
        strncpy(ele->sourceName, name, sizeof(ele->sourceName) - 1);
    }
    else
    {
        ele->sourceName[0] = '\0';
    }
}

static int add_event_stream(DefaultVideoSwitch* swtch, int streamId, const StreamInfo* streamInfo)
{
    VideoStreamElement* ele;

    if (!msc_add_stream_to_map(&swtch->eventStreamMap, streamId, streamInfo->sourceId))
    {
        return 0;
    }

    if (swtch->numEventStreams + 1 > (int)(sizeof(swtch->eventStreams) / sizeof(int)))
    {
        ml_log_error("(code limit) Number event streams exceeds maximum (%d) expected\n", sizeof(swtch->eventStreams) / sizeof(int));
        return 0;
    }
    swtch->eventStreams[swtch->numEventStreams] = streamId;
    swtch->numEventStreams++;

    ele = swtch->firstInputStream;
    while (ele != NULL)
    {
        if (ele->sourceNameEventStreamIndex < 0 &&
            ele->streamInfo.sourceId == streamInfo->sourceId)
        {
            ele->sourceNameEventStreamIndex = streamId;
        }

        ele = ele->next;
    }

    return 1;
}

static void associate_event_stream(DefaultVideoSwitch* swtch, VideoStreamElement* ele)
{
    int i;
    int sourceId;

    for (i = 0; i < swtch->numEventStreams; i++)
    {
        if (msc_get_source_id(&swtch->eventStreamMap, swtch->eventStreams[i], &sourceId) &&
            sourceId == ele->streamInfo.sourceId)
        {
            ele->sourceNameEventStreamIndex = swtch->eventStreams[i];
            return;
        }
    }
}

static void process_event(DefaultVideoSwitch* swtch, int eventStreamId, const unsigned char* eventBuffer)
{
    int numEvents = svt_read_num_events(eventBuffer);

    int i;
    SourceEvent event;
    VideoStreamElement* ele;
    for (i = 0; i < numEvents; i++)
    {
        svt_read_event(eventBuffer, i, &event);
        if (event.type == SOURCE_NAME_UPDATE_EVENT_TYPE)
        {
            ele = swtch->firstInputStream;
            while (ele != NULL)
            {
                if (ele->sourceNameEventStreamIndex == eventStreamId)
                {
                    set_source_name(ele, event.value.nameUpdate.name);

                    ml_log_info("Updated stream %d name to '%s'\n", ele->streamId, ele->sourceName);
                }

                ele = ele->next;
            }
        }
    }
}

static void fit_image(int imageWidth, int imageHeight, int windowWidth, int windowHeight,
    int posX, int posY, StreamFormat streamFormat, const unsigned char* input, unsigned char* output)
{
    int i, j;

    if (streamFormat == UYVY_FORMAT)
    {
        const unsigned char* inputPtr = input;
        unsigned char* outputPtr;

        for (i = 0; i < imageHeight; i++)
        {
            outputPtr = output + ((posY + i) * windowWidth + posX) * 2;

            for (j = 0; j < imageWidth; j += 2)
            {
                *outputPtr++ = *inputPtr++;
                *outputPtr++ = *inputPtr++;
                *outputPtr++ = *inputPtr++;
                *outputPtr++ = *inputPtr++;
            }
        }
    }
    else if (streamFormat == YUV422_FORMAT)
    {
        const unsigned char* yInput = input;
        const unsigned char* uInput = input + imageWidth * imageHeight;
        const unsigned char* vInput = input + imageWidth * imageHeight * 3 / 2;
        unsigned char* yOutput;
        unsigned char* uOutput;
        unsigned char* vOutput;

        for (i = 0; i < imageHeight; i++)
        {
            yOutput = output + (posY + i) * windowWidth + posX;
            uOutput = output + windowWidth * windowHeight + ((posY + i) * windowWidth + posX) / 2;
            vOutput = output + windowWidth * windowHeight * 3 / 2 + ((posY + i) * windowWidth + posX) / 2;

            for (j = 0; j < imageWidth; j += 2)
            {
                *yOutput++ = *yInput++;
                *uOutput++ = *uInput++;
                *yOutput++ = *yInput++;
                *vOutput++ = *vInput++;
            }
        }
    }
    else if (streamFormat == YUV420_FORMAT)
    {
        const unsigned char* yInput = input;
        const unsigned char* uInput = input + imageWidth * imageHeight;
        const unsigned char* vInput = input + imageWidth * imageHeight * 5 / 4;
        unsigned char* yOutput;
        unsigned char* uOutput;
        unsigned char* vOutput;

        for (i = 0; i < imageHeight; i++)
        {
            yOutput = output + (posY + i) * windowWidth + posX;
            uOutput = output + windowWidth * windowHeight + ((posY + i) * windowWidth / 2 + posX) / 2;
            vOutput = output + windowWidth * windowHeight * 5 / 4 + ((posY + i) * windowWidth / 2 + posX) / 2;

            for (j = 0; j < imageWidth; j += 2)
            {
                /* copy input */
                *yOutput++ = *yInput++;
                *yOutput++ = *yInput++;
                if (i % 2 == 0)
                {
                    *uOutput++ = *uInput++;
                    *vOutput++ = *vInput++;
                }
            }
        }
    }
}

static void set_split_select_background(DefaultVideoSwitch* swtch)
{
    int selectedBox[9];
    int internalSelectedBox[9];

    switch (swtch->videoSwitchSplit)
    {
        case QUAD_SPLIT_VIDEO_SWITCH:
            selectedBox[0] = 0 + (swtch->splitOutputWidth / 2) * ((swtch->currentStream->index - 1) % 2);
            selectedBox[1] = 0 + (swtch->splitOutputHeight / 2) * ((swtch->currentStream->index - 1) / 2);
            selectedBox[2] = (swtch->splitOutputWidth / 2) + (swtch->splitOutputWidth / 2) * ((swtch->currentStream->index - 1) % 2);
            selectedBox[3] = (swtch->splitOutputHeight / 2) + (swtch->splitOutputHeight / 2) * ((swtch->currentStream->index - 1) / 2);

            internalSelectedBox[0] = selectedBox[0] + swtch->borderWidth;
            internalSelectedBox[1] = selectedBox[1] + swtch->borderHeight;
            internalSelectedBox[2] = selectedBox[2] - swtch->borderWidth;
            internalSelectedBox[3] = selectedBox[3] - swtch->borderHeight;
            break;

        case NONA_SPLIT_VIDEO_SWITCH:
            selectedBox[0] = 0 + (swtch->splitOutputWidth / 3) * ((swtch->currentStream->index - 1) % 3);
            selectedBox[1] = 0 + (swtch->splitOutputHeight / 3) * ((swtch->currentStream->index - 1) / 3);
            selectedBox[2] = (swtch->splitOutputWidth / 3) + (swtch->splitOutputWidth / 3) * ((swtch->currentStream->index - 1) % 3);
            selectedBox[3] = (swtch->splitOutputHeight / 3) + (swtch->splitOutputHeight / 3) * ((swtch->currentStream->index - 1) / 3);

            internalSelectedBox[0] = selectedBox[0] + swtch->borderWidth;
            internalSelectedBox[1] = selectedBox[1] + swtch->borderHeight;
            internalSelectedBox[2] = selectedBox[2] - swtch->borderWidth;
            internalSelectedBox[3] = selectedBox[3] - swtch->borderHeight;
            break;

        default:
            assert(0);
            return;
    }

    if (swtch->firstInputStream->streamInfo.format == UYVY_FORMAT)
    {
        int xPos, yPos;
        unsigned char* uyvy;

        for (yPos = selectedBox[1]; yPos < selectedBox[3]; yPos++)
        {
            if (yPos < internalSelectedBox[1] || yPos >= internalSelectedBox[3])
            {
                /* top and bottom borders */

                uyvy = swtch->splitOutputBuffer + yPos * swtch->splitOutputWidth * 2 + selectedBox[0] * 2;
                for (xPos = selectedBox[0]; xPos < selectedBox[2]; xPos += 2)
                {
                    *uyvy++ = 53;
                    *uyvy++ = 144;
                    *uyvy++ = 34;
                    *uyvy++ = 144;
                }
            }
            else
            {
                /* left border */

                uyvy = swtch->splitOutputBuffer + yPos * swtch->splitOutputWidth * 2 + selectedBox[0] * 2;
                for (xPos = selectedBox[0]; xPos < internalSelectedBox[0]; xPos += 2)
                {
                    *uyvy++ = 53;
                    *uyvy++ = 144;
                    *uyvy++ = 34;
                    *uyvy++ = 144;
                }

                /* right border */

                uyvy = swtch->splitOutputBuffer + yPos * swtch->splitOutputWidth * 2 + internalSelectedBox[2] * 2;
                for (xPos = internalSelectedBox[2]; xPos < selectedBox[2]; xPos += 2)
                {
                    *uyvy++ = 53;
                    *uyvy++ = 144;
                    *uyvy++ = 34;
                    *uyvy++ = 144;
                }
            }
        }
    }
    else if (swtch->firstInputStream->streamInfo.format == YUV422_FORMAT)
    {
        int xPos, yPos;
        unsigned char* yPlane = swtch->splitOutputBuffer;
        unsigned char* uPlane = yPlane + swtch->splitOutputWidth * swtch->splitOutputHeight;
        unsigned char* vPlane = uPlane + swtch->splitOutputWidth * swtch->splitOutputHeight / 2;
        unsigned char* y;
        unsigned char* u;
        unsigned char* v;

        for (yPos = selectedBox[1]; yPos < selectedBox[3]; yPos++)
        {
            if (yPos < internalSelectedBox[1] || yPos >= internalSelectedBox[3])
            {
                /* top and bottom borders */

                y = yPlane + yPos * swtch->splitOutputWidth + selectedBox[0];
                u = uPlane + yPos * swtch->splitOutputWidth / 2 + selectedBox[0] / 2;
                v = vPlane + yPos * swtch->splitOutputWidth / 2 + selectedBox[0] / 2;
                for (xPos = selectedBox[0]; xPos < selectedBox[2]; xPos++)
                {
                    *y++ = 144;
                    if (xPos % 2 == 0)
                    {
                        *u++ = 53;
                        *v++ = 34;
                    }
                }
            }
            else
            {
                /* left border */

                y = yPlane + yPos * swtch->splitOutputWidth + selectedBox[0];
                u = uPlane + yPos * swtch->splitOutputWidth / 2 + selectedBox[0] / 2;
                v = vPlane + yPos * swtch->splitOutputWidth / 2 + selectedBox[0] / 2;
                for (xPos = selectedBox[0]; xPos < internalSelectedBox[0]; xPos++)
                {
                    *y++ = 144;
                    if (xPos % 2 == 0)
                    {
                        *u++ = 53;
                        *v++ = 34;
                    }
                }

                /* right border */

                y = yPlane + yPos * swtch->splitOutputWidth + internalSelectedBox[2];
                u = uPlane + yPos * swtch->splitOutputWidth / 2 + internalSelectedBox[2] / 2;
                v = vPlane + yPos * swtch->splitOutputWidth / 2 + internalSelectedBox[2] / 2;
                for (xPos = internalSelectedBox[2]; xPos < selectedBox[2]; xPos++)
                {
                    *y++ = 144;
                    if (xPos % 2 == 0)
                    {
                        *u++ = 53;
                        *v++ = 34;
                    }
                }
            }
        }
    }
    else /* YUV420_FORMAT */
    {
        int xPos, yPos;
        unsigned char* yPlane = swtch->splitOutputBuffer;
        unsigned char* uPlane = yPlane + swtch->splitOutputWidth * swtch->splitOutputHeight;
        unsigned char* vPlane = uPlane + swtch->splitOutputWidth * swtch->splitOutputHeight / 4;
        unsigned char* y;
        unsigned char* u;
        unsigned char* v;

        for (yPos = selectedBox[1]; yPos < selectedBox[3]; yPos++)
        {
            if (yPos < internalSelectedBox[1] || yPos >= internalSelectedBox[3])
            {
                /* top and bottom borders */

                y = yPlane + yPos * swtch->splitOutputWidth + selectedBox[0];
                u = uPlane + yPos * swtch->splitOutputWidth / 4 + selectedBox[0] / 2;
                v = vPlane + yPos * swtch->splitOutputWidth / 4 + selectedBox[0] / 2;
                for (xPos = selectedBox[0]; xPos < selectedBox[2]; xPos++)
                {
                    *y++ = 144;
                    if (xPos % 2 == 0 && yPos % 2 == 0)
                    {
                        *u++ = 53;
                        *v++ = 34;
                    }
                }
            }
            else
            {
                /* left border */

                y = yPlane + yPos * swtch->splitOutputWidth + selectedBox[0];
                u = uPlane + yPos * swtch->splitOutputWidth / 4 + selectedBox[0] / 2;
                v = vPlane + yPos * swtch->splitOutputWidth / 4 + selectedBox[0] / 2;
                for (xPos = selectedBox[0]; xPos < internalSelectedBox[0]; xPos++)
                {
                    *y++ = 144;
                    if (xPos % 2 == 0 && yPos % 2 == 0)
                    {
                        *u++ = 53;
                        *v++ = 34;
                    }
                }

                /* right border */

                y = yPlane + yPos * swtch->splitOutputWidth + internalSelectedBox[2];
                u = uPlane + yPos * swtch->splitOutputWidth / 4 + internalSelectedBox[2] / 2;
                v = vPlane + yPos * swtch->splitOutputWidth / 4 + internalSelectedBox[2] / 2;
                for (xPos = internalSelectedBox[2]; xPos < selectedBox[2]; xPos++)
                {
                    *y++ = 144;
                    if (xPos % 2 == 0 && yPos % 2 == 0)
                    {
                        *u++ = 53;
                        *v++ = 34;
                    }
                }
            }
        }
    }
}

static void get_output_stream_info(DefaultVideoSwitch* swtch, const StreamInfo* streamInfo, StreamInfo* outputStreamInfo)
{
    *outputStreamInfo = *streamInfo;

    if (swtch->prescaledSplit)
    {
        switch (swtch->videoSwitchSplit)
        {
            case QUAD_SPLIT_VIDEO_SWITCH:
                outputStreamInfo->width = streamInfo->width * 2;
                outputStreamInfo->height = streamInfo->height * 2;
                break;
            case NONA_SPLIT_VIDEO_SWITCH:
                outputStreamInfo->width = streamInfo->width * 3;
                outputStreamInfo->height = streamInfo->height * 3;
                break;
            case NO_SPLIT_VIDEO_SWITCH:
            default:
                outputStreamInfo->width = streamInfo->width;
                outputStreamInfo->height = streamInfo->height;
                break;
        }
    }
    else
    {
        outputStreamInfo->width = streamInfo->width;
        outputStreamInfo->height = streamInfo->height;
    }

}

static int add_stream(DefaultVideoSwitch* swtch, int streamId, const StreamInfo* streamInfo)
{
    VideoStreamElement* ele = &swtch->streams;
    VideoStreamElement* newEle = NULL;
    int i;
    StreamInfo outputStreamInfo;

    /* handle first append */
    if (swtch->firstInputStream == NULL && swtch->videoSwitchSplit == NO_SPLIT_VIDEO_SWITCH)
    {
        ele->index = 1;
        ele->streamId = streamId;
        ele->streamInfo = *streamInfo;
        set_source_name(ele, get_known_source_info_value(streamInfo, SRC_INFO_NAME));
        ele->sourceNameEventStreamIndex = -1;
        associate_event_stream(swtch, ele);
        swtch->firstInputStream = ele;
        swtch->currentStream = ele;
        swtch->nextCurrentStream = ele;
        return 1;
    }

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
    set_source_name(newEle, get_known_source_info_value(streamInfo, SRC_INFO_NAME));
    newEle->sourceNameEventStreamIndex = -1;
    associate_event_stream(swtch, newEle);
    if (swtch->videoSwitchSplit != NO_SPLIT_VIDEO_SWITCH && swtch->splitStream == NULL)
    {
        /* initialise the split stream */

        /* TODO: allow for different size inputs */

        /* allocate the quad split input buffer and initialise the YUV_lib frame for the input */
        if (streamInfo->format == UYVY_FORMAT)
        {
            swtch->yuvFormat = UYVY;
            swtch->splitInputBufferSize = streamInfo->width * streamInfo->height * 2;
        }
        else if (streamInfo->format == YUV422_FORMAT)
        {
            swtch->yuvFormat = YV16;
            swtch->splitInputBufferSize = streamInfo->width * streamInfo->height * 2;
        }
        else if (streamInfo->format == YUV420_FORMAT)
        {
            swtch->yuvFormat = I420;
            swtch->splitInputBufferSize = streamInfo->width * streamInfo->height * 3 / 2;
        }
        else
        {
            ml_log_error("Video format not supported: %d\n", streamInfo->format);
        }

        for (i = 0; i < swtch->splitCount; i++)
        {
            MALLOC_OFAIL(swtch->splitInputBuffer[i], unsigned char, swtch->splitInputBufferSize);
            YUV_frame_from_buffer(&swtch->splitFrameIn[i], swtch->splitInputBuffer[i],
                streamInfo->width, streamInfo->height, swtch->yuvFormat);
        }

        /* initialise the split work buffers */
        get_output_stream_info(swtch, streamInfo, &outputStreamInfo);
        swtch->splitOutputWidth = outputStreamInfo.width;
        swtch->splitOutputHeight = outputStreamInfo.height;

        if (streamInfo->format == UYVY_FORMAT)
        {
            swtch->splitOutputBufferSize = swtch->splitOutputWidth * swtch->splitOutputHeight * 2;
        }
        else if (streamInfo->format == YUV422_FORMAT)
        {
            swtch->splitOutputBufferSize = swtch->splitOutputWidth * swtch->splitOutputHeight * 2;
        }
        else /* streamInfo->format == YUV420_FORMAT */
        {
            swtch->splitOutputBufferSize = swtch->splitOutputWidth * swtch->splitOutputHeight * 3 / 2;
        }

        for (i = 0; i < swtch->splitCount; i++)
        {
            MALLOC_OFAIL(swtch->splitWorkspace[i], unsigned char, swtch->splitOutputWidth * 3);
        }


        /* border widths */
        swtch->borderWidth = (swtch->splitOutputWidth / 500) * 4;
        swtch->borderWidth = (swtch->borderWidth == 0) ? 4 : swtch->borderWidth;
        swtch->borderHeight = (swtch->splitOutputHeight / 500) * 4;
        swtch->borderHeight = (swtch->borderHeight == 0) ? 4 : swtch->borderHeight;


        /* split stream is the list root */
        swtch->splitStream = &swtch->streams;
        swtch->firstInputStream = newEle;
        if (swtch->splitSelect)
        {
            swtch->currentStream = swtch->firstInputStream;
        }
        else
        {
            swtch->currentStream = &swtch->streams;
        }
        swtch->nextCurrentStream = swtch->currentStream;
    }
    ele->next = newEle;

    assert(swtch->firstInputStream != NULL);

    return 1;

fail:
    SAFE_FREE(&newEle);
    return 0;
}

static int is_switchable_stream(DefaultVideoSwitch* swtch, int streamId)
{
    VideoStreamElement* ele = swtch->firstInputStream;
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

static int is_switchable_event_stream(DefaultVideoSwitch* swtch, int streamId)
{
    VideoStreamElement* ele = swtch->firstInputStream;
    while (ele != NULL)
    {
        if (ele->sourceNameEventStreamIndex == streamId)
        {
            return 1;
        }
        ele = ele->next;
    }
    return 0;
}

/* returns 1..splitCount, or 0 if it is not part of the split */
static int get_split_index(DefaultVideoSwitch* swtch, int streamId)
{
    VideoStreamElement* ele = swtch->firstInputStream;
    while (ele != NULL)
    {
        if (ele->streamId == streamId)
        {
            if (ele->index >= 1 && ele->index <= swtch->splitCount)
            {
                return ele->index;
            }
            else
            {
                /* not part of the split */
                return 0;
            }

        }
        ele = ele->next;
    }
    return 0;
}



static void qvs_frame_displayed(void* data, const FrameInfo* frameInfo)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    msl_frame_displayed(swtch->switchListener, frameInfo);
}

static void qvs_frame_dropped(void* data, const FrameInfo* lastFrameInfo)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    msl_frame_dropped(swtch->switchListener, lastFrameInfo);
}

static void qvs_refresh_required(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    msl_refresh_required(swtch->switchListener);
}



static int qvs_register_listener(void* data, MediaSinkListener* listener)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    swtch->switchListener = listener;

    return msk_register_listener(swtch->targetSink, &swtch->targetSinkListener);
}

static void qvs_unregister_listener(void* data, MediaSinkListener* listener)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    if (swtch->switchListener == listener)
    {
        swtch->switchListener = NULL;
    }

    msk_unregister_listener(swtch->targetSink, &swtch->targetSinkListener);
}

static int qvs_accept_stream(void* data, const StreamInfo* streamInfo)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    StreamInfo outputStreamInfo;

    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        (streamInfo->format == UYVY_FORMAT ||
            streamInfo->format == YUV422_FORMAT ||
            streamInfo->format == YUV420_FORMAT))
    {
        get_output_stream_info(swtch, streamInfo, &outputStreamInfo);
        return msk_accept_stream(swtch->targetSink, &outputStreamInfo);
    }
    else if (streamInfo->type == EVENT_STREAM_TYPE &&
        streamInfo->format == SOURCE_EVENT_FORMAT)
    {
        return 1;
    }

    return msk_accept_stream(swtch->targetSink, streamInfo);
}

static int qvs_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    StreamInfo outputStreamInfo;

    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        (streamInfo->format == UYVY_FORMAT ||
            streamInfo->format == YUV422_FORMAT ||
            streamInfo->format == YUV420_FORMAT))
    {
        if (swtch->firstInputStream == NULL)
        {
            get_output_stream_info(swtch, streamInfo, &outputStreamInfo);

            /* if sink accepts this stream then it is the first input stream */
            if (msk_register_stream(swtch->targetSink, streamId, &outputStreamInfo))
            {
                return add_stream(swtch, streamId, streamInfo);
            }
        }
        else
        {
            /* if stream matches 1st stream then add to list */
            if (streamInfo->format == swtch->firstInputStream->streamInfo.format &&
                memcmp(&streamInfo->frameRate, &swtch->firstInputStream->streamInfo.frameRate, sizeof(Rational)) == 0 &&
                streamInfo->width == swtch->firstInputStream->streamInfo.width &&
                streamInfo->height == swtch->firstInputStream->streamInfo.height)
            {
                return add_stream(swtch, streamId, streamInfo);
            }
        }

        return 0;
    }
    else if (streamInfo->type == EVENT_STREAM_TYPE &&
        streamInfo->format == SOURCE_EVENT_FORMAT)
    {
        return add_event_stream(swtch, streamId, streamInfo);
    }


    return msk_register_stream(swtch->targetSink, streamId, streamInfo);
}

static int qvs_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    if (is_switchable_stream(swtch, streamId))
    {
        if (!swtch->disableSwitching)
        {
            /* switch stream if required */
            PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
            if (swtch->currentStream != swtch->nextCurrentStream ||
                swtch->showSplitSelect != swtch->nextShowSplitSelect)
            {
                swtch->haveSwitched = 1;
                swtch->currentStream = swtch->nextCurrentStream;
                swtch->showSplitSelect = swtch->nextShowSplitSelect;
            }
            swtch->disableSwitching = 1;
            PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);
        }

        if (swtch->showSplitSelect || swtch->currentStream == swtch->splitStream)
        {
            if (!swtch->haveCheckedFirstInputStream)
            {
                swtch->haveAcceptedFirstInputStream = msk_accept_stream_frame(swtch->targetSink,
                    swtch->firstInputStream->streamId, frameInfo);
                swtch->haveCheckedFirstInputStream = 1;
            }
            return swtch->haveAcceptedFirstInputStream && get_split_index(swtch, streamId) != 0;
        }
        else
        {
            if (!swtch->haveCheckedFirstInputStream)
            {
                swtch->haveAcceptedFirstInputStream = msk_accept_stream_frame(swtch->targetSink,
                    swtch->firstInputStream->streamId, frameInfo);
                swtch->haveCheckedFirstInputStream = 1;
            }
            return swtch->haveAcceptedFirstInputStream && swtch->currentStream->streamId == streamId;
        }
    }
    else if (is_switchable_event_stream(swtch, streamId))
    {
        return 1;
    }
    else
    {
        return msk_accept_stream_frame(swtch->targetSink, streamId, frameInfo);
    }
}

static int qvs_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    int splitIndex;

    if (is_switchable_stream(swtch, streamId))
    {
        if (swtch->showSplitSelect || swtch->currentStream == swtch->splitStream)
        {
            /* allocate the output buffer if we haven't done so already */
            if (!swtch->haveSplitOutputBuffer)
            {
                /* check the buffer size */
                if (swtch->splitInputBufferSize != bufferSize)
                {
                    fprintf(stderr, "Requested buffer size (%d) != split buffer size (%d)\n",
                        bufferSize, swtch->splitInputBufferSize);
                    return 0;
                }

                CHK_ORET(msk_get_stream_buffer(swtch->targetSink, swtch->firstInputStream->streamId,
                    swtch->splitOutputBufferSize, &swtch->splitOutputBuffer));

                /* initialise the YUV_lib frame for the sink buffer */
                YUV_frame_from_buffer(&swtch->splitFrameOut, swtch->splitOutputBuffer,
                    swtch->splitOutputWidth, swtch->splitOutputHeight, swtch->yuvFormat);

                /* always clear the frame, eg. to prevent video from non-split showing through
                after a switch or a disabled OSD */
                clear_YUV_frame(&swtch->splitFrameOut);

                swtch->haveSplitOutputBuffer = 1;
            }

            /* set the input buffer */
            splitIndex = get_split_index(swtch, streamId);
            if (splitIndex < 1 || splitIndex > swtch->splitCount)
            {
                /* shouldn't be here if accept_stream_frame was called */
                assert(0);
                return 0;
            }

            *buffer = swtch->splitInputBuffer[splitIndex - 1];
            return 1;
        }
        else if (swtch->currentStream->streamId == streamId)
        {
            if (swtch->prescaledSplit)
            {
                CHK_ORET(msk_get_stream_buffer(swtch->targetSink, swtch->firstInputStream->streamId, swtch->splitOutputBufferSize, &swtch->splitOutputBuffer));

                /* always clear the output frame, eg. to prevent video from non-split showing through
                after a switch or a disabled OSD */
                fill_black(swtch->firstInputStream->streamInfo.format, swtch->splitOutputWidth, swtch->splitOutputHeight, swtch->splitOutputBuffer);

                *buffer = swtch->splitInputBuffer[0];
                return 1;
            }
            else
            {
                return msk_get_stream_buffer(swtch->targetSink, swtch->firstInputStream->streamId, bufferSize, buffer);
            }
        }

        /* shouldn't be here if qvs_accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else if (is_switchable_event_stream(swtch, streamId))
    {
        if (swtch->eventBufferSize < bufferSize)
        {
            SAFE_FREE(&swtch->eventBuffer);

            CALLOC_ORET(swtch->eventBuffer, unsigned char, bufferSize);
            swtch->eventBufferSize = bufferSize;
        }
        *buffer = swtch->eventBuffer;
        return 1;
    }
    else
    {
        return msk_get_stream_buffer(swtch->targetSink, streamId, bufferSize, buffer);
    }
}

static int qvs_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    int splitIndex;
    int hPos, vPos;

    if (is_switchable_stream(swtch, streamId))
    {
        if (swtch->showSplitSelect || swtch->currentStream == swtch->splitStream)
        {
            /* check the buffer size */
            if (swtch->splitInputBufferSize != bufferSize)
            {
                ml_log_error("Buffer size (%d) != split data size (%d)\n", bufferSize, swtch->splitInputBufferSize);
                return 0;
            }

            /* do split */
            splitIndex = get_split_index(swtch, streamId);
            if (splitIndex < 1 || splitIndex > swtch->splitCount)
            {
                /* shouldn't be here if accept_stream_frame was called */
                assert(0);
                return 0;
            }
            switch (swtch->videoSwitchSplit)
            {
                case QUAD_SPLIT_VIDEO_SWITCH:
                    if (swtch->prescaledSplit)
                    {
                        hPos = (splitIndex == 1 || splitIndex == 3) ? 0 : swtch->splitOutputWidth / 2;
                        vPos = (splitIndex == 1 || splitIndex == 2) ? 0 : swtch->splitOutputHeight / 2;

                        fit_image(swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                            swtch->splitOutputWidth, swtch->splitOutputHeight,
                            hPos, vPos,
                            swtch->firstInputStream->streamInfo.format,
                            swtch->splitInputBuffer[splitIndex - 1], swtch->splitOutputBuffer);
                    }
                    else
                    {
                        hPos = (splitIndex == 1 || splitIndex == 3) ? 0 : swtch->firstInputStream->streamInfo.width / 2;
                        vPos = (splitIndex == 1 || splitIndex == 2) ? 0 : swtch->firstInputStream->streamInfo.height / 2;

                        small_pic(&swtch->splitFrameIn[splitIndex - 1],
                            &swtch->splitFrameOut,
                            hPos,
                            vPos,
                            2,
                            2,
                            1, /* TODO: don't hardcode interlaces flag */
                            swtch->applySplitFilter,
                            swtch->applySplitFilter,
                            swtch->splitWorkspace[splitIndex - 1]);
                    }
                    break;

                case NONA_SPLIT_VIDEO_SWITCH:
                    if (swtch->prescaledSplit)
                    {
                        hPos = ((splitIndex - 1) % 3) * swtch->splitOutputWidth / 3;
                        vPos = ((splitIndex - 1) / 3) * swtch->splitOutputHeight / 3;

                        fit_image(swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                            swtch->splitOutputWidth, swtch->splitOutputHeight,
                            hPos, vPos,
                            swtch->firstInputStream->streamInfo.format,
                            swtch->splitInputBuffer[splitIndex - 1], swtch->splitOutputBuffer);
                    }
                    else
                    {
                        hPos = ((splitIndex - 1) % 3) * swtch->firstInputStream->streamInfo.width / 3;
                        vPos = ((splitIndex - 1) / 3) * swtch->firstInputStream->streamInfo.height / 3;

                        small_pic(&swtch->splitFrameIn[splitIndex - 1],
                            &swtch->splitFrameOut,
                            hPos,
                            vPos,
                            3,
                            3,
                            1, /* TODO: don't hardcode interlaces flag */
                            swtch->applySplitFilter,
                            swtch->applySplitFilter,
                            swtch->splitWorkspace[splitIndex - 1]);
                    }
                    break;

                default:
                    assert(0);
                    return 0;
            }

            /* we don't send the quad split until complete_frame is called */
            return 1;
        }
        else if (swtch->currentStream->streamId == streamId)
        {
            if (swtch->prescaledSplit)
            {
                hPos = (swtch->splitOutputWidth - swtch->firstInputStream->streamInfo.width) / 2;
                vPos = (swtch->splitOutputHeight - swtch->firstInputStream->streamInfo.height) / 2;

                fit_image(swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                    swtch->splitOutputWidth, swtch->splitOutputHeight,
                    hPos, vPos,
                    swtch->firstInputStream->streamInfo.format,
                    buffer, swtch->splitOutputBuffer);

                return msk_receive_stream_frame(swtch->targetSink, swtch->firstInputStream->streamId, swtch->splitOutputBuffer, swtch->splitOutputBufferSize);
            }
            else
            {
                return msk_receive_stream_frame(swtch->targetSink, swtch->firstInputStream->streamId, buffer, bufferSize);
            }
        }

        /* shouldn't be here if accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else if (is_switchable_event_stream(swtch, streamId))
    {
        process_event(swtch, streamId, buffer);
        return 1;
    }
    else
    {
        return msk_receive_stream_frame(swtch->targetSink, streamId, buffer, bufferSize);
    }
}

static int qvs_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    YUV_frame inputFrame;
    int splitIndex;
    int hPos, vPos;

    if (is_switchable_stream(swtch, streamId))
    {
        if (swtch->showSplitSelect || swtch->currentStream == swtch->splitStream)
        {
            /* check the buffer size */
            if (swtch->splitInputBufferSize != bufferSize)
            {
                ml_log_error("Buffer size (%d) != split data size (%d)\n", bufferSize, swtch->splitInputBufferSize);
                return 0;
            }

            /* allocate the output buffer if we haven't done so already */
            if (!swtch->haveSplitOutputBuffer)
            {
                CHK_ORET(msk_get_stream_buffer(swtch->targetSink, swtch->firstInputStream->streamId,
                    bufferSize, &swtch->splitOutputBuffer));

                /* initialise the YUV_lib frame for the sink buffer */
                YUV_frame_from_buffer(&swtch->splitFrameOut, swtch->splitOutputBuffer,
                    swtch->splitOutputWidth, swtch->splitOutputHeight, swtch->yuvFormat);

                /* always clear the frame, eg. to prevent video from non-quad showing through
                after a switch or a disabled OSD */
                clear_YUV_frame(&swtch->splitFrameOut);

                swtch->haveSplitOutputBuffer = 1;
            }

            /* initialise the input YUV frame. We know small_pic will not modify the buffer,
            so we can cast buffer to non-const */
            YUV_frame_from_buffer(&inputFrame, (void*)buffer, swtch->firstInputStream->streamInfo.width,
                swtch->firstInputStream->streamInfo.height, swtch->yuvFormat);

            /* do split */
            splitIndex = get_split_index(swtch, streamId);
            if (splitIndex < 1 || splitIndex > swtch->splitCount)
            {
                /* shouldn't be here if accept_stream_frame was called */
                assert(0);
                return 0;
            }
            switch (swtch->videoSwitchSplit)
            {
                case QUAD_SPLIT_VIDEO_SWITCH:
                    if (swtch->prescaledSplit)
                    {
                        hPos = (splitIndex == 1 || splitIndex == 3) ? 0 : swtch->splitOutputWidth / 2;
                        vPos = (splitIndex == 1 || splitIndex == 2) ? 0 : swtch->splitOutputHeight / 2;

                        fit_image(swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                            swtch->splitOutputWidth, swtch->splitOutputHeight,
                            hPos, vPos,
                            swtch->firstInputStream->streamInfo.format,
                            buffer, swtch->splitOutputBuffer);
                    }
                    else
                    {
                        hPos = (splitIndex == 1 || splitIndex == 3) ? 0 : swtch->firstInputStream->streamInfo.width / 2;
                        vPos = (splitIndex == 1 || splitIndex == 2) ? 0 : swtch->firstInputStream->streamInfo.height / 2;

                        small_pic(&inputFrame,
                            &swtch->splitFrameOut,
                            hPos,
                            vPos,
                            2,
                            2,
                            1, /* TODO: don't hardcode interlaced flag */
                            swtch->applySplitFilter,
                            swtch->applySplitFilter,
                            swtch->splitWorkspace[splitIndex - 1]);
                    }
                    break;

                case NONA_SPLIT_VIDEO_SWITCH:
                    if (swtch->prescaledSplit)
                    {
                        hPos = ((splitIndex - 1) % 3) * swtch->splitOutputWidth / 3;
                        vPos = ((splitIndex - 1) / 3) * swtch->splitOutputHeight / 3;

                        fit_image(swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                            swtch->splitOutputWidth, swtch->splitOutputHeight,
                            hPos, vPos,
                            swtch->firstInputStream->streamInfo.format,
                            buffer, swtch->splitOutputBuffer);
                    }
                    else
                    {
                        hPos = ((splitIndex - 1) % 3) * swtch->firstInputStream->streamInfo.width / 3;
                        vPos = ((splitIndex - 1) / 3) * swtch->firstInputStream->streamInfo.height / 3;

                        small_pic(&inputFrame,
                            &swtch->splitFrameOut,
                            hPos,
                            vPos,
                            3,
                            3,
                            1, /* TODO: don't hardcode interlaced flag */
                            swtch->applySplitFilter,
                            swtch->applySplitFilter,
                            swtch->splitWorkspace[splitIndex - 1]);
                    }
                    break;

                default:
                    assert(0);
                    return 0;
            }

            /* we don't send the quad split until complete_frame is called */
            return 1;
        }
        else if (swtch->currentStream->streamId == streamId)
        {
            if (swtch->prescaledSplit)
            {
                hPos = (swtch->splitOutputWidth - swtch->firstInputStream->streamInfo.width) / 2;
                vPos = (swtch->splitOutputHeight - swtch->firstInputStream->streamInfo.height) / 2;

                fit_image(swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                    swtch->splitOutputWidth, swtch->splitOutputHeight,
                    hPos, vPos,
                    swtch->firstInputStream->streamInfo.format,
                    buffer, swtch->splitOutputBuffer);

                return msk_receive_stream_frame_const(swtch->targetSink, swtch->firstInputStream->streamId, swtch->splitOutputBuffer, swtch->splitOutputBufferSize);
            }
            else
            {
                return msk_receive_stream_frame_const(swtch->targetSink, swtch->firstInputStream->streamId, buffer, bufferSize);
            }
        }

        /* shouldn't be here if accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else if (is_switchable_event_stream(swtch, streamId))
    {
        process_event(swtch, streamId, buffer);
        return 1;
    }
    else
    {
        return msk_receive_stream_frame_const(swtch->targetSink, streamId, buffer, bufferSize);
    }
}

static int qvs_complete_frame(void* data, const FrameInfo* frameInfo)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    Timecode timecode;
    int hPos, vPos;
    int xPos, yPos;
    int w, h;
    VideoStreamElement* ele;
    float fontScale;
    char labelText[32];


    if (swtch->firstInputStream != NULL)  /* only if there is a video stream */
    {
        /* send updates to database */
        if (swtch->database != NULL)
        {
            if ((swtch->haveSwitched ||
                    strcmp(swtch->currentSourceName, swtch->currentStream->sourceName) != 0) &&
                select_frame_timecode(frameInfo, swtch->masterTimecodeIndex, swtch->masterTimecodeType,
                    swtch->masterTimecodeSubType, &timecode))
            {
                vsd_append_entry(swtch->database, swtch->currentStream->streamId,
                    swtch->currentStream->sourceName, &timecode);

                strncpy(swtch->currentSourceName, swtch->currentStream->sourceName, sizeof(swtch->currentSourceName) - 1);
            }
        }

        /* resets */
        swtch->haveCheckedFirstInputStream = 0;
        swtch->haveAcceptedFirstInputStream = 0;
        swtch->haveSplitOutputBuffer = 0;
        swtch->disableSwitching = 0;
        swtch->haveSwitched = 0;

        /* send the split buffer */
        if (swtch->showSplitSelect || swtch->currentStream == swtch->splitStream)
        {
            if (swtch->showSplitSelect)
            {
                /* clear frame but also highlight selected stream with green border */
                set_split_select_background(swtch);
            }

            if (!msk_receive_stream_frame(swtch->targetSink, swtch->firstInputStream->streamId,
                swtch->splitOutputBuffer, swtch->splitOutputBufferSize))
            {
                ml_log_error("Failed to send video switch split to sink\n");
                return 0;
            }
        }

        /* set source name label(s) */
        if (swtch->showSourceName)
        {
            if (swtch->showSplitSelect || swtch->currentStream == swtch->splitStream)
            {
                ele = swtch->firstInputStream;
                while (ele != NULL)
                {
                    if (strlen(ele->sourceName) == 0)
                    {
                        ele = ele->next;
                        continue;
                    }

                    /* max length is 32 characters */
                    strncpy(labelText, ele->sourceName, sizeof(labelText) - 1);
                    labelText[sizeof(labelText) - 1] = '\0';


                    switch (swtch->videoSwitchSplit)
                    {
                        case QUAD_SPLIT_VIDEO_SWITCH:
                            hPos = (ele->index == 1 || ele->index == 3) ? 0 : swtch->firstInputStream->streamInfo.width / 2;
                            vPos = (ele->index == 1 || ele->index == 2) ? 0 : swtch->firstInputStream->streamInfo.height / 2;
                            w = swtch->firstInputStream->streamInfo.width / 2;
                            h = swtch->firstInputStream->streamInfo.height / 2;
                            fontScale = swtch->firstInputStream->streamInfo.height / 576.0 / 2.0;
                            break;

                        case NONA_SPLIT_VIDEO_SWITCH:
                            hPos = ((ele->index - 1) % 3) * swtch->firstInputStream->streamInfo.width / 3;
                            vPos = ((ele->index - 1) / 3) * swtch->firstInputStream->streamInfo.height / 3;
                            w = swtch->firstInputStream->streamInfo.width / 3;
                            h = swtch->firstInputStream->streamInfo.height / 3;
                            fontScale = swtch->firstInputStream->streamInfo.height / 576.0 / 3.0;
                            break;

                        default:
                            assert(0);
                            return 0;
                    }

                    xPos = hPos + w / 2;
                    yPos = vPos + h * 19 / 20;
                    if (yPos + 32 > vPos + h)
                    {
                        yPos = vPos + h - 32;
                    }
                    osd_set_label(msk_get_osd(swtch->targetSink), xPos, yPos,
                        swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                        48 * fontScale, LIGHT_WHITE_COLOUR, 40, labelText);

                    ele = ele->next;
                }

            }
            else if (swtch->currentStream != NULL)
            {
                /* max length is 32 characters */
                strncpy(labelText, swtch->currentStream->sourceName, sizeof(labelText) - 1);
                labelText[sizeof(labelText) - 1] = '\0';

                hPos = 0;
                vPos = 0;
                w = swtch->currentStream->streamInfo.width;
                h = swtch->currentStream->streamInfo.height;

                fontScale = swtch->currentStream->streamInfo.height / 576.0;

                xPos = hPos + w / 2;
                yPos = vPos + h * 19 / 20;
                if (yPos + 32 > vPos + h)
                {
                    yPos = vPos + h - 32;
                }
                osd_set_label(msk_get_osd(swtch->targetSink), xPos, yPos,
                    swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height,
                    48 * fontScale, LIGHT_WHITE_COLOUR, 40, labelText);
            }
        }
    }

    return msk_complete_frame(swtch->targetSink, frameInfo);
}

static void qvs_cancel_frame(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    if (swtch->firstInputStream != NULL) /* only if there is a video stream */
    {
        /* resets */
        swtch->haveCheckedFirstInputStream = 0;
        swtch->haveAcceptedFirstInputStream = 0;
        swtch->haveSplitOutputBuffer = 0;
        swtch->disableSwitching = 0;
        swtch->haveSwitched = 0;
    }

    msk_cancel_frame(swtch->targetSink);
}

static OnScreenDisplay* qvs_get_osd(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return msk_get_osd(swtch->targetSink);
}

static VideoSwitchSink* qvs_get_video_switch(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return &swtch->switchSink;
}

static AudioSwitchSink* qvs_get_audio_switch(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return msk_get_audio_switch(swtch->targetSink);
}

static HalfSplitSink* qvs_get_half_split(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return msk_get_half_split(swtch->targetSink);
}

static FrameSequenceSink* qvs_get_frame_sequence(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return msk_get_frame_sequence(swtch->targetSink);
}

static int qvs_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return msk_get_buffer_state(swtch->targetSink, numBuffers, numBuffersFilled);
}

static int qvs_mute_audio(void* data, int mute)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return msk_mute_audio(swtch->targetSink, mute);
}

static void qvs_close(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    VideoStreamElement* ele;
    VideoStreamElement* nextEle;
    int i;

    if (data == NULL)
    {
        return;
    }

    msk_close(swtch->targetSink);

    ele = swtch->streams.next;
    while (ele != NULL)
    {
        nextEle = ele->next;
        SAFE_FREE(&ele);
        ele = nextEle;
    }

    for (i = 0; i < MAX_SPLITS; i++)
    {
        SAFE_FREE(&swtch->splitInputBuffer[i]);
        SAFE_FREE(&swtch->splitWorkspace[i]);
    }

    destroy_mutex(&swtch->nextCurrentStreamMutex);

    SAFE_FREE(&swtch->eventBuffer);

    SAFE_FREE(&swtch);
}

static int qvs_reset_or_close(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    VideoStreamElement* ele;
    VideoStreamElement* nextEle;
    int result;
    int i;

    ele = swtch->streams.next;
    while (ele != NULL)
    {
        nextEle = ele->next;
        SAFE_FREE(&ele);
        ele = nextEle;
    }
    memset(&swtch->streams, 0, sizeof(VideoStreamElement));

    swtch->firstInputStream = NULL;
    swtch->splitStream = NULL;
    swtch->currentStream = NULL;

    swtch->nextCurrentStream = NULL;
    swtch->disableSwitching = 0;
    swtch->haveSwitched = 0;

    for (i = 0; i < MAX_SPLITS; i++)
    {
        SAFE_FREE(&swtch->splitInputBuffer[i]);
        SAFE_FREE(&swtch->splitWorkspace[i]);
        memset(&swtch->splitFrameIn[i], 0, sizeof(YUV_frame));
    }
    swtch->splitOutputBuffer = NULL;

    swtch->haveCheckedFirstInputStream = 0;
    swtch->haveAcceptedFirstInputStream = 0;
    swtch->haveSplitOutputBuffer = 0;

    result = msk_reset_or_close(swtch->targetSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            swtch->targetSink = NULL;
        }
        goto fail;
    }

    return 1;

fail:
    qvs_close(data);
    return 2;
}


static MediaSink* qvs_get_media_sink(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    return &swtch->sink;
}

static int qvs_switch_next_video(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    int haveSwitched = 0;

    if (swtch->nextCurrentStream == NULL)
    {
        return 0;
    }

    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
    if (swtch->nextCurrentStream->next != NULL &&
        (!(swtch->splitSelect && swtch->showSplitSelect) ||
            swtch->nextCurrentStream->next->index <= swtch->splitCount))
    {
        swtch->nextCurrentStream = swtch->nextCurrentStream->next;
        swtch->nextShowSplitSelect = swtch->showSplitSelect;
        haveSwitched = 1;
    }
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);

    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }

    return haveSwitched;
}

static int qvs_switch_prev_video(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    int haveSwitched = 0;

    if (swtch->nextCurrentStream == NULL)
    {
        return 0;
    }

    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
    if (swtch->nextCurrentStream->prev != NULL &&
        swtch->nextCurrentStream != swtch->firstInputStream)
    {
        swtch->nextCurrentStream = swtch->nextCurrentStream->prev;
        swtch->nextShowSplitSelect = swtch->showSplitSelect;
        haveSwitched = 1;
    }
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);

    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }

    return haveSwitched;
}

static int qvs_switch_video(void* data, int index)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    VideoStreamElement* ele;
    int haveSwitched = 0;

    if (swtch->nextCurrentStream == NULL || index < 0)
    {
        return 0;
    }
    if (swtch->splitSelect && swtch->showSplitSelect && index > swtch->splitCount)
    {
        /* currently showing the split select and the stream is beyond the streams shown in the split select */
        return 0;
    }

    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
    if (index == 0 && swtch->splitSelect)
    {
        /* toggle show split */
        swtch->nextShowSplitSelect = !swtch->showSplitSelect;
        if (swtch->nextShowSplitSelect && swtch->nextCurrentStream->index > swtch->splitCount)
        {
            /* switch to the last split select stream if the current stream is beyond the split count */
            while (swtch->nextCurrentStream->index > swtch->splitCount)
            {
                swtch->nextCurrentStream = swtch->nextCurrentStream->prev;
            }
        }
        haveSwitched = 1;
    }
    else
    {
        /* show stream with given index */
        ele = &swtch->streams;
        while (ele != NULL)
        {
            if (index == ele->index)
            {
                swtch->nextCurrentStream = ele;
                swtch->nextShowSplitSelect = swtch->showSplitSelect;
                haveSwitched = 1;
                break;
            }
            ele = ele->next;
        }
    }
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);

    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }

    return haveSwitched;
}

static void qvs_show_source_name(void* data, int enable)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    if (swtch->showSourceName != enable)
    {
        swtch->showSourceName = enable;
        msl_refresh_required(swtch->switchListener);
    }
}

static void qvs_toggle_show_source_name(void* data)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;

    swtch->showSourceName = !swtch->showSourceName;

    msl_refresh_required(swtch->switchListener);
}

static int qvs_get_video_index(void* data, int imageWidth, int imageHeight, int xPos, int yPos, int* index)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    int subImageWidth;
    int subImageHeight;

    /* if showing video from a single source then return the current source index */
    if (swtch->videoSwitchSplit == NO_SPLIT_VIDEO_SWITCH ||
        (!swtch->showSplitSelect && swtch->currentStream != swtch->splitStream))
    {
        *index = swtch->currentStream->index;
        return 1;
    }

    /* return the current stream index if the position is outside the image */
    if (xPos >= imageWidth || yPos >= imageHeight)
    {
        *index = swtch->currentStream->index;
        return 1;
    }

    /* return the index of the source showing at xPos and yPos */
    switch (swtch->videoSwitchSplit)
    {
        case QUAD_SPLIT_VIDEO_SWITCH:
            subImageWidth = imageWidth / 2;
            subImageHeight = imageHeight / 2;

            *index = 1 /* 0 is the split index */ + (yPos / subImageHeight) * 2 + (xPos / subImageWidth);
            break;
        case NONA_SPLIT_VIDEO_SWITCH:
            subImageWidth = imageWidth / 3;
            subImageHeight = imageHeight / 3;

            *index = 1 /* 0 is the split index */ + (yPos / subImageHeight) * 3 + (xPos / subImageWidth);
            break;
        default:
            /* shouldn't be here */
            *index = swtch->currentStream->index;
            break;
    }

    return 1;
}

static int qvs_get_first_active_clip_id(void* data, char* clipId, int* sourceId)
{
    DefaultVideoSwitch* swtch = (DefaultVideoSwitch*)data;
    int result = 0;
    int showSplitSelect;
    VideoStreamElement* currentStream;

    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);

    if (!swtch->disableSwitching)
    {
        /* Note: it is still possible that next... changes after this function call and
        before the switch is made, possibly resulting in the wrong active clip id returned */
        currentStream = swtch->nextCurrentStream;
        showSplitSelect = swtch->nextShowSplitSelect;
    }
    else
    {
        /* any switches have already been made */
        currentStream = swtch->currentStream;
        showSplitSelect = swtch->showSplitSelect;
    }

    if (currentStream != NULL)
    {
        if (!showSplitSelect && currentStream == swtch->splitStream)
        {
            /* the active clip is the clip associated with the first video stream in the split view */
            strcpy(clipId, swtch->firstInputStream->streamInfo.clipId);
            *sourceId = swtch->firstInputStream->streamInfo.sourceId;
            result = 1;
        }
        else
        {
            /* the active clip is the clip associated with the current video stream */
            strcpy(clipId, currentStream->streamInfo.clipId);
            *sourceId = currentStream->streamInfo.sourceId;
            result = 1;
        }
    }

    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);

    return result;
}


int qvs_create_video_switch(MediaSink* sink, VideoSwitchSplit split, int applySplitFilter, int splitSelect, int prescaledSplit,
    VideoSwitchDatabase* database, int masterTimecodeIndex, int masterTimecodeType, int masterTimecodeSubType,
    VideoSwitchSink** swtch)
{
    DefaultVideoSwitch* newSwitch;

    CALLOC_ORET(newSwitch, DefaultVideoSwitch, 1);

    newSwitch->showSourceName = 0;
    newSwitch->showSplitSelect = splitSelect;
    newSwitch->nextShowSplitSelect = splitSelect;
    newSwitch->prescaledSplit = prescaledSplit;

    newSwitch->videoSwitchSplit = split;
    switch (split)
    {
        case NO_SPLIT_VIDEO_SWITCH:
            newSwitch->splitCount = 1;
            break;
        case QUAD_SPLIT_VIDEO_SWITCH:
            newSwitch->splitCount = 4;
            break;
        case NONA_SPLIT_VIDEO_SWITCH:
            newSwitch->splitCount = 9;
            break;
        default:
            assert(0);
            ml_log_error("Unknown video switch split %d, in %s:%d\n", (int)split, __FILE__, __LINE__); \
            goto fail;
    }
    newSwitch->applySplitFilter = applySplitFilter;
    newSwitch->splitSelect = splitSelect;
    newSwitch->database = database;
    newSwitch->masterTimecodeIndex = masterTimecodeIndex;
    newSwitch->masterTimecodeType = masterTimecodeType;
    newSwitch->masterTimecodeSubType = masterTimecodeSubType;

    newSwitch->targetSink = sink;

    newSwitch->haveSwitched = 1; /* force clear of quad split frame */

    newSwitch->switchSink.data = newSwitch;
    newSwitch->switchSink.get_media_sink = qvs_get_media_sink;
    newSwitch->switchSink.switch_next_video = qvs_switch_next_video;
    newSwitch->switchSink.switch_prev_video = qvs_switch_prev_video;
    newSwitch->switchSink.switch_video = qvs_switch_video;
    newSwitch->switchSink.show_source_name = qvs_show_source_name;
    newSwitch->switchSink.toggle_show_source_name = qvs_toggle_show_source_name;
    newSwitch->switchSink.get_video_index = qvs_get_video_index;
    newSwitch->switchSink.get_first_active_clip_id = qvs_get_first_active_clip_id;

    newSwitch->targetSinkListener.data = newSwitch;
    newSwitch->targetSinkListener.frame_displayed = qvs_frame_displayed;
    newSwitch->targetSinkListener.frame_dropped = qvs_frame_dropped;
    newSwitch->targetSinkListener.refresh_required = qvs_refresh_required;

    newSwitch->sink.data = newSwitch;
    newSwitch->sink.register_listener = qvs_register_listener;
    newSwitch->sink.unregister_listener = qvs_unregister_listener;
    newSwitch->sink.accept_stream = qvs_accept_stream;
    newSwitch->sink.register_stream = qvs_register_stream;
    newSwitch->sink.accept_stream_frame = qvs_accept_stream_frame;
    newSwitch->sink.get_stream_buffer = qvs_get_stream_buffer;
    newSwitch->sink.receive_stream_frame = qvs_receive_stream_frame;
    newSwitch->sink.receive_stream_frame_const = qvs_receive_stream_frame_const;
    newSwitch->sink.complete_frame = qvs_complete_frame;
    newSwitch->sink.cancel_frame = qvs_cancel_frame;
    newSwitch->sink.get_osd = qvs_get_osd;
    newSwitch->sink.get_video_switch = qvs_get_video_switch;
    newSwitch->sink.get_audio_switch = qvs_get_audio_switch;
    newSwitch->sink.get_half_split = qvs_get_half_split;
    newSwitch->sink.get_frame_sequence = qvs_get_frame_sequence;
    newSwitch->sink.get_buffer_state = qvs_get_buffer_state;
    newSwitch->sink.mute_audio = qvs_mute_audio;
    newSwitch->sink.reset_or_close = qvs_reset_or_close;
    newSwitch->sink.close = qvs_close;

    CHK_OFAIL(init_mutex(&newSwitch->nextCurrentStreamMutex));

    msc_init_stream_map(&newSwitch->eventStreamMap);


    *swtch = &newSwitch->switchSink;
    return 1;

fail:
    qvs_close(newSwitch);
    return 0;
}


MediaSink* vsw_get_media_sink(VideoSwitchSink* swtch)
{
    if (swtch && swtch->get_media_sink)
    {
        return swtch->get_media_sink(swtch->data);
    }
    return NULL;
}

int vsw_switch_next_video(VideoSwitchSink* swtch)
{
    if (swtch && swtch->switch_next_video)
    {
        return swtch->switch_next_video(swtch->data);
    }
    return 0;
}

int vsw_switch_prev_video(VideoSwitchSink* swtch)
{
    if (swtch && swtch->switch_prev_video)
    {
        return swtch->switch_prev_video(swtch->data);
    }
    return 0;
}

int vsw_switch_video(VideoSwitchSink* swtch, int index)
{
    if (swtch && swtch->switch_video)
    {
        return swtch->switch_video(swtch->data, index);
    }
    return 0;
}

void vsw_show_source_name(VideoSwitchSink* swtch, int enable)
{
    if (swtch && swtch->show_source_name)
    {
        swtch->show_source_name(swtch->data, enable);
    }
}

void vsw_toggle_show_source_name(VideoSwitchSink* swtch)
{
    if (swtch && swtch->toggle_show_source_name)
    {
        swtch->toggle_show_source_name(swtch->data);
    }
}

int vsw_get_video_index(VideoSwitchSink* swtch, int width, int height, int xPos, int yPos, int* index)
{
    if (swtch && swtch->get_video_index)
    {
        return swtch->get_video_index(swtch->data, width, height, xPos, yPos, index);
    }
    return 0;
}

int vsw_get_first_active_clip_id(VideoSwitchSink* swtch, char* clipId, int* sourceId)
{
    if (swtch && swtch->get_first_active_clip_id)
    {
        return swtch->get_first_active_clip_id(swtch->data, clipId, sourceId);
    }
    return 0;
}

