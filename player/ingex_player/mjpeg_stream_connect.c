/*
 * $Id: mjpeg_stream_connect.c,v 1.3 2008/10/29 17:47:42 john_f Exp $
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


#include "mjpeg_stream_connect.h"
#include "video_conversion.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#if !defined(HAVE_FFMPEG)

int mjpeg_connect_accept(MediaSink* sink, const StreamInfo* streamInfo)
{
    return 0;
}

int create_mjpeg_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, 
    StreamConnect** connect)
{
    return 0;
}

int init_mjpeg_decoder_resources()
{
    return 0;
}

void free_mjpeg_decoder_resources()
{
}


#else


#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#endif


/* some large number that we would not exceed */
#define MAX_STATIC_MJPEG_DECODERS          32


typedef struct
{
    StreamFormat format;
    int width;
    int height;
    
    int inUse;
    
    AVCodecContext* dec;
    int isThreaded;
    int openedDecoder; /* only close if decoder opened */
    AVFrame *decFrame;
} MJPEGDecoder;

typedef struct
{
    pthread_mutex_t resourceMutex;
    MJPEGDecoder* decoder[MAX_STATIC_MJPEG_DECODERS];
    int numDecoders;
    int numDecodersInUse;
} MJPEGDecoderResource;

typedef struct
{
    int useWorkerThread;
    
    int sourceStreamId;
    int sinkStreamId;
    
    StreamConnect streamConnect;
    MediaSourceListener sourceListener;
    
    MediaSink* sink;
    
    StreamInfo streamInfo;
    StreamFormat decodedFormat;
    
    MJPEGDecoder* decoder;

    unsigned char* mjpegData;
    unsigned int mjpegDataSize;
    unsigned int mjpegDataAllocSize;

    unsigned char* sinkBuffer;
    unsigned int sinkBufferSize;

    int frameWasReceived;
    
    pthread_t workerThreadId;
    pthread_mutex_t workerMutex;
    pthread_cond_t frameIsReadyCond;
    pthread_cond_t workerIsBusyCond;
    int frameIsReady;
    int workerIsBusy;
    int workerResult;

    int stopped; /* set when the worker thread is stopped */
} MJPEGDecodeStreamConnect;


static MJPEGDecoderResource g_decoderResource;
static int g_decoderResourceRefCount = 0;



static void free_mjpeg_decoder(MJPEGDecoder** decoder)
{
    int decoderResourceRefCount = g_decoderResourceRefCount;
    int i;
    int numDecoders = g_decoderResource.numDecoders;
    
    if (*decoder == NULL)
    {
        return;
    }
    
    /* check if decoder is a static resource - return to pile if it is */
    
    if (decoderResourceRefCount > 0)
    {
        for (i = 0; i < numDecoders; i++)
        {
            if (g_decoderResource.decoder[i] == (*decoder))
            {
                (*decoder)->inUse = 0;
                *decoder = NULL;
                return;
            }
        }
    }
    
    
    /* free the decoder */
    
    if ((*decoder)->isThreaded)
    {
        /* TODO: disabled this because it sometimes blocks */
        /* NOTE: this is the reason for the static resources stuff */
        /* avcodec_thread_free((*decoder)->dec); */
    }
    if ((*decoder)->openedDecoder)
    {
        avcodec_close((*decoder)->dec);
    }
    SAFE_FREE(&(*decoder)->dec);
    SAFE_FREE(&(*decoder)->decFrame);
    
    SAFE_FREE(decoder);
}

static int create_mjpeg_decoder(StreamFormat format, int width, int height, int numFFMPEGThreads, MJPEGDecoder** decoder)
{
    int decoderResourceRefCount = g_decoderResourceRefCount;
    int i;
    MJPEGDecoder* newDecoder = NULL;
    int numDecoders = g_decoderResource.numDecoders;
    AVCodec* avDecoder = NULL;
    
    /* see if there is matching decoder not in use */
    if (decoderResourceRefCount > 0)
    {
        for (i = 0; i < numDecoders; i++)
        {
            if (!g_decoderResource.decoder[i]->inUse && 
                g_decoderResource.decoder[i]->format == format &&
                g_decoderResource.decoder[i]->width == width &&
                g_decoderResource.decoder[i]->height == height)
            {
                /* found one not in use */
                *decoder = g_decoderResource.decoder[i];
                g_decoderResource.decoder[i]->inUse = 1;
                return 1;
            }
        }
    }
    
    
    /* create a new one */
    
    CALLOC_ORET(newDecoder, MJPEGDecoder, 1);

    newDecoder->inUse = 1;
    newDecoder->format = format;
    newDecoder->width = width;
    newDecoder->height = height;
    
    
    avDecoder = avcodec_find_decoder(CODEC_ID_MJPEG);
    if (!avDecoder) 
    {
        ml_log_error("Could not find the MJPEG decoder\n");
        goto fail;
    }

    newDecoder->dec = avcodec_alloc_context();
    if (!newDecoder->dec) 
    {
        ml_log_error("Could not allocate MJPEG decoder context\n");
        goto fail;
    }

    if (numFFMPEGThreads > 1)
    {
        avcodec_thread_init(newDecoder->dec, numFFMPEGThreads);
        newDecoder->isThreaded = 1;
    }
    
    
    avcodec_set_dimensions(newDecoder->dec, width, height);
    newDecoder->dec->pix_fmt = PIX_FMT_YUV422P;

    if (avcodec_open(newDecoder->dec, avDecoder) < 0) 
    {
        ml_log_error("Could not open decoder\n");
        goto fail;
    }
    newDecoder->openedDecoder = 1;

    newDecoder->decFrame = avcodec_alloc_frame();
    if (!newDecoder->decFrame) 
    {
        ml_log_error("Could not allocate decoded frame\n");
        goto fail;
    }

    
    /* add to static resources if they have been initialised */
    
    if (decoderResourceRefCount > 0)
    {
        if ((size_t)g_decoderResource.numDecoders >= sizeof(g_decoderResource.decoder) / sizeof(MJPEGDecoder*))
        {
            /* more than x decoders? what are you doing? */
            ml_log_error("Number of MJPEG decoders exceeded hard coded limit %d\n", 
                sizeof(g_decoderResource.decoder) / sizeof(MJPEGDecoder));
            goto fail;
        }
        
        PTHREAD_MUTEX_LOCK(&g_decoderResource.resourceMutex);
        g_decoderResource.decoder[g_decoderResource.numDecoders] = newDecoder;
        g_decoderResource.numDecoders++;
        PTHREAD_MUTEX_UNLOCK(&g_decoderResource.resourceMutex);
    }
    
    *decoder = newDecoder;
    return 1;
    
fail:
    free_mjpeg_decoder(&newDecoder);
    return 0;
}


static int decode_and_send_const(MJPEGDecodeStreamConnect* connect, const unsigned char* buffer, 
    unsigned int bufferSize)
{
    int finished;
    
    /* We know that avcodec_decode_video will not modify the input data, so we can cast buffer to non-const */
    avcodec_decode_video(connect->decoder->dec, connect->decoder->decFrame, &finished, (unsigned char*)buffer, bufferSize);
    if (!finished) 
    {
        ml_log_error("error decoding MJPEG video\n");
        return 0;
    }

    /* reformat decoded frame to UYVY */
    if (connect->decodedFormat == UYVY_FORMAT)
    {
        yuv422_to_uyvy(connect->streamInfo.width, connect->streamInfo.height, 0,
            connect->decoder->decFrame, connect->sinkBuffer);
    }
    else /* YUV422 */
    {
        yuv422_to_yuv422(connect->streamInfo.width, connect->streamInfo.height, 0,
            connect->decoder->decFrame, connect->sinkBuffer);
    }

    /* send decoded frame to sink */
    if (!msk_receive_stream_frame(connect->sink, connect->sinkStreamId, connect->sinkBuffer, 
        connect->sinkBufferSize))
    {
        ml_log_error("failed to write frame to media sink\n");
        return 0;
    }

    return 1;
}

static int decode_and_send(MJPEGDecodeStreamConnect* connect)
{
    return decode_and_send_const(connect, connect->mjpegData, connect->mjpegDataSize);
}

static void* worker_thread(void* arg)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)arg;
    int status;
    int workerResult;
    int doneWaiting;

    while (!connect->stopped)
    {
        /* wait until a frame is ready */
        doneWaiting = 0;
        while (!doneWaiting && !connect->stopped)
        {
            /* wait for a frame */
            PTHREAD_MUTEX_LOCK(&connect->workerMutex);
            if (!connect->frameIsReady)
            {
                status = pthread_cond_wait(&connect->frameIsReadyCond, &connect->workerMutex);
                if (status != 0)
                {
                    ml_log_error("MJPEG connect worker thread failed to wait for condition\n");
                    /* TODO: don't try again? */ 
                }
            }
    
            /* done waiting if there is a frame ready for processing */        
            if (connect->frameIsReady)
            {
                /* worker will now be busy */
                connect->workerIsBusy = 1;
                connect->workerResult = 0;
                connect->frameIsReady = 0;
                doneWaiting = 1;
            }
            PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
        }
        
        /* no more work */
        if (connect->stopped)
        {
            break;
        }

        /* decode and send frame to sink */
        
        workerResult = decode_and_send(connect);
        

        /* signal that we are done with the frame */
        
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        connect->workerResult = workerResult;
        connect->workerIsBusy = 0;
        status = pthread_cond_signal(&connect->workerIsBusyCond);
        if (status != 0)
        {
            ml_log_error("MJPEG connect worker thread failed to send worker busy condition signal\n");
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }
 
    pthread_exit((void*) 0);
}





static MediaSourceListener* ddc_get_source_listener(void* data)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)data;
    
    return &connect->sourceListener;
}

static int ddc_sync(void* data)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)data;
    int status;
    int workerResult = 0;
    int doneWaiting;

    if (!connect->frameWasReceived)
    {
        /* no work was required */
        return 1;
    }

    /* reset for next time */
    connect->frameWasReceived = 0;
    
    if (!connect->useWorkerThread)
    {
        /* work is already complete */
        return 1;
    }

    /* TODO: timed wait to prevent eternal loop? */

    /* wait until worker has processed the frame and is no longer busy */
    doneWaiting = 0;
    while (!doneWaiting && !connect->stopped)
    {
        /* wait for worker */
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        if (connect->workerIsBusy)
        {
            status = pthread_cond_wait(&connect->workerIsBusyCond, &connect->workerMutex);
            if (status != 0)
            {
                ml_log_error("MJPEG connect worker thread failed to wait for condition\n");
                /* TODO: don't try again? */ 
            }
        }
        
        /* worker is not busy and no frame is waiting to be processed */
        if (!connect->workerIsBusy && !connect->frameIsReady)
        {
            workerResult = connect->workerResult;
            doneWaiting = 1;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }

    return workerResult;
}

static void ddc_close(void* data)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)data;

    if (data == NULL)
    {
        return;
    }
    
    if (connect->useWorkerThread)
    {
        connect->stopped = 1;
        
        /* wake up threads - this is to avoid valgrind saying the mutx is 
        still in use when pthread_mutex_destroy is called below */
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        pthread_cond_broadcast(&connect->frameIsReadyCond);
        pthread_cond_broadcast(&connect->workerIsBusyCond);
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
        
        join_thread(&connect->workerThreadId, NULL, NULL);
    }

    free_mjpeg_decoder(&connect->decoder);
    free_mjpeg_decoder_resources();

    SAFE_FREE(&connect->mjpegData);
    
    if (connect->useWorkerThread)
    {
        destroy_cond_var(&connect->workerIsBusyCond);
        destroy_cond_var(&connect->frameIsReadyCond);
        destroy_mutex(&connect->workerMutex);
    }
    
    
    SAFE_FREE(&connect);
}



static int ddc_accept_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)data;

    connect->frameWasReceived = 0;
    
    return connect->sourceStreamId == streamId && msk_accept_stream_frame(connect->sink, streamId, frameInfo);
}
    
static int ddc_allocate_buffer(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)data;
    int result;
    
    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Buffer allocation request for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    if (connect->mjpegDataAllocSize < bufferSize + FF_INPUT_BUFFER_PADDING_SIZE)
    {
        /* re-allocate buffer */
        SAFE_FREE(&connect->mjpegData);
        connect->mjpegDataSize = 0;
        connect->mjpegDataAllocSize = 0;
        CALLOC_ORET(connect->mjpegData, unsigned char, bufferSize + FF_INPUT_BUFFER_PADDING_SIZE);
        connect->mjpegDataAllocSize = bufferSize + FF_INPUT_BUFFER_PADDING_SIZE;
    }
    connect->mjpegDataSize = bufferSize;
    
    /* ask sink to allocate buffer for decoded frame */
    result = msk_get_stream_buffer(connect->sink, connect->sinkStreamId, connect->sinkBufferSize, &connect->sinkBuffer);
    if (!result)
    {
        ml_log_error("Sink failed to allocate buffer for stream %d for MJPEG decoder connector\n", streamId);
        return 0;
    }
    
    *buffer = connect->mjpegData;
    return 1;
}

static void ddc_deallocate_buffer(void* data, int streamId, unsigned char** buffer)
{
    /* do nothing because buffer is reused */
}

static int ddc_receive_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)data;
    int status;
    int result = 1;

    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Received frame for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    /* signal to ddc_sync at later time that we have received a frame to decode and send */
    connect->frameWasReceived = 1;
    
    
    if (!connect->useWorkerThread)
    {
        result = decode_and_send(connect);
    }
    else
    {
        
        /* check that the worker isn't busy */
    
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        if (connect->workerIsBusy)
        {
            ml_log_error("MJPEG connect worker thread is still busy, and therefore cannot receive a new frame\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
        
        if (result != 1)
        {
            return result;
        }
    
        
        /* signal worker that a new frame is ready */
    
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        connect->frameIsReady = 1;
        status = pthread_cond_signal(&connect->frameIsReadyCond);
        if (status != 0)
        {
            ml_log_error("MJPEG connect worker thread failed to send frame is ready condition signal\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }
    
    return result;
}

static int ddc_receive_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    MJPEGDecodeStreamConnect* connect = (MJPEGDecodeStreamConnect*)data;
    int status;
    int result;
    unsigned char* nonconstBuffer;

    if (connect->useWorkerThread)
    {
        /* the worker thread requires the data to be copied into connect->mjpegData */
        result = ddc_allocate_buffer(data, streamId, &nonconstBuffer, bufferSize);
        if (result)
        {
            memcpy(nonconstBuffer, buffer, bufferSize);
            result = ddc_receive_frame(data, streamId, nonconstBuffer, bufferSize);
        }
        return result;
    }
    
    
    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Received frame for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    /* ask sink to allocate buffer for decoded frame */
    result = msk_get_stream_buffer(connect->sink, connect->sinkStreamId, connect->sinkBufferSize, 
        &connect->sinkBuffer);
    if (!result)
    {
        ml_log_error("Sink failed to allocate buffer for stream %d for MJPEG decoder connector\n", streamId);
        return 0;
    }

    
    /* signal to ddc_sync at later time that we have received a frame to decode and send */
    connect->frameWasReceived = 1;
    
    
    if (!connect->useWorkerThread)
    {
        result = decode_and_send_const(connect, buffer, bufferSize);
    }
    else
    {
        
        /* check that the worker isn't busy */
    
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        if (connect->workerIsBusy)
        {
            ml_log_error("MJPEG connect worker thread is still busy, and therefore cannot receive a new frame\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
        
        if (result != 1)
        {
            return result;
        }
    
        
        /* signal worker that a new frame is ready */
    
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        connect->frameIsReady = 1;
        status = pthread_cond_signal(&connect->frameIsReadyCond);
        if (status != 0)
        {
            ml_log_error("MJPEG connect worker thread failed to send frame is ready condition signal\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }
    
    return result;
}



int mjpeg_connect_accept(MediaSink* sink, const StreamInfo* streamInfo)
{
    StreamInfo decodedStreamInfo;
    int result;
    
    if (streamInfo->type != PICTURE_STREAM_TYPE ||
        streamInfo->format != AVID_MJPEG_FORMAT)
    {
        return 0;
    }
    
    decodedStreamInfo = *streamInfo;
    decodedStreamInfo.format = YUV422_FORMAT;

    result = msk_accept_stream(sink, &decodedStreamInfo);
    
    if (!result)
    {
        decodedStreamInfo = *streamInfo;
        decodedStreamInfo.format = UYVY_FORMAT;

        result = msk_accept_stream(sink, &decodedStreamInfo);
    }
    
    return result;
}

int create_mjpeg_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, StreamConnect** connect)
{
    MJPEGDecodeStreamConnect* newConnect;
    StreamInfo decodedStreamInfo;
    int result;

    decodedStreamInfo = *streamInfo;
    decodedStreamInfo.format = YUV422_FORMAT;

    result = msk_accept_stream(sink, &decodedStreamInfo);
    if (!result)
    {
        decodedStreamInfo = *streamInfo;
        decodedStreamInfo.format = UYVY_FORMAT;

        result = msk_accept_stream(sink, &decodedStreamInfo);
    }
    
    if (!msk_register_stream(sink, sinkStreamId, &decodedStreamInfo))
    {
        /* could have failed if max streams exceeded for example */
        return 0;
    }

    
    CALLOC_ORET(newConnect, MJPEGDecodeStreamConnect, 1);
    
    newConnect->useWorkerThread = useWorkerThread;
    newConnect->decodedFormat = decodedStreamInfo.format;
    
    newConnect->sink = sink;
    newConnect->sourceStreamId = sourceStreamId;
    newConnect->sinkStreamId = sinkStreamId;
    newConnect->streamInfo = *streamInfo;
    if (decodedStreamInfo.format == UYVY_FORMAT)
    {
        newConnect->sinkBufferSize = streamInfo->width * streamInfo->height * 2;
    }
    else /* (decodedStreamInfo.format == YUV422_FORMAT) */
    {
        newConnect->sinkBufferSize = streamInfo->width * streamInfo->height * 2;
    }
    
    newConnect->streamConnect.data = newConnect;
    newConnect->streamConnect.get_source_listener = ddc_get_source_listener;
    newConnect->streamConnect.sync = ddc_sync;
    newConnect->streamConnect.close = ddc_close;

    newConnect->sourceListener.data = newConnect;
    newConnect->sourceListener.accept_frame = ddc_accept_frame;
    newConnect->sourceListener.allocate_buffer = ddc_allocate_buffer;
    newConnect->sourceListener.deallocate_buffer = ddc_deallocate_buffer;
    newConnect->sourceListener.receive_frame = ddc_receive_frame;
    newConnect->sourceListener.receive_frame_const = ddc_receive_frame_const;
    
    
    
    /* create MJPEG decoder */
    
    CHK_OFAIL(init_mjpeg_decoder_resources());
        
    CHK_OFAIL(create_mjpeg_decoder(streamInfo->format, streamInfo->width, streamInfo->height,
        numFFMPEGThreads, &newConnect->decoder));
        

    /* create worker thread */
    
    if (useWorkerThread)
    {
        CHK_OFAIL(init_mutex(&newConnect->workerMutex));
        CHK_OFAIL(init_cond_var(&newConnect->frameIsReadyCond));
        CHK_OFAIL(init_cond_var(&newConnect->workerIsBusyCond));
        
        CHK_OFAIL(create_joinable_thread(&newConnect->workerThreadId, worker_thread, newConnect));
    }
    
    
    *connect = &newConnect->streamConnect;
    return 1;
    
fail:
    ddc_close(newConnect);
    return 0;
}



int init_mjpeg_decoder_resources()
{
    if (g_decoderResourceRefCount == 0)
    {
        av_register_all();
        
        memset(&g_decoderResource, 0, sizeof(MJPEGDecoderResource));
        CHK_ORET(init_mutex(&g_decoderResource.resourceMutex));
        g_decoderResourceRefCount = 1;
    }
    else
    {
        g_decoderResourceRefCount++;
    }
    
    return 1;
}

void free_mjpeg_decoder_resources()
{
    int i;
    MJPEGDecoder* decoder = NULL;
    int refCount;

    if (g_decoderResourceRefCount <= 0)
    {
        return;
    }
    
    PTHREAD_MUTEX_LOCK(&g_decoderResource.resourceMutex);
    g_decoderResourceRefCount--;
    refCount = g_decoderResourceRefCount;
    PTHREAD_MUTEX_UNLOCK(&g_decoderResource.resourceMutex);
    
    if (refCount == 0)
    {
        if (g_decoderResource.numDecodersInUse > 0)
        {
            ml_log_warn("There are %d MJPEG decoder resources still in use - please fix the source code\n", 
                g_decoderResource.numDecodersInUse);
        }
        
        for (i = 0; i < g_decoderResource.numDecoders; i++)
        {
            /* set decoder NULL in list so that free doesn't just change it to !inUse */
            decoder = g_decoderResource.decoder[i];
            g_decoderResource.decoder[i] = NULL;
            
            free_mjpeg_decoder(&decoder);
        }
        
        destroy_mutex(&g_decoderResource.resourceMutex);

        memset(&g_decoderResource, 0, sizeof(MJPEGDecoderResource));
    }
}


#endif /* HAVE_FFMPEG */


