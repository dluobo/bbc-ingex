/*
 * $Id: audio_sink.c,v 1.8 2009/01/29 07:10:26 stuart_hc Exp $
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
#include <unistd.h>

#if !defined(HAVE_PORTAUDIO)

#include "audio_sink.h"

int aus_create_audio_sink(MediaSink* targetSink, int audioDevice, AudioSink** sink)
{
    return 0;
}

MediaSink* aus_get_media_sink(AudioSink* sink)
{
    return NULL;
}


#else /* defined(HAVE_PORTAUDIO) */


#include <portaudio.h>

#include "audio_sink.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define MAX_AUDIO_STREAMS           2

typedef struct
{
    int streamId;

    unsigned int bufferSize[2];
    unsigned char* buffer[2];
    unsigned int allocatedBufferSize[2];
    int bufferIsReadyForRead[2];

    unsigned int bufferBytesUsed[2];

    unsigned char* nextSinkBuffer;
    unsigned int nextSinkBufferSize;
    int nextSinkAcceptedFrame;
} AudioStream;

struct AudioSink
{
    int audioDevice;

    int muteAudio;

    MediaSink* nextSink;
    MediaSink sink;

    AudioStream audioStreams[MAX_AUDIO_STREAMS];
    int numAudioStreams;
    int readBuffer;
    int writeBuffer;
    pthread_mutex_t bufferMutex;

    Rational samplingRate;
    int numChannels;
    int bitsPerSample;
    int byteAlignment;


    PaStream* paStream;
    int paStreamStarted;
    int paStreamInitialised;
    PaSampleFormat paFormat;

    int sinkDisabled;

    int stop;
};

static int paAudioCallback(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{

    AudioSink* sink = (AudioSink*)userData;
    unsigned int j;
    unsigned char* buffer = NULL;
    unsigned char* bufferL = NULL;
    unsigned char* bufferR = NULL;
    unsigned int framesToWrite = 0;
    unsigned int bytesToWrite = 0;
    unsigned long totalFramesWritten = 0;
    int bufferIsReadyForRead;
    int returnEmptyFrame;

#if 0
   printf( "Timing info given to callback: Adc: %g, Current: %g, Dac: %g\n",
            timeInfo->inputBufferAdcTime,
            timeInfo->currentTime,
            timeInfo->outputBufferDacTime );
    {
        static struct timeval prev;
        struct timeval now;

        gettimeofday(&now, NULL);
        long diff = (now.tv_sec - prev.tv_sec) * 1000000 + now.tv_usec - prev.tv_usec;
        printf("%ld\n", diff);

        prev = now;
    }
#endif

    while (!sink->stop && totalFramesWritten < framesPerBuffer)
    {
        bufferIsReadyForRead = 1;
        returnEmptyFrame = 0;

        /* check if the source buffer is ready */
        PTHREAD_MUTEX_LOCK(&sink->bufferMutex);
        if (sink->numAudioStreams == 1)
        {
            if (!sink->audioStreams[0].bufferIsReadyForRead[sink->readBuffer])
            {
                bufferIsReadyForRead = 0;
            }
        }
        else /* (sink->numAudioStreams == 2) */
        {
            if (!sink->audioStreams[0].bufferIsReadyForRead[sink->readBuffer] || !sink->audioStreams[1].bufferIsReadyForRead[sink->readBuffer])
            {
                bufferIsReadyForRead = 0;
            }
        }

        if (!bufferIsReadyForRead && sink->writeBuffer == sink->readBuffer)
        {
            returnEmptyFrame = 1;
        }
        PTHREAD_MUTEX_UNLOCK(&sink->bufferMutex);

        /* return an empty frame if no data is available and not busy filling a buffer */
        if (returnEmptyFrame)
        {
            if (sink->byteAlignment <= 2)
            {
                memset(outputBuffer, 0, framesPerBuffer * sink->byteAlignment * sink->numAudioStreams);
            }
            else
            {
                memset(outputBuffer, 0, framesPerBuffer * sizeof(float) * sink->numAudioStreams);
            }
            break;
        }

        /* sleep and try again if the source buffer is not ready */
        if (!bufferIsReadyForRead)
        {
            usleep(500);
            continue;
        }


        /* transfer samples from source buffer to output */
        if (sink->numAudioStreams == 1)
        {
            buffer = sink->audioStreams[0].buffer[sink->readBuffer] + sink->audioStreams[0].bufferBytesUsed[sink->readBuffer];

            framesToWrite = (sink->audioStreams[0].bufferSize[sink->readBuffer] - sink->audioStreams[0].bufferBytesUsed[sink->readBuffer]) / sink->byteAlignment;
            if (totalFramesWritten + framesToWrite > framesPerBuffer)
            {
                framesToWrite = framesPerBuffer - totalFramesWritten;
            }
            bytesToWrite = framesToWrite * sink->byteAlignment;

            if (sink->byteAlignment == 1)
            {
                int8_t* out = ((int8_t*)outputBuffer) + totalFramesWritten;
                for (j = 0; j < bytesToWrite; j++)
                {
                    *out++ = buffer[j];
                }
            }
            else if (sink->byteAlignment == 2)
            {
                int16_t* out = ((int16_t*)outputBuffer) + totalFramesWritten;
                for (j = 0; j < bytesToWrite; j += 2)
                {
                    *out++ = (int16_t)(((uint16_t)buffer[j]) |
                        ((uint16_t)buffer[j + 1]) << 8);
                }
            }
            else if (sink->byteAlignment == 3)
            {
                float* out = ((float*)outputBuffer) + totalFramesWritten;
                for (j = 0; j < bytesToWrite; j += 3)
                {
                    *out++ = (int32_t)((((uint32_t)buffer[j]) << 8) |
                        (((uint32_t)buffer[j + 1]) << 16) |
                        (((uint32_t)buffer[j + 2]) << 24)) / 2147483648.0;
                }
            }
            else /* (sink->byteAlignment == 4) */
            {
                float* out = ((float*)outputBuffer) + totalFramesWritten;
                for (j = 0; j < bytesToWrite; j += 4)
                {
                    *out++ = (int32_t)(((uint32_t)buffer[j]) |
                        (((uint32_t)buffer[j + 1]) << 8) |
                        (((uint32_t)buffer[j + 2]) << 16) |
                        (((uint32_t)buffer[j + 3]) << 24)) / 2147483648.0;
                }
            }

            sink->audioStreams[0].bufferBytesUsed[sink->readBuffer] += bytesToWrite;
            totalFramesWritten += framesToWrite;
        }
        else /* (sink->numAudioStreams == 2) */
        {
            bufferL = sink->audioStreams[0].buffer[sink->readBuffer] + sink->audioStreams[0].bufferBytesUsed[sink->readBuffer];
            bufferR = sink->audioStreams[1].buffer[sink->readBuffer] + sink->audioStreams[1].bufferBytesUsed[sink->readBuffer];

            framesToWrite = (sink->audioStreams[0].bufferSize[sink->readBuffer] - sink->audioStreams[0].bufferBytesUsed[sink->readBuffer]) / sink->byteAlignment;
            if (totalFramesWritten + framesToWrite > framesPerBuffer)
            {
                framesToWrite = framesPerBuffer - totalFramesWritten;
            }
            bytesToWrite = framesToWrite * sink->byteAlignment;

            if (sink->byteAlignment == 1)
            {
                int8_t* out = ((int8_t*)outputBuffer) + 2 * totalFramesWritten;
                for (j = 0; j < bytesToWrite; j++)
                {
                    *out++ = bufferL[j];
                    *out++ = bufferR[j];
                }
            }
            else if (sink->byteAlignment == 2)
            {
                int16_t* out = ((int16_t*)outputBuffer) + 2 * totalFramesWritten;
                for (j = 0; j < bytesToWrite; j += 2)
                {
                    *out++ = (int16_t)(((uint16_t)bufferL[j]) |
                        ((uint16_t)bufferL[j + 1]) << 8);
                    *out++ = (int16_t)(((uint16_t)bufferR[j]) |
                        ((uint16_t)bufferR[j + 1]) << 8);
                }
            }
            else if (sink->byteAlignment == 3)
            {
                float* out = ((float*)outputBuffer) + 2 * totalFramesWritten;
                for (j = 0; j < bytesToWrite; j += 3)
                {
                    *out++ = (int32_t)((((uint32_t)bufferL[j]) << 8) |
                        (((uint32_t)bufferL[j + 1]) << 16) |
                        (((uint32_t)bufferL[j + 2]) << 24)) / 2147483648.0;
                    *out++ = (int32_t)((((uint32_t)bufferR[j]) << 8) |
                        (((uint32_t)bufferR[j + 1]) << 16) |
                        (((uint32_t)bufferR[j + 2]) << 24)) / 2147483648.0;
                }
            }
            else /* (sink->byteAlignment == 4) */
            {
                float* out = ((float*)outputBuffer) + 2 * totalFramesWritten;
                for (j = 0; j < bytesToWrite; j += 4)
                {
                    *out++ = (int32_t)(((uint32_t)bufferL[j]) |
                        (((uint32_t)bufferL[j + 1]) << 8) |
                        (((uint32_t)bufferL[j + 2]) << 16) |
                        (((uint32_t)bufferL[j + 3]) << 24)) / 2147483648.0;
                    *out++ = (int32_t)(((uint32_t)bufferR[j]) |
                        (((uint32_t)bufferR[j + 1]) << 8) |
                        (((uint32_t)bufferR[j + 2]) << 16) |
                        (((uint32_t)bufferR[j + 3]) << 24)) / 2147483648.0;
                }
            }

            sink->audioStreams[0].bufferBytesUsed[sink->readBuffer] += bytesToWrite;
            sink->audioStreams[1].bufferBytesUsed[sink->readBuffer] += bytesToWrite;
            totalFramesWritten += framesToWrite;
        }

        /* signal buffer can be written to if all samples have been transferred */
        if (sink->audioStreams[0].bufferBytesUsed[sink->readBuffer] == sink->audioStreams[0].bufferSize[sink->readBuffer])
        {
            PTHREAD_MUTEX_LOCK(&sink->bufferMutex);

            sink->audioStreams[0].bufferIsReadyForRead[sink->readBuffer] = 0;
            sink->audioStreams[0].bufferBytesUsed[sink->readBuffer] = 0;
            sink->audioStreams[1].bufferIsReadyForRead[sink->readBuffer] = 0;
            sink->audioStreams[1].bufferBytesUsed[sink->readBuffer] = 0;
            sink->readBuffer = (sink->readBuffer + 1) % 2;

            PTHREAD_MUTEX_UNLOCK(&sink->bufferMutex);
        }
    }


    return paContinue;
}



static int aus_register_listener(void* data, MediaSinkListener* listener)
{
    AudioSink* sink = (AudioSink*)data;

    return msk_register_listener(sink->nextSink, listener);
}

static void aus_unregister_listener(void* data, MediaSinkListener* listener)
{
    AudioSink* sink = (AudioSink*)data;

    msk_unregister_listener(sink->nextSink, listener);
}

static int aus_accept_stream(void* data, const StreamInfo* streamInfo)
{
    AudioSink* sink = (AudioSink*)data;

    if (streamInfo->type == SOUND_STREAM_TYPE &&
        streamInfo->format == PCM_FORMAT &&
        streamInfo->numChannels == 1 && /* only 1 channel per audio track supported for now */
        streamInfo->bitsPerSample <= 32)
    {
        return 1;
    }

    return msk_accept_stream(sink->nextSink, streamInfo);
}

static int aus_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    AudioSink* sink = (AudioSink*)data;
    int nextSinkResult;


    if (msk_accept_stream(sink->nextSink, streamInfo))
    {
        nextSinkResult = msk_register_stream(sink->nextSink, streamId, streamInfo);
    }
    else
    {
        nextSinkResult = 0;
    }


    if (streamInfo->type == SOUND_STREAM_TYPE &&
        streamInfo->format == PCM_FORMAT &&
        streamInfo->numChannels == 1 && /* only 1 channel per audio track supported for now */
        streamInfo->bitsPerSample <= 32)
    {
        if (sink->numAudioStreams < MAX_AUDIO_STREAMS)
        {
            if (sink->numAudioStreams > 0)
            {
                /* check that the second audio has the same parameters as the first */
                if (memcmp(&streamInfo->samplingRate, &sink->samplingRate, sizeof(sink->samplingRate)) != 0 ||
                    streamInfo->numChannels != sink->numChannels ||
                    streamInfo->bitsPerSample != sink->bitsPerSample)
                {
                    return nextSinkResult;
                }
            }

            sink->samplingRate = streamInfo->samplingRate;
            sink->numChannels = streamInfo->numChannels;
            sink->bitsPerSample = streamInfo->bitsPerSample;
            sink->byteAlignment = (streamInfo->bitsPerSample + 7) / 8;

            switch (sink->byteAlignment)
            {
                case 1:
                    sink->paFormat = paInt8;
                    break;
                case 2:
                    sink->paFormat = paInt16;
                    break;
                case 3:
                    sink->paFormat = paFloat32;
                    break;
                default: /* 4 */
                    sink->paFormat = paFloat32;
                    break;
            }

            sink->audioStreams[sink->numAudioStreams].streamId = streamId;
            sink->numAudioStreams++;

            return 1;
        }
    }

    return nextSinkResult;
}

static int aus_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    AudioSink* sink = (AudioSink*)data;
    int i;
    int bufferIsReadyForWrite;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            sink->audioStreams[i].nextSinkAcceptedFrame = msk_accept_stream_frame(sink->nextSink, streamId, frameInfo);

            if (sink->sinkDisabled)
            {
                /* some error occurred which has disabled the stream */
                return 0;
            }

            /* take opportunity to do first time initialisation */
            if (!sink->paStreamInitialised)
            {
                PaError err;
                if (sink->audioDevice < 0)
                {
                    /* Open an audio I/O stream on the default audio device. */
                    err = Pa_OpenDefaultStream(&sink->paStream,
                        0,          /* no input channels */
                        sink->numAudioStreams,
                        sink->paFormat,
                        sink->samplingRate.num / (double)sink->samplingRate.den,
                        0,
                        paAudioCallback,
                        sink);
                }
                else
                {
                    /* Open an audio I/O stream on specific audio device */
                    PaStreamParameters outParam;
                    outParam.device = sink->audioDevice;
                    outParam.channelCount = sink->numAudioStreams;
                    outParam.sampleFormat = sink->paFormat;
                    outParam.suggestedLatency = Pa_GetDeviceInfo(sink->audioDevice)->defaultHighOutputLatency;
                    outParam.hostApiSpecificStreamInfo = NULL;
                    err = Pa_OpenStream(&sink->paStream,
                        NULL, /* no input channels */
                        &outParam,
                        sink->samplingRate.num / (double)sink->samplingRate.den,
                        0,
                        paNoFlag,
                        paAudioCallback,
                        sink);
                }
                if (err != paNoError)
                {
                    ml_log_error("An error occured while initialising the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
                    ml_log_warn("The audio sink has been disabled\n");
                    sink->sinkDisabled = 1;
                    return 0;
                }

                sink->paStreamInitialised = 1;

                /* start the stream */
                err = Pa_StartStream(sink->paStream);
                if (err != paNoError)
                {
                    ml_log_error("An error occured while starting the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
                    ml_log_warn("The audio sink has been disabled\n");
                    sink->sinkDisabled = 1;
                    return 0;
                }
                sink->paStreamStarted = 1;
            }

            /* wait until a buffer is ready if the audio stream is active */
            PaError result = Pa_IsStreamActive(sink->paStream);
            if (result == 1)
            {
                while (1)
                {
                    bufferIsReadyForWrite = 1;
                    PTHREAD_MUTEX_LOCK(&sink->bufferMutex);
                    if (sink->writeBuffer == sink->readBuffer &&
                        sink->audioStreams[i].bufferIsReadyForRead[sink->writeBuffer])
                    {
                        bufferIsReadyForWrite = 0;
                    }
                    PTHREAD_MUTEX_UNLOCK(&sink->bufferMutex);

                    if (bufferIsReadyForWrite)
                    {
                        break;
                    }

                    usleep(100);
                }
            }
            else if (result < 0)
            {
                ml_log_error("An error occured while checking the status of the portaudio stream: (%d) %s\n", result, Pa_GetErrorText(result));
                ml_log_warn("The audio sink has been disabled\n");
                sink->sinkDisabled = 1;
                return 0;
            }


            return 1;
        }
    }

    return msk_accept_stream_frame(sink->nextSink, streamId, frameInfo);
}

static int aus_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    AudioSink* sink = (AudioSink*)data;
    int i;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            if (sink->audioStreams[i].nextSinkAcceptedFrame)
            {
                if (msk_get_stream_buffer(sink->nextSink, streamId, bufferSize, &sink->audioStreams[i].nextSinkBuffer))
                {
                    sink->audioStreams[i].nextSinkBufferSize = bufferSize;
                }
            }

            if (bufferSize != sink->audioStreams[i].bufferSize[sink->writeBuffer])
            {
                if (bufferSize > sink->audioStreams[i].allocatedBufferSize[sink->writeBuffer])
                {
                    /* the source required more work space - reallocate the buffer */

                    SAFE_FREE(&sink->audioStreams[i].buffer[sink->writeBuffer]);
                    sink->audioStreams[i].allocatedBufferSize[sink->writeBuffer] = 0;
                    sink->audioStreams[i].bufferSize[sink->writeBuffer] = 0;

                    CALLOC_ORET(sink->audioStreams[i].buffer[sink->writeBuffer], unsigned char, bufferSize);
                    sink->audioStreams[i].allocatedBufferSize[sink->writeBuffer] = bufferSize;
                }

                sink->audioStreams[i].bufferSize[sink->writeBuffer] = bufferSize;
            }

            *buffer = sink->audioStreams[i].buffer[sink->writeBuffer];

            return 1;
        }
    }

    return msk_get_stream_buffer(sink->nextSink, streamId, bufferSize, buffer);
}

static int aus_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    AudioSink* sink = (AudioSink*)data;
    int i;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            if (sink->audioStreams[i].nextSinkAcceptedFrame && sink->audioStreams[i].nextSinkBuffer != NULL)
            {
                if (bufferSize > sink->audioStreams[i].nextSinkBufferSize)
                {
                    ml_log_error("Buffer size (%d) > audio sink's next sink buffer size (%d)\n", bufferSize, sink->audioStreams[i].nextSinkBufferSize);
                    return 0;
                }
                memcpy(sink->audioStreams[i].nextSinkBuffer, buffer, bufferSize);
                msk_receive_stream_frame(sink->nextSink, streamId, sink->audioStreams[i].nextSinkBuffer, bufferSize);
            }

            /* the buffer size could have shrinked */
            sink->audioStreams[i].bufferSize[sink->writeBuffer] = bufferSize;

            return 1;
        }
    }

    return msk_receive_stream_frame(sink->nextSink, streamId, buffer, bufferSize);
}

static int aus_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    AudioSink* sink = (AudioSink*)data;
    int i;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            if (sink->audioStreams[i].nextSinkAcceptedFrame)
            {
                msk_receive_stream_frame_const(sink->nextSink, streamId, buffer, bufferSize);
            }

            if (bufferSize != sink->audioStreams[i].bufferSize[sink->writeBuffer])
            {
                if (bufferSize > sink->audioStreams[i].allocatedBufferSize[sink->writeBuffer])
                {
                    /* the source required more work space - reallocate the buffer */

                    SAFE_FREE(&sink->audioStreams[i].buffer[sink->writeBuffer]);
                    sink->audioStreams[i].allocatedBufferSize[sink->writeBuffer] = 0;
                    sink->audioStreams[i].bufferSize[sink->writeBuffer] = 0;

                    CALLOC_ORET(sink->audioStreams[i].buffer[sink->writeBuffer], unsigned char, bufferSize);
                    sink->audioStreams[i].allocatedBufferSize[sink->writeBuffer] = bufferSize;
                }

                sink->audioStreams[i].bufferSize[sink->writeBuffer] = bufferSize;
            }

            memcpy(sink->audioStreams[i].buffer[sink->writeBuffer], buffer, bufferSize);
            return 1;
        }
    }

    return msk_receive_stream_frame_const(sink->nextSink, streamId, buffer, bufferSize);
}

static int aus_complete_frame(void* data, const FrameInfo* frameInfo)
{
    AudioSink* sink = (AudioSink*)data;
    int i;
    unsigned int j;
    unsigned char tmp[4];
    unsigned char* front;
    unsigned char* back;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        sink->audioStreams[i].nextSinkBuffer = NULL;
        sink->audioStreams[i].nextSinkBufferSize = 0;
        sink->audioStreams[i].nextSinkAcceptedFrame = 0;
    }

    if (sink->numAudioStreams > 0)
    {
        if (!(frameInfo->isRepeat || frameInfo->muteAudio || sink->muteAudio))
        {
            if (frameInfo->reversePlay)
            {
                /* reverse the audio */
                for (i = 0; i < sink->numAudioStreams; i++)
                {
                    front = sink->audioStreams[i].buffer[sink->writeBuffer];
                    back = &sink->audioStreams[i].buffer[sink->writeBuffer][sink->audioStreams[i].bufferSize[sink->writeBuffer] - sink->byteAlignment];
                    for (j = 0; j < sink->audioStreams[i].bufferSize[sink->writeBuffer] / 2; j += sink->byteAlignment)
                    {
                        memcpy(tmp, front, sink->byteAlignment);
                        memcpy(front, back, sink->byteAlignment);
                        memcpy(back, tmp, sink->byteAlignment);
                        front += sink->byteAlignment;
                        back -= sink->byteAlignment;
                    }
                }
            }

            /* signal buffer is ready */
            PTHREAD_MUTEX_LOCK(&sink->bufferMutex);
            for (i = 0; i < sink->numAudioStreams; i++)
            {
                sink->audioStreams[i].bufferIsReadyForRead[sink->writeBuffer] = 1;
            }
            sink->writeBuffer = (sink->writeBuffer + 1) % 2;
            PTHREAD_MUTEX_UNLOCK(&sink->bufferMutex);

        }
        /* else don't send any audio */
    }


    return msk_complete_frame(sink->nextSink, frameInfo);
}

static void aus_cancel_frame(void* data)
{
    AudioSink* sink = (AudioSink*)data;
    int i;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        sink->audioStreams[i].nextSinkBuffer = NULL;
        sink->audioStreams[i].nextSinkBufferSize = 0;
        sink->audioStreams[i].nextSinkAcceptedFrame = 0;
    }


    msk_cancel_frame(sink->nextSink);
}

static OnScreenDisplay* aus_get_osd(void* data)
{
    AudioSink* sink = (AudioSink*)data;

    return msk_get_osd(sink->nextSink);
}

static VideoSwitchSink* aus_get_video_switch(void* data)
{
    AudioSink* sink = (AudioSink*)data;

    return msk_get_video_switch(sink->nextSink);
}

static AudioSwitchSink* aus_get_audio_switch(void* data)
{
    AudioSink* sink = (AudioSink*)data;

    return msk_get_audio_switch(sink->nextSink);
}

static HalfSplitSink* aus_get_half_split(void* data)
{
    AudioSink* sink = (AudioSink*)data;

    return msk_get_half_split(sink->nextSink);
}

static FrameSequenceSink* aus_get_frame_sequence(void* data)
{
    AudioSink* sink = (AudioSink*)data;

    return msk_get_frame_sequence(sink->nextSink);
}

static int aus_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    AudioSink* sink = (AudioSink*)data;

    return msk_get_buffer_state(sink->nextSink, numBuffers, numBuffersFilled);
}

static int aus_mute_audio(void* data, int mute)
{
    AudioSink* sink = (AudioSink*)data;

    if (mute < 0)
    {
        sink->muteAudio = !sink->muteAudio;
    }
    else
    {
        sink->muteAudio = mute;
    }

    return 1;
}

static void aus_close(void* data)
{
    AudioSink* sink = (AudioSink*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

    sink->stop = 1; /* stop the pa callback */

    if (sink->paStream != NULL)
    {
        PaError err;
        if (sink->paStreamStarted)
        {
            err = Pa_StopStream(sink->paStream);
            if (err != paNoError)
            {
                ml_log_warn("An error occured while stopping the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
            }
            sink->paStreamStarted = 0;
        }
        err = Pa_CloseStream(sink->paStream);
        if (err != paNoError)
        {
            ml_log_warn("An error occured while closing the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
        }
        sink->paStream = NULL;
    }

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        SAFE_FREE(&sink->audioStreams[i].buffer[0]);
        SAFE_FREE(&sink->audioStreams[i].buffer[1]);
        sink->audioStreams[i].bufferSize[0] = 0;
        sink->audioStreams[i].bufferSize[1] = 0;
    }

    destroy_mutex(&sink->bufferMutex);

    msk_close(sink->nextSink);

    Pa_Terminate();

    SAFE_FREE(&sink);
}

static int aus_reset_or_close(void* data)
{
    AudioSink* sink = (AudioSink*)data;
    int i;
    int result;

    sink->stop = 1; /* stop the pa callback */

    if (sink->paStreamStarted)
    {
        PaError err = Pa_StopStream(sink->paStream);
        if (err != paNoError)
        {
            ml_log_warn("An error occured while stopping the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
        }
        sink->paStreamStarted = 0;
        err = Pa_CloseStream(sink->paStream);
        if (err != paNoError)
        {
            ml_log_warn("An error occured while closing the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
        }
        sink->paStream = NULL;
        sink->paStreamInitialised = 0;
        sink->sinkDisabled = 0;
    }

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        SAFE_FREE(&sink->audioStreams[i].buffer[0]);
        SAFE_FREE(&sink->audioStreams[i].buffer[1]);
        sink->audioStreams[i].allocatedBufferSize[0] = 0;
        sink->audioStreams[i].allocatedBufferSize[1] = 0;
        sink->audioStreams[i].bufferSize[0] = 0;
        sink->audioStreams[i].bufferSize[1] = 0;
        sink->audioStreams[i].bufferIsReadyForRead[0] = 0;
        sink->audioStreams[i].bufferIsReadyForRead[1] = 0;
        sink->audioStreams[i].bufferBytesUsed[0] = 0;
        sink->audioStreams[i].bufferBytesUsed[1] = 0;
    }
    sink->numAudioStreams = 0;

    result = msk_reset_or_close(sink->nextSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            sink->nextSink = NULL;
        }
        goto fail;
    }

    sink->stop = 0; /* allow pa callback to be used again */

    return 1;

fail:
    aus_close(data);
    return 2;
}

int aus_create_audio_sink(MediaSink* nextSink, int audioDevice, AudioSink** sink)
{
    AudioSink* newSink;

    /* check there is at least one device available */
    PaDeviceIndex count = Pa_GetDeviceCount();
    if (count == 0)
    {
        ml_log_error("Audio device is not available\n");
        return 0;
    }


    /* initialise the port audio library */
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        goto paerror;
    }


    if (audioDevice < 0)
    {
        /* Pa_GetDefaultOutputDevice was found to return -1 if the device is already in use */
        PaDeviceIndex devIndex = Pa_GetDefaultOutputDevice();
        if (devIndex < 0)
        {
            ml_log_error("Default audio device is not available\n");
            Pa_Terminate();
            return 0;
        }
    }
    else
    {
        const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(audioDevice);
        if (devInfo == NULL)
        {
            ml_log_error("Audio device %d is not available\n", audioDevice);
            Pa_Terminate();
            return 0;
        }
    }


    CALLOC_ORET(newSink, AudioSink, 1);

    newSink->nextSink = nextSink;
    newSink->audioDevice = audioDevice;

    newSink->sink.data = newSink;
    newSink->sink.register_listener = aus_register_listener;
    newSink->sink.unregister_listener = aus_unregister_listener;
    newSink->sink.accept_stream = aus_accept_stream;
    newSink->sink.register_stream = aus_register_stream;
    newSink->sink.accept_stream_frame = aus_accept_stream_frame;
    newSink->sink.get_stream_buffer = aus_get_stream_buffer;
    newSink->sink.receive_stream_frame = aus_receive_stream_frame;
    newSink->sink.receive_stream_frame_const = aus_receive_stream_frame_const;
    newSink->sink.complete_frame = aus_complete_frame;
    newSink->sink.cancel_frame = aus_cancel_frame;
    newSink->sink.get_osd = aus_get_osd;
    newSink->sink.get_video_switch = aus_get_video_switch;
    newSink->sink.get_audio_switch = aus_get_audio_switch;
    newSink->sink.get_half_split = aus_get_half_split;
    newSink->sink.get_frame_sequence = aus_get_frame_sequence;
    newSink->sink.get_buffer_state = aus_get_buffer_state;
    newSink->sink.mute_audio = aus_mute_audio;
    newSink->sink.reset_or_close = aus_reset_or_close;
    newSink->sink.close = aus_close;

    CHK_OFAIL(init_mutex(&newSink->bufferMutex));


    *sink = newSink;
    return 1;

paerror:
    Pa_Terminate();
    ml_log_error("An error occured while using the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
    return 0;

fail:
    aus_close(newSink);
    return 0;
}

MediaSink* aus_get_media_sink(AudioSink* sink)
{
    return &sink->sink;
}


#endif


