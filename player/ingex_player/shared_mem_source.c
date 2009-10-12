/*
 * $Id: shared_mem_source.c,v 1.11 2009/10/12 16:06:30 philipn Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Stuart Cunningham
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "shared_mem_source.h"
#include "video_conversion.h"
#include "source_event.h"
#include "logging.h"
#include "macros.h"


#if defined(DISABLE_SHARED_MEM_SOURCE)

struct SharedMemSource
{
    int nothing;
};

int shms_open(const char *channel_name, double timeout, SharedMemSource** source)
{
    return 0;
}


#else



#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#undef PTHREAD_MUTEX_LOCK
#undef PTHREAD_MUTEX_UNLOCK
#include <nexus_control.h>

#define MAX_TRACKS              19

#define NUM_TIMECODE_TRACKS     (SYSTEM_TC_TRACK + 1)



typedef enum
{
    LTC_TRACK = 0,
    VITC_TRACK,
    DLTC_TRACK,
    DVITC_TRACK,
    SYSTEM_TC_TRACK
} TimecodeTrackType;

typedef struct
{
    int streamIndex;
    StreamInfo streamInfo;
    int frameSize;
    int isDisabled;
} TrackInfo;

struct SharedMemSource
{
    MediaSource mediaSource;
    int channel;
    int primary;                    // boolean: primary video buffer or secondary
    CaptureFormat captureFormat;
    
    TimecodeType defaultTCType;
    TimecodeSubType defaultTCSubType;

    TrackInfo tracks[MAX_TRACKS];
    int numTracks;
    int numAudioTracks;

    Rational frameRate;
    int64_t position;

    int prevLastFrame;

    char sourceName[64];
    int sourceNameUpdate;
};


/* Contains the pctl and ring pointers for the shared mem connection */
static NexusConnection conn;
static int connected = 0;

static int shm_get_num_streams(void* data)
{
    SharedMemSource* source = (SharedMemSource*)data;

    return source->numTracks;
}

static int shm_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    SharedMemSource* source = (SharedMemSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    *streamInfo = &source->tracks[streamIndex].streamInfo;
    return 1;
}

static void shm_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    SharedMemSource* source = (SharedMemSource*)data;
    int i;

    /* disable this source if the frame rate differs */
    if (memcmp(frameRate, &source->frameRate, sizeof(*frameRate)) != 0)
    {
        for (i = 0; i < source->numTracks; i++)
        {
            msc_disable_stream(&source->mediaSource, i);
        }
    }
}

static int shm_disable_stream(void* data, int streamIndex)
{
    SharedMemSource* source = (SharedMemSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    source->tracks[streamIndex].isDisabled = 1;
    return 1;
}

static void shm_disable_audio(void* data)
{
    SharedMemSource* source = (SharedMemSource*)data;
    int i;

    for (i = 0; i < source->numTracks; i++)
    {
        if (source->tracks[i].streamInfo.type == SOUND_STREAM_TYPE)
        {
            source->tracks[i].isDisabled = 1;
        }
    }
}

static int shm_stream_is_disabled(void* data, int streamIndex)
{
    SharedMemSource* source = (SharedMemSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    return source->tracks[streamIndex].isDisabled;
}

static const uint8_t *rec_ring_video(SharedMemSource *source, int lastFrame)
{
    if (source->primary)
        return nexus_primary_video(conn.pctl, conn.ring, source->channel, lastFrame);
    else
        return nexus_secondary_video(conn.pctl, conn.ring, source->channel, lastFrame);
}

static const uint8_t *rec_ring_audio(SharedMemSource *source, int track, int lastFrame)
{
    if (track == 0 || track == 1)
        return nexus_audio12(conn.pctl, conn.ring, source->channel, lastFrame);

    if (track == 2 || track == 3)
        return nexus_audio34(conn.pctl, conn.ring, source->channel, lastFrame);

    if (track == 4 || track == 5)
        return nexus_audio56(conn.pctl, conn.ring, source->channel, lastFrame);

    return nexus_audio78(conn.pctl, conn.ring, source->channel, lastFrame);
}

static int rec_ring_num_aud_samp(SharedMemSource *source, int track, int lastFrame)
{
    return nexus_num_aud_samp(conn.pctl, conn.ring, source->channel, lastFrame);
}

static int rec_ring_timecode(SharedMemSource *source, int track, int lastFrame)
{
    assert(track < NUM_TIMECODE_TRACKS);
    TimecodeTrackType timecodeTrack = (TimecodeTrackType)track;
    
    NexusTimecode tc_type = NexusTC_VITC;
    switch (timecodeTrack)
    {
        case LTC_TRACK:
            tc_type = NexusTC_LTC;
            break;
        case VITC_TRACK:
            tc_type = NexusTC_VITC;
            break;
        case DLTC_TRACK:
            tc_type = NexusTC_DLTC;
            break;
        case DVITC_TRACK:
            tc_type = NexusTC_DVITC;
            break;
        case SYSTEM_TC_TRACK:
            tc_type = NexusTC_SYSTEM;
            break;
    }

    return nexus_tc(conn.pctl, conn.ring, source->channel, lastFrame, tc_type);
}

static void dvsaudio32_to_16bitmono(int channel, int num_samples, const uint8_t *buf32, uint8_t *buf16)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    int channel_offset = 0;
    if (channel == 1)
        channel_offset = 4;

    // Skip every other channel, copying 16 most significant bits of 32 bits
    // from little-endian DVS format to little-endian 16bits
    for (i = channel_offset; i < num_samples*4*2; i += 8) {
        *buf16++ = buf32[i+2];
        *buf16++ = buf32[i+3];
    }
}

/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int shm_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    SharedMemSource* source = (SharedMemSource*)data;
    int i;
    TrackInfo* track;
    unsigned char* buffer;
    int lastFrame;
    int waitCount = 0;
    const int sleepUSec = 100;
    int updateCount = 0;
    int nameUpdated;
    int roundedFrameRate = get_rounded_frame_rate(&source->frameRate);

    /* Check nexus connection is good */
    if (!connected || !nexus_connection_status(&conn, NULL, NULL))
    {
        if (connected)
        {
            nexus_disconnect_from_shared_mem(&conn);
            connected = 0;
        }

        /* Try to reconnect */
        if (! nexus_connect_to_shared_mem(100000, 0, 0, &conn))
        {
            /* Could not reconnect - time out */
            ml_log_info("Could not re-connect to shared mem - timeout\n");
            return -2;
        }
        
        connected = 1;
        ml_log_info("Connected to shared memory\n");
    }

    /* wait a little while until a new frame is available in the ring buffer */
    lastFrame = nexus_lastframe(conn.pctl, source->channel) - 1;
    waitCount = 0;
    while (lastFrame == source->prevLastFrame && waitCount < 60000 / sleepUSec)
    {
        usleep(sleepUSec);
        waitCount++;

        lastFrame = nexus_lastframe(conn.pctl, source->channel) - 1;
    }
    if (lastFrame == source->prevLastFrame && waitCount >= 60000 / sleepUSec)
    {
        /* warn about waits > 1.5 frames */
        ml_log_info("shared_mem_source: waited >= 60ms for next frame\n");
    }
    source->prevLastFrame = lastFrame;


    /* check for updated source name */
    
    nexus_get_source_name(conn.pctl, source->channel, source->sourceName, sizeof(source->sourceName), &updateCount);
    nameUpdated = (updateCount != source->sourceNameUpdate);
    source->sourceNameUpdate = updateCount;

    // Track numbers are hard-coded by setup code in shms_open()
    // 1 x video, numAudioTracks x audio, NUM_TIMECODE_TRACKS x timecode, event

    for (i = 0; i < source->numTracks; i++)
    {
        track = &source->tracks[i];

        if (track->isDisabled)
            continue;

        if (! sdl_accept_frame(listener, i, frameInfo))
            continue;

        /* calculate current frame size for sound and events streams */
        if (track->streamInfo.type == SOUND_STREAM_TYPE)
        {
            int num_samples = rec_ring_num_aud_samp(source, i - 1, lastFrame);
            track->frameSize = num_samples * 2;        // * 2 for 16bit audio
        }
        if (track->streamInfo.type == EVENT_STREAM_TYPE)
        {
            if (nameUpdated)
            {
                track->frameSize = svt_get_buffer_size(1);
            }
            else
            {
                track->frameSize = svt_get_buffer_size(0);
            }
        }

        if (! sdl_allocate_buffer(listener, i, &buffer, track->frameSize))
        {
            /* listener failed to allocate a buffer for us */
            return -1;
        }

        /* copy data into buffer */
        if (track->streamInfo.type == PICTURE_STREAM_TYPE)
        {
            const uint8_t* data = rec_ring_video(source, lastFrame);

            if (source->captureFormat == Format422PlanarYUVShifted)
            {
                /* shift picture back up one line */

                /* Y */
                memcpy(buffer, &data[track->streamInfo.width],
                    track->streamInfo.width * (track->streamInfo.height - 1));
                memset(&buffer[track->streamInfo.width * (track->streamInfo.height - 1)],
                    0x80, track->streamInfo.width);

                /* U */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height],
                    &data[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width / 2],
                    track->streamInfo.width * (track->streamInfo.height - 1) / 2);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width * (track->streamInfo.height - 1) / 2],
                    0x10, track->streamInfo.width / 2);

                /* V */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height * 3 / 2],
                    &data[track->streamInfo.width * track->streamInfo.height * 3 / 2 + track->streamInfo.width / 2],
                    track->streamInfo.width * (track->streamInfo.height - 1) / 2);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height * 3 / 2 + track->streamInfo.width * (track->streamInfo.height - 1) / 2],
                    0x10, track->streamInfo.width / 2);
            }
            else if (source->captureFormat == Format420PlanarYUVShifted)
            {
                /* shift picture back up one line */

                /* Y */
                memcpy(buffer, &data[track->streamInfo.width],
                    track->streamInfo.width * (track->streamInfo.height - 1));
                memset(&buffer[track->streamInfo.width * (track->streamInfo.height - 1)],
                    0x80, track->streamInfo.width);

                /* U */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height],
                    &data[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width / 2],
                    track->streamInfo.width * (track->streamInfo.height - 1) / 4);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width * (track->streamInfo.height - 1) / 4],
                    0x10, track->streamInfo.width / 2);

                /* V */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height * 5 / 4],
                    &data[track->streamInfo.width * track->streamInfo.height * 5 / 4 + track->streamInfo.width / 2],
                    track->streamInfo.width * (track->streamInfo.height - 1) / 4);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height * 5 / 4 + track->streamInfo.width * (track->streamInfo.height - 1) / 4],
                    0x10, track->streamInfo.width / 2);
            }
            else
            {
                memcpy(buffer, data, track->frameSize);
            }
        }

        if (track->streamInfo.type == SOUND_STREAM_TYPE) {
            // audio size is variable so set frameSize for this frame
            int num_samples = rec_ring_num_aud_samp(source, i - 1, lastFrame);

            // Get a pointer to the correct DVS audio buffer
            const uint8_t *buf32 = rec_ring_audio(source, i - 1, lastFrame);

            // determine which channel (0 or 1) of the DVS pair to use
            int pair_num = (i - 1) % 2;

            // reformat audio from interleaved DVS audio format to single channel audio
            dvsaudio32_to_16bitmono(pair_num, num_samples, buf32, buffer);
        }

        if (track->streamInfo.type == TIMECODE_STREAM_TYPE) {
            int tc_as_int = rec_ring_timecode(source, i - (source->numAudioTracks + 1), lastFrame);
            // convert ts-as-int to Timecode
            Timecode tc;
            tc.isDropFrame = 0;
            tc.frame = tc_as_int % roundedFrameRate;
            tc.hour = (int)(tc_as_int / (60 * 60 * roundedFrameRate));
            tc.min = (int)((tc_as_int - (tc.hour * 60 * 60 * roundedFrameRate)) / (60 * roundedFrameRate));
            tc.sec = (int)((tc_as_int - (tc.hour * 60 * 60 * roundedFrameRate) - (tc.min * 60 * roundedFrameRate)) / roundedFrameRate);
            memcpy(buffer, &tc, track->frameSize);
        }

        if (track->streamInfo.type == EVENT_STREAM_TYPE)
        {
            if (nameUpdated)
            {
                SourceEvent event;
                svt_set_name_update_event(&event, source->sourceName);

                svt_write_num_events(buffer, 1);
                svt_write_event(buffer, 0, &event);
            }
            else
            {
                svt_write_num_events(buffer, 0);
            }
        }

        sdl_receive_frame(listener, i, buffer, track->frameSize);
    }
    source->position += 1;

    return 0;
}

static int shm_is_seekable(void* data)
{
    /* cannot seek */
    return 0;
}

/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int shm_seek(void* data, int64_t position)
{
    /* cannot seek and shouldn't have called this function */
    return -1;
}

static int shm_get_length(void* data, int64_t* length)
{
    /* unknown length */
    return 0;
}

static int shm_get_position(void* data, int64_t* position)
{
    SharedMemSource* source = (SharedMemSource*)data;

    *position = source->position;
    return 1;
}

static int shm_get_available_length(void* data, int64_t* length)
{
    return shm_get_length(data, length);
}

static int shm_eof(void* data)
{
    /* never at "end of file" */
    return 0;
}

static void shm_set_source_name(void* data, const char* name)
{
    SharedMemSource* source = (SharedMemSource*)data;

    int i;
    for (i = 0; i < source->numTracks; i++)
    {
        add_known_source_info(&source->tracks[i].streamInfo, SRC_INFO_NAME, name);
    }
}

static void shm_set_clip_id(void* data, const char* id)
{
    SharedMemSource* source = (SharedMemSource*)data;

    int i;
    for (i = 0; i < source->numTracks; i++)
    {
        set_stream_clip_id(&source->tracks[i].streamInfo, id);
    }
}

static void shm_close(void* data)
{
    SharedMemSource* source = (SharedMemSource*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

    for (i = 0; i < source->numTracks; i++)
    {
        clear_stream_info(&source->tracks[i].streamInfo);
    }

    SAFE_FREE(&source);
}

int shms_open(const char* channel_name, double timeout, SharedMemSource** source)
{
    SharedMemSource* newSource = NULL;
    int channelNum;
    int primary;
    int sourceId;
    CaptureFormat captureFormat;
    int disable = 0;
    int i;
    char nameBuf[32];
    TimecodeTrackType timecodeType;

    /* check channel number is within range */
    channelNum = atol(channel_name);
    if (channelNum < 0 || channelNum >= MAX_CHANNELS)
    {
        ml_log_error("Card %s is out of range of maximum %d supported channels\n", channel_name, MAX_CHANNELS);
        return 0;
    }

    /* Support "0p" or "0s" for primary/secondary video */
    if (strchr(channel_name, 'p') == NULL)
    {
        primary = 0;
    }
    else
    {
        primary = 1;
    }

    char title[FILENAME_MAX];
    if (primary)
    {
        sprintf(title, "Shared Memory Video: channel %d primary\n", channelNum);
    }
    else
    {
        sprintf(title, "Shared Memory Video: channel %d secondary\n", channelNum);
    }

    /* first disconnect from shared memory if connected */
    if (connected)
    {
        nexus_disconnect_from_shared_mem(&conn);
        connected = 0;
    }
    
    /* (re-)connect to shared memory */
    /* If timeout is negative, loop forever until shared mem connection made */
    if (timeout < 0)
    {
        ml_log_info("Waiting for shared memory to appear...\n");
        while (1)
        {
            if (nexus_connect_to_shared_mem(500000, 0, 0, &conn))
                break;
        }
    }
    else
    {
        /* try to connect to shared mem within supplied timeout */
        if (! nexus_connect_to_shared_mem(timeout * 1000000, 0, 1, &conn))    /* convert timeout to microsec */
        {
            ml_log_error("Failed to connect to shared memory\n");
            return 0;
        }
    }
    connected = 1;
    ml_log_info("Connected to shared memory\n");
    

    captureFormat = primary ? conn.pctl->pri_video_format : conn.pctl->sec_video_format;

    if (captureFormat != Format422UYVY &&
        captureFormat != Format422PlanarYUV &&
        captureFormat != Format422PlanarYUVShifted &&
        captureFormat != Format420PlanarYUV &&
        captureFormat != Format420PlanarYUVShifted)
    {
        ml_log_error("Shared memory video format %s is not supported\n", nexus_capture_format_name(captureFormat));
        return 0;
    }

    if (channelNum >= conn.pctl->channels)
    {
        ml_log_warn("Shared memory channel %d is not in use - streams will be disabled\n", channelNum);
        disable = 1;
    }

    CALLOC_ORET(newSource, SharedMemSource, 1);

    newSource->channel = channelNum;
    newSource->prevLastFrame = -1;
    newSource->sourceNameUpdate = -1;
    newSource->captureFormat = captureFormat;
    newSource->primary = primary;

    int width, height;
    if (primary)
    {
        width = conn.pctl->width;
        height = conn.pctl->height;
    }
    else
    {
        width = conn.pctl->sec_width;
        height = conn.pctl->sec_height;
    }
    
    newSource->numAudioTracks = 4;
    if (conn.pctl->audio56_offset > 0)
    {
        newSource->numAudioTracks += 2;
        if (conn.pctl->audio78_offset > 0)
        {
            newSource->numAudioTracks += 2;
        }
    }
    
    switch (conn.pctl->default_tc_type)
    {
        case NexusTC_LTC:
            newSource->defaultTCType = SOURCE_TIMECODE_TYPE;
            newSource->defaultTCSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
            break;
        case NexusTC_VITC:
            newSource->defaultTCType = SOURCE_TIMECODE_TYPE;
            newSource->defaultTCSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
            break;
        case NexusTC_DLTC:
            newSource->defaultTCType = SOURCE_TIMECODE_TYPE;
            newSource->defaultTCSubType = DLTC_SOURCE_TIMECODE_SUBTYPE;
            break;
        case NexusTC_DVITC:
            newSource->defaultTCType = SOURCE_TIMECODE_TYPE;
            newSource->defaultTCSubType = DVITC_SOURCE_TIMECODE_SUBTYPE;
            break;
        case NexusTC_SYSTEM:
            newSource->defaultTCType = SYSTEM_TIMECODE_TYPE;
            newSource->defaultTCSubType = NO_TIMECODE_SUBTYPE;
            break;
        case NexusTC_DEFAULT:
            newSource->defaultTCType = SOURCE_TIMECODE_TYPE;
            newSource->defaultTCSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
            break;
    }

    newSource->frameRate.num = conn.pctl->frame_rate_numer;
    newSource->frameRate.den = conn.pctl->frame_rate_denom;

    /* setup media source */
    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = shm_get_num_streams;
    newSource->mediaSource.get_stream_info = shm_get_stream_info;
    newSource->mediaSource.set_frame_rate_or_disable = shm_set_frame_rate_or_disable;
    newSource->mediaSource.disable_stream = shm_disable_stream;
    newSource->mediaSource.disable_audio = shm_disable_audio;
    newSource->mediaSource.stream_is_disabled = shm_stream_is_disabled;
    newSource->mediaSource.read_frame = shm_read_frame;
    newSource->mediaSource.is_seekable = shm_is_seekable;
    newSource->mediaSource.seek = shm_seek;
    newSource->mediaSource.get_length = shm_get_length;
    newSource->mediaSource.get_position = shm_get_position;
    newSource->mediaSource.get_available_length = shm_get_available_length;
    newSource->mediaSource.eof = shm_eof;
    newSource->mediaSource.set_source_name = shm_set_source_name;
    newSource->mediaSource.set_clip_id = shm_set_clip_id;
    newSource->mediaSource.close = shm_close;


    sourceId = msc_create_id();

    /* video track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = PICTURE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    newSource->tracks[newSource->numTracks].streamInfo.width = width;
    newSource->tracks[newSource->numTracks].streamInfo.height = height;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.num = 16;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.den = 9;
    switch (captureFormat)
    {
        case Format422UYVY:
            newSource->tracks[newSource->numTracks].frameSize = width * height * 2;
            newSource->tracks[newSource->numTracks].streamInfo.format = UYVY_FORMAT;
            break;

        case Format422PlanarYUV:
            newSource->tracks[newSource->numTracks].frameSize = width * height * 2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV422_FORMAT;
            break;

        case Format422PlanarYUVShifted:
            newSource->tracks[newSource->numTracks].frameSize = width * height * 2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV422_FORMAT;
            break;

        case Format420PlanarYUV:
            newSource->tracks[newSource->numTracks].frameSize = width * height * 3/2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV420_FORMAT;
            break;

        case Format420PlanarYUVShifted:
            newSource->tracks[newSource->numTracks].frameSize = width * height * 3/2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV420_FORMAT;
            break;
        default:
            goto fail;
            break;
    }
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, title));
    newSource->numTracks++;

    /* audio tracks */
    for (i = 0; i < newSource->numAudioTracks; i++)
    {
        sprintf(nameBuf, "Shared Memory Audio %d\n", i);
        
        CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
        newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
        newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
        newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
        newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
        newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
        newSource->tracks[newSource->numTracks].streamInfo.samplingRate = g_profAudioSamplingRate;
        newSource->tracks[newSource->numTracks].streamInfo.numChannels = 1;
        newSource->tracks[newSource->numTracks].streamInfo.bitsPerSample = 16;
        CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, nameBuf));
        newSource->tracks[newSource->numTracks].frameSize = 0;  /* zero indicates variable size */
        newSource->numTracks++;
    }

    /* timecode tracks */
    for (timecodeType = LTC_TRACK; timecodeType <= SYSTEM_TC_TRACK; timecodeType++)
    {
        CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
        newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
        newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
        newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
        newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
        newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
        newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);

        switch (timecodeType)
        {
            case LTC_TRACK:
                newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
                newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
                CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE,
                    "Shared Memory LTC"));
                break;
            case VITC_TRACK:
                newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
                newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
                CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE,
                    "Shared Memory VITC"));
                break;
            case DLTC_TRACK:
                newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
                newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = DLTC_SOURCE_TIMECODE_SUBTYPE;
                CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE,
                    "Shared Memory DVITC"));
                break;
            case DVITC_TRACK:
                newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
                newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = DVITC_SOURCE_TIMECODE_SUBTYPE;
                CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE,
                    "Shared Memory DVITC"));
                break;
            case SYSTEM_TC_TRACK:
                newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SYSTEM_TIMECODE_TYPE;
                newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = NO_TIMECODE_SUBTYPE;
                CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE,
                    "Shared Memory SYSTEM TC"));
                break;
        }

        newSource->numTracks++;
    }
        

    /* event track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = EVENT_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = SOURCE_EVENT_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Event"));
    newSource->numTracks++;


    /* disabled tracks if the channel is not in use */
    if (disable)
    {
        for (i = 0; i < newSource->numTracks; i++)
        {
            CHK_OFAIL(msc_disable_stream(&newSource->mediaSource, i));
        }
    }


    *source = newSource;
    return 1;

fail:
    shm_close(newSource);
    return 0;
}

MediaSource* shms_get_media_source(SharedMemSource* source)
{
    return &source->mediaSource;
}

int shms_get_default_timecode(SharedMemSource* source, TimecodeType* type, TimecodeSubType* subType)
{
    *type = source->defaultTCType;
    *subType = source->defaultTCSubType;
    
    return 1;
}


#endif


