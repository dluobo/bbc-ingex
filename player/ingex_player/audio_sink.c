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
    
    unsigned char* buffer[2];
    unsigned int bufferSize;
    
    int bufferIsReady[2];
} AudioStream;

struct AudioSink
{
    int audioDevice;
    
    MediaSink* nextSink;
    MediaSink sink;

    AudioStream audioStreams[MAX_AUDIO_STREAMS];
    int numAudioStreams;
    int readBuffer;
    int writeBuffer;
    
    Rational samplingRate;
    int numChannels;
    int bitsPerSample;
    int byteAlignment;
    
    
    PaStream* paStream;
    int paStreamStarted;
    int paStreamInitialised;
    PaSampleFormat paFormat;
    unsigned long paFramesPerBuffer;
    
    int sinkDisabled;
};

static int paAudioCallback(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{

    AudioSink* sink = (AudioSink*)userData;
    unsigned int j;
    int fillWithSilence = 0;

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


    
    if (sink->numAudioStreams == 1)
    {
        unsigned char* buffer = NULL;
        
        if (sink->readBuffer == 0)
        {
            buffer = sink->audioStreams[0].buffer[0];
            if (!sink->audioStreams[0].bufferIsReady[0])
            {
                fillWithSilence = 1;
            }
        }
        else /* sink->readBuffer == 1 */
        {
            buffer = sink->audioStreams[0].buffer[1];
            if (!sink->audioStreams[0].bufferIsReady[1])
            {
                fillWithSilence = 1;
            }
        }
    
        if (sink->byteAlignment == 1)
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, sink->audioStreams[0].bufferSize);
            }
            else
            {
                int8_t* out = (int8_t*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j++)
                {
                    *out++ = buffer[j];
                }
            }
        }
        else if (sink->byteAlignment == 2)
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, sink->audioStreams[0].bufferSize);
            }
            else
            {
                int16_t* out = (int16_t*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j += 2)
                {
                    *out++ = (int16_t)(((uint16_t)buffer[j]) |
                        ((uint16_t)buffer[j + 1]) << 8);
                }
            }
        }
        else if (sink->byteAlignment == 3)
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, sizeof(float) * sink->audioStreams[0].bufferSize / 3);
            }
            else
            {
                float* out = (float*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j += 3)
                {
                    *out++ = (int32_t)((((uint32_t)buffer[j]) << 8) |
                        (((uint32_t)buffer[j + 1]) << 16) |
                        (((uint32_t)buffer[j + 2]) << 24)) / 2147483648.0;
                }
            }
        }
        else /* (sink->byteAlignment == 4) */
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, sizeof(float) * sink->audioStreams[0].bufferSize / 4);
            }
            else
            {
                float* out = (float*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j += 4)
                {
                    *out++ = (int32_t)(((uint32_t)buffer[j]) |
                        (((uint32_t)buffer[j + 1]) << 8) |
                        (((uint32_t)buffer[j + 2]) << 16) |
                        (((uint32_t)buffer[j + 3]) << 24)) / 2147483648.0;
                }
            }
        }
        
    }
    else /* (sink->numAudioStreams == 2) */
    {
        unsigned char* bufferL = NULL;
        unsigned char* bufferR = NULL;
        
        if (sink->readBuffer == 0)
        {
            bufferL = sink->audioStreams[0].buffer[0];
            bufferR = sink->audioStreams[1].buffer[0];
            if (!sink->audioStreams[0].bufferIsReady[0] || !sink->audioStreams[1].bufferIsReady[0])
            {
                fillWithSilence = 1;
            }
        }
        else /* sink->readBuffer == 1 */
        {
            bufferL = sink->audioStreams[0].buffer[1];
            bufferR = sink->audioStreams[1].buffer[1];
            if (!sink->audioStreams[0].bufferIsReady[1] || !sink->audioStreams[1].bufferIsReady[1])
            {
                fillWithSilence = 1;
            }
        }
    
        if (sink->byteAlignment == 1)
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, 2 * sink->audioStreams[0].bufferSize);
            }
            else
            {
                int8_t* out = (int8_t*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j++)
                {
                    *out++ = bufferL[j];
                    *out++ = bufferR[j];
                }
            }
        }
        else if (sink->byteAlignment == 2)
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, 2 * sink->audioStreams[0].bufferSize);
            }
            else
            {
                int16_t* out = (int16_t*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j += 2)
                {
                    *out++ = (int16_t)(((uint16_t)bufferL[j]) |
                        ((uint16_t)bufferL[j + 1]) << 8);
                    *out++ = (int16_t)(((uint16_t)bufferR[j]) |
                        ((uint16_t)bufferR[j + 1]) << 8);
                }
            }
        }
        else if (sink->byteAlignment == 3)
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, sizeof(float) * 2 * sink->audioStreams[0].bufferSize / 3);
            }
            else
            {
                float* out = (float*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j += 3)
                {
                    *out++ = (int32_t)((((uint32_t)bufferL[j]) << 8) |
                        (((uint32_t)bufferL[j + 1]) << 16) |
                        (((uint32_t)bufferL[j + 2]) << 24)) / 2147483648.0;
                    *out++ = (int32_t)((((uint32_t)bufferR[j]) << 8) |
                        (((uint32_t)bufferR[j + 1]) << 16) |
                        (((uint32_t)bufferR[j + 2]) << 24)) / 2147483648.0;
                }
            }
        }
        else /* (sink->byteAlignment == 4) */
        {
            if (fillWithSilence)
            {
                memset(outputBuffer, 0, sizeof(float) * 2 * sink->audioStreams[0].bufferSize / 4);
            }
            else
            {
                float* out = (float*)outputBuffer;
                for (j = 0; j < sink->audioStreams[0].bufferSize; j += 4)
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
        }
    }
    
    if (!fillWithSilence)
    {
        sink->audioStreams[0].bufferIsReady[sink->readBuffer] = 0;
        if (sink->numAudioStreams > 1)
        {
            sink->audioStreams[1].bufferIsReady[sink->readBuffer] = 0;
        }
        sink->readBuffer = (sink->readBuffer + 1) % 2;
    }
    
    return 0;
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
                    return 0;
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
            
            /* TODO: don't assume 25 fps */
            sink->paFramesPerBuffer = (int)((sink->samplingRate.num / (double)(25 * sink->samplingRate.den) + 0.5));
            
            sink->audioStreams[sink->numAudioStreams].streamId = streamId;
            sink->audioStreams[sink->numAudioStreams].bufferSize = streamInfo->numChannels *
                sink->paFramesPerBuffer * sink->byteAlignment;   
            CALLOC_ORET(sink->audioStreams[sink->numAudioStreams].buffer[0], unsigned char, 
                sink->audioStreams[sink->numAudioStreams].bufferSize);
            CALLOC_ORET(sink->audioStreams[sink->numAudioStreams].buffer[1], unsigned char, 
                sink->audioStreams[sink->numAudioStreams].bufferSize);
            sink->numAudioStreams++;
            
            return 1;
        }
        
        return 0;
    }

    return msk_register_stream(sink->nextSink, streamId, streamInfo);
}

static int aus_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    AudioSink* sink = (AudioSink*)data;
    int i;
    
    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
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
                        sink->paFramesPerBuffer,
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
                        sink->paFramesPerBuffer,
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
                /* stream is active */
                while (sink->writeBuffer == sink->readBuffer &&
                    sink->audioStreams[i].bufferIsReady[sink->writeBuffer])
                {
                    /* TODO: more or less */
                    usleep(50);
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
            if (bufferSize != sink->audioStreams[i].bufferSize)
            {
                ml_log_error("Buffer size (%d) != audio data size (%d)\n", bufferSize, sink->audioStreams[i].bufferSize);
                return 0;
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

    if (sink->numAudioStreams > 0)
    {
        if (!frameInfo->isRepeat)
        {
            if (frameInfo->reversePlay)
            {
                /* reverse the audio */
                for (i = 0; i < sink->numAudioStreams; i++)
                {
                    front = sink->audioStreams[i].buffer[sink->writeBuffer];
                    back = &sink->audioStreams[i].buffer[sink->writeBuffer][sink->audioStreams[i].bufferSize - sink->byteAlignment];
                    for (j = 0; j < sink->audioStreams[i].bufferSize / 2; j += sink->byteAlignment)
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
            for (i = 0; i < sink->numAudioStreams; i++)
            {
                sink->audioStreams[i].bufferIsReady[sink->writeBuffer] = 1;
            }
            
            sink->writeBuffer = (sink->writeBuffer + 1) % 2;
        }
        /* else the frame is a repeat and we don't send any audio */
    }
    
    return msk_complete_frame(sink->nextSink, frameInfo);
}

static void aus_cancel_frame(void* data)
{
    AudioSink* sink = (AudioSink*)data;
    
    // TODO

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

static void aus_close(void* data)
{
    AudioSink* sink = (AudioSink*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

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
        sink->audioStreams[i].bufferSize = 0;
    }
    
    msk_close(sink->nextSink);

    Pa_Terminate();
    
    SAFE_FREE(&sink);
}

static int aus_reset_or_close(void* data)
{
    AudioSink* sink = (AudioSink*)data;
    int i;
    int result;

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
        sink->audioStreams[i].bufferSize = 0;
        sink->audioStreams[i].bufferIsReady[0] = 0;
        sink->audioStreams[i].bufferIsReady[1] = 0;
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
    newSink->sink.reset_or_close = aus_reset_or_close;
    newSink->sink.close = aus_close;
    
    *sink = newSink;
    return 1;
    
paerror:
    Pa_Terminate();
    ml_log_error("An error occured while using the portaudio stream: (%d) %s\n", err, Pa_GetErrorText(err));
    return 0;
}

MediaSink* aus_get_media_sink(AudioSink* sink)
{
    return &sink->sink;
}


#endif


