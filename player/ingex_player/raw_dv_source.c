/*
 * $Id: raw_dv_source.c,v 1.4 2009/01/29 07:10:27 stuart_hc Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "raw_dv_source.h"
#include "logging.h"
#include "macros.h"


/* TODO: add support for DV audio */


#define DV_DIF_BLOCK_SIZE           80

#define DV_DIF_SEQUENCE_SIZE        (150 * DV_DIF_BLOCK_SIZE)




typedef struct
{
    int channelSequenceLength;
    int is625;
    int isIEC;
    int is4By3; /* else 16:9 */
    int mbps; /* 25Mbps, 50Mbps */
} SequenceInfo;

typedef struct
{
    MediaSource mediaSource;
    StreamInfo streamInfo;
    FILE* fs;
    int frameSize;
    int isDisabled;

    int64_t position;
    int positionValid;

    int64_t length;
} RawDVSource;



static int read_dv_sequence(FILE* dvFile, SequenceInfo* info)
{
    unsigned char buffer[DV_DIF_SEQUENCE_SIZE];
    size_t result;
    unsigned char byte;

    result = fread(buffer, DV_DIF_SEQUENCE_SIZE, 1, dvFile);
    if (result != 1)
    {
        if (feof(dvFile))
        {
            return 2;
        }

        ml_log_error("Failed to read: %s\n", strerror(errno));
        return 1;
    }


    /* check section ids */
    if ((buffer[0] & 0xe0) != 0x00 || /* header */
        (buffer[DV_DIF_BLOCK_SIZE] & 0xe0) != 0x20 || /* subcode */
        (buffer[3 * DV_DIF_BLOCK_SIZE] & 0xe0) != 0x40 || /* vaux */
        (buffer[6 * DV_DIF_BLOCK_SIZE] & 0xe0) != 0x60 || /* audio */
        (buffer[15 * DV_DIF_BLOCK_SIZE] & 0xe0) != 0x80) /* video */
    {
        return 3;
    }

    /* check video and vaux section are transmitted */
    if (buffer[6] & 0x80)
    {
        ml_log_error("No video in DV file\n");
        return 4;
    }


    /* channel sequence length and 525/625 is extracted from the DSF in byte 3 */
    byte = buffer[3];
    if (byte & 0x80)
    {
        info->channelSequenceLength = 12;
        info->is625 = 1;
    }
    else
    {
        info->channelSequenceLength = 10;
        info->is625 = 0;
    }


    /* IEC/DV is extracted from the APT in byte 4 */
    byte = buffer[4];
    if (byte & 0x03) /* DV-based if APT all 001 or all 111 */
    {
        info->isIEC = 0;
    }
    else
    {
        info->isIEC = 1;
    }

    /* aspect ratio is extracted from the VAUX section, VSC pack, DISP bits */
    byte = buffer[3 * DV_DIF_BLOCK_SIZE + 2 * DV_DIF_BLOCK_SIZE + 3 + 10 * 5 + 2];
    if ((byte & 0x07) == 0x02)
    {
        info->is4By3 = 0;
    }
    else
    {
        /* either byte & 0x07 == 0x00 or we default to 4:3 */
        info->is4By3 = 1;
    }

    /* mbps is extracted from the VAUX section, VS pack, STYPE bits */
    byte = buffer[3 * DV_DIF_BLOCK_SIZE + 2 * DV_DIF_BLOCK_SIZE + 3 + 9 * 5 + 3];
    byte &= 0x1f;
    if (byte == 0x00)
    {
        info->mbps = 25;
    }
    else if (byte == 0x04)
    {
        info->mbps = 50;
    }
    else if (byte == 0x14 || byte == 0x15 || byte == 0x18)
    {
        ml_log_error("DV 100 not yet supported\n");
        return 5;
    }
    else
    {
        return 6;
    }

    return 0;
}

static int get_dv_stream_info(RawDVSource* source)
{
    SequenceInfo info;
    int result;

    result = read_dv_sequence(source->fs, &info);
    if (result != 0)
    {
        return 0;
    }

    source->streamInfo.sourceId = msc_create_id();
    source->streamInfo.type = PICTURE_STREAM_TYPE;
    source->streamInfo.singleField = 0;

    if (info.is625)
    {
        source->streamInfo.frameRate.num = 25;
        source->streamInfo.frameRate.den = 1;
        source->streamInfo.isHardFrameRate = 1;
        source->streamInfo.width = 720;
        source->streamInfo.height = 576;
    }
    else
    {
        source->streamInfo.frameRate.num = 30000;
        source->streamInfo.frameRate.den = 1001;
        source->streamInfo.isHardFrameRate = 1;
        source->streamInfo.width = 720;
        source->streamInfo.height = 480;
    }

    if (info.is4By3)
    {
        source->streamInfo.aspectRatio.num = 4;
        source->streamInfo.aspectRatio.den = 3;
    }
    else
    {
        source->streamInfo.aspectRatio.num = 16;
        source->streamInfo.aspectRatio.den = 9;
    }

    if (info.mbps == 25)
    {
        source->streamInfo.format = (info.isIEC && info.is625) ? DV25_YUV420_FORMAT : DV25_YUV411_FORMAT;
        source->frameSize = (info.is625 ? 144000 : 120000);
    }
    else /* info.mbps == 50 */
    {
        source->streamInfo.format = DV50_FORMAT;
        source->frameSize = (info.is625 ? 288000 : 240000);
    }


    CHK_ORET(fseeko(source->fs, 0, SEEK_SET) == 0);
    return 1;
}


static int position_is_valid(RawDVSource* source)
{
    long result;

    if (source->positionValid)
    {
        return 1;
    }
    else
    {
        /* get the position */
        result = ftello(source->fs);
        if (result < 0)
        {
            ml_log_error("Failed to ftello raw file source\n");
            return 0;
        }

        /* make sure the file is positioned at the expected position */
        if (result % source->frameSize != 0 || result != source->position * source->frameSize)
        {
            if (fseeko(source->fs, source->position * source->frameSize, SEEK_SET) != 0)
            {
                ml_log_error("Failed to fseeko in raw file source\n");
                return 0;
            }
        }

        source->positionValid = 1;
    }

    return 1;
}



static int rds_get_num_streams(void* data)
{
    return 1;
}

static int rds_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    RawDVSource* source = (RawDVSource*)data;

    if (streamIndex < 0 || streamIndex >= 1)
    {
        return 0;
    }

    *streamInfo = &source->streamInfo;
    return 1;
}

static void rds_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    RawDVSource* source = (RawDVSource*)data;

    /* disable if frame rate differs from DV frame rate */
    if (memcmp(frameRate, &source->streamInfo.frameRate, sizeof(*frameRate)) != 0)
    {
        msc_disable_stream(&source->mediaSource, 0);
    }
}

static int rds_disable_stream(void* data, int streamIndex)
{
    RawDVSource* source = (RawDVSource*)data;

    if (streamIndex != 0)
    {
        return 0;
    }

    source->isDisabled = 1;
    return 1;
}

static void rds_disable_audio(void* data)
{
    RawDVSource* source = (RawDVSource*)data;

    if (source->streamInfo.type == SOUND_STREAM_TYPE)
    {
        source->isDisabled = 1;
    }
}

static int rds_stream_is_disabled(void* data, int streamIndex)
{
    RawDVSource* source = (RawDVSource*)data;

    if (streamIndex != 0)
    {
        return 0;
    }

    return source->isDisabled;
}

static int rds_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    RawDVSource* source = (RawDVSource*)data;
    unsigned char* buffer;

    if (source->isDisabled)
    {
        return 0;
    }

    if (!position_is_valid(source))
    {
        return -1;
    }


    if (sdl_accept_frame(listener, 0, frameInfo))
    {
        if (!sdl_allocate_buffer(listener, 0, &buffer, source->frameSize))
        {
            /* position is still valid because the file position hasn't changed */
            return -1;
        }

        if (fread(buffer, source->frameSize, 1, source->fs) != 1)
        {
            sdl_deallocate_buffer(listener, 0, &buffer);
            source->positionValid = 0;
            return -1;
        }

        sdl_receive_frame(listener, 0, buffer, source->frameSize);
    }
    else
    {
        /* skip this frame */
        if (fseeko(source->fs, source->frameSize, SEEK_CUR) != 0)
        {
            source->positionValid = 0;
            return -1;
        }
    }

    source->position += 1;

    return 0;
}

static int rds_is_seekable(void* data)
{
    return 1;
}

static int rds_seek(void* data, int64_t position)
{
    RawDVSource* source = (RawDVSource*)data;

    if (source->isDisabled)
    {
        return 0;
    }

    if (!position_is_valid(source))
    {
        return -1;
    }

    if (fseeko(source->fs, position * source->frameSize, SEEK_SET) != 0)
    {
        source->positionValid = 0;
        return -1;
    }

    source->position = position;
    return 0;
}

static int rds_get_length(void* data, int64_t* length)
{
    RawDVSource* source = (RawDVSource*)data;
    struct stat statBuf;
    int fo;

    if (source->length < 0)
    {
        if ((fo = fileno(source->fs)) == -1)
        {
            ml_log_error("Failed to get raw source file length; fileno failed\n");
            return 0;
        }
        else if (fstat(fo, &statBuf) != 0)
        {
            ml_log_error("Failed to get raw source file length; fstat failed\n");
            return 0;
        }

        source->length = statBuf.st_size / source->frameSize;
    }

    *length = source->length;
    return 1;
}

static int rds_get_position(void* data, int64_t* position)
{
    RawDVSource* source = (RawDVSource*)data;

    if (source->isDisabled)
    {
        return 0;
    }

    *position = source->position;
    return 1;
}

static int rds_get_available_length(void* data, int64_t* length)
{
    return rds_get_length(data, length);
}

static int rds_eof(void* data)
{
    RawDVSource* source = (RawDVSource*)data;
    int64_t length;

    if (source->isDisabled)
    {
        return 0;
    }

    /* if we can determine the length and the position >=, then we are at eof */
    if (source->length < 0)
    {
        rds_get_length(data, &length);
    }
    if (source->length >= 0 && source->position >= source->length)
    {
        return 1;
    }
    return 0;
}

static void rds_set_source_name(void* data, const char* name)
{
    RawDVSource* source = (RawDVSource*)data;

    add_known_source_info(&source->streamInfo, SRC_INFO_NAME, name);
}

static void rds_set_clip_id(void* data, const char* id)
{
    RawDVSource* source = (RawDVSource*)data;

    set_stream_clip_id(&source->streamInfo, id);
}

static void rds_close(void* data)
{
    RawDVSource* source = (RawDVSource*)data;

    if (data == NULL)
    {
        return;
    }

    if (source->fs != NULL)
    {
        fclose(source->fs);
    }

    clear_stream_info(&source->streamInfo);

    SAFE_FREE(&source);
}


int rds_open(const char* filename, MediaSource** source)
{
    RawDVSource* newSource = NULL;
    int64_t duration = -1;
    int timecodeBase;

    CALLOC_ORET(newSource, RawDVSource, 1);
    newSource->length = -1;


    if ((newSource->fs = fopen(filename, "rb")) == NULL)
    {
        ml_log_error("Failed to open raw file '%s'\n", filename);
        goto fail;
    }

    CHK_OFAIL(get_dv_stream_info(newSource));


    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = rds_get_num_streams;
    newSource->mediaSource.get_stream_info = rds_get_stream_info;
    newSource->mediaSource.set_frame_rate_or_disable = rds_set_frame_rate_or_disable;
    newSource->mediaSource.disable_stream = rds_disable_stream;
    newSource->mediaSource.disable_audio = rds_disable_audio;
    newSource->mediaSource.stream_is_disabled = rds_stream_is_disabled;
    newSource->mediaSource.read_frame = rds_read_frame;
    newSource->mediaSource.is_seekable = rds_is_seekable;
    newSource->mediaSource.seek = rds_seek;
    newSource->mediaSource.get_length = rds_get_length;
    newSource->mediaSource.get_position = rds_get_position;
    newSource->mediaSource.get_available_length = rds_get_available_length;
    newSource->mediaSource.eof = rds_eof;
    newSource->mediaSource.set_source_name = rds_set_source_name;
    newSource->mediaSource.set_clip_id = rds_set_clip_id;
    newSource->mediaSource.close = rds_close;


    rds_get_length(newSource, &duration);

    timecodeBase = (int)(newSource->streamInfo.frameRate.num / (double)newSource->streamInfo.frameRate.den + 0.5);

    CHK_OFAIL(add_filename_source_info(&newSource->streamInfo, SRC_INFO_FILE_NAME, filename));
    switch (newSource->streamInfo.format)
    {
        case DV25_YUV420_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "DV 25 4:2:0"));
            break;
        case DV25_YUV411_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "DV 25 4:1:1"));
            break;
        case DV50_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "DV 50"));
            break;
        default:
            goto fail;
    }
    CHK_OFAIL(add_timecode_source_info(&newSource->streamInfo, SRC_INFO_FILE_DURATION, duration, timecodeBase));


    *source = &newSource->mediaSource;
    return 1;

fail:
    rds_close(newSource);
    return 0;
}

