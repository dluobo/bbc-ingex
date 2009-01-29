/*
 * $Id: ffmpeg_source.c,v 1.5 2009/01/29 07:10:26 stuart_hc Exp $
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

/* used ffmpeg's ffplay.c as a starting point */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


#if !defined(HAVE_FFMPEG) || !defined(HAVE_FFMPEG_SWSCALE)

#include "ffmpeg_source.h"

int fms_open(const char* filename, int threadCount, MediaSource** source)
{
    return 0;
}



#else /* defined(HAVE_FFMPEG) && defined(HAVE_FFMPEG_SWSCALE) */


#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avformat.h>
#include <ffmpeg/swscale.h>
#else
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

#include "ffmpeg_source.h"
#include "video_conversion.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


/* results returned when processing video and audio packet data */
#define PROCESS_PACKETS_SUCCESS         0
#define PROCESS_PACKETS_FAILED          -1
#define NO_FRAME_DATA                   1
#define EARLY_FRAME                     2
#define LATE_FRAME                      3
#define NO_PACKET_IN_QUEUE              4

/* max frames to go back if a read returns frame data beyond the target frame */
#define MAX_ADJUST_READ_START_COUNT     32


/* anything beyond this and we stop processing packets */
#define MAX_PACKET_QUEUE_SIZE           200


#if 0
#define DEBUG(format, ...) {printf("Debug: "); printf(format, ## __VA_ARGS__);}
#else
#define DEBUG(format, ...)
#endif


typedef struct
{
    AVPacketList* first;
    AVPacketList* last;
    int size;
} PacketQueue;

typedef struct
{
    int streamIndex;
    StreamInfo streamInfo;
    int isDisabled;

    unsigned char* buffer;
    int bufferSize;
    int dataSize; /* will be different from bufferSize when audio with 525 line video is supported */
} OutputStreamData;

typedef struct
{
    int index;
    int avStreamIndex;

    PacketQueue packetQueue;

    AVFrame* frame;
    struct SwsContext* imageConvertContext;
    int dataOffset[3];
    int lineSize[3];
    enum PixelFormat outputPixelFormat;

    OutputStreamData outputStream;
    int isReady;
} VideoStream;

typedef struct
{
    int index;
    int avStreamIndex;

    PacketQueue packetQueue;

    OutputStreamData outputStreams[16];
    int numOutputStreams;
    int isReady;

    AVPacket packet;
    uint8_t* packetNextData;
    int packetNextSize;

    unsigned char* decodeBuffer;
    int decodeBufferSize;
    int decodeDataSize;

    int64_t initialDecodeSample;
    int64_t firstDecodeSample;
    int numDecodeSamples;

    int64_t firstOutputSample;
    int numOutputSamples;
} AudioStream;

typedef struct
{
    int index;

    int64_t startTimecode;
    int timecodeBase;

    OutputStreamData outputStream;
} TimecodeStream;

typedef struct
{
    int threadCount;

    MediaSource mediaSource;

    AVFormatContext* formatContext;
    int isMXFFormat;
    int isPAL;

    VideoStream videoStreams[32];
    int numVideoStreams;
    AudioStream audioStreams[32];
    int numAudioStreams;
    TimecodeStream timecodeStream;

    Rational frameRate;
    int isHardFrameRate;

    int64_t position;
    int64_t length;
} FFMPEGSource;


static int64_t g_videoPacketPTS = (int64_t)AV_NOPTS_VALUE;


static void clear_packet_queue(PacketQueue* queue)
{
    AVPacketList* item = queue->first;
    AVPacketList* nextItem = NULL;

    while (item != NULL)
    {
        av_free_packet(&item->pkt);
        nextItem = item->next;
        av_freep(&item);

        item = nextItem;
    }

    queue->first = NULL;
    queue->last = NULL;
    queue->size = 0;
}

static int push_packet(PacketQueue* queue, AVPacket* packet)
{
    if (queue->size >= MAX_PACKET_QUEUE_SIZE)
    {
        ml_log_error("Maximum packet queue size (%d) exceeded\n", MAX_PACKET_QUEUE_SIZE);
        return 0;
    }

    CHK_ORET(av_dup_packet(packet) >= 0);

    if (queue->last == NULL)
    {
        CHK_ORET((queue->first = av_malloc(sizeof(AVPacketList))) != NULL);
        memset(queue->first, 0, sizeof(AVPacketList));
        queue->last = queue->first;
    }
    else
    {
        CHK_ORET((queue->last->next = av_malloc(sizeof(AVPacketList))) != NULL);
        memset(queue->last->next, 0, sizeof(AVPacketList));
        queue->last = queue->last->next;
    }

    queue->last->pkt = *packet;
    queue->size++;

    return 1;
}

static int pop_packet(PacketQueue* queue, AVPacket* packet)
{
    AVPacketList* nextFirst = NULL;

    if (queue->first == NULL)
    {
        return 0;
    }

    *packet = queue->first->pkt;

    nextFirst = queue->first->next;
    av_freep(&queue->first);
    queue->first = nextFirst;
    queue->size--;

    if (queue->first == NULL)
    {
        queue->last = NULL;
    }

    return 1;
}


static void deinterleave_audio(const int16_t* input, int numChannels, int numSamples, int outputChannel, unsigned char* output)
{
    int i;
    const int16_t* inputPtr = input + outputChannel;
    unsigned char* outputPtr = output;

    for (i = 0; i < numSamples; i++)
    {
        /* output is 16-bit PCM little endian */
        *outputPtr++ = *inputPtr & 0x00ff;
        *outputPtr++ = (*inputPtr >> 8) & 0x00ff;

        inputPtr += numChannels;
    }
}


static VideoStream* get_video_stream(FFMPEGSource* source, int avStreamIndex)
{
    int i;

    for (i = 0; i < source->numVideoStreams; i++)
    {
        if (source->videoStreams[i].avStreamIndex == avStreamIndex)
        {
            return &source->videoStreams[i];
        }
    }

    return NULL;
}

static AudioStream* get_audio_stream(FFMPEGSource* source, int avStreamIndex)
{
    int i;

    for (i = 0; i < source->numAudioStreams; i++)
    {
        if (source->audioStreams[i].avStreamIndex == avStreamIndex)
        {
            return &source->audioStreams[i];
        }
    }

    return NULL;
}

static void clear_stream_data(FFMPEGSource* source)
{
    int i;

    for (i = 0; i < source->numVideoStreams; i++)
    {
        source->videoStreams[i].isReady = 0;
    }

    for (i = 0; i < source->numAudioStreams; i++)
    {
        source->audioStreams[i].isReady = 0;
        source->audioStreams[i].firstOutputSample = 0;
        source->audioStreams[i].numOutputSamples = 0;
    }
}

static OutputStreamData* get_output_stream(FFMPEGSource* source, int streamIndex)
{
    int i;
    int j;

    for (i = 0; i < source->numVideoStreams; i++)
    {
        if (source->videoStreams[i].outputStream.streamIndex == streamIndex)
        {
            return &source->videoStreams[i].outputStream;
        }
    }

    for (i = 0; i < source->numAudioStreams; i++)
    {
        for (j = 0; j < source->audioStreams[i].numOutputStreams; j++)
        {
            if (source->audioStreams[i].outputStreams[j].streamIndex == streamIndex)
            {
                return &source->audioStreams[i].outputStreams[j];
            }
        }
    }

    if (source->timecodeStream.outputStream.streamIndex == streamIndex)
    {
        return &source->timecodeStream.outputStream;
    }

    return NULL;
}

static int is_disabled(FFMPEGSource* source)
{
    int i;
    int j;

    for (i = 0; i < source->numVideoStreams; i++)
    {
        if (!source->videoStreams[i].outputStream.isDisabled)
        {
            return 0;
        }
    }

    for (i = 0; i < source->numAudioStreams; i++)
    {
        for (j = 0; j < source->audioStreams[i].numOutputStreams; j++)
        {
            if (!source->audioStreams[i].outputStreams[j].isDisabled)
            {
                return 0;
            }
        }
    }

    if (!source->timecodeStream.outputStream.isDisabled)
    {
        return 0;
    }

    return 1;
}

static void free_audiostream_packet(AudioStream* audioStream)
{
    if (audioStream->packet.size != 0)
    {
        av_free_packet(&audioStream->packet);
        audioStream->packet.size = 0;
    }
    audioStream->packetNextData = NULL;
    audioStream->packetNextSize = 0;
}



static void flush_video_buffers(FFMPEGSource* source, VideoStream* videoStream)
{
    avcodec_flush_buffers(source->formatContext->streams[videoStream->avStreamIndex]->codec);

    clear_packet_queue(&videoStream->packetQueue);
}

static void flush_audio_buffers(FFMPEGSource* source, AudioStream* audioStream)
{
    avcodec_flush_buffers(source->formatContext->streams[audioStream->avStreamIndex]->codec);

    clear_packet_queue(&audioStream->packetQueue);

    free_audiostream_packet(audioStream);
    audioStream->firstDecodeSample = 0;
    audioStream->numDecodeSamples = 0;
}

static int get_video_buffer(struct AVCodecContext* c, AVFrame *pic)
{
    int ret = avcodec_default_get_buffer(c, pic);

    /* attach the pts from the packet so we know the pts of the returned decoded frame */
    int64_t* pts = av_malloc(sizeof(int64_t));
    *pts = g_videoPacketPTS;
    pic->opaque = pts;

    return ret;
}

static void release_video_buffer(struct AVCodecContext* c, AVFrame* pic)
{
    if (pic != NULL)
    {
        av_freep(&pic->opaque);
    }

    avcodec_default_release_buffer(c, pic);
}

static int open_video_stream(FFMPEGSource* source, int sourceId, int avStreamIndex, int* outputStreamIndex)
{
    AVStream* stream = source->formatContext->streams[avStreamIndex];
    AVCodecContext* codecContext = stream->codec;
    AVCodec* codec = NULL;
    VideoStream* videoStream = &source->videoStreams[source->numVideoStreams];
    const double imageAspectRatio4By3 = 4 / (double)3;
    const double imageAspectRatio16By9 = 16 / (double)9;
    double imageAspectRatio;

    if (source->numVideoStreams + 1 >= (int)(sizeof(source->videoStreams) / sizeof(VideoStream)))
    {
        ml_log_error("Maximum video streams (%d) exceeded\n", sizeof(source->videoStreams) / sizeof(VideoStream));
        return 0;
    }


    codec = avcodec_find_decoder(codecContext->codec_id);
    if (codec == NULL)
    {
        ml_log_error("Failed to find video decoder\n");
        return 0;
    }

    codecContext->debug_mv = 0;
    codecContext->debug = 0;
    codecContext->workaround_bugs = 1;
    codecContext->lowres = 0;
    if (codecContext->lowres)
    {
        codecContext->flags |= CODEC_FLAG_EMU_EDGE;
    }
    codecContext->idct_algo = FF_IDCT_AUTO;
    codecContext->skip_frame = AVDISCARD_DEFAULT;
    codecContext->skip_idct = AVDISCARD_DEFAULT;
    codecContext->skip_loop_filter = AVDISCARD_DEFAULT;
    codecContext->error_concealment = 3;

    if (avcodec_open(codecContext, codec) < 0)
    {
        ml_log_error("Failed to open ffmpeg video codec\n");
        return 0;
    }

    if (source->threadCount > 1)
    {
        if (avcodec_thread_init(codecContext, source->threadCount) < 0)
        {
            ml_log_error("Failed to initialise ffmpeg threads\n");
            avcodec_close(codecContext);
            return 0;
        }
        codecContext->thread_count = source->threadCount;
    }

    codecContext->get_buffer = get_video_buffer;
    codecContext->release_buffer = release_video_buffer;


    videoStream->frame = avcodec_alloc_frame();
    if (videoStream->frame == NULL)
    {
        ml_log_error("Failed to allocate ffmpeg video frame\n");
        avcodec_close(codecContext);
        return 0;
    }


    videoStream->index = source->numVideoStreams;
    videoStream->avStreamIndex = avStreamIndex;

    videoStream->outputStream.streamIndex = (*outputStreamIndex)++;

    videoStream->outputStream.streamInfo.type = PICTURE_STREAM_TYPE;
    videoStream->outputStream.streamInfo.sourceId = sourceId;

    videoStream->outputStream.streamInfo.width = codecContext->width;
    videoStream->outputStream.streamInfo.height = codecContext->height;

    videoStream->outputStream.streamInfo.singleField = 0; /* TODO: can we tell? */

    if ((stream->r_frame_rate.den * 25) % stream->r_frame_rate.num == 0)
    {
        source->isPAL = 1;
        videoStream->outputStream.streamInfo.frameRate = g_palFrameRate;
        videoStream->outputStream.streamInfo.isHardFrameRate = 1;
        source->frameRate = g_palFrameRate;
        source->isHardFrameRate = 1;
    }
    else if ((stream->r_frame_rate.den * 30000) % (stream->r_frame_rate.num * 1001) == 0 ||
        (stream->r_frame_rate.den * 2997) % (stream->r_frame_rate.num * 1000) == 0)
    {
        source->isPAL = 0;
        videoStream->outputStream.streamInfo.frameRate = g_ntscFrameRate;
        videoStream->outputStream.streamInfo.isHardFrameRate = 1;
        source->frameRate = g_ntscFrameRate;
        source->isHardFrameRate = 1;

    }
    else
    {
        ml_log_error("Frame rate is %d/%d. Only PAL and NTSC frame rates are supported\n",
            stream->r_frame_rate.num, stream->r_frame_rate.den);
        avcodec_close(codecContext);
        return 0;
    }

    if (codecContext->sample_aspect_ratio.num == 0 || codecContext->sample_aspect_ratio.den == 0)
    {
        /* assume 4:3 */
        videoStream->outputStream.streamInfo.aspectRatio = (Rational){4, 3};
    }
    else
    {
        if (source->isPAL &&
            ((codecContext->sample_aspect_ratio.num == 16 && codecContext->sample_aspect_ratio.den == 15) ||
                (codecContext->sample_aspect_ratio.num == 12 && codecContext->sample_aspect_ratio.den == 11) ||
                (codecContext->sample_aspect_ratio.num == 59 && codecContext->sample_aspect_ratio.den == 54)))
        {
            /* 4:3 PAL */
            videoStream->outputStream.streamInfo.aspectRatio = (Rational){4, 3};
        }
        else if (source->isPAL &&
            ((codecContext->sample_aspect_ratio.num == 64 && codecContext->sample_aspect_ratio.den == 45) ||
                (codecContext->sample_aspect_ratio.num == 16 && codecContext->sample_aspect_ratio.den == 11) ||
                (codecContext->sample_aspect_ratio.num == 118 && codecContext->sample_aspect_ratio.den == 81)))
        {
            /* 16:9 PAL */
            videoStream->outputStream.streamInfo.aspectRatio = (Rational){16, 9};
        }
        else if (!source->isPAL &&
            ((codecContext->sample_aspect_ratio.num == 8 && codecContext->sample_aspect_ratio.den == 9) ||
                (codecContext->sample_aspect_ratio.num == 10 && codecContext->sample_aspect_ratio.den == 11)))
        {
            /* 4:3 NTSC */
            videoStream->outputStream.streamInfo.aspectRatio = (Rational){4, 3};
        }
        else if (!source->isPAL &&
            ((codecContext->sample_aspect_ratio.num == 32 && codecContext->sample_aspect_ratio.den == 27) ||
                (codecContext->sample_aspect_ratio.num == 40 && codecContext->sample_aspect_ratio.den == 33)))
        {
            /* 16:9 NTSC */
            videoStream->outputStream.streamInfo.aspectRatio = (Rational){16, 9};
        }
        else
        {
            /* TODO: is this code correct? */

            imageAspectRatio = (codecContext->width * codecContext->sample_aspect_ratio.num) /
                (double)(codecContext->height * codecContext->sample_aspect_ratio.den);
            if (imageAspectRatio <= imageAspectRatio4By3 + 0.01 && imageAspectRatio >= imageAspectRatio4By3 - 0.01)
            {
                videoStream->outputStream.streamInfo.aspectRatio = (Rational){4, 3};
            }
            else if (imageAspectRatio <= imageAspectRatio16By9 + 0.01 && imageAspectRatio >= imageAspectRatio16By9 - 0.01)
            {
                videoStream->outputStream.streamInfo.aspectRatio = (Rational){16, 9};
            }
            else
            {
                ml_log_warn("Unknown sample aspect ratio %d/%d; assuming 4:3\n", codecContext->sample_aspect_ratio.num, codecContext->sample_aspect_ratio.den);
                videoStream->outputStream.streamInfo.aspectRatio = (Rational){4, 3};
            }
        }
    }

    switch (codecContext->pix_fmt)
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUVJ420P:
            videoStream->outputStream.streamInfo.format = YUV420_FORMAT;
            videoStream->dataOffset[0] = 0;
            videoStream->dataOffset[1] = codecContext->width * codecContext->height;
            videoStream->dataOffset[2] = codecContext->width * codecContext->height * 5 / 4;
            videoStream->lineSize[0] = codecContext->width;
            videoStream->lineSize[1] = codecContext->width / 2;
            videoStream->lineSize[2] = codecContext->width / 2;
            videoStream->outputPixelFormat = PIX_FMT_YUV420P;
            videoStream->outputStream.bufferSize = codecContext->width * codecContext->height * 3 / 2;
            videoStream->outputStream.dataSize = videoStream->outputStream.bufferSize;
            break;
#if 0 /* TODO: add support for YUV411 to the sinks */
        case PIX_FMT_YUV411P:
            videoStream->outputStream.streamInfo.format = YUV411_FORMAT;
            videoStream->dataOffset[0] = 0;
            videoStream->dataOffset[1] = codecContext->width * codecContext->height;
            videoStream->dataOffset[2] = codecContext->width * codecContext->height * 5 / 4;
            videoStream->lineSize[0] = codecContext->width;
            videoStream->lineSize[1] = codecContext->width / 4;
            videoStream->lineSize[2] = codecContext->width / 4;
            videoStream->outputPixelFormat = PIX_FMT_YUV411P;
            videoStream->outputStream.bufferSize = codecContext->width * codecContext->height * 3 / 2;
            videoStream->outputStream.dataSize = videoStream->outputStream.bufferSize;
            break;
#endif
        case PIX_FMT_YUV422P:
        case PIX_FMT_YUVJ422P:
            videoStream->outputStream.streamInfo.format = YUV422_FORMAT;
            videoStream->dataOffset[0] = 0;
            videoStream->dataOffset[1] = codecContext->width * codecContext->height;
            videoStream->dataOffset[2] = codecContext->width * codecContext->height * 3 / 2;
            videoStream->lineSize[0] = codecContext->width;
            videoStream->lineSize[1] = codecContext->width / 2;
            videoStream->lineSize[2] = codecContext->width / 2;
            videoStream->outputPixelFormat = PIX_FMT_YUV422P;
            videoStream->outputStream.bufferSize = codecContext->width * codecContext->height * 2;
            videoStream->outputStream.dataSize = videoStream->outputStream.bufferSize;
            break;
        default:
            videoStream->outputStream.streamInfo.format = UYVY_FORMAT;
            videoStream->dataOffset[0] = 0;
            videoStream->dataOffset[1] = 0;
            videoStream->dataOffset[2] = 0;
            videoStream->lineSize[0] = codecContext->width * 2;
            videoStream->lineSize[1] = 0;
            videoStream->lineSize[2] = 0;
            videoStream->outputPixelFormat = PIX_FMT_UYVY422;
            videoStream->outputStream.bufferSize = codecContext->width * codecContext->height * 2;
            videoStream->outputStream.dataSize = videoStream->outputStream.bufferSize;
            break;
    }

    videoStream->outputStream.buffer = av_malloc(videoStream->outputStream.bufferSize);
    if (videoStream->outputStream.buffer == NULL)
    {
        ml_log_error("Failed to allocate video output buffer\n");
        avcodec_close(codecContext);
        return 0;
    }


    source->numVideoStreams++;

    return 1;
}

static void close_video_stream(FFMPEGSource* source, VideoStream* videoStream)
{
    AVStream* stream = source->formatContext->streams[videoStream->avStreamIndex];
    AVCodecContext* codecContext = stream->codec;

    if (videoStream->frame != NULL)
    {
        av_freep(&videoStream->frame);
    }

    if (videoStream->outputStream.buffer != NULL)
    {
        av_freep(&videoStream->outputStream.buffer);
        videoStream->outputStream.bufferSize = 0;
        videoStream->outputStream.dataSize = 0;
    }

    if (videoStream->imageConvertContext != NULL)
    {
        sws_freeContext(videoStream->imageConvertContext);
        videoStream->imageConvertContext = NULL;
    }

    avcodec_close(codecContext);

    clear_stream_info(&videoStream->outputStream.streamInfo);
}

static int allocate_audio_output_buffers(FFMPEGSource* source)
{
    AudioStream* audioStream;
    int i;
    int j;
    int audioFrameSize = (int)(48000 * source->frameRate.den / (double)source->frameRate.num + 0.5);

    for (i = 0; i < source->numAudioStreams; i++)
    {
        audioStream = &source->audioStreams[i];

        for (j = 0; j < audioStream->numOutputStreams; j++)
        {
            /* deallocate previously allocated buffer, e.g. when setting the frame rate */
            SAFE_FREE(&audioStream->outputStreams[j].buffer);

            audioStream->outputStreams[j].bufferSize = audioFrameSize * 2;
            audioStream->outputStreams[j].dataSize = audioStream->outputStreams[j].bufferSize;
            audioStream->outputStreams[j].buffer = av_malloc(audioStream->outputStreams[j].bufferSize);
            if (audioStream->outputStreams[j].buffer == NULL)
            {
                ml_log_error("Failed to allocate audio output buffer\n");
                return 0;
            }
        }
    }

    return 1;
}

static int open_audio_stream(FFMPEGSource* source, int sourceId, int avStreamIndex, int* outputStreamIndex)
{
    AVStream* stream = source->formatContext->streams[avStreamIndex];
    AVCodecContext* codecContext = stream->codec;
    AVCodec* codec = NULL;
    AudioStream* audioStream = &source->audioStreams[source->numAudioStreams];
    int j;


    if (source->numAudioStreams + 1 >= (int)(sizeof(source->audioStreams) / sizeof(AudioStream)))
    {
        ml_log_error("Maximum audio streams (%d) exceeded\n", sizeof(source->audioStreams) / sizeof(AudioStream));
        return 0;
    }


    codec = avcodec_find_decoder(codecContext->codec_id);
    if (codec == NULL)
    {
        ml_log_error("Failed to find audio decoder\n");
        return 0;
    }

    codecContext->debug_mv = 0;
    codecContext->debug = 0;
    codecContext->workaround_bugs = 1;
    codecContext->lowres = 0;
    if (codecContext->lowres)
    {
        codecContext->flags |= CODEC_FLAG_EMU_EDGE;
    }
    codecContext->idct_algo = FF_IDCT_AUTO;
    codecContext->skip_frame = AVDISCARD_DEFAULT;
    codecContext->skip_idct = AVDISCARD_DEFAULT;
    codecContext->skip_loop_filter = AVDISCARD_DEFAULT;
    codecContext->error_concealment = 3;

    /* TODO: will this force sample rate conversion if the source is not 48 kHz? */
    codecContext->sample_rate = 48000;

    if (avcodec_open(codecContext, codec) < 0)
    {
        ml_log_error("Failed to open ffmpeg video codec\n");
        return 0;
    }

    if (source->threadCount > 1)
    {
        if (avcodec_thread_init(codecContext, source->threadCount) < 0)
        {
            ml_log_error("Failed to initialise ffmpeg threads\n");
            avcodec_close(codecContext);
            return 0;
        }
        codecContext->thread_count = source->threadCount;
    }


    audioStream->index = source->numAudioStreams;
    audioStream->avStreamIndex = avStreamIndex;

    /* the player doesn't (yet) support multi-channel audio and therefore each channel
    becomes a separate output stream */

    audioStream->numOutputStreams = codecContext->channels;
    if (audioStream->numOutputStreams > (int)(sizeof(audioStream->outputStreams) / sizeof(OutputStreamData)))
    {
        audioStream->numOutputStreams = (int)(sizeof(audioStream->outputStreams) / sizeof(OutputStreamData));
        ml_log_warn("Limiting audio channel-to-streams to %d streams\n", audioStream->numOutputStreams);
    }

    for (j = 0; j < audioStream->numOutputStreams; j++)
    {
        audioStream->outputStreams[j].streamIndex = (*outputStreamIndex)++;

        audioStream->outputStreams[j].streamInfo.type = SOUND_STREAM_TYPE;
        audioStream->outputStreams[j].streamInfo.format = PCM_FORMAT;
        audioStream->outputStreams[j].streamInfo.sourceId = sourceId;

        audioStream->outputStreams[j].streamInfo.samplingRate = (Rational){48000, 1};;
        audioStream->outputStreams[j].streamInfo.numChannels = 1;
        audioStream->outputStreams[j].streamInfo.bitsPerSample = 16;
    }

    audioStream->decodeBufferSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    audioStream->decodeBuffer = av_malloc(audioStream->decodeBufferSize);
    if (audioStream->decodeBuffer == NULL)
    {
        ml_log_error("Failed to allocate audio decode buffer\n");
        avcodec_close(codecContext);
        return 0;
    }


    source->numAudioStreams++;

    return 1;
}

static void close_audio_stream(FFMPEGSource* source, AudioStream* audioStream)
{
    AVStream* stream = source->formatContext->streams[audioStream->avStreamIndex];
    AVCodecContext* codecContext = stream->codec;
    int j;

    for (j = 0; j < audioStream->numOutputStreams; j++)
    {
        if (audioStream->outputStreams[j].buffer != NULL)
        {
            av_freep(&audioStream->outputStreams[j].buffer);
            audioStream->outputStreams[j].bufferSize = 0;
            audioStream->outputStreams[j].dataSize = 0;
        }

        clear_stream_info(&audioStream->outputStreams[j].streamInfo);
    }

    if (audioStream->decodeBuffer != NULL)
    {
        av_freep(&audioStream->decodeBuffer);
        audioStream->decodeBufferSize = 0;
        audioStream->decodeDataSize = 0;
    }

    free_audiostream_packet(audioStream);

    avcodec_close(codecContext);
}

static int open_timecode_stream(FFMPEGSource* source, int sourceId, int* outputStreamIndex)
{
    source->timecodeStream.outputStream.streamIndex = (*outputStreamIndex)++;

    source->timecodeStream.outputStream.streamInfo.type = TIMECODE_STREAM_TYPE;
    source->timecodeStream.outputStream.streamInfo.sourceId = sourceId;
    source->timecodeStream.outputStream.streamInfo.format = TIMECODE_FORMAT;
    source->timecodeStream.outputStream.streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    source->timecodeStream.outputStream.streamInfo.timecodeSubType = NO_TIMECODE_SUBTYPE;

    if (source->formatContext->start_time != (int64_t)AV_NOPTS_VALUE)
    {
        source->timecodeStream.startTimecode = (int64_t)(source->formatContext->start_time / ((double)AV_TIME_BASE) *
            source->frameRate.num / (double)source->frameRate.den + 0.5);
    }
    else
    {
        source->timecodeStream.startTimecode = 0;
    }
    source->timecodeStream.timecodeBase = get_rounded_frame_rate(&source->frameRate);

    return 1;
}

static void close_timecode_stream(FFMPEGSource* source)
{
    clear_stream_info(&source->timecodeStream.outputStream.streamInfo);
}

static int process_video_packets(FFMPEGSource* source, VideoStream* videoStream)
{
    AVStream* stream = source->formatContext->streams[videoStream->avStreamIndex];
    AVCodecContext* codecContext = stream->codec;
    AVPacket packet;
    AVPicture pict;
    int havePicture;
    int result;
    double pts;
    int64_t packetPosition;


    if (!pop_packet(&videoStream->packetQueue, &packet))
    {
        return NO_PACKET_IN_QUEUE;
    }
    DEBUG("Popped video packet (%d)\n", videoStream->packetQueue.size);


    /* decode the packet data */

    /* g_videoPacketPTS is set in the AVFrame and therefore tells us the pts for the decoded frame
       which could be in a different order than the packets */
    g_videoPacketPTS = packet.pts;

    result = avcodec_decode_video(codecContext, videoStream->frame, &havePicture, packet.data, packet.size);
    if (result < 0)
    {
        ml_log_error("Failed to decode video frame\n");
        result = PROCESS_PACKETS_FAILED;
        goto done;
    }
    if (!havePicture)
    {
        /* why? non-index frame? */
        DEBUG("Video packet has no video data\n");
        result = NO_FRAME_DATA;
        goto done;
    }

#if 0
    {
        int ftype;
        if (videoStream->frame->pict_type == FF_B_TYPE)
            ftype = 'B';
        else if (videoStream->frame->pict_type == FF_I_TYPE)
            ftype = 'I';
        else
            ftype = 'P';
        printf("++++++++ frame_type=%c (%d)\n", ftype, videoStream->frame->pict_type);
    }
#endif

    /* get the decoded frame's presentation timestamp */

    if (packet.dts == (int64_t)AV_NOPTS_VALUE &&
        videoStream->frame->opaque && *(int64_t*)videoStream->frame->opaque != (int64_t)AV_NOPTS_VALUE)
    {
        if (stream->start_time != (int64_t)AV_NOPTS_VALUE)
        {
            pts = *(int64_t*)videoStream->frame->opaque - stream->start_time;
        }
        else
        {
            pts = *(int64_t*)videoStream->frame->opaque;
        }
    }
    else if (packet.dts != (int64_t)AV_NOPTS_VALUE)
    {
        if (stream->start_time != (int64_t)AV_NOPTS_VALUE)
        {
            pts = packet.dts - stream->start_time;
        }
        else
        {
            pts = packet.dts;
        }
    }
    else
    {
        /* TODO: what now? */
        pts = 0;
    }
    pts *= av_q2d(stream->time_base);
    packetPosition = (int64_t)(pts * source->frameRate.num / (double)source->frameRate.den + 0.5);

    DEBUG("Video pts=%"PFi64", audio pts=%"PFi64", packet pos=%"PFi64", source pos=%"PFi64", file pos=%"PFi64" (%"PFi64")\n",
        packetPosition, (int64_t)(pts * 48000 + 0.5), packetPosition, source->position, packet.pos, url_ftell(source->formatContext->pb));
    DEBUG("Video search index = %d\n", av_index_search_timestamp(stream, pts, AVSEEK_FLAG_BACKWARD));

    if (packetPosition < source->position)
    {
        /* not there yet - keep decoding */
        result = EARLY_FRAME;
        goto done;
    }
    else if (packetPosition > source->position)
    {
        /* missed the frame - need to go back */
        result = LATE_FRAME;
        goto done;
    }


    /* convert frame to required pixel format */

    pict.data[0] = &videoStream->outputStream.buffer[0] + videoStream->dataOffset[0];
    pict.linesize[0] = videoStream->lineSize[0];
    if (videoStream->dataOffset[1] <= videoStream->dataOffset[0])
    {
        pict.data[1] = NULL;
        pict.linesize[1] = 0;
    }
    else
    {
        pict.data[1] = &videoStream->outputStream.buffer[0] + videoStream->dataOffset[1];
        pict.linesize[1] = videoStream->lineSize[1];
    }
    if (videoStream->dataOffset[2] <= videoStream->dataOffset[1])
    {
        pict.data[2] = NULL;
        pict.linesize[2] = 0;
    }
    else
    {
        pict.data[2] = &videoStream->outputStream.buffer[0] + videoStream->dataOffset[2];
        pict.linesize[2] = videoStream->lineSize[2];
    }

    videoStream->imageConvertContext = sws_getCachedContext(videoStream->imageConvertContext,
        codecContext->width, codecContext->height,
        codecContext->pix_fmt,
        codecContext->width, codecContext->height,
        videoStream->outputPixelFormat,
        SWS_BICUBIC, NULL, NULL, NULL);

    if (videoStream->imageConvertContext == NULL)
    {
        ml_log_error("Cannot initialize the FFmpeg video conversion context\n");
        result = PROCESS_PACKETS_FAILED;
        goto done;
    }

    sws_scale(videoStream->imageConvertContext,
        videoStream->frame->data, videoStream->frame->linesize, 0, codecContext->height,
        pict.data, pict.linesize);


    videoStream->isReady = 1;
    result = PROCESS_PACKETS_SUCCESS;

done:
    av_free_packet(&packet);
    return result;
}

static int transfer_audio_samples(FFMPEGSource* source, AudioStream* audioStream, int64_t firstDecodeSample, int numDecodeSamples,
    int64_t targetFirstSample, int targetNumSamplesMin, int targetNumSamplesMax)
{
    AVStream* stream = source->formatContext->streams[audioStream->avStreamIndex];
    AVCodecContext* codecContext = stream->codec;
    int64_t firstTransferSample;
    int numTransferSamples;
    int j;

    DEBUG("Transfer: target=%"PFi64"+%d-%d, decode=%"PFi64"+%d, initial=%"PFi64", ",
        targetFirstSample, targetNumSamplesMin, targetNumSamplesMax, firstDecodeSample, numDecodeSamples, audioStream->initialDecodeSample);

    /* check whether the samples are within the target range for the output streams */

    if (firstDecodeSample + numDecodeSamples <= targetFirstSample)
    {
        /* either old samples or not there yet - keep decoding */
        DEBUG("\n");
        return EARLY_FRAME;
    }
    else if (audioStream->numOutputSamples == 0 && firstDecodeSample > targetFirstSample)
    {
        /* missed the start of the frame - need to go back */
        DEBUG("\n");
        DEBUG("Missed the start of the audio frame\n");
        return LATE_FRAME;
    }
    else if (firstDecodeSample >= targetFirstSample + targetNumSamplesMax)
    {
        /* past the end */
        DEBUG("\n");
        DEBUG("Packet data is beyond the end of the audio frame\n");
        return LATE_FRAME;
    }


    /* deinterleave the target samples from the decode buffer and write to the output buffers */

    firstTransferSample = targetFirstSample;
    if (firstTransferSample < firstDecodeSample)
    {
        firstTransferSample = firstDecodeSample;
    }
    numTransferSamples = (firstDecodeSample + numDecodeSamples) - firstTransferSample;
    if (firstTransferSample + numTransferSamples > targetFirstSample + targetNumSamplesMax)
    {
        numTransferSamples = (targetFirstSample + targetNumSamplesMax) - firstTransferSample;
    }

    for (j = 0; j < audioStream->numOutputStreams; j++)
    {
        deinterleave_audio(
            (int16_t*)(audioStream->decodeBuffer + codecContext->channels * sizeof(int16_t) * (firstTransferSample - firstDecodeSample)),
            codecContext->channels, numTransferSamples, j,
            audioStream->outputStreams[j].buffer + 2 * (firstTransferSample - targetFirstSample));
    }

    if (audioStream->numOutputSamples == 0)
    {
        audioStream->firstOutputSample = firstTransferSample;
        audioStream->numOutputSamples = numTransferSamples;
    }
    else
    {
        if (audioStream->firstOutputSample > firstTransferSample)
        {
            audioStream->firstOutputSample = firstTransferSample;
        }
        if (firstTransferSample + numTransferSamples > audioStream->firstOutputSample + audioStream->numOutputSamples)
        {
            audioStream->numOutputSamples = (firstTransferSample + numTransferSamples) - audioStream->firstOutputSample;
        }
    }

    DEBUG("output=%"PFi64"+%d\n", audioStream->firstOutputSample, audioStream->numOutputSamples);


    return PROCESS_PACKETS_SUCCESS;
}

static int process_audio_packets(FFMPEGSource* source, AudioStream* audioStream)
{
    AVStream* stream = source->formatContext->streams[audioStream->avStreamIndex];
    AVCodecContext* codecContext = stream->codec;
    int dataSize;
    int result;
    double pts;
    int64_t firstDecodeSample;
    int numDecodeSamples;
    int64_t targetFirstSample;
    int targetNumSamplesMin;
    int targetNumSamplesMax;


    targetFirstSample = (int64_t)(source->position * 48000 * source->frameRate.den / (double)source->frameRate.num + 0.5);
    if (source->isPAL)
    {
        targetNumSamplesMin = 1920;
        targetNumSamplesMax = 1920;
    }
    else
    {
        /* TODO: set sequence min and max based on source type */
        targetNumSamplesMin = 1600;
        targetNumSamplesMax = 1602;
    }


    /* transfer existing samples from the decode buffer */

    if (audioStream->numDecodeSamples > 0)
    {
        result = transfer_audio_samples(source, audioStream, audioStream->firstDecodeSample, audioStream->numDecodeSamples,
            targetFirstSample, targetNumSamplesMin, targetNumSamplesMax);
        if (result != PROCESS_PACKETS_SUCCESS && result != EARLY_FRAME)
        {
            goto done;
        }

        if (result == PROCESS_PACKETS_SUCCESS)
        {
            /* check if we have all the samples */

            if (audioStream->firstOutputSample == targetFirstSample &&
                audioStream->numOutputSamples >= targetNumSamplesMin &&
                audioStream->numOutputSamples <= targetNumSamplesMax)
            {
                audioStream->isReady = 1;
                result = PROCESS_PACKETS_SUCCESS;
                goto done;
            }
        }
    }


    /* read the next packet if current packet is empty */

    if (audioStream->packet.size == 0)
    {
        if (!pop_packet(&audioStream->packetQueue, &audioStream->packet))
        {
            return NO_PACKET_IN_QUEUE;
        }
        DEBUG("Popped audio packet (%d)\n", audioStream->packetQueue.size);
        audioStream->packetNextData = audioStream->packet.data;
        audioStream->packetNextSize = audioStream->packet.size;

        audioStream->firstDecodeSample = 0;
        audioStream->numDecodeSamples = 0;
    }


    /* get the packet's presentation timestamp */

    if (audioStream->packet.pts != (int64_t)AV_NOPTS_VALUE)
    {
        if (stream->start_time != (int64_t)AV_NOPTS_VALUE)
        {
            pts = av_q2d(stream->time_base) * (audioStream->packet.pts - stream->start_time);
        }
        else
        {
            pts = av_q2d(stream->time_base) * audioStream->packet.pts;
        }
    }
    else
    {
        /* TODO: what now? */
        pts = 0;
    }


    /* decode the packet data */

    while (audioStream->packetNextSize > 0)
    {
        dataSize = audioStream->decodeBufferSize;
        result = avcodec_decode_audio2(codecContext, (int16_t*)audioStream->decodeBuffer, &dataSize,
            audioStream->packetNextData, audioStream->packetNextSize);
        if (result < 0)
        {
            ml_log_error("Failed to decode audio packet data\n");
            result = PROCESS_PACKETS_FAILED;
            goto done;
        }

        audioStream->packetNextData += result;
        audioStream->packetNextSize -= result;

        if (dataSize <= 0)
        {
            continue;
        }

        /* calc samples available in the decode buffer */

        if (audioStream->numDecodeSamples == 0)
        {
            firstDecodeSample = (int64_t)(pts * 48000 + 0.5);
            audioStream->initialDecodeSample = firstDecodeSample;
        }
        else
        {
            firstDecodeSample = audioStream->firstDecodeSample + audioStream->numDecodeSamples;
        }
        numDecodeSamples = dataSize / (sizeof(int16_t) * codecContext->channels);

        audioStream->firstDecodeSample = firstDecodeSample;
        audioStream->numDecodeSamples = numDecodeSamples;


        /* transfer samples from decode buffer */

        result = transfer_audio_samples(source, audioStream, firstDecodeSample, numDecodeSamples, targetFirstSample, targetNumSamplesMin, targetNumSamplesMax);
        if (result != PROCESS_PACKETS_SUCCESS && result != EARLY_FRAME)
        {
            goto done;
        }


        /* check if we have all the samples */

        if (audioStream->firstOutputSample == targetFirstSample &&
            audioStream->numOutputSamples >= targetNumSamplesMin &&
            audioStream->numOutputSamples <= targetNumSamplesMax)
        {
            audioStream->isReady = 1;
            result = PROCESS_PACKETS_SUCCESS;
            goto done;
        }
    }


    result = PROCESS_PACKETS_SUCCESS;

done:
    if (audioStream->packetNextSize <= 0)
    {
        free_audiostream_packet(audioStream);
    }
    return result;
}


static int add_source_infos(FFMPEGSource* source, StreamInfo* streamInfo, const char* filename)
{
    int timecodeBase;

    timecodeBase = (int)(source->frameRate.num / (double)(source->frameRate.den) + 0.5);

    CHK_ORET(add_filename_source_info(streamInfo, SRC_INFO_FILE_NAME, filename));
    if (source->formatContext->iformat->name != NULL)
    {
        CHK_ORET(add_known_source_info(streamInfo, SRC_INFO_FILE_TYPE, source->formatContext->iformat->name));
    }
    CHK_ORET(add_timecode_source_info(streamInfo, SRC_INFO_FILE_DURATION, source->length, timecodeBase));
    if (source->formatContext->title[0] != '\0')
    {
        CHK_ORET(add_known_source_info(streamInfo, SRC_INFO_TITLE, source->formatContext->title));
    }

    return 1;
}





static int fms_get_num_streams(void* data)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int result;
    int i;

    result = source->numVideoStreams;
    for (i = 0; i < source->numAudioStreams; i++)
    {
        result += source->audioStreams[i].numOutputStreams;
    }
    result += 1; /* timecode stream */

    return result;
}

static int fms_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    OutputStreamData* outputStream = NULL;

    outputStream = get_output_stream(source, streamIndex);
    if (outputStream == NULL)
    {
        return 0;
    }

    *streamInfo = &outputStream->streamInfo;
    return 1;
}

static void fms_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int i;
    int j;
    int videoFrameRateSet = 0;
    int isHardFrameRate = 0;

    /* check frame rate if source frame rate can't be changed */
    if (source->isHardFrameRate)
    {
        /* disable all streams if frame rate does not match the source's frame rate */
        if (frameRate->num != source->frameRate.num ||
            (frameRate->den != source->frameRate.den))
        {
            for (i = 0; i < source->numVideoStreams; i++)
            {
                VideoStream* videoStream = &source->videoStreams[i];
                OutputStreamData* outputStream = &videoStream->outputStream;

                msc_disable_stream(&source->mediaSource, outputStream->streamIndex);
            }

            msc_disable_audio(&source->mediaSource);

            msc_disable_stream(&source->mediaSource, source->timecodeStream.outputStream.streamIndex);
        }

        return;
    }

    /* try set video streams frame rate */
    for (i = 0; i < source->numVideoStreams; i++)
    {
        VideoStream* videoStream = &source->videoStreams[i];
        OutputStreamData* outputStream = &videoStream->outputStream;

        if (outputStream->streamInfo.isHardFrameRate &&
            memcmp(frameRate, &outputStream->streamInfo.frameRate, sizeof(*frameRate)) != 0)
        {
            msc_disable_stream(&source->mediaSource, outputStream->streamIndex);
            continue;
        }

        outputStream->streamInfo.frameRate = *frameRate;
        videoFrameRateSet = 1;
        isHardFrameRate = isHardFrameRate || outputStream->streamInfo.isHardFrameRate;
        if (!add_timecode_source_info(&outputStream->streamInfo, SRC_INFO_FILE_DURATION, source->length, get_rounded_frame_rate(frameRate)))
        {
            ml_log_error("Failed to add SRC_INFO_FILE_DURATION to video stream\n");
        }
    }

    /* disable audio and timecode streams if failed to set any video stream frame rate */
    if (source->numVideoStreams > 0 && !videoFrameRateSet)
    {
        msc_disable_audio(&source->mediaSource);
        msc_disable_stream(&source->mediaSource, source->timecodeStream.outputStream.streamIndex);
        return;
    }

    /* set the audio streams frame rate */
    for (i = 0; i < source->numAudioStreams; i++)
    {
        AudioStream* audioStream = &source->audioStreams[i];

        for (j = 0; j < audioStream->numOutputStreams; j++)
        {
            OutputStreamData* outputStream = &audioStream->outputStreams[j];

            if (outputStream->streamInfo.isHardFrameRate &&
                memcmp(frameRate, &outputStream->streamInfo.frameRate, sizeof(*frameRate)) != 0)
            {
                msc_disable_stream(&source->mediaSource, outputStream->streamIndex);
                continue;
            }

            outputStream->streamInfo.frameRate = *frameRate;
            if (!add_timecode_source_info(&outputStream->streamInfo, SRC_INFO_FILE_DURATION, source->length, get_rounded_frame_rate(frameRate)))
            {
                ml_log_error("Failed to add SRC_INFO_FILE_DURATION to audio stream\n");
            }
        }
    }
    /* reallocate the audio stream buffers */
    if (!allocate_audio_output_buffers(source))
    {
        ml_log_error("Failed to re-allocate audio output buffers - disabling ffmpeg audio stream\n");
        msc_disable_audio(&source->mediaSource);
    }

    /* set the timecode stream frame rate */
    if (source->timecodeStream.outputStream.streamInfo.isHardFrameRate &&
        memcmp(frameRate, &source->timecodeStream.outputStream.streamInfo.frameRate, sizeof(*frameRate)) != 0)
    {
        msc_disable_stream(&source->mediaSource, source->timecodeStream.outputStream.streamIndex);
    }
    else
    {
        source->timecodeStream.startTimecode = convert_non_drop_timecode(source->timecodeStream.startTimecode,
            &source->timecodeStream.outputStream.streamInfo.frameRate, frameRate);
        source->timecodeStream.timecodeBase = get_rounded_frame_rate(frameRate);
        source->timecodeStream.outputStream.streamInfo.frameRate = *frameRate;
    }


    /* set the source frame rate */
    source->length = convert_length(source->length, &source->frameRate, frameRate);
    source->position = convert_length(source->position, &source->frameRate, frameRate);
    source->frameRate = *frameRate;
    source->isHardFrameRate = isHardFrameRate;
}

static int fms_disable_stream(void* data, int streamIndex)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    OutputStreamData* outputStream = NULL;

    outputStream = get_output_stream(source, streamIndex);
    if (outputStream == NULL)
    {
        return 0;
    }

    outputStream->isDisabled = 1;
    return 1;
}

static void fms_disable_audio(void* data)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int i;
    int j;

    for (i = 0; i < source->numAudioStreams; i++)
    {
        for (j = 0; j < source->audioStreams[i].numOutputStreams; j++)
        {
            source->audioStreams[i].outputStreams[j].isDisabled = 1;
        }
    }
}

static int fms_stream_is_disabled(void* data, int streamIndex)
{
    FFMPEGSource* source = (FFMPEGSource*)data;

    OutputStreamData* outputStream = NULL;

    outputStream = get_output_stream(source, streamIndex);
    if (outputStream == NULL)
    {
        return 1;
    }

    return outputStream->isDisabled;
}

static int fms_is_seekable(void* data)
{
    return 1;
}

static int fms_seek(void* data, int64_t position)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int64_t timestamp;
    int result;
    int i;
    int64_t seekPosition;

    if (is_disabled(source))
    {
        return 0;
    }

    /* FFmpeg's MXF format implementation only seeks in whole seconds and therefore we
    seek to the previous whole second position and rely on the EARLY_FRAME result when
    processing packets to get the frame reading positioned at the target frame */
    if (source->isMXFFormat)
    {
        seekPosition = (int64_t)(position * source->frameRate.den / source->frameRate.num) *
            source->frameRate.num / source->frameRate.den;
    }
    else
    {
        seekPosition = position;
    }

    timestamp = (int64_t)(seekPosition * (source->frameRate.den / (double)source->frameRate.num) * AV_TIME_BASE + 0.5);
    if (source->formatContext->start_time != (int64_t)AV_NOPTS_VALUE)
    {
        timestamp += source->formatContext->start_time;
    }

    /* the AVSEEK_FLAG_BACKWARD flag means we end up at or before the target position */
    result = av_seek_frame(source->formatContext, -1, timestamp, AVSEEK_FLAG_BACKWARD);
    if (result < 0)
    {
        ml_log_error("Could not seek to position %0.3f\n", (double)timestamp / AV_TIME_BASE);
        return -1;
    }


    /* flush the buffers */
    for (i = 0; i < source->numVideoStreams; i++)
    {
        flush_video_buffers(source, &source->videoStreams[i]);
    }
    for (i = 0; i < source->numAudioStreams; i++)
    {
        flush_audio_buffers(source, &source->audioStreams[i]);
    }


    source->position = position;

    return 0;
}

static int fms_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int result;
    AVPacket packet;
    AudioStream* audioStream = NULL;
    VideoStream* videoStream = NULL;
    int i;
    int j;
    int allStreamsReady;
    unsigned char* buffer = NULL;
    int error;
    int adjustReadStartCount = 0;
    int sendBlankFrame = 0;
    int64_t position;
    Timecode* timecodeEvent;

    memset(&packet, 0, sizeof(packet));


    if (is_disabled(source))
    {
        return 0;
    }


    DEBUG("********************** read frame\n");

    clear_stream_data(source);


    while (1)
    {
        /* process audio and video packets in the queue */

        result = PROCESS_PACKETS_SUCCESS;
        allStreamsReady = 1;
        for (i = 0; i < source->numVideoStreams + source->numAudioStreams; i++)
        {
            if (i < source->numVideoStreams)
            {
                videoStream = &source->videoStreams[i];

                while (!videoStream->isReady)
                {
                    result = process_video_packets(source, videoStream);
                    if (result == PROCESS_PACKETS_FAILED)
                    {
                        ml_log_error("Failed to process video packet data\n");
                        return -1;
                    }
                    else if (result == NO_PACKET_IN_QUEUE || result == LATE_FRAME)
                    {
                        break;
                    }
                }

                allStreamsReady = allStreamsReady && videoStream->isReady;
            }
            else
            {
                audioStream = &source->audioStreams[i - source->numVideoStreams];

                while (!audioStream->isReady)
                {
                    result = process_audio_packets(source, audioStream);
                    if (result == PROCESS_PACKETS_FAILED)
                    {
                        ml_log_error("Failed to process audio packet data\n");
                        return -1;
                    }
                    else if (result == NO_PACKET_IN_QUEUE || result == LATE_FRAME)
                    {
                        break;
                    }
                }

                allStreamsReady = allStreamsReady && audioStream->isReady;
            }

            if (result == LATE_FRAME || result == NO_PACKET_IN_QUEUE)
            {
                break;
            }
        }

        if (allStreamsReady)
        {
            break;
        }

        DEBUG("Process state = %d\n", result);

        if (result == LATE_FRAME)
        {
            /* seek back adjustReadStartCount + 1 frames */

            clear_stream_data(source);

            adjustReadStartCount++;

            if (adjustReadStartCount > MAX_ADJUST_READ_START_COUNT)
            {
                ml_log_error("Exceeded maximum seek back frames (%d) to find target frame\n", MAX_ADJUST_READ_START_COUNT);
                ml_log_warn("Failed to read frame at position %"PFi64" - sending blank frame\n", source->position);
                sendBlankFrame = 1;
                break;
            }
            else if (source->position - adjustReadStartCount < 0)
            {
                /* can't go back any further than the start - send a blank frame */

                position = source->position;
                if (fms_seek(source->mediaSource.data, 0) != 0)
                {
                    return -1;
                }
                /* fms_seek changed the position - changed it back to the target position */
                source->position = position;

                ml_log_warn("Failed to read frame at position %"PFi64" - sending blank frame\n", source->position);
                sendBlankFrame = 1;
                break;
            }

            DEBUG("---- Adjusting start to position %"PFi64"\n", source->position - adjustReadStartCount);
            position = source->position;
            if (fms_seek(source->mediaSource.data, source->position - adjustReadStartCount) != 0)
            {
                return -1;
            }
            /* fms_seek changed the position - changed it back to the target position */
            source->position = position;
        }
        else /* result == NO_PACKET_IN_QUEUE */
        {
            /* read the next frame */

            DEBUG("File position before av_read_frame=%"PFi64"\n", url_ftell(source->formatContext->pb));

            result = av_read_frame(source->formatContext, &packet);
            if (result < 0)
            {
#if (LIBAVFORMAT_VERSION_INT >= ((52<<16)+(0<<8)+0))
                error = url_ferror(source->formatContext->pb);
#else
                error = url_ferror(&source->formatContext->pb);
#endif
                if (error != 0)
                {
                    ml_log_error("Failed to read frame from file (%d)\n", error);
                    return -1;
                }
                else
                {
                    ml_log_warn("Unexpected EOF\n");
                    /* TODO: modify the source length and the media_player needs to read the source length again */
                    return -1;
                }
            }

            DEBUG("File position after av_read_frame=%"PFi64"\n", url_ftell(source->formatContext->pb));


            /* push packet onto queue */

            if ((videoStream = get_video_stream(source, packet.stream_index)) != NULL)
            {
                if (!push_packet(&videoStream->packetQueue, &packet))
                {
                    av_free_packet(&packet);
                    return -1;
                }
                DEBUG("Pushed video packet (%d)\n", videoStream->packetQueue.size);
            }
            else if ((audioStream = get_audio_stream(source, packet.stream_index)) != NULL)
            {
                if (!push_packet(&audioStream->packetQueue, &packet))
                {
                    av_free_packet(&packet);
                    return -1;
                }
                DEBUG("Pushed audio packet (%d)\n", audioStream->packetQueue.size);
            }
            else
            {
                DEBUG("Freeing unknown packet type\n");
                av_free_packet(&packet);
            }
        }
    }

    source->position++;


    DEBUG("Sending frame\n");

    /* send video output stream data */

    for (i = 0; i < source->numVideoStreams; i++)
    {
        videoStream = &source->videoStreams[i];
        if (!videoStream->outputStream.isDisabled)
        {
            if (sdl_accept_frame(listener, videoStream->outputStream.streamIndex, frameInfo))
            {
                if (!sdl_allocate_buffer(listener, videoStream->outputStream.streamIndex, &buffer, videoStream->outputStream.dataSize))
                {
                    return -1;
                }

                if (sendBlankFrame)
                {
                    fill_black(videoStream->outputStream.streamInfo.format, videoStream->outputStream.streamInfo.width,
                        videoStream->outputStream.streamInfo.height, buffer);
                }
                else
                {
                    memcpy(buffer, videoStream->outputStream.buffer, videoStream->outputStream.dataSize);
                }
                sdl_receive_frame(listener, videoStream->outputStream.streamIndex, buffer, videoStream->outputStream.dataSize);
            }
        }
    }


    /* send audio output stream data */

    for (i = 0; i < source->numAudioStreams; i++)
    {
        audioStream = &source->audioStreams[i];

        for (j = 0; j < audioStream->numOutputStreams; j++)
        {
            if (!audioStream->outputStreams[j].isDisabled)
            {
                if (sdl_accept_frame(listener, audioStream->outputStreams[j].streamIndex, frameInfo))
                {
                    if (!sdl_allocate_buffer(listener, audioStream->outputStreams[j].streamIndex, &buffer, audioStream->outputStreams[j].dataSize))
                    {
                        return -1;
                    }

                    if (sendBlankFrame)
                    {
                        memset(buffer, 0, audioStream->outputStreams[j].dataSize);
                    }
                    else
                    {
                        memcpy(buffer, audioStream->outputStreams[j].buffer, audioStream->outputStreams[j].dataSize);
                    }
                    sdl_receive_frame(listener, audioStream->outputStreams[j].streamIndex, buffer, audioStream->outputStreams[j].dataSize);
                }
            }
        }
    }


    /* send timecode stream data */

    position = source->timecodeStream.startTimecode + source->position - 1;

    if (sdl_accept_frame(listener, source->timecodeStream.outputStream.streamIndex, frameInfo))
    {
        CHK_ORET(sdl_allocate_buffer(listener, source->timecodeStream.outputStream.streamIndex, &buffer, sizeof(Timecode)));

        timecodeEvent = (Timecode*)buffer;
        timecodeEvent->isDropFrame = 0;
        timecodeEvent->hour = position / (60 * 60 * source->timecodeStream.timecodeBase);
        timecodeEvent->min = (position % (60 * 60 * source->timecodeStream.timecodeBase)) /
            (60 * source->timecodeStream.timecodeBase);
        timecodeEvent->sec = ((position % (60 * 60 * source->timecodeStream.timecodeBase)) %
            (60 * source->timecodeStream.timecodeBase)) /
            source->timecodeStream.timecodeBase;
        timecodeEvent->frame = ((position % (60 * 60 * source->timecodeStream.timecodeBase)) %
            (60 * source->timecodeStream.timecodeBase)) %
            source->timecodeStream.timecodeBase;

        CHK_ORET(sdl_receive_frame(listener, source->timecodeStream.outputStream.streamIndex, buffer, sizeof(Timecode)));
    }


    return 0;
}

static int fms_get_length(void* data, int64_t* length)
{
    FFMPEGSource* source = (FFMPEGSource*)data;

    if (source->length <= 0)
    {
        return 0;
    }

    *length = source->length;
    return 1;
}

static int fms_get_position(void* data, int64_t* position)
{
    FFMPEGSource* source = (FFMPEGSource*)data;

    *position = source->position;
    return 1;
}

static int fms_get_available_length(void* data, int64_t* length)
{
    FFMPEGSource* source = (FFMPEGSource*)data;

    if (source->length <= 0)
    {
        return 0;
    }

    *length = source->length;
    return 1;
}

static int fms_eof(void* data)
{
    FFMPEGSource* source = (FFMPEGSource*)data;

    if (source->length <= 0)
    {
        /* if length is unknown then we haven't reached the end */
        return 0;
    }
    else if (source->position >= source->length)
    {
        return 1;
    }

    return 0;
}

static void fms_set_source_name(void* data, const char* name)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int i;
    int j;

    for (i = 0; i < source->numVideoStreams; i++)
    {
        add_known_source_info(&source->videoStreams[i].outputStream.streamInfo, SRC_INFO_NAME, name);
    }

    for (i = 0; i < source->numAudioStreams; i++)
    {
        for (j = 0; j < source->audioStreams[i].numOutputStreams; j++)
        {
            add_known_source_info(&source->audioStreams[i].outputStreams[j].streamInfo, SRC_INFO_NAME, name);
        }
    }

    add_known_source_info(&source->timecodeStream.outputStream.streamInfo, SRC_INFO_NAME, name);
}

static void fms_set_clip_id(void* data, const char* id)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int i;
    int j;

    for (i = 0; i < source->numVideoStreams; i++)
    {
        set_stream_clip_id(&source->videoStreams[i].outputStream.streamInfo, id);
    }

    for (i = 0; i < source->numAudioStreams; i++)
    {
        for (j = 0; j < source->audioStreams[i].numOutputStreams; j++)
        {
            set_stream_clip_id(&source->audioStreams[i].outputStreams[j].streamInfo, id);
        }
    }

    set_stream_clip_id(&source->timecodeStream.outputStream.streamInfo, id);
}

static void fms_close(void* data)
{
    FFMPEGSource* source = (FFMPEGSource*)data;
    int i;

    if (data == NULL)
    {
        return;
    }


    for (i = 0; i < source->numVideoStreams; i++)
    {
        close_video_stream(source, &source->videoStreams[i]);
    }
    for (i = 0; i < source->numAudioStreams; i++)
    {
        close_audio_stream(source, &source->audioStreams[i]);
    }
    close_timecode_stream(source);

    if (source->formatContext != NULL)
    {
        av_close_input_file(source->formatContext);
        source->formatContext = NULL;
    }


    SAFE_FREE(&source);
}



int fms_open(const char* filename, int threadCount, MediaSource** source)
{
    FFMPEGSource* newSource = NULL;
    AVFormatParameters formatParams;
    AVInputFormat* inputFormat = NULL;
    AVCodecContext* codecContext = NULL;
    int result;
    int i;
    int j;
    int outputStreamIndex = 0;
    int sourceId;

    memset(&formatParams, 0, sizeof(formatParams));

    av_register_all();


    CALLOC_ORET(newSource, FFMPEGSource, 1);
    newSource->threadCount = threadCount;
    newSource->frameRate = g_palFrameRate;
    newSource->isHardFrameRate = 0;
    newSource->isPAL = 1;


    /* open the file */

    formatParams.width = 0;
    formatParams.height= 0;
    formatParams.time_base = (AVRational){1, 25}; /* default */
    formatParams.pix_fmt = PIX_FMT_NONE;

    result = av_open_input_file(&newSource->formatContext, filename, inputFormat, 0, &formatParams);
    if (result < 0)
    {
        switch (result)
        {
            case AVERROR_NUMEXPECTED:
                ml_log_error("%s: Incorrect image filename syntax.\n", filename);
                break;
            case AVERROR_INVALIDDATA:
                ml_log_error("%s: Error while parsing header\n", filename);
                break;
            case AVERROR_NOFMT:
                ml_log_error("%s: Unknown format\n", filename);
                break;
            case AVERROR(EIO):
                ml_log_error("%s: I/O error occured\n", filename);
                break;
            case AVERROR(ENOMEM):
                ml_log_error("%s: memory allocation error occured\n", filename);
                break;
            case AVERROR(ENOENT):
                ml_log_error("%s: no such file or directory\n", filename);
                break;
            default:
                ml_log_error("%s: Error while opening file\n", filename);
                break;
        }
        goto fail;
    }

    result = av_find_stream_info(newSource->formatContext);
    if (result < 0)
    {
        ml_log_error("%s: could not find codec parameters\n", filename);
        goto fail;
    }


    /* Detect the MXF format to workaround it's rudimentary byte seek */

    if (newSource->formatContext->iformat != NULL && newSource->formatContext->iformat->name != NULL &&
        strcmp(newSource->formatContext->iformat->name, "mxf") == 0)
    {
        newSource->isMXFFormat = 1;
    }


    /* open the audio, video and timecode streams */

    sourceId = msc_create_id();

    for (i = 0; i < (int)newSource->formatContext->nb_streams; i++)
    {
        codecContext = newSource->formatContext->streams[i]->codec;

        switch (codecContext->codec_type)
        {
            case CODEC_TYPE_AUDIO:
                DEBUG("AV stream %d is audio\n", i);
                CHK_OFAIL(open_audio_stream(newSource, sourceId, i, &outputStreamIndex));
                break;

            case CODEC_TYPE_VIDEO:
                DEBUG("AV stream %d is video\n", i);
                CHK_OFAIL(open_video_stream(newSource, sourceId, i, &outputStreamIndex));
                break;

            default:
                break;
        }
    }
    CHK_OFAIL(open_timecode_stream(newSource, sourceId, &outputStreamIndex));

    /* set the frame rate in the audio streams and timecode stream */

    for (i = 0; i < newSource->numAudioStreams; i++)
    {
        for (j = 0; j < newSource->audioStreams[i].numOutputStreams; j++)
        {
            newSource->audioStreams[i].outputStreams[j].streamInfo.frameRate = newSource->frameRate;
            newSource->audioStreams[i].outputStreams[j].streamInfo.isHardFrameRate = newSource->isHardFrameRate;
        }
    }
    newSource->timecodeStream.outputStream.streamInfo.frameRate = newSource->frameRate;
    newSource->timecodeStream.outputStream.streamInfo.isHardFrameRate = newSource->isHardFrameRate;

    /* allocate the audio buffers now that we know what the video frame rate it */

    CHK_OFAIL(allocate_audio_output_buffers(newSource));


    /* length in newSource->frameRate units */

    newSource->length = (int64_t)(newSource->formatContext->duration / ((double)AV_TIME_BASE) *
        newSource->frameRate.num / (double)(newSource->frameRate.den) + 0.5);
    DEBUG("Duration = %"PFi64"\n", newSource->length);
    /* TODO: found that FFMPEG sometimes rounds the length up rather than down */

    DEBUG("Default stream index for seeking is %d\n", av_find_default_stream_index(newSource->formatContext));


    /* add source infos */

    for (i = 0; i < newSource->numVideoStreams; i++)
    {
        CHK_OFAIL(add_source_infos(newSource, &newSource->videoStreams[i].outputStream.streamInfo, filename));
    }
    for (i = 0; i < newSource->numAudioStreams; i++)
    {
        for (j = 0; j < newSource->audioStreams[i].numOutputStreams; j++)
        {
            CHK_OFAIL(add_source_infos(newSource, &newSource->audioStreams[i].outputStreams[j].streamInfo, filename));
        }
    }
    CHK_OFAIL(add_source_infos(newSource, &newSource->timecodeStream.outputStream.streamInfo, filename));



    newSource->mediaSource.get_num_streams = fms_get_num_streams;
    newSource->mediaSource.get_stream_info = fms_get_stream_info;
    newSource->mediaSource.set_frame_rate_or_disable = fms_set_frame_rate_or_disable;
    newSource->mediaSource.disable_stream = fms_disable_stream;
    newSource->mediaSource.disable_audio = fms_disable_audio;
    newSource->mediaSource.stream_is_disabled = fms_stream_is_disabled;
    newSource->mediaSource.read_frame = fms_read_frame;
    newSource->mediaSource.is_seekable = fms_is_seekable;
    newSource->mediaSource.seek = fms_seek;
    newSource->mediaSource.get_length = fms_get_length;
    newSource->mediaSource.get_position = fms_get_position;
    newSource->mediaSource.get_available_length = fms_get_available_length;
    newSource->mediaSource.eof = fms_eof;
    newSource->mediaSource.set_source_name = fms_set_source_name;
    newSource->mediaSource.set_clip_id = fms_set_clip_id;
    newSource->mediaSource.close = fms_close;
    newSource->mediaSource.data = newSource;



    *source = &newSource->mediaSource;
    return 1;


fail:
    fms_close(newSource);
    return 0;
}


#endif

