#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "raw_file_source.h"
#include "logging.h"
#include "macros.h"


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
} RawFileSource;



static int position_is_valid(RawFileSource* source)
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

static char* convert_duration(int64_t duration, const Rational* frameRate, char* str)
{
    int timeBase = frameRate->num / frameRate->den;
    
    if (duration < 0)
    {
        sprintf(str, "?"); 
    }
    else
    {
        sprintf(str, "%02"PFi64":%02"PFi64":%02"PFi64":%02"PFi64, 
            duration / (60 * 60 * timeBase),
            (duration % (60 * 60 * timeBase)) / (60 * timeBase),
            ((duration % (60 * 60 * timeBase)) % (60 * timeBase)) / timeBase,
            ((duration % (60 * 60 * timeBase)) % (60 * timeBase)) % timeBase);
    }
    return str;
}



static int rfs_get_num_streams(void* data)
{
    return 1;
}

static int rfs_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    RawFileSource* source = (RawFileSource*)data;
    
    if (streamIndex < 0 || streamIndex >= 1)
    {
        return 0;
    }
    
    *streamInfo = &source->streamInfo;
    return 1;
}

static int rfs_disable_stream(void* data, int streamIndex)
{
    RawFileSource* source = (RawFileSource*)data;
    
    if (streamIndex != 0)
    {
        return 0;
    }
    
    source->isDisabled = 1;
    return 1;
}

static void rfs_disable_audio(void* data)
{
    RawFileSource* source = (RawFileSource*)data;
    
    if (source->streamInfo.type == SOUND_STREAM_TYPE)
    {
        source->isDisabled = 1;
    }
}

static int rfs_stream_is_disabled(void* data, int streamIndex)
{
    RawFileSource* source = (RawFileSource*)data;
    
    if (streamIndex != 0)
    {
        return 0;
    }
    
    return source->isDisabled;
}

static int rfs_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    RawFileSource* source = (RawFileSource*)data;
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

static int rfs_is_seekable(void* data)
{
    return 1;
}

static int rfs_seek(void* data, int64_t position)
{
    RawFileSource* source = (RawFileSource*)data;
    
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

static int rfs_get_length(void* data, int64_t* length)
{
    RawFileSource* source = (RawFileSource*)data;
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

static int rfs_get_position(void* data, int64_t* position)
{
    RawFileSource* source = (RawFileSource*)data;

    if (source->isDisabled)
    {
        return 0;
    }
    
    *position = source->position;
    return 1;
}

static int rfs_get_available_length(void* data, int64_t* length)
{
    return rfs_get_length(data, length);
}

static int rfs_eof(void* data)
{
    RawFileSource* source = (RawFileSource*)data;
    int64_t length;
    
    if (source->isDisabled)
    {
        return 0;
    }
    
    /* if we can determine the length and the position >=, then we are at eof */
    if (source->length < 0)
    {
        rfs_get_length(data, &length);
    }
    if (source->length >= 0 && source->position >= source->length)
    {
        return 1;
    }
    return 0;
}

static void rfs_close(void* data)
{
    RawFileSource* source = (RawFileSource*)data;
    
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


int rfs_open(const char* filename, const StreamInfo* streamInfo, MediaSource** source)
{
    RawFileSource* newSource = NULL;
    char stringBuf[128];
    int64_t duration = -1;
    
    CALLOC_ORET(newSource, RawFileSource, 1);

    if ((newSource->fs = fopen(filename, "rb")) == NULL)
    {
        ml_log_error("Failed to open raw file '%s'\n", filename);
        goto fail;
    }

    int frameSize;
    switch (streamInfo->format) 
    {
        case UYVY_FORMAT:
        case YUV422_FORMAT:
            frameSize = streamInfo->width * streamInfo->height * 2;
            break;
        case YUV420_FORMAT:
            frameSize = streamInfo->width * streamInfo->height * 3 / 2;
            break;
        case YUV444_FORMAT:
            frameSize = streamInfo->width * streamInfo->height * 3;
            break;
        case UYVY_10BIT_FORMAT:
            frameSize = streamInfo->width * streamInfo->height * 2 * 10 / 8;
            break;
        case PCM_FORMAT:
            frameSize = streamInfo->numChannels *
                (streamInfo->bitsPerSample + 7) / 8 * 
                streamInfo->samplingRate.num * streamInfo->frameRate.den / 
                    (streamInfo->samplingRate.den * streamInfo->frameRate.num);
            break;
        case TIMECODE_FORMAT:
            frameSize = sizeof(Timecode); /* yes, not very portable. TODO: make it so */
            break;
        default:
            goto fail;
    }
    newSource->frameSize = frameSize;
    newSource->length = -1;
    
    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = rfs_get_num_streams;
    newSource->mediaSource.get_stream_info = rfs_get_stream_info;
    newSource->mediaSource.disable_stream = rfs_disable_stream;
    newSource->mediaSource.disable_audio = rfs_disable_audio;
    newSource->mediaSource.stream_is_disabled = rfs_stream_is_disabled;
    newSource->mediaSource.read_frame = rfs_read_frame;
    newSource->mediaSource.is_seekable = rfs_is_seekable;
    newSource->mediaSource.seek = rfs_seek;
    newSource->mediaSource.get_length = rfs_get_length;
    newSource->mediaSource.get_position = rfs_get_position;
    newSource->mediaSource.get_available_length = rfs_get_available_length;
    newSource->mediaSource.eof = rfs_eof;
    newSource->mediaSource.close = rfs_close;
    
    newSource->streamInfo = *streamInfo;
    
    rfs_get_length(newSource, &duration);
    
    CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_NAME, filename));
    switch (streamInfo->format) 
    {    
        case UYVY_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "Raw UYVY"));
            break;
        case YUV422_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "Raw YUV422"));
            break;
        case YUV420_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "Raw YUV420"));
            break;
        case YUV444_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "Raw YUV444"));
            break;
        case UYVY_10BIT_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "Raw 10 bit UYVY"));
            break;
        case PCM_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "Raw PCM"));
            break;
        case TIMECODE_FORMAT:
            CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_TYPE, "Raw Timecode"));
            break;
        default:
            goto fail;
    }
    CHK_OFAIL(add_known_source_info(&newSource->streamInfo, SRC_INFO_FILE_DURATION, 
        convert_duration(duration, &streamInfo->frameRate, stringBuf)));
    
    *source = &newSource->mediaSource;
    return 1;
    
fail:
    rfs_close(newSource);
    return 0;
}