/*
 * $Id: frame_sequence_sink.c,v 1.5 2008/10/29 17:47:41 john_f Exp $
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
#include <assert.h>

#include "frame_sequence_sink.h"
#include "video_conversion.h"
#include "YUV_frame.h"
#include "YUV_small_pic.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


struct FrameSequenceSink
{
    int numImageQuads;
    int numImages;
    int imageScale;
    
    MediaSink* targetSink;
    
    MediaSink sink;
    
    /* targetSinkListener listens to target sink and forwards to the frame sequence sink listener */
    MediaSinkListener targetSinkListener;
    MediaSinkListener* switchListener;
    
    int streamId;
    StreamInfo streamInfo;
    
    int64_t previousPosition;
    
    unsigned char* inBuffer;
    unsigned int inBufferSize;
    int imageLineBytes;

    unsigned char** intermediateBuffers;
    unsigned char* workBuffer;
    
    int numBufferedImages;
    unsigned char** scaledImageBuffers;
    int64_t* positions;
    unsigned int scaledImageBufferSize;
    int scaledImageLineBytes;
    int scaledImageHeight;
    int positionInImageBuffers;

    unsigned char* outBuffer;
};



static void fss_frame_displayed(void* data, const FrameInfo* frameInfo)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    
    msl_frame_displayed(sequence->switchListener, frameInfo);
}

static void fss_frame_dropped(void* data, const FrameInfo* lastFrameInfo)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    msl_frame_dropped(sequence->switchListener, lastFrameInfo);
}

static void fss_refresh_required(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    msl_refresh_required(sequence->switchListener);
}



static int fss_register_listener(void* data, MediaSinkListener* listener)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    sequence->switchListener = listener;
    
    return msk_register_listener(sequence->targetSink, &sequence->targetSinkListener);
}

static void fss_unregister_listener(void* data, MediaSinkListener* listener)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    if (sequence->switchListener == listener)
    {
        sequence->switchListener = NULL;
    }
    
    msk_unregister_listener(sequence->targetSink, &sequence->targetSinkListener);
}

static int fss_accept_stream(void* data, const StreamInfo* streamInfo)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    int result;

    result = msk_accept_stream(sequence->targetSink, streamInfo);
    if (result && 
        streamInfo->type == PICTURE_STREAM_TYPE && 
        streamInfo->format != UYVY_FORMAT)
    {
        return 0;
    }
    return result;
}

static int fss_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    int i;
    int scale;

    if (sequence->streamId < 0 &&
        streamInfo->type == PICTURE_STREAM_TYPE && 
        streamInfo->format == UYVY_FORMAT)
    {
        if (msk_register_stream(sequence->targetSink, streamId, streamInfo))
        {
            sequence->streamId = streamId;
            sequence->streamInfo = *streamInfo;

            sequence->inBufferSize = streamInfo->width * streamInfo->height * 2;
            sequence->imageLineBytes = streamInfo->width * 2;
            
            CALLOC_ORET(sequence->inBuffer, unsigned char, sequence->inBufferSize);
            CALLOC_ORET(sequence->workBuffer, unsigned char, sequence->inBufferSize);
            
            CALLOC_ORET(sequence->intermediateBuffers, unsigned char*, sequence->numImageQuads);
            scale = 2;
            for (i = 0; i < sequence->numImageQuads; i++)
            {
                CALLOC_ORET(sequence->intermediateBuffers[i], unsigned char, sequence->inBufferSize / scale);
                scale *= 2;
            }
            
            CALLOC_ORET(sequence->scaledImageBuffers, unsigned char*, sequence->numBufferedImages);
            CALLOC_ORET(sequence->positions, int64_t, sequence->numBufferedImages);
            sequence->scaledImageLineBytes = streamInfo->width * 2 / sequence->imageScale;
            sequence->scaledImageHeight = streamInfo->height / sequence->imageScale;
            sequence->scaledImageBufferSize = sequence->scaledImageLineBytes *
                sequence->scaledImageHeight;
            for (i = 0; i < sequence->numBufferedImages; i++)
            {
                CALLOC_ORET(sequence->scaledImageBuffers[i], unsigned char, sequence->scaledImageBufferSize);
                sequence->positions[i] = -1;
            }
            
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return msk_register_stream(sequence->targetSink, streamId, streamInfo);
    }
}

static int fss_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    return msk_accept_stream_frame(sequence->targetSink, streamId, frameInfo);
}

static int fss_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    if (streamId == sequence->streamId)
    {
        if (bufferSize != sequence->inBufferSize)
        {
            ml_log_error("Buffer size (%d) != sequence data size (%d)\n", bufferSize, sequence->inBufferSize);
            return 0;
        }
        
        CHK_ORET(msk_get_stream_buffer(sequence->targetSink, streamId, bufferSize, &sequence->outBuffer));
        
        *buffer = sequence->inBuffer;
        return 1;
    }
    else
    {
        return msk_get_stream_buffer(sequence->targetSink, streamId, bufferSize, buffer);
    }
}

static int fss_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    unsigned char* srcBuffer = buffer;
    int i;
    YUV_frame intermediateFrame;
    YUV_frame srcFrame;
    int scale;
    

    if (streamId == sequence->streamId)
    {
        if (sequence->inBufferSize != bufferSize)
        {
            ml_log_error("Buffer size (%d) != sequence data size (%d)\n", bufferSize, sequence->inBufferSize);
            return 0;
        }
        
        scale = 2;
        for (i = 0; i < sequence->numImageQuads; i++)
        {
            YUV_frame_from_buffer(&srcFrame, srcBuffer, 
                sequence->streamInfo.width * 2 / scale, sequence->streamInfo.height * 2 / scale,
                UYVY);
        
            YUV_frame_from_buffer(&intermediateFrame, sequence->intermediateBuffers[i], 
                sequence->streamInfo.width / scale, sequence->streamInfo.height / scale, 
                UYVY);
        
            small_pic(&srcFrame, 
                &intermediateFrame,
                0, 
                0,
                2,
                2,
                1, 
                i == sequence->numImageQuads - 1, 
                i == sequence->numImageQuads - 1, 
                sequence->workBuffer);
                
            srcBuffer = sequence->intermediateBuffers[i];
            scale *= 2;
        }
        
        return 1;
    }
    else
    {
        return msk_receive_stream_frame(sequence->targetSink, streamId, buffer, bufferSize);
    }
}

static int fss_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    unsigned char* srcBuffer = (unsigned char*)buffer;
    int i;
    YUV_frame intermediateFrame;
    YUV_frame srcFrame;
    int scale;

    if (streamId == sequence->streamId)
    {
        if (sequence->inBufferSize != bufferSize)
        {
            ml_log_error("Buffer size (%d) != sequence data size (%d)\n", bufferSize, sequence->inBufferSize);
            return 0;
        }

        scale = 2;
        for (i = 0; i < sequence->numImageQuads; i++)
        {
            YUV_frame_from_buffer(&srcFrame, srcBuffer, 
                sequence->streamInfo.width * 2 / scale, sequence->streamInfo.height * 2 / scale, 
                UYVY);
        
            YUV_frame_from_buffer(&intermediateFrame, sequence->intermediateBuffers[i], 
                sequence->streamInfo.width / scale, sequence->streamInfo.height / scale, 
                UYVY);
        
            small_pic(&srcFrame, 
                &intermediateFrame,
                0, 
                0,
                2,
                2,
                1, 
                i == sequence->numImageQuads - 1, 
                i == sequence->numImageQuads - 1, 
                sequence->workBuffer);
                
            srcBuffer = sequence->intermediateBuffers[i];
            scale *= 2;
        }
        
        return 1;
    }
    else
    {
        return msk_receive_stream_frame_const(sequence->targetSink, streamId, buffer, bufferSize);
    }
}

static int fss_complete_frame(void* data, const FrameInfo* frameInfo)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    int i, j;
    int64_t imagePosition;
    unsigned char* inBufPtr;
    unsigned char* outBufPtr;
    int count;

    if (sequence->streamId >= 0)  /* only if there is a video stream */
    {
        /* fill output with black */
        fill_black(sequence->streamInfo.format, sequence->streamInfo.width, sequence->streamInfo.height,
            sequence->outBuffer);
        
        /* copy image into slot */
        memcpy(sequence->scaledImageBuffers[frameInfo->position % sequence->numBufferedImages],
            sequence->intermediateBuffers[sequence->numImageQuads - 1],
            sequence->scaledImageBufferSize);
        sequence->positions[frameInfo->position % sequence->numBufferedImages] = frameInfo->position;            
            

        /* set the position in the list of images for the current frame */
        if (sequence->previousPosition > frameInfo->position && 
            sequence->positionInImageBuffers > 0)
        {
            sequence->positionInImageBuffers--;
        }
        else if (sequence->previousPosition >= 0 &&
            sequence->previousPosition < frameInfo->position && 
            sequence->positionInImageBuffers < sequence->numImages - 1)
        {
            sequence->positionInImageBuffers++;
        }

        /* reset to the beginning if only 1 image is available */
        count = 0;
        for (i = 0; i < sequence->numImages; i++)
        {
            imagePosition = frameInfo->position - sequence->positionInImageBuffers + i;
            
            if (imagePosition >= 0)
            {
                if (sequence->positions[imagePosition % sequence->numBufferedImages] == imagePosition) 
                {
                    count++;
                }
            }
        }
        if (count <= 1)
        {
            sequence->positionInImageBuffers = 0;;
        }
        
        /* copy images to slots */
        for (i = 0; i < sequence->numImages; i++)
        {
            imagePosition = frameInfo->position - sequence->positionInImageBuffers + i;
            
            if (imagePosition >= 0)
            {
                if (sequence->positions[imagePosition % sequence->numBufferedImages] == imagePosition) 
                {
                    inBufPtr = sequence->scaledImageBuffers[imagePosition % sequence->numBufferedImages];
                    outBufPtr = sequence->outBuffer +  
                        (i % sequence->imageScale) * sequence->scaledImageLineBytes +
                        (i / sequence->imageScale) * sequence->imageLineBytes * sequence->scaledImageHeight;
                    for (j = 0; j < sequence->scaledImageHeight; j++)
                    {
                        memcpy(outBufPtr, inBufPtr, sequence->scaledImageLineBytes);
                        inBufPtr += sequence->scaledImageLineBytes;
                        outBufPtr += sequence->imageLineBytes;
                    }
                }
                /* else don't have image */
            }
            /* else don't have image */
        }
        
        sequence->previousPosition = frameInfo->position;
        
        /* send the sequence */
        if (!msk_receive_stream_frame(sequence->targetSink, sequence->streamId, sequence->outBuffer, sequence->inBufferSize))
        {
            ml_log_error("Failed to send sequence to sink\n");
            return 0;
        }
    }
    
    return msk_complete_frame(sequence->targetSink, frameInfo);
}

static void fss_cancel_frame(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    msk_cancel_frame(sequence->targetSink);
}

static OnScreenDisplay* fss_get_osd(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    return msk_get_osd(sequence->targetSink);
}
    
static VideoSwitchSink* fss_get_video_switch(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    return msk_get_video_switch(sequence->targetSink);
}
    
static AudioSwitchSink* fss_get_audio_switch(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    return msk_get_audio_switch(sequence->targetSink);
}
    
static HalfSplitSink* fss_get_half_split(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    return msk_get_half_split(sequence->targetSink);
}
    
static FrameSequenceSink* fss_get_frame_sequence(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    return sequence;
}
    
static int fss_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;

    return msk_get_buffer_state(sequence->targetSink, numBuffers, numBuffersFilled);
}
    
static void fss_close(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    int i;
    
    if (data == NULL)
    {
        return;
    }
    
    msk_close(sequence->targetSink);

    for (i = 0; i < sequence->numBufferedImages; i++)
    {
        SAFE_FREE(&sequence->scaledImageBuffers[i]);
    }
    SAFE_FREE(&sequence->scaledImageBuffers);
    SAFE_FREE(&sequence->positions);

    SAFE_FREE(&sequence->inBuffer);

    for (i = 0; i < sequence->numImageQuads; i++)
    {
        SAFE_FREE(&sequence->intermediateBuffers[i]);
    }    
    SAFE_FREE(&sequence->intermediateBuffers);
    SAFE_FREE(&sequence->workBuffer);
    
    SAFE_FREE(&sequence);
}

static int fss_reset_or_close(void* data)
{
    FrameSequenceSink* sequence = (FrameSequenceSink*)data;
    int i;
    int result;

    
    for (i = 0; i < sequence->numBufferedImages; i++)
    {
        SAFE_FREE(&sequence->scaledImageBuffers[i]);
    }
    SAFE_FREE(&sequence->scaledImageBuffers);
    SAFE_FREE(&sequence->positions);

    SAFE_FREE(&sequence->inBuffer);

    for (i = 0; i < sequence->numImageQuads; i++)
    {
        SAFE_FREE(&sequence->intermediateBuffers[i]);
    }    
    SAFE_FREE(&sequence->intermediateBuffers);
    SAFE_FREE(&sequence->workBuffer);
    
    sequence->streamId = -1;
    
    
    result = msk_reset_or_close(sequence->targetSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            sequence->targetSink = NULL;
        }
        goto fail;
    }
    
    return 1;
    
fail:
    fss_close(data);
    return 2;
}



int fss_create_frame_sequence(MediaSink* sink, FrameSequenceSink** sequence)
{
    FrameSequenceSink* newSequence;
    int i;
    
    CALLOC_ORET(newSequence, FrameSequenceSink, 1);
    
    newSequence->targetSink = sink;
    
    newSequence->numImageQuads = 2;
    newSequence->numImages = 1;
    newSequence->imageScale = 1;
    for (i = 0; i < newSequence->numImageQuads; i++)
    {
        newSequence->numImages *= 4;
        newSequence->imageScale *= 2;
    }
    newSequence->numBufferedImages = newSequence->numImages;
    newSequence->streamId = -1;
    newSequence->positionInImageBuffers = 0;
    newSequence->previousPosition = -1;

    newSequence->targetSinkListener.data = newSequence;
    newSequence->targetSinkListener.frame_displayed = fss_frame_displayed;
    newSequence->targetSinkListener.frame_dropped = fss_frame_dropped;
    newSequence->targetSinkListener.refresh_required = fss_refresh_required;
    
    newSequence->sink.data = newSequence;
    newSequence->sink.register_listener = fss_register_listener;
    newSequence->sink.unregister_listener = fss_unregister_listener;
    newSequence->sink.accept_stream = fss_accept_stream;
    newSequence->sink.register_stream = fss_register_stream;
    newSequence->sink.accept_stream_frame = fss_accept_stream_frame;
    newSequence->sink.get_stream_buffer = fss_get_stream_buffer;
    newSequence->sink.receive_stream_frame = fss_receive_stream_frame;
    newSequence->sink.receive_stream_frame_const = fss_receive_stream_frame_const;
    newSequence->sink.complete_frame = fss_complete_frame;
    newSequence->sink.cancel_frame = fss_cancel_frame;
    newSequence->sink.get_osd = fss_get_osd;
    newSequence->sink.get_video_switch = fss_get_video_switch;
    newSequence->sink.get_audio_switch = fss_get_audio_switch;
    newSequence->sink.get_half_split = fss_get_half_split;
    newSequence->sink.get_frame_sequence = fss_get_frame_sequence;
    newSequence->sink.get_buffer_state = fss_get_buffer_state;
    newSequence->sink.reset_or_close = fss_reset_or_close;
    newSequence->sink.close = fss_close;
    
    
    *sequence = newSequence;
    return 1;
}


MediaSink* fss_get_media_sink(FrameSequenceSink* sequence)
{
    return &sequence->sink;
}

