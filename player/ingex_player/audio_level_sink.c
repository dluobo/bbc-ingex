#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "audio_level_sink.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define MAX_AUDIO_STREAMS           16

typedef struct
{
    int targetAcceptedStream;
    
    int streamId;
    int byteAlignment;
    double level;
    
    int targetSinkAcceptedFrame;
    unsigned char* buffer;
    unsigned int bufferSize;
} AudioStream;

struct AudioLevelSink
{
    int maxAudioStreams;
    
    MediaSink* targetSink;
    MediaSink sink;

    AudioStream audioStreams[MAX_AUDIO_STREAMS];
    int numAudioStreams;    
    
    float minAudioLevel;
    float audioLineupLevel;
    float nullAudioLevel;
};


static void reset_audio_levels(AudioLevelSink* sink)
{
    int i;
    
    for (i = 0; i < sink->numAudioStreams; i++)
    {
        sink->audioStreams[i].level = sink->nullAudioLevel;
    }
}


static int als_register_listener(void* data, MediaSinkListener* listener)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    return msk_register_listener(sink->targetSink, listener);
}

static void als_unregister_listener(void* data, MediaSinkListener* listener)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    msk_unregister_listener(sink->targetSink, listener);
}

static int als_accept_stream(void* data, const StreamInfo* streamInfo)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int result;

    result = msk_accept_stream(sink->targetSink, streamInfo);
    result = result || (streamInfo->type == SOUND_STREAM_TYPE && streamInfo->format == PCM_FORMAT);
    
    return result;
}

static int als_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int result = 0;

    if (streamInfo->type == SOUND_STREAM_TYPE && streamInfo->format == PCM_FORMAT)
    {
        if (sink->numAudioStreams < sink->maxAudioStreams &&
            sink->numAudioStreams < MAX_AUDIO_STREAMS)
        {
            /* monitor this PCM audio stream's level */
            sink->audioStreams[sink->numAudioStreams].streamId = streamId;
            sink->audioStreams[sink->numAudioStreams].byteAlignment = (streamInfo->bitsPerSample + 7) / 8;
            sink->audioStreams[sink->numAudioStreams].level = 0.0;
            sink->numAudioStreams++;
            
            /* register stream in OSD */
            osd_register_audio_stream(msk_get_osd(sink->targetSink), streamId);
            
            result = 1;
        }
    }

    /* only try register is target sink if it accepts the stream */
    if (msk_accept_stream(sink->targetSink, streamInfo))
    {
        result = msk_register_stream(sink->targetSink, streamId, streamInfo) || result;
    }
    
    return result;
}

static int als_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int result;
    int i;
    
    result = msk_accept_stream_frame(sink->targetSink, streamId, frameInfo);
    
    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            sink->audioStreams[i].targetSinkAcceptedFrame = result;
            result = 1;
            break;
        }
    }
    
    return result;
}

static int als_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int i;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            if (sink->audioStreams[i].targetSinkAcceptedFrame)
            {
                /* target sink will hold audio data */
                return msk_get_stream_buffer(sink->targetSink, streamId, bufferSize, buffer);
            }
            else
            {
                /* we need to hold the audio data */
                if (bufferSize != sink->audioStreams[i].bufferSize)
                {
                    SAFE_FREE(&sink->audioStreams[i].buffer);
                    sink->audioStreams[i].bufferSize = 0;
                    
                    MALLOC_ORET(sink->audioStreams[i].buffer, unsigned char, bufferSize);
                    sink->audioStreams[i].bufferSize = bufferSize;
                }
                *buffer = sink->audioStreams[i].buffer;
                return 1;
            }
        }
    }

    return msk_get_stream_buffer(sink->targetSink, streamId, bufferSize, buffer);
}

static int als_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int i;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            sink->audioStreams[i].level = calc_audio_peak_power(buffer, 
                bufferSize / sink->audioStreams[i].byteAlignment, 
                sink->audioStreams[i].byteAlignment,
                sink->minAudioLevel);
            
            if (sink->audioStreams[i].targetSinkAcceptedFrame)
            {
                return msk_receive_stream_frame(sink->targetSink, streamId, buffer, bufferSize);
            }
            else
            {
                return 1;
            }
        }
    }

    return msk_receive_stream_frame(sink->targetSink, streamId, buffer, bufferSize);
}

static int als_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int i;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStreams[i].streamId == streamId)
        {
            /* calculate level */
            sink->audioStreams[i].level = calc_audio_peak_power(buffer, 
                bufferSize / sink->audioStreams[i].byteAlignment, 
                sink->audioStreams[i].byteAlignment,
                sink->minAudioLevel);
            
            if (sink->audioStreams[i].targetSinkAcceptedFrame)
            {
                return msk_receive_stream_frame_const(sink->targetSink, streamId, buffer, bufferSize);
            }
            else
            {
                return 1;
            }
        }
    }

    return msk_receive_stream_frame_const(sink->targetSink, streamId, buffer, bufferSize);
}

static int als_complete_frame(void* data, const FrameInfo* frameInfo)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int i;
    int result;

    if (sink->numAudioStreams > 0)
    {
        for (i = 0; i < sink->numAudioStreams; i++)
        {
            if (sink->audioStreams[i].level != sink->nullAudioLevel)
            {
                osd_set_audio_stream_level(msk_get_osd(sink->targetSink), 
                    sink->audioStreams[i].streamId,
                    sink->audioStreams[i].level);
            }
        }        
    
        reset_audio_levels(sink);
        
        result = msk_complete_frame(sink->targetSink, frameInfo);
        
        osd_reset_audio_stream_levels(msk_get_osd(sink->targetSink));

        return result;
    }
    else
    {
        return msk_complete_frame(sink->targetSink, frameInfo);
    }
}

static void als_cancel_frame(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    reset_audio_levels(sink);

    msk_cancel_frame(sink->targetSink);

    osd_reset_audio_stream_levels(msk_get_osd(sink->targetSink));
}

static OnScreenDisplay* als_get_osd(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    return msk_get_osd(sink->targetSink);
}
    
static VideoSwitchSink* als_get_video_switch(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    return msk_get_video_switch(sink->targetSink);
}
    
static AudioSwitchSink* als_get_audio_switch(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    return msk_get_audio_switch(sink->targetSink);
}
    
static HalfSplitSink* als_get_half_split(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    return msk_get_half_split(sink->targetSink);
}
    
static FrameSequenceSink* als_get_frame_sequence(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    return msk_get_frame_sequence(sink->targetSink);
}
    
static int als_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;

    return msk_get_buffer_state(sink->targetSink, numBuffers, numBuffersFilled);
}

static void als_close(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        SAFE_FREE(&sink->audioStreams[i].buffer);;
        sink->audioStreams[i].bufferSize = 0;
    }
    
    msk_close(sink->targetSink);
    
    SAFE_FREE(&sink);
}

static int als_reset_or_close(void* data)
{
    AudioLevelSink* sink = (AudioLevelSink*)data;
    int i;
    int result;

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        SAFE_FREE(&sink->audioStreams[i].buffer);
        sink->audioStreams[i].bufferSize = 0;
    }
    sink->numAudioStreams = 0;
    
    result = msk_reset_or_close(sink->targetSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            sink->targetSink = NULL;
        }
        goto fail;
    }
    
    return 1;
    
fail:
    als_close(data);
    return 2;
}

int als_create_audio_level_sink(MediaSink* targetSink, int numAudioStreams,  float audioLineupLevel, 
    AudioLevelSink** sink)
{
    AudioLevelSink* newSink;
    
    CALLOC_ORET(newSink, AudioLevelSink, 1);

    newSink->maxAudioStreams = (numAudioStreams > 0) ? numAudioStreams : 0;
    
    newSink->targetSink = targetSink;

    newSink->sink.data = newSink;
    newSink->sink.register_listener = als_register_listener;
    newSink->sink.unregister_listener = als_unregister_listener;
    newSink->sink.accept_stream = als_accept_stream;
    newSink->sink.register_stream = als_register_stream;
    newSink->sink.accept_stream_frame = als_accept_stream_frame;
    newSink->sink.get_stream_buffer = als_get_stream_buffer;
    newSink->sink.receive_stream_frame = als_receive_stream_frame;
    newSink->sink.receive_stream_frame_const = als_receive_stream_frame_const;
    newSink->sink.complete_frame = als_complete_frame;
    newSink->sink.cancel_frame = als_cancel_frame;
    newSink->sink.get_osd = als_get_osd;
    newSink->sink.get_video_switch = als_get_video_switch;
    newSink->sink.get_audio_switch = als_get_audio_switch;
    newSink->sink.get_half_split = als_get_half_split;
    newSink->sink.get_frame_sequence = als_get_frame_sequence;
    newSink->sink.get_buffer_state = als_get_buffer_state;
    newSink->sink.reset_or_close = als_reset_or_close;
    newSink->sink.close = als_close;
    
    /* ~ minimum for 16-bit audio */    
    newSink->minAudioLevel = -96.0;
    newSink->audioLineupLevel = audioLineupLevel;
    osd_set_minimum_audio_stream_level(msk_get_osd(targetSink), newSink->minAudioLevel);
    osd_set_audio_lineup_level(msk_get_osd(targetSink), newSink->audioLineupLevel);
    
    newSink->nullAudioLevel = -100.0; /* any invalid audio level */
    reset_audio_levels(newSink);
    
    
    *sink = newSink;
    return 1;
}

MediaSink* als_get_media_sink(AudioLevelSink* sink)
{
    return &sink->sink;
}


