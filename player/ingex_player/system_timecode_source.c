#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "system_timecode_source.h"
#include "logging.h"
#include "macros.h"


/* 23:59:59:24 + 1 */
#define MAX_TIMECODE_COUNT          2160000


typedef struct
{
    MediaSource mediaSource;
    StreamInfo streamInfo;

    int isDisabled;

    int64_t startTimecode;
    
    int64_t length;
    int64_t position;
} SystemTimecodeSource;


static void get_timecode(SystemTimecodeSource* source, Timecode* timecode)
{
    int64_t count = (source->startTimecode + source->position) % (MAX_TIMECODE_COUNT);
    
    timecode->isDropFrame = 0;
    timecode->hour = count / (60 * 60 * 25);
    timecode->min = (count % (60 * 60 * 25)) / (60 * 25);
    timecode->sec = ((count % (60 * 60 * 25)) % (60 * 25)) / 25;
    timecode->frame = ((count % (60 * 60 * 25)) % (60 * 25)) % 25;
}


static int sts_get_num_streams(void* data)
{
    return 1;
}

static int sts_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;
    
    if (streamIndex < 0 || streamIndex >= 1)
    {
        return 0;
    }
    
    *streamInfo = &source->streamInfo;
    return 1;
}

static int sts_disable_stream(void* data, int streamIndex)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;
    
    if (streamIndex != 0)
    {
        return 0;
    }
    
    source->isDisabled = 1;
    return 1;
}

static int sts_stream_is_disabled(void* data, int streamIndex)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;
    
    if (streamIndex != 0)
    {
        return 0;
    }
    
    return source->isDisabled;
}

static int sts_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;
    unsigned char* buffer;
    
    if (source->isDisabled)
    {
        return 0;
    }
    
    if (sdl_accept_frame(listener, 0, frameInfo))
    {
        if (!sdl_allocate_buffer(listener, 0, &buffer, sizeof(Timecode)))
        {
            return -1;
        }
        
        get_timecode(source, (Timecode*)buffer);
        
        sdl_receive_frame(listener, 0, buffer, sizeof(Timecode));
    }
    
    source->position += 1;
    
    return 0;
}

static int sts_is_seekable(void* data)
{
    return 1;
}

static int sts_seek(void* data, int64_t position)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;
    
    if (source->isDisabled)
    {
        return 0;
    }
    
    source->position = position;
    return 0;
}

static int sts_get_length(void* data, int64_t* length)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;

    *length = source->length;
    return 1;
}

static int sts_get_position(void* data, int64_t* position)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;

    if (source->isDisabled)
    {
        return 0;
    }
    
    *position = source->position;
    return 1;
}

static int sts_get_available_length(void* data, int64_t* length)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;

    *length = source->length;
    return 1;
}

static int sts_eof(void* data)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;
    
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

static void sts_close(void* data)
{
    SystemTimecodeSource* source = (SystemTimecodeSource*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    clear_stream_info(&source->streamInfo);
    
    SAFE_FREE(&source);
}


int sts_create(int64_t startTimecode, MediaSource** source)
{
    SystemTimecodeSource* newSource = NULL;
    
    CALLOC_ORET(newSource, SystemTimecodeSource, 1);

    newSource->startTimecode = startTimecode;
    newSource->length = 0x7fffffffffffffffLL;
    
    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = sts_get_num_streams;
    newSource->mediaSource.get_stream_info = sts_get_stream_info;
    newSource->mediaSource.disable_stream = sts_disable_stream;
    newSource->mediaSource.stream_is_disabled = sts_stream_is_disabled;
    newSource->mediaSource.read_frame = sts_read_frame;
    newSource->mediaSource.is_seekable = sts_is_seekable;
    newSource->mediaSource.seek = sts_seek;
    newSource->mediaSource.get_length = sts_get_length;
    newSource->mediaSource.get_position = sts_get_position;
    newSource->mediaSource.get_available_length = sts_get_available_length;
    newSource->mediaSource.eof = sts_eof;
    newSource->mediaSource.close = sts_close;
    
    CHK_OFAIL(initialise_stream_info(&newSource->streamInfo));
    newSource->streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->streamInfo.format = TIMECODE_FORMAT;
    newSource->streamInfo.timecodeType = SYSTEM_TIMECODE_TYPE;
    newSource->streamInfo.timecodeSubType = NO_TIMECODE_SUBTYPE;
    
    *source = &newSource->mediaSource;
    return 1;
    
fail:
    sts_close(newSource);
    return 0;
}
