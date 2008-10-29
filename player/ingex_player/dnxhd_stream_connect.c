/*
 * $Id: dnxhd_stream_connect.c,v 1.3 2008/10/29 17:47:41 john_f Exp $
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


#include "dnxhd_stream_connect.h"
#include "video_conversion.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#if !defined(HAVE_FFMPEG) || !defined(HAVE_DNXHD)

int dnxhd_connect_accept(MediaSink* sink, const StreamInfo* streamInfo)
{
    return 0;
}

int create_dnxhd_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, 
    StreamConnect** connect)
{
    return 0;
}


#else


#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#endif


typedef struct
{
    StreamFormat format;
    int width;
    int height;
    
    AVCodecContext* dec;
    int isThreaded;
    int openedDecoder; /* only close if decoder opened */
    AVFrame *decFrame;
} DNxHDDecoder;

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
    
    DNxHDDecoder* decoder;

    unsigned char* dnxhdData;
    unsigned int dnxhdDataSize;

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
} DNxHDDecodeStreamConnect;



static void free_dnxhd_decoder(DNxHDDecoder** decoder)
{
    if ((*decoder)->isThreaded)
    {
        avcodec_thread_free((*decoder)->dec);
    }
    if ((*decoder)->openedDecoder)
    {
        avcodec_close((*decoder)->dec);
    }
    SAFE_FREE(&(*decoder)->dec);
    SAFE_FREE(&(*decoder)->decFrame);
    
    SAFE_FREE(decoder);
}

static int create_dnxhd_decoder(StreamFormat format, int width, int height, int numFFMPEGThreads, DNxHDDecoder** decoder)
{
    DNxHDDecoder* newDecoder = NULL;
    AVCodec* avDecoder = NULL;

    
    CALLOC_ORET(newDecoder, DNxHDDecoder, 1);

    newDecoder->format = format;
    newDecoder->width = width;
    newDecoder->height = height;
    
    
    avDecoder = avcodec_find_decoder(CODEC_ID_DNXHD);
    if (!avDecoder) 
    {
        ml_log_error("Could not find the DNxHD decoder\n");
        goto fail;
    }

    newDecoder->dec = avcodec_alloc_context();
    if (!newDecoder->dec) 
    {
        ml_log_error("Could not allocate DNxHD decoder context\n");
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

    *decoder = newDecoder;
    return 1;
    
fail:
    free_dnxhd_decoder(&newDecoder);
    return 0;
}


static int decode_and_send_const(DNxHDDecodeStreamConnect* connect, const unsigned char* buffer, 
    unsigned int bufferSize)
{
    int finished;
    
    /* We know that avcodec_decode_video will not modify the input data, so we can cast buffer to non-const */
    avcodec_decode_video(connect->decoder->dec, connect->decoder->decFrame, &finished, (unsigned char*)buffer, bufferSize);
    if (!finished) 
    {
        ml_log_error("Failed to decode DNxHD video\n");
        return 0;
    }

    /* reformat decoded frame */
    yuv422_to_yuv422(connect->streamInfo.width, connect->streamInfo.height, 1,
        connect->decoder->decFrame, connect->sinkBuffer);

    /* send decoded frame to sink */
    if (!msk_receive_stream_frame(connect->sink, connect->sinkStreamId, connect->sinkBuffer, 
        connect->sinkBufferSize))
    {
        ml_log_error("failed to write frame to media sink\n");
        return 0;
    }

    return 1;
}

static int decode_and_send(DNxHDDecodeStreamConnect* connect)
{
    return decode_and_send_const(connect, connect->dnxhdData, connect->dnxhdDataSize);
}

static void* worker_thread(void* arg)
{
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)arg;
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
                    ml_log_error("DNxHD connect worker thread failed to wait for condition\n");
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
            ml_log_error("DNxHD connect worker thread failed to send worker busy condition signal\n");
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }
 
    pthread_exit((void*) 0);
}





static MediaSourceListener* ddc_get_source_listener(void* data)
{
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)data;
    
    return &connect->sourceListener;
}

static int ddc_sync(void* data)
{
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)data;
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
                ml_log_error("DNxHD connect worker thread failed to wait for condition\n");
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
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)data;

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

    free_dnxhd_decoder(&connect->decoder);

    SAFE_FREE(&connect->dnxhdData);
    
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
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)data;

    connect->frameWasReceived = 0;
    
    return connect->sourceStreamId == streamId && msk_accept_stream_frame(connect->sink, streamId, frameInfo);
}
    
static int ddc_allocate_buffer(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)data;
    int result;
    
    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Buffer allocation request for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    if (connect->dnxhdDataSize != bufferSize)
    {
        ml_log_error("Invalid DNxHD buffer allocation request for stream %d in copy connect\n", streamId);
        return 0;
    }
    
    /* ask sink to allocate buffer for decoded frame */
    result = msk_get_stream_buffer(connect->sink, connect->sinkStreamId, connect->sinkBufferSize, 
        &connect->sinkBuffer);
    if (!result)
    {
        ml_log_error("Sink failed to allocate buffer for stream %d for DNxHD decoder connector\n", streamId);
        return 0;
    }
    
    *buffer = connect->dnxhdData;
    return 1;
}

static void ddc_deallocate_buffer(void* data, int streamId, unsigned char** buffer)
{
    /* do nothing because buffer is reused */
}

static int ddc_receive_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)data;
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
            ml_log_error("DNxHD connect worker thread is still busy, and therefore cannot receive a new frame\n");
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
            ml_log_error("DNxHD connect worker thread failed to send frame is ready condition signal\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }
    
    return result;
}

static int ddc_receive_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    DNxHDDecodeStreamConnect* connect = (DNxHDDecodeStreamConnect*)data;
    int status;
    int result;
    unsigned char* nonconstBuffer;

    if (connect->useWorkerThread)
    {
        /* the worker thread requires the data to be copied into connect->dnxhdData */
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
        ml_log_error("Sink failed to allocate buffer for stream %d for DNxHD decoder connector\n", streamId);
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
            ml_log_error("DNxHD connect worker thread is still busy, and therefore cannot receive a new frame\n");
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
            ml_log_error("DNxHD connect worker thread failed to send frame is ready condition signal\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }
    
    return result;
}



int dnxhd_connect_accept(MediaSink* sink, const StreamInfo* streamInfo)
{
    StreamInfo decodedStreamInfo;
    int result;
    
    if (streamInfo->type != PICTURE_STREAM_TYPE ||
        streamInfo->format != AVID_DNxHD_FORMAT)
    {
        return 0;
    }
    
    decodedStreamInfo = *streamInfo;
    decodedStreamInfo.format = YUV422_FORMAT;

    result = msk_accept_stream(sink, &decodedStreamInfo);
    
    return result;
}

int create_dnxhd_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, StreamConnect** connect)
{
    DNxHDDecodeStreamConnect* newConnect;
    StreamInfo decodedStreamInfo;
    int result;

    /* register stream with sink */    
    if (streamInfo->format == AVID_DNxHD_FORMAT)
    {
        decodedStreamInfo = *streamInfo;
        decodedStreamInfo.format = YUV422_FORMAT;

        result = msk_accept_stream(sink, &decodedStreamInfo);
    }
    
    if (!msk_register_stream(sink, sinkStreamId, &decodedStreamInfo))
    {
        /* could have failed if max streams exceeded for example */
        return 0;
    }

    
    CALLOC_ORET(newConnect, DNxHDDecodeStreamConnect, 1);
    
    newConnect->useWorkerThread = useWorkerThread;
    newConnect->decodedFormat = decodedStreamInfo.format;
    newConnect->dnxhdDataSize = 606208;
    
    if ((newConnect->dnxhdData = (unsigned char*)calloc(
        newConnect->dnxhdDataSize + FF_INPUT_BUFFER_PADDING_SIZE /* FFMPEG for some reason needs the extra space */, 
        sizeof(unsigned char))) == NULL)
    {
        ml_log_error("Failed to allocate memory\n");
        goto fail;
    }
    
    newConnect->sink = sink;
    newConnect->sourceStreamId = sourceStreamId;
    newConnect->sinkStreamId = sinkStreamId;
    newConnect->streamInfo = *streamInfo;
    newConnect->sinkBufferSize = streamInfo->width * streamInfo->height * 2;
    
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
    
    
    
    /* create DNxHD decoder */

    av_register_all();
    
    CHK_OFAIL(create_dnxhd_decoder(streamInfo->format, streamInfo->width, streamInfo->height,
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



#endif


