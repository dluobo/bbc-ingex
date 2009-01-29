/*
 * $Id: blank_source.c,v 1.5 2009/01/29 07:10:26 stuart_hc Exp $
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
#include <math.h>

#include "blank_source.h"
#include "YUV_frame.h"
#include "YUV_text_overlay.h"
#include "video_conversion.h"
#include "utils.h"
#include "types.h"
#include "logging.h"
#include "macros.h"


typedef struct
{
    MediaSource mediaSource;
    StreamInfo streamInfo;
    int wasFinalised;

    int isDisabled;

    int64_t length;
    int64_t position;

    unsigned char* image;
    unsigned int imageSize;
} BlankSource;


static int bks_finalise_blank_source(void* data, const StreamInfo* streamInfo)
{
    BlankSource* source = (BlankSource*)data;

    if (source->streamInfo.format == UNKNOWN_FORMAT)
    {
        source->streamInfo.format = streamInfo->format;
    }
    if (source->streamInfo.width < 1)
    {
        source->streamInfo.width = streamInfo->width;
    }
    if (source->streamInfo.height < 1)
    {
        source->streamInfo.height = streamInfo->height;
    }
    if (source->streamInfo.frameRate.num < 1 || source->streamInfo.frameRate.den < 1)
    {
        source->streamInfo.frameRate = streamInfo->frameRate;
        source->streamInfo.isHardFrameRate = 0;
    }
    if (source->streamInfo.aspectRatio.num < 1 || source->streamInfo.aspectRatio.den < 1)
    {
        source->streamInfo.aspectRatio = streamInfo->aspectRatio;
    }

    if (source->streamInfo.format == UYVY_FORMAT)
    {
        source->imageSize = source->streamInfo.width * source->streamInfo.height * 2;
        MALLOC_ORET(source->image, unsigned char, source->imageSize);
        fill_black(UYVY_FORMAT, source->streamInfo.width, source->streamInfo.height, source->image);
    }
    else if (source->streamInfo.format == YUV422_FORMAT)
    {
        source->imageSize = source->streamInfo.width * source->streamInfo.height * 2;
        MALLOC_ORET(source->image, unsigned char, source->imageSize);
        fill_black(YUV422_FORMAT, source->streamInfo.width, source->streamInfo.height, source->image);
    }
    else if (source->streamInfo.format == YUV420_FORMAT)
    {
        source->imageSize = source->streamInfo.width * source->streamInfo.height * 3 / 2;
        MALLOC_ORET(source->image, unsigned char, source->imageSize);
        fill_black(YUV420_FORMAT, source->streamInfo.width, source->streamInfo.height, source->image);
    }
    else
    {
        ml_log_error("Unsupported video format for blank video source: %d\n", source->streamInfo.format);
        return 0;
    }

    source->wasFinalised = 1;
    return 1;
}

static int bks_get_num_streams(void* data)
{
    return 1;
}

static int bks_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    BlankSource* source = (BlankSource*)data;

    if (streamIndex < 0 || streamIndex >= 1)
    {
        return 0;
    }

    *streamInfo = &source->streamInfo;
    return 1;
}

static void bks_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    BlankSource* source = (BlankSource*)data;

    if (source->streamInfo.isHardFrameRate &&
        memcmp(frameRate, &source->streamInfo.frameRate, sizeof(*frameRate)) != 0)
    {
        msc_disable_stream(&source->mediaSource, 0);
        return;
    }

    source->length = convert_length(source->length, &source->streamInfo.frameRate, frameRate);
    source->position = convert_length(source->position, &source->streamInfo.frameRate, frameRate);
    source->streamInfo.frameRate = *frameRate;
}

static int bks_disable_stream(void* data, int streamIndex)
{
    BlankSource* source = (BlankSource*)data;

    if (streamIndex != 0)
    {
        return 0;
    }

    source->isDisabled = 1;
    return 1;
}

static int bks_stream_is_disabled(void* data, int streamIndex)
{
    BlankSource* source = (BlankSource*)data;

    if (streamIndex != 0)
    {
        return 0;
    }

    return source->isDisabled;
}

static int bks_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    BlankSource* source = (BlankSource*)data;
    CHK_ORET(source->wasFinalised);

    if (source->isDisabled)
    {
        return 0;
    }

    if (sdl_accept_frame(listener, 0, frameInfo))
    {
        if (!sdl_receive_frame_const(listener, 0, source->image, source->imageSize))
        {
            return -1;
        }
    }

    source->position += 1;

    return 0;
}

static int bks_is_seekable(void* data)
{
    return 1;
}

static int bks_seek(void* data, int64_t position)
{
    BlankSource* source = (BlankSource*)data;

    if (source->isDisabled)
    {
        return 0;
    }

    source->position = position;
    return 0;
}

static int bks_get_length(void* data, int64_t* length)
{
    BlankSource* source = (BlankSource*)data;

    *length = source->length;
    return 1;
}

static int bks_get_position(void* data, int64_t* position)
{
    BlankSource* source = (BlankSource*)data;

    if (source->isDisabled)
    {
        return 0;
    }

    *position = source->position;
    return 1;
}

static int bks_get_available_length(void* data, int64_t* length)
{
    BlankSource* source = (BlankSource*)data;

    *length = source->length;
    return 1;
}

static int bks_eof(void* data)
{
    BlankSource* source = (BlankSource*)data;

    if (source->isDisabled)
    {
        return 0;
    }

    if (source->position >= source->length)
    {
        return 1;
    }
    return 0;
}

static void bks_set_source_name(void* data, const char* name)
{
    BlankSource* source = (BlankSource*)data;

    add_known_source_info(&source->streamInfo, SRC_INFO_NAME, name);
}

static void bks_set_clip_id(void* data, const char* id)
{
    BlankSource* source = (BlankSource*)data;

    set_stream_clip_id(&source->streamInfo, id);
}

static void bks_close(void* data)
{
    BlankSource* source = (BlankSource*)data;

    if (data == NULL)
    {
        return;
    }

    clear_stream_info(&source->streamInfo);

    SAFE_FREE(&source->image);

    SAFE_FREE(&source);
}


int bks_create(const StreamInfo* videoStreamInfo, int64_t length, MediaSource** source)
{
    BlankSource* newSource = NULL;

    if (videoStreamInfo->type != PICTURE_STREAM_TYPE)
    {
        ml_log_error("Blank source only does video\n");
        return 0;
    }


    CALLOC_ORET(newSource, BlankSource, 1);

    newSource->length = length;

    newSource->mediaSource.data = newSource;
    newSource->mediaSource.finalise_blank_source = bks_finalise_blank_source;
    newSource->mediaSource.get_num_streams = bks_get_num_streams;
    newSource->mediaSource.get_stream_info = bks_get_stream_info;
    newSource->mediaSource.set_frame_rate_or_disable = bks_set_frame_rate_or_disable;
    newSource->mediaSource.disable_stream = bks_disable_stream;
    newSource->mediaSource.stream_is_disabled = bks_stream_is_disabled;
    newSource->mediaSource.read_frame = bks_read_frame;
    newSource->mediaSource.is_seekable = bks_is_seekable;
    newSource->mediaSource.seek = bks_seek;
    newSource->mediaSource.get_length = bks_get_length;
    newSource->mediaSource.get_position = bks_get_position;
    newSource->mediaSource.get_available_length = bks_get_available_length;
    newSource->mediaSource.eof = bks_eof;
    newSource->mediaSource.set_source_name = bks_set_source_name;
    newSource->mediaSource.set_clip_id = bks_set_clip_id;
    newSource->mediaSource.close = bks_close;

    newSource->streamInfo = *videoStreamInfo;
    newSource->streamInfo.sourceId = msc_create_id();

    CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_TITLE, "Blank Source"));


    *source = &newSource->mediaSource;
    return 1;

fail:
    bks_close(newSource);
    return 0;
}




