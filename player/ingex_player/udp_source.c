/*
 * $Id: udp_source.c,v 1.5 2009/01/29 07:10:27 stuart_hc Exp $
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

#include "udp_source.h"
#include "video_conversion.h"
#include "source_event.h"
#include "logging.h"
#include "macros.h"

#if defined(DISABLE_UDP_SOURCE)

int udp_open(const char *address, MediaSource** source)
{
    return 0;
}


#else


#include "multicast_video.h"

#define MAX_TRACKS      19


typedef struct
{
    int streamIndex;
    StreamInfo streamInfo;
    int frameSize;
    int isDisabled;
} TrackInfo;

typedef struct
{
    MediaSource mediaSource;

    int socket_fd;
    udp_reader_thread_t udp_reader;
    uint8_t *video;
    uint8_t *audio;

    TrackInfo tracks[MAX_TRACKS];
    int numTracks;

    Rational frameRate;
    int64_t position;

    int prevLastFrame;

    char sourceName[MULTICAST_SOURCE_NAME_SIZE];
} UDPSource;


static int udp_get_num_streams(void* data)
{
    UDPSource* source = (UDPSource*)data;

    return source->numTracks;
}

static int udp_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    UDPSource* source = (UDPSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    *streamInfo = &source->tracks[streamIndex].streamInfo;
    return 1;
}

static void udp_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    UDPSource* source = (UDPSource*)data;
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

static int udp_disable_stream(void* data, int streamIndex)
{
    UDPSource* source = (UDPSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    source->tracks[streamIndex].isDisabled = 1;
    return 1;
}

static void udp_disable_audio(void* data)
{
    UDPSource* source = (UDPSource*)data;
    int i;

    for (i = 0; i < source->numTracks; i++)
    {
        if (source->tracks[i].streamInfo.type == SOUND_STREAM_TYPE)
        {
            source->tracks[i].isDisabled = 1;
        }
    }
}

static int udp_stream_is_disabled(void* data, int streamIndex)
{
    UDPSource* source = (UDPSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    return source->tracks[streamIndex].isDisabled;
}


/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int udp_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    UDPSource* source = (UDPSource*)data;
    int i;
    TrackInfo* track;
    unsigned char* buffer;
    int nameUpdated = 0;
    int roundedFrameRate = get_rounded_frame_rate(&source->frameRate);

    /* Read a frame from the network */
    IngexNetworkHeader header;
    memset(&header, 0, sizeof(header));
    int packets_read = 0;
    int bad_frame = 0;
    double timeout = 0.045;    /* experiment found this worked ok */

#ifdef MULTICAST_SINGLE_THREAD
    int res = udp_read_frame_audio_video(source->socket_fd, timeout, &header, source->video, source->audio, &packets_read);
    if (res == -1) {
        printf("udp_read_frame_audio_video() timeout packets_read=%d\n", packets_read);
        bad_frame = 1;
    }
#else
    int res = udp_read_next_frame(&source->udp_reader, timeout, &header, source->video, source->audio, &packets_read);
    if (res == -1) {
        printf("udp_read_next_frame() timeout packets_read=%d\n", packets_read);
        bad_frame = 1;
    }
#endif

    source->prevLastFrame = header.frame_number;

    /* check for updated source name */
    // TODO: user strcmp to check header.source_name against last name
    if (! bad_frame)
    {
        if (strlen(source->sourceName) == 0 ||
            strncmp(source->sourceName, header.source_name, MULTICAST_SOURCE_NAME_SIZE-1) != 0) {
            nameUpdated = 1;
            strncpy(source->sourceName, header.source_name, MULTICAST_SOURCE_NAME_SIZE-1);
        }
    }


    // Track numbers are hard-coded by setup code in udp_open()
    // 0 - video
    // 1 - audio 1
    // 2 - audio 2
    // 3 - timecode VITC
    // 4 - timecode LTC
    // 5 - event
    //
    for (i = 0; i < source->numTracks; i++)
    {
        track = &source->tracks[i];

        if (track->isDisabled)
            continue;

        if (! sdl_accept_frame(listener, i, frameInfo))
            continue;

        /* calculate current frame size for events stream */
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
            memcpy(buffer, source->video, track->frameSize);
        }

        if (track->streamInfo.type == SOUND_STREAM_TYPE)
        {
            if (bad_frame) {
                /* TODO: audio buffer size must be read from the incoming data */
                /* silence audio when bad frame received */
                memset(buffer, 0, 1920*2);
            }
            else {
                /* TODO: audio buffer size must be read from the incoming data */
                /* audio 1 is on track i==1, audio 2 is on track i==2 */
                int channel = i - 1;
                memcpy(buffer, source->audio + channel * 1920*2, 1920*2);
            }
        }

        if (track->streamInfo.type == TIMECODE_STREAM_TYPE) {
            // choose vitc or ltc based on current track (i)
            int tc_as_int = (i == 2) ? header.vitc : header.ltc;
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

static int udp_is_seekable(void* data)
{
    /* cannot seek */
    return 0;
}

/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int udp_seek(void* data, int64_t position)
{
    /* cannot seek and shouldn't have called this function */
    return -1;
}

static int udp_get_length(void* data, int64_t* length)
{
    /* unknown length */
    return 0;
}

static int udp_get_position(void* data, int64_t* position)
{
    UDPSource* source = (UDPSource*)data;

    *position = source->position;
    return 1;
}

static int udp_get_available_length(void* data, int64_t* length)
{
    return udp_get_length(data, length);
}

static int udp_eof(void* data)
{
    /* never at "end of file" */
    return 0;
}

static void udp_set_source_name(void* data, const char* name)
{
    UDPSource* source = (UDPSource*)data;

    int i;
    for (i = 0; i < source->numTracks; i++)
    {
        add_known_source_info(&source->tracks[i].streamInfo, SRC_INFO_NAME, name);
    }
}

static void udp_set_clip_id(void* data, const char* id)
{
    UDPSource* source = (UDPSource*)data;

    int i;
    for (i = 0; i < source->numTracks; i++)
    {
        set_stream_clip_id(&source->tracks[i].streamInfo, id);
    }
}

static void udp_close(void* data)
{
    UDPSource* source = (UDPSource*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

#ifndef MULTICAST_SINGLE_THREAD
	udp_shutdown_reader(&source->udp_reader);
#endif

    for (i = 0; i < source->numTracks; i++)
    {
        clear_stream_info(&source->tracks[i].streamInfo);
    }

    free(source->video);
    free(source->audio);

    SAFE_FREE(&source);
}

#define DEFAULT_PORT 2000

// The multicast address is of the form 239.255.x.y:port
// where port by convention is:
//   2000 for channel 0
//   2001 for channel 1
//   ...
int udp_open(const char *address, MediaSource** source)
{
    UDPSource* newSource = NULL;
    int sourceId;

    // Extract address and port number from string e.g. "239.255.1.1:2000"
    char remote[FILENAME_MAX];
    strncpy(remote, address, sizeof(remote)-1);
    char *p;
    int port;
    if ((p = strchr(remote, ':')) == NULL) {    /* couldn't find ':' */
        port = MULTICAST_DEFAULT_PORT;        /* default port to 2000 (channel 0) */
    }
    else {
        port = atol(p+1);    // extract port
        *p = '\0';          // terminate remote string
    }

    // setup network socket
    int fd;
    if ((fd = connect_to_multicast_address(remote, port)) == -1)
    {
        ml_log_error("Failed to connect to UDP address %s:%d\n", remote, port);
        return 0;
    }

    // Read video parameters from multicast stream
    IngexNetworkHeader header;
    if (udp_read_frame_header(fd, &header) == -1)
    {
        ml_log_error("Failed to read from UDP address %s:%d\n", remote, port);
        return 0;
    }

    /* TODO: handle varying NTSC audio frame sizes */
    if (header.framerate_numer != 25 && header.framerate_denom != 1)
    {
        ml_log_error("TODO: udp_source() only supports PAL frame rates\n");
        return 0;
    }


    CALLOC_ORET(newSource, UDPSource, 1);

    newSource->socket_fd = fd;
    newSource->frameRate.num = header.framerate_numer;
    newSource->frameRate.den = header.framerate_denom;

#ifndef MULTICAST_SINGLE_THREAD
    newSource->udp_reader.fd = fd;
    udp_init_reader(header.width, header.height, &newSource->udp_reader);
#endif

    newSource->prevLastFrame = -1;
    memset(newSource->sourceName, 0, MULTICAST_SOURCE_NAME_SIZE);

    char title[FILENAME_MAX];
    sprintf(title, "%s from %s\n", header.source_name, address);


    /* FIXME: can this be done later? */
    /* allocate video and audio buffers for this source */
    newSource->video = malloc(header.width * header.height * 3/2);
    newSource->audio = malloc(header.audio_size);


    // setup media source
    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = udp_get_num_streams;
    newSource->mediaSource.get_stream_info = udp_get_stream_info;
    newSource->mediaSource.set_frame_rate_or_disable = udp_set_frame_rate_or_disable;
    newSource->mediaSource.disable_stream = udp_disable_stream;
    newSource->mediaSource.disable_audio = udp_disable_audio;
    newSource->mediaSource.stream_is_disabled = udp_stream_is_disabled;
    newSource->mediaSource.read_frame = udp_read_frame;
    newSource->mediaSource.is_seekable = udp_is_seekable;
    newSource->mediaSource.seek = udp_seek;
    newSource->mediaSource.get_length = udp_get_length;
    newSource->mediaSource.get_position = udp_get_position;
    newSource->mediaSource.get_available_length = udp_get_available_length;
    newSource->mediaSource.set_source_name = udp_set_source_name;
    newSource->mediaSource.set_clip_id = udp_set_clip_id;
    newSource->mediaSource.eof = udp_eof;
    newSource->mediaSource.close = udp_close;


    sourceId = msc_create_id();

    /* video track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = PICTURE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    newSource->tracks[newSource->numTracks].streamInfo.width = header.width;
    newSource->tracks[newSource->numTracks].streamInfo.height = header.height;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.num = 4;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.den = 3;
    newSource->tracks[newSource->numTracks].frameSize = header.width * header.height * 3/2;
    newSource->tracks[newSource->numTracks].streamInfo.format = YUV420_FORMAT;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, title));
    newSource->numTracks++;

    /* audio track 1 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    newSource->tracks[newSource->numTracks].streamInfo.samplingRate = g_profAudioSamplingRate;
    newSource->tracks[newSource->numTracks].streamInfo.numChannels = 1;
    newSource->tracks[newSource->numTracks].streamInfo.bitsPerSample = 16;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Audio 1"));
    newSource->tracks[newSource->numTracks].frameSize = 2 * 48000 / 25; // TODO: varying for NTSC frame rates
    newSource->numTracks++;

    /* audio track 2 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    newSource->tracks[newSource->numTracks].streamInfo.samplingRate = g_profAudioSamplingRate;
    newSource->tracks[newSource->numTracks].streamInfo.numChannels = 1;
    newSource->tracks[newSource->numTracks].streamInfo.bitsPerSample = 16;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Audio 2"));
    newSource->tracks[newSource->numTracks].frameSize = 2 * 48000 / 25; // TODO: varying for NTSC frame rates
    newSource->numTracks++;

    /* timecode track 1 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Timecode 1"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    /* timecode track 2 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Timecode 2"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    /* event track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = EVENT_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = SOURCE_EVENT_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = newSource->frameRate;
    newSource->tracks[newSource->numTracks].streamInfo.isHardFrameRate = 1;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Event"));
    newSource->numTracks++;


    *source = &newSource->mediaSource;
    return 1;

fail:
    udp_close(newSource);
    return 0;
}


#endif

