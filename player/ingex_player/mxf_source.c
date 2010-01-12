/*
 * $Id: mxf_source.c,v 1.12 2010/01/12 16:32:29 john_f Exp $
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
#include <sys/time.h>
#include <assert.h>

#include "mxf_source.h"
#include "logging.h"

#include <mxf_reader.h>
#include <archive_mxf_info_lib.h>
#include <mxf/mxf_page_file.h>
#include <mxf/mxf_avid_labels_and_keys.h>

/* undefine macros from libMXF */
#undef CHK_ORET
#undef CHK_OFAIL

#include "macros.h"


struct _MXFReaderListenerData
{
    MediaSourceListener* streamListener;
    struct MXFFileSource* mxfSource;
    FrameInfo frameInfo;
};

typedef struct
{
    StreamInfo streamInfo;
    int streamId;
    int isDisabled;

    int frameAccepted;
    int frameAllocated;
    uint8_t* buffer;
    uint32_t bufferSize;
} OutputStreamData;

typedef struct
{
    OutputStreamData* streamData;
    int numOutputStreams;

    uint8_t* audioBuffer;
    uint32_t audioBufferSize;
} InputTrackData;

struct MXFFileSource
{
    int markPSEFailures;
    int markVTRErrors;
    int markDigiBetaDropouts;
    char* filename;

    MediaSource mediaSource;
    
    VTRErrorSource vtrErrorSource;
    VTRErrorLevel vtrErrorLevel;
    VTRErrorAtPos* vtrErrors;
    long numVTRErrors;
    
    MXFDataModel* dataModel;
    MXFReader* mxfReader;
    MXFReaderListener mxfListener;
    MXFReaderListenerData mxfListenerData;
    InputTrackData* trackData;
    int numInputTracks;
    int numTimecodeTracks;
    int eof;
    int isArchiveMXF;

    struct timeval lastPostCompleteTry;
    int postCompleteTryCount;
    int donePostComplete;
};



static void mxf_log_connect(MXFLogLevel level, const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(level, 0, format, p_arg);
    va_end(p_arg);
}

static char* convert_date(mxfTimestamp* date, char* str)
{
    sprintf(str, "%04u-%02u-%02u", date->year, date->month, date->day);
    return str;
}

static char* convert_prog_duration(int64_t duration, char* str)
{
    if (duration < 0)
    {
        sprintf(str, "?");
    }
    else
    {
        sprintf(str, "%02"PFi64":%02"PFi64":%02"PFi64":00",
            duration / (60 * 60),
            (duration % (60 * 60)) / 60,
            (duration % (60 * 60)) % 60);
    }
    return str;
}

static char* convert_uint32(uint32_t value, char* str)
{
    sprintf(str, "%d", value);
    return str;
}

static void convert_frame_rate(const mxfRational* mxfFrameRate, Rational* frameRate)
{
    frameRate->num = mxfFrameRate->numerator;
    frameRate->den = mxfFrameRate->denominator;
}

static void convert_frame_rate_back(const Rational* frameRate, mxfRational* mxfFrameRate)
{
    mxfFrameRate->numerator = frameRate->num;
    mxfFrameRate->denominator = frameRate->den;
}

static void read_clip_umid_string(MXFReader* reader, char* clipId)
{
    MXFHeaderMetadata* headerMetadata = get_header_metadata(reader);
    MXFMetadataSet* materialPackageSet = NULL;
    mxfUMID umid;

    if (!mxf_find_singular_set_by_key(headerMetadata, &MXF_SET_K(MaterialPackage), &materialPackageSet))
    {
        ml_log_warn("Failed to find the MaterialPackage set in the MXF file\n");
        return;
    }

    if (!mxf_get_umid_item(materialPackageSet, &MXF_ITEM_K(GenericPackage, PackageUID), &umid))
    {
        ml_log_warn("Failed to get the MaterialPackage PackageUID item from the MXF file\n");
        return;
    }

    sprintf(clipId, "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        umid.octet0, umid.octet1, umid.octet2, umid.octet3,
        umid.octet4, umid.octet5, umid.octet6, umid.octet7,
        umid.octet8, umid.octet9, umid.octet10, umid.octet11,
        umid.octet12, umid.octet13, umid.octet14, umid.octet15,
        umid.octet16, umid.octet17, umid.octet18, umid.octet19,
        umid.octet20, umid.octet21, umid.octet22, umid.octet23,
        umid.octet24, umid.octet25, umid.octet26, umid.octet27,
        umid.octet28, umid.octet29, umid.octet30, umid.octet31);
}

static int deinterleave_audio(uint8_t* input, uint32_t inputSize, int numChannels, int bitsPerSample,
    int channelIndex, uint8_t* output, uint32_t outputSize)
{
    uint32_t i;
    int j;
    int blockAlign;
    int channelOffset;
    uint32_t numSamples;
    int bytesPerSample;

    blockAlign = (bitsPerSample + 7) / 8;
    channelOffset = channelIndex * blockAlign;
    bytesPerSample = blockAlign * numChannels;
    numSamples = inputSize / bytesPerSample;

    CHK_ORET(outputSize >= numSamples * blockAlign);

    for (i = 0; i < numSamples; i++)
    {
        for (j = 0; j < blockAlign; j++)
        {
            output[i * blockAlign + j] = input[i * bytesPerSample + channelOffset + j];
        }
    }

    return 1;
}


static void mark_vtr_errors(MXFFileSource* source, MediaSource* rootSource, MediaControl* mediaControl)
{
    long i;
    int64_t convertedPosition;
    long numMarked = 0;
    uint8_t audioErrorCode, videoErrorCode;
    
    if (source->vtrErrorLevel == VTR_NO_ERROR_LEVEL)
    {
        ml_log_info("Not marking VTR errors for level == %d\n", source->vtrErrorLevel);
        return;
    }
    
    for (i = 0; i < source->numVTRErrors; i++)
    {
        audioErrorCode = source->vtrErrors[i].errorCode & 0x0f;
        videoErrorCode = (source->vtrErrors[i].errorCode & 0xf0) >> 4;
        
        if (audioErrorCode >= (uint8_t)source->vtrErrorLevel ||
            videoErrorCode >= (uint8_t)source->vtrErrorLevel)
        {
            convertedPosition = msc_convert_position(rootSource, source->vtrErrors[i].position, &source->mediaSource);
            mc_mark_position(mediaControl, convertedPosition, VTR_ERROR_MARK_TYPE, 0);
            numMarked++;
        }
    }

    ml_log_info("Marked %ld of %ld VTR errors at level >= %d\n", numMarked, source->numVTRErrors, source->vtrErrorLevel);
}


static int create_output_streams(InputTrackData* trackData, int numOutputStreams, const StreamInfo* commonStreamInfo, int* nextOutputStreamId)
{
    int j;

    CALLOC_ORET(trackData->streamData, OutputStreamData, numOutputStreams);
    trackData->numOutputStreams = numOutputStreams;

    for (j = 0; j < trackData->numOutputStreams; j++)
    {
        trackData->streamData[j].streamId = *nextOutputStreamId + j;

        CHK_ORET(duplicate_stream_info(commonStreamInfo, &trackData->streamData[j].streamInfo));
    }

    *nextOutputStreamId += trackData->numOutputStreams;
    return 1;
}

static int get_output_stream(MXFFileSource* source, int streamId, InputTrackData** inputTrack, OutputStreamData** outputStream)
{
    int i;
    int j;

    for (i = 0; i < source->numInputTracks; i++)
    {
        for (j = 0; j < source->trackData[i].numOutputStreams; j++)
        {
            if (source->trackData[i].streamData[j].streamId == streamId)
            {
                *inputTrack = &source->trackData[i];
                *outputStream = &source->trackData[i].streamData[j];
                return 1;
            }
        }
    }

    return 0;
}

static void disable_source(MXFFileSource* source)
{
    int i;
    int j;

    for (i = 0; i < source->numInputTracks; i++)
    {
        for (j = 0; j < source->trackData[i].numOutputStreams; j++)
        {
            source->trackData[i].streamData[j].isDisabled = 1;
        }
    }
}


static int map_accept_frame(MXFReaderListener* mxfListener, int trackIndex)
{
    MXFFileSource* source = mxfListener->data->mxfSource;
    int i;
    int result;
    OutputStreamData* outputStream;

    if (trackIndex >= (source->numInputTracks - source->numTimecodeTracks))
    {
        return 0;
    }

    result = 0;
    for (i = 0; i < source->trackData[trackIndex].numOutputStreams; i++)
    {
        outputStream = &source->trackData[trackIndex].streamData[i];

        if (!outputStream->isDisabled)
        {
            if (sdl_accept_frame(mxfListener->data->streamListener, outputStream->streamId,
                &mxfListener->data->frameInfo))
            {
                outputStream->frameAccepted = 1;
                result = 1;
            }
        }
    }

    return result;
}

static int map_allocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer, uint32_t bufferSize)
{
    MXFFileSource* source = mxfListener->data->mxfSource;
    int i;
    int result;
    OutputStreamData* outputStream;
    uint32_t outputBufferSize;

    if (trackIndex >= (source->numInputTracks - source->numTimecodeTracks))
    {
        return 0;
    }

    if (source->trackData[trackIndex].numOutputStreams > 1)
    {
        CHK_ORET(source->trackData[trackIndex].streamData[0].streamInfo.type == SOUND_STREAM_TYPE);

        /* multi-channel audio will divide up the data equally */
        outputBufferSize = bufferSize / source->trackData[trackIndex].numOutputStreams;

        /* allocate the input buffer */
        if (source->trackData[trackIndex].audioBufferSize <= outputBufferSize)
        {
            SAFE_FREE(&source->trackData[trackIndex].audioBuffer);
            source->trackData[trackIndex].audioBufferSize = 0;

            MALLOC_ORET(source->trackData[trackIndex].audioBuffer, uint8_t, bufferSize);
            source->trackData[trackIndex].audioBufferSize = bufferSize;
        }
    }
    else
    {
        outputBufferSize = bufferSize;
    }

    result = 0;
    for (i = 0; i < source->trackData[trackIndex].numOutputStreams; i++)
    {
        outputStream = &source->trackData[trackIndex].streamData[i];

        if (!outputStream->isDisabled && outputStream->frameAccepted)
        {
            if (sdl_allocate_buffer(mxfListener->data->streamListener, outputStream->streamId, &outputStream->buffer, outputBufferSize))
            {
                outputStream->frameAllocated = 1;
                outputStream->bufferSize = outputBufferSize;
                result = 1;
            }
        }
    }

    if (result)
    {
        if (source->trackData[trackIndex].numOutputStreams > 1)
        {
            *buffer = source->trackData[trackIndex].audioBuffer;
        }
        else
        {
            *buffer = source->trackData[trackIndex].streamData[0].buffer;
        }
    }

    return result;
}

static void map_deallocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer)
{
    MXFFileSource* source = mxfListener->data->mxfSource;
    int i;
    OutputStreamData* outputStream;

    if (trackIndex >= (source->numInputTracks - source->numTimecodeTracks))
    {
        return;
    }

    for (i = 0; i < source->trackData[trackIndex].numOutputStreams; i++)
    {
        outputStream = &source->trackData[trackIndex].streamData[i];

        if (!outputStream->isDisabled && outputStream->frameAllocated)
        {
            sdl_deallocate_buffer(mxfListener->data->streamListener, outputStream->streamId, &outputStream->buffer);
        }

        outputStream->frameAllocated = 0;
        outputStream->buffer = NULL;
        outputStream->bufferSize = 0;
    }

    /* the audio buffer is not deallocated */
}

static int map_receive_frame(MXFReaderListener* mxfListener, int trackIndex, uint8_t* buffer, uint32_t bufferSize)
{
    MXFFileSource* source = mxfListener->data->mxfSource;
    int i;
    int result;
    OutputStreamData* outputStream;
    uint32_t outputBufferSize;

    if (trackIndex >= (source->numInputTracks - source->numTimecodeTracks))
    {
        return 0;
    }

    result = 0;
    for (i = 0; i < source->trackData[trackIndex].numOutputStreams; i++)
    {
        outputStream = &source->trackData[trackIndex].streamData[i];

        if (!outputStream->isDisabled && outputStream->frameAllocated)
        {
            if (source->trackData[trackIndex].numOutputStreams > 1)
            {
                /* multi-channel audio will divide up the data equally */
                outputBufferSize = bufferSize / source->trackData[trackIndex].numOutputStreams;

                CHK_ORET(outputStream->streamInfo.type == SOUND_STREAM_TYPE);

                CHK_ORET(deinterleave_audio(buffer, bufferSize, source->trackData[trackIndex].numOutputStreams,
                    outputStream->streamInfo.bitsPerSample, i, outputStream->buffer, outputBufferSize));
            }
            else
            {
                outputBufferSize = bufferSize;
            }

            if (sdl_receive_frame(mxfListener->data->streamListener, outputStream->streamId, outputStream->buffer, outputBufferSize))
            {
                result = 1;
            }
        }

        outputStream->frameAccepted = 0;
    }

    return result;
}


static int send_timecode(MediaSourceListener* listener, OutputStreamData* outputStream, const FrameInfo* frameInfo, MXFTimecode* mxfTimecode)
{
    Timecode* timecodeEvent;
    unsigned char* timecodeBuffer;

    if (sdl_accept_frame(listener, outputStream->streamId, frameInfo))
    {
        CHK_ORET(sdl_allocate_buffer(listener, outputStream->streamId, &timecodeBuffer, sizeof(Timecode)));

        timecodeEvent = (Timecode*)timecodeBuffer;
        timecodeEvent->isDropFrame = 0;
        timecodeEvent->hour = mxfTimecode->hour;
        timecodeEvent->min = mxfTimecode->min;
        timecodeEvent->sec = mxfTimecode->sec;
        timecodeEvent->frame = mxfTimecode->frame;

        CHK_ORET(sdl_receive_frame(listener, outputStream->streamId, timecodeBuffer, sizeof(Timecode)));
    }

    return 1;
}

static int mxfs_get_num_streams(void* data)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int numOutputStreams = 0;
    int i;

    for (i = 0; i < source->numInputTracks; i++)
    {
        numOutputStreams += source->trackData[i].numOutputStreams;
    }

    return numOutputStreams;
}

static int mxfs_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    MXFFileSource* source = (MXFFileSource*)data;
    InputTrackData* inputTrack = NULL;
    OutputStreamData* outputStream = NULL;

    if (!get_output_stream(source, streamIndex, &inputTrack, &outputStream))
    {
        return 0;
    }

    *streamInfo = &outputStream->streamInfo;
    return 1;
}

static void mxfs_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    MXFFileSource* source = (MXFFileSource*)data;
    mxfRational clipFrameRate;
    OutputStreamData* outputStream;
    int i;
    int j;

    get_frame_rate(source->mxfReader, &clipFrameRate);

    if (frameRate->num == clipFrameRate.numerator &&
        frameRate->den == clipFrameRate.denominator)
    {
        return;
    }

    if (clip_has_video(source->mxfReader))
    {
        disable_source(source);
    }
    else
    {
        convert_frame_rate_back(frameRate, &clipFrameRate);
        if (!set_frame_rate(source->mxfReader, &clipFrameRate))
        {
            disable_source(source);
        }
        else
        {
            for (i = 0; i < source->numInputTracks; i++)
            {
                for (j = 0; j < source->trackData[i].numOutputStreams; j++)
                {
                    outputStream = &source->trackData[i].streamData[j];

                    outputStream->streamInfo.frameRate = *frameRate;
                    if (!add_timecode_source_info(&outputStream->streamInfo, SRC_INFO_FILE_DURATION,
                        get_duration(source->mxfReader), get_rounded_frame_rate(frameRate)))
                    {
                        ml_log_error("Failed to add SRC_INFO_FILE_DURATION to mxf stream\n");
                    }
                }
            }
        }
    }
}

static int mxfs_disable_stream(void* data, int streamIndex)
{
    MXFFileSource* source = (MXFFileSource*)data;
    InputTrackData* inputTrack = NULL;
    OutputStreamData* outputStream = NULL;

    if (!get_output_stream(source, streamIndex, &inputTrack, &outputStream))
    {
        return 0;
    }

    outputStream->isDisabled = 1;
    return 1;
}

static void mxfs_disable_audio(void* data)
{
    MXFFileSource* source = (MXFFileSource*)data;
    OutputStreamData* outputStream;
    int i;
    int j;

    for (i = 0; i < source->numInputTracks; i++)
    {
        for (j = 0; j < source->trackData[i].numOutputStreams; j++)
        {
            outputStream = &source->trackData[i].streamData[j];

            if (outputStream->streamInfo.type == SOUND_STREAM_TYPE)
            {
                outputStream->isDisabled = 1;
            }
        }
    }
}

static int mxfs_stream_is_disabled(void* data, int streamIndex)
{
    MXFFileSource* source = (MXFFileSource*)data;
    InputTrackData* inputTrack = NULL;
    OutputStreamData* outputStream = NULL;

    if (!get_output_stream(source, streamIndex, &inputTrack, &outputStream))
    {
        return 0;
    }

    return outputStream->isDisabled;
}

static int mxfs_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int result;
    MXFTimecode mxfTimecode;
    int trackIndex;
    int i;
    int numSourceTimecodes;
    int mxfTimecodeType;
    int count;
    OutputStreamData* outputStream;

    if (source->eof)
    {
        return -1;
    }

    source->mxfListener.data = &source->mxfListenerData;
    source->mxfListener.data->streamListener = listener;
    source->mxfListener.data->mxfSource = source;
    source->mxfListener.data->frameInfo = *frameInfo;

    /* audio and video */
    if ((result = read_next_frame(source->mxfReader, &source->mxfListener)) != 1)
    {
        if (result == -1) /* EOF */
        {
            source->eof = 1;
        }
        return -1;
    }

    /* timecodes */
    if (source->numTimecodeTracks > 0)
    {
        trackIndex = source->numInputTracks - source->numTimecodeTracks;

        /* playout timecode */
        outputStream = &source->trackData[trackIndex].streamData[0];
        if (!outputStream->isDisabled)
        {
            if (get_playout_timecode(source->mxfReader, &mxfTimecode))
            {
                send_timecode(listener, outputStream, frameInfo, &mxfTimecode);
            }
        }
        trackIndex++;

        /* source timecodes */
        numSourceTimecodes = get_num_source_timecodes(source->mxfReader);
        for (i = 0; i < numSourceTimecodes; i++)
        {
            outputStream = &source->trackData[trackIndex].streamData[0];
            if (!outputStream->isDisabled)
            {
                if (get_source_timecode(source->mxfReader, i, &mxfTimecode, &mxfTimecodeType, &count) == 1)
                {
                    send_timecode(listener, outputStream, frameInfo, &mxfTimecode);
                }
            }

            trackIndex++;
        }
    }


    return 0;
}

static int mxfs_is_seekable(void* data)
{
    MXFFileSource* source = (MXFFileSource*)data;

    return mxfr_is_seekable(source->mxfReader);
}

static int mxfs_seek(void* data, int64_t position)
{
    MXFFileSource* source = (MXFFileSource*)data;

    if (!position_at_frame(source->mxfReader, position))
    {
        return -1;
    }

    source->eof = 0;
    return 0;
}

/* TODO: VITC and LTC only works for archive MXF */
static int mxfs_seek_timecode(void* data, const Timecode* timecode, TimecodeType type, TimecodeSubType subType)
{
    MXFFileSource* source = (MXFFileSource*)data;
    MXFTimecode mxfTimecode;

    mxfTimecode.isDropFrame = timecode->isDropFrame;
    mxfTimecode.hour = timecode->hour;
    mxfTimecode.min = timecode->min;
    mxfTimecode.sec = timecode->sec;
    mxfTimecode.frame = timecode->frame;

    if (type == CONTROL_TIMECODE_TYPE)
    {
        return position_at_playout_timecode(source->mxfReader, &mxfTimecode) ? 0 : -1;
    }
    else if (type == SOURCE_TIMECODE_TYPE && subType == VITC_SOURCE_TIMECODE_SUBTYPE)
    {
        return position_at_source_timecode(source->mxfReader, &mxfTimecode, SYSTEM_ITEM_TC_ARRAY_TIMECODE, 0) ? 0 : -1;
    }
    else if (type == SOURCE_TIMECODE_TYPE && subType == LTC_SOURCE_TIMECODE_SUBTYPE)
    {
        return position_at_source_timecode(source->mxfReader, &mxfTimecode, SYSTEM_ITEM_TC_ARRAY_TIMECODE, 1) ? 0 : -1;
    }
    return -1;
}

static int mxfs_get_length(void* data, int64_t* length)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int64_t duration;

    if ((duration = get_duration(source->mxfReader)) >= 0)
    {
        *length = duration;
        return 1;
    }
    return 0;
}

static int mxfs_get_position(void* data, int64_t* position)
{
    MXFFileSource* source = (MXFFileSource*)data;

    *position = get_frame_number(source->mxfReader) + 1;
    return 1;
}

static int mxfs_get_available_length(void* data, int64_t* length)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int64_t lastPosition;

    lastPosition = get_last_written_frame_number(source->mxfReader);
    if (lastPosition < 0)
    {
        return 0;
    }

    *length = lastPosition + 1;
    return 1;
}

static int mxfs_eof(void* data)
{
    MXFFileSource* source = (MXFFileSource*)data;

    if (source->eof)
    {
        return 1;
    }

    return get_duration(source->mxfReader) == get_frame_number(source->mxfReader) + 1;
}

static int mxfs_is_complete(void* data)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int64_t availableLength;
    int64_t length;

    /* only archive MXF files with option to mark PSE failures, VTR errors or digibeta dropouts require post complete step */
    if (!source->isArchiveMXF ||
        (!source->markPSEFailures && !source->markVTRErrors && !source->markDigiBetaDropouts))
    {
        return 1;
    }

    /* wait until all essence data has been written to disk */
    if (mxfs_get_available_length(data, &availableLength) &&
        mxfs_get_length(data, &length) &&
        availableLength >= length)
    {
        return 1;
    }

    return 0;
}

static int mxfs_post_complete(void* data, MediaSource* rootSource, MediaControl* mediaControl)
{
    MXFFileSource* source = (MXFFileSource*)data;
    MXFHeaderMetadata* headerMetadata = NULL;
    struct timeval now;
    long timeDiff;
    int result;
    int freeHeaderMetadata = 0;
    int64_t convertedPosition;

    /* only archive MXF files with option to mark PSE failures, VTR errors or digibeta dropouts require post complete step */
    if (source->donePostComplete ||
        !source->isArchiveMXF ||
        (!source->markPSEFailures && !source->markVTRErrors && !source->markDigiBetaDropouts))
    {
        source->donePostComplete = 1;
        return 1;
    }


    if (have_footer_metadata(source->mxfReader))
    {
        /* the archive MXF file is complete and the header metadata in the footer was already read by the MXF reader */
        headerMetadata = get_header_metadata(source->mxfReader);
        source->donePostComplete = 1;
    }
    else
    {
        /* try reading the header metadata from the footer partition every 2 seconds */

        gettimeofday(&now, NULL);
        if (source->postCompleteTryCount != 0)
        {
            timeDiff = (now.tv_sec - source->lastPostCompleteTry.tv_sec) * 1000000 +
                now.tv_usec - source->lastPostCompleteTry.tv_usec;
        }

        if (source->postCompleteTryCount == 0 || timeDiff < 0)
        {
            /* first time we do nothing */
            source->lastPostCompleteTry = now;
            timeDiff = 0;
            source->postCompleteTryCount++;
        }
        else if (timeDiff > 2000000) /* 2 second */
        {
            result = archive_mxf_read_footer_metadata(source->filename, source->dataModel, &headerMetadata);
            if (result == 1)
            {
                /* have header metadata */
                freeHeaderMetadata = 1;
                source->donePostComplete = 1;
            }
            else if (result == 2)
            {
                /* no header metadata is present */
                ml_log_warn("Failed to read footer header metadata\n");
                source->donePostComplete = 1;
            }
            else /* result == 0 */
            {
                if (source->postCompleteTryCount >= 15)
                {
                    ml_log_warn("Failed to read footer header metadata within 30 seconds of last frame being available\n");
                    source->donePostComplete = 1;
                }
            }
            source->lastPostCompleteTry = now;
            source->postCompleteTryCount++;
        }
    }

    if (headerMetadata != NULL)
    {
        if (source->markPSEFailures)
        {
            PSEFailure* failures = NULL;
            long numFailures = 0;
            long i;

            if (archive_mxf_get_pse_failures(headerMetadata, &failures, &numFailures))
            {
                ml_log_info("Marking %ld PSE failures\n", numFailures);
                for (i = 0; i < numFailures; i++)
                {
                    convertedPosition = msc_convert_position(rootSource, failures[i].position, &source->mediaSource);
                    mc_mark_position(mediaControl, convertedPosition, PSE_FAILURE_MARK_TYPE, 0);
                }

                SAFE_FREE(&failures);
            }
        }

        if (source->markVTRErrors)
        {
            if (archive_mxf_get_vtr_errors(headerMetadata, &source->vtrErrors, &source->numVTRErrors))
            {
                mark_vtr_errors(source, rootSource, mediaControl);
            }
        }

        if (source->markDigiBetaDropouts)
        {
            DigiBetaDropout* digiBetaDropouts = NULL;
            long numDigiBetaDropouts = 0;
            long i;

            if (archive_mxf_get_digibeta_dropouts(headerMetadata, &digiBetaDropouts, &numDigiBetaDropouts))
            {
                ml_log_info("Marking %ld digibeta dropouts\n", numDigiBetaDropouts);
                for (i = 0; i < numDigiBetaDropouts; i++)
                {
                    convertedPosition = msc_convert_position(rootSource, digiBetaDropouts[i].position, &source->mediaSource);
                    mc_mark_position(mediaControl, convertedPosition, DIGIBETA_DROPOUT_MARK_TYPE, 0);
                }

                SAFE_FREE(&digiBetaDropouts);
            }
        }
    }

    if (freeHeaderMetadata)
    {
        mxf_free_header_metadata(&headerMetadata);
    }


    return source->donePostComplete;
}

static void mxfs_set_source_name(void* data, const char* name)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int i;
    int j;

    for (i = 0; i < source->numInputTracks; i++)
    {
        for (j = 0; j < source->trackData[i].numOutputStreams; j++)
        {
            add_known_source_info(&source->trackData[i].streamData[j].streamInfo, SRC_INFO_NAME, name);
        }
    }
}

static void mxfs_set_clip_id(void* data, const char* id)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int i;
    int j;

    for (i = 0; i < source->numInputTracks; i++)
    {
        for (j = 0; j < source->trackData[i].numOutputStreams; j++)
        {
            set_stream_clip_id(&source->trackData[i].streamData[j].streamInfo, id);
        }
    }
}

static void mxfs_close(void* data)
{
    MXFFileSource* source = (MXFFileSource*)data;
    int i;
    int j;

    if (data == NULL)
    {
        return;
    }

    for (i = 0; i < source->numInputTracks; i++)
    {
        for (j = 0; j < source->trackData[i].numOutputStreams; j++)
        {
            clear_stream_info(&source->trackData[i].streamData[j].streamInfo);
        }

        SAFE_FREE(&source->trackData[i].audioBuffer);
        source->trackData[i].audioBufferSize = 0;

        SAFE_FREE(&source->trackData[i].streamData);
        source->trackData[i].numOutputStreams = 0;
    }
    SAFE_FREE(&source->trackData);
    source->numInputTracks = 0;

    if (source->mxfReader != NULL)
    {
        close_mxf_reader(&source->mxfReader);
    }

    mxf_free_data_model(&source->dataModel);
    SAFE_FREE(&source->filename);
    
    SAFE_FREE(&source->vtrErrors);
    source->numVTRErrors = 0;

    SAFE_FREE(&source);
}


void mxfs_set_vtr_error_level(void* data, VTRErrorLevel level)
{
    MXFFileSource* source = (MXFFileSource*)data;

    source->vtrErrorLevel = level;
}

void mxfs_mark_vtr_errors(void* data, MediaSource* rootSource, MediaControl* mediaControl)
{
    MXFFileSource* source = (MXFFileSource*)data;

    if (!source->markVTRErrors || !source->isArchiveMXF)
    {
        return;
    }
    
    if (source->numVTRErrors > 0)
    {
        mark_vtr_errors(source, rootSource, mediaControl);
    }
}


int mxfs_open(const char* filename, int forceD3MXF, int markPSEFailures, int markVTRErrors, int markDigiBetaDropouts,
    MXFFileSource** source)
{
    MXFFileSource* newSource = NULL;
    MXFFile* mxfFile = NULL;
    MXFPageFile* mxfPageFile = NULL;
    int i;
    int j;
    ArchiveMXFInfo archiveMXFInfo;
    int haveArchiveInfo = 0;
    char stringBuf[128];
    int numSourceTimecodes;
    int timecodeType;
    int haveArchiveVITC = 0;
    int haveArchiveLTC = 0;
    int64_t duration = -1;
    char progNo[16];
    char clipUMIDStr[65] = {0};
    int nextOutputStreamId;
    OutputStreamData* outputStream;
    StreamInfo commonStreamInfo;
    int numTracks;
    int isPAL;
    mxfRational mxfFrameRate;

    CHK_ORET(initialise_stream_info(&commonStreamInfo));

    assert(MAX_SOURCE_INFO_VALUE_LEN <= 128);
    assert(sizeof(uint8_t) == sizeof(unsigned char));
    assert(sizeof(uint32_t) <= sizeof(unsigned int));


    /* connect the MXF logging */
    mxf_log = mxf_log_connect;


    if (strstr(filename, "%d") != NULL)
    {
        if (!mxf_page_file_open_read(filename, &mxfPageFile))
        {
            ml_log_error("Failed to open MXF page file '%s'\n", filename);
            return 0;
        }
        mxfFile = mxf_page_file_get_file(mxfPageFile);
    }
    else
    {
        if (!mxf_disk_file_open_read(filename, &mxfFile))
        {
            ml_log_error("Failed to open MXF disk file '%s'\n", filename);
            return 0;
        }
    }

    /* TODO: test non-seekable files */
    if (!mxf_file_is_seekable(mxfFile))
    {
        CHK_OFAIL(format_is_supported(mxfFile));
        mxf_file_seek(mxfFile, 0, SEEK_SET);
    }

    CALLOC_ORET(newSource, MXFFileSource, 1);

    newSource->markPSEFailures = markPSEFailures;
    newSource->markVTRErrors = markVTRErrors;
    newSource->markDigiBetaDropouts = markDigiBetaDropouts;
    CALLOC_OFAIL(newSource->filename, char, strlen(filename) + 1);
    strcpy(newSource->filename, filename);

    CHK_OFAIL(mxf_load_data_model(&newSource->dataModel));
    CHK_OFAIL(archive_mxf_load_extensions(newSource->dataModel));
    CHK_OFAIL(mxf_finalise_data_model(newSource->dataModel));

    CHK_OFAIL(init_mxf_reader_2(&mxfFile, newSource->dataModel, &newSource->mxfReader));
    if (forceD3MXF || is_archive_mxf(get_header_metadata(newSource->mxfReader)))
    {
        if (forceD3MXF)
        {
            ml_log_info("MXF file is forced to be treated as a BBC D3 MXF file\n");
        }
        else
        {
            ml_log_info("MXF file is a BBC Archive MXF file\n");
        }
        newSource->isArchiveMXF = 1;
        memset(&archiveMXFInfo, 0, sizeof(ArchiveMXFInfo));
        if (archive_mxf_get_info(get_header_metadata(newSource->mxfReader), &archiveMXFInfo))
        {
            haveArchiveInfo = 1;
        }
        else
        {
            haveArchiveInfo = 0;
            ml_log_warn("Failed to read archive MXF information\n");
        }
    }

    duration = get_duration(newSource->mxfReader);

    read_clip_umid_string(newSource->mxfReader, clipUMIDStr);

    newSource->mediaSource.is_complete = mxfs_is_complete;
    newSource->mediaSource.post_complete = mxfs_post_complete;
    newSource->mediaSource.get_num_streams = mxfs_get_num_streams;
    newSource->mediaSource.get_stream_info = mxfs_get_stream_info;
    newSource->mediaSource.set_frame_rate_or_disable = mxfs_set_frame_rate_or_disable;
    newSource->mediaSource.disable_stream = mxfs_disable_stream;
    newSource->mediaSource.disable_audio = mxfs_disable_audio;
    newSource->mediaSource.stream_is_disabled = mxfs_stream_is_disabled;
    newSource->mediaSource.read_frame = mxfs_read_frame;
    newSource->mediaSource.is_seekable = mxfs_is_seekable;
    newSource->mediaSource.seek = mxfs_seek;
    newSource->mediaSource.seek_timecode = mxfs_seek_timecode;
    newSource->mediaSource.get_length = mxfs_get_length;
    newSource->mediaSource.get_position = mxfs_get_position;
    newSource->mediaSource.get_available_length = mxfs_get_available_length;
    newSource->mediaSource.eof = mxfs_eof;
    newSource->mediaSource.set_source_name = mxfs_set_source_name;
    newSource->mediaSource.set_clip_id = mxfs_set_clip_id;
    newSource->mediaSource.close = mxfs_close;
    newSource->mediaSource.data = newSource;
    
    newSource->vtrErrorSource.set_vtr_error_level = mxfs_set_vtr_error_level;
    newSource->vtrErrorSource.mark_vtr_errors = mxfs_mark_vtr_errors;
    newSource->vtrErrorSource.data = newSource;

    newSource->mxfListener.accept_frame = map_accept_frame;
    newSource->mxfListener.allocate_buffer = map_allocate_buffer;
    newSource->mxfListener.deallocate_buffer = map_deallocate_buffer;
    newSource->mxfListener.receive_frame = map_receive_frame;


    numTracks = get_num_tracks(newSource->mxfReader);
    numSourceTimecodes = get_num_source_timecodes(newSource->mxfReader);
    newSource->numTimecodeTracks = 1 + numSourceTimecodes;
    newSource->numInputTracks = numTracks + newSource->numTimecodeTracks;

    CALLOC_OFAIL(newSource->trackData, InputTrackData, newSource->numInputTracks);


    /* create the common source information */

    commonStreamInfo.sourceId = msc_create_id();

    get_frame_rate(newSource->mxfReader, &mxfFrameRate);
    convert_frame_rate(&mxfFrameRate, &commonStreamInfo.frameRate);
    commonStreamInfo.isHardFrameRate = clip_has_video(newSource->mxfReader);
    isPAL = is_pal_frame_rate(&commonStreamInfo.frameRate);
    set_stream_clip_id(&commonStreamInfo, clipUMIDStr);

    CHK_OFAIL(add_filename_source_info(&commonStreamInfo, SRC_INFO_FILE_NAME, filename));
    if (newSource->isArchiveMXF)
    {
        CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_FILE_TYPE, "Archive-MXF"));
    }
    else
    {
        CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_FILE_TYPE, "MXF"));
    }
    CHK_OFAIL(add_timecode_source_info(&commonStreamInfo, SRC_INFO_FILE_DURATION, duration, get_rounded_frame_rate(&commonStreamInfo.frameRate)));

    if (newSource->isArchiveMXF)
    {
        if (haveArchiveInfo)
        {
            CHK_OFAIL(add_filename_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_ORIG_FILENAME,
                archiveMXFInfo.filename));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_CREATION_DATE,
                convert_date(&archiveMXFInfo.creationDate, stringBuf)));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_LTO_SPOOL_NO,
                archiveMXFInfo.ltoInfaxData.spoolNo));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_SOURCE_SPOOL_NO,
                archiveMXFInfo.sourceInfaxData.spoolNo));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_SOURCE_ITEM_NO,
                convert_uint32(archiveMXFInfo.sourceInfaxData.itemNo, stringBuf)));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_PROGRAMME_TITLE,
                archiveMXFInfo.sourceInfaxData.progTitle));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_EPISODE_TITLE,
                archiveMXFInfo.sourceInfaxData.epTitle));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_TX_DATE,
                convert_date(&archiveMXFInfo.sourceInfaxData.txDate, stringBuf)));
            progNo[0] = '\0';
            if (strlen(archiveMXFInfo.sourceInfaxData.magPrefix) > 0)
            {
                strcat(progNo, archiveMXFInfo.sourceInfaxData.magPrefix);
                strcat(progNo, ":");
            }
            strcat(progNo, archiveMXFInfo.sourceInfaxData.progNo);
            if (strlen(archiveMXFInfo.sourceInfaxData.prodCode) > 0)
            {
                strcat(progNo, "/");
                if (strlen(archiveMXFInfo.sourceInfaxData.prodCode) == 1 &&
                    archiveMXFInfo.sourceInfaxData.prodCode[0] >= '0' && archiveMXFInfo.sourceInfaxData.prodCode[0] <= '9')
                {
                    strcat(progNo, "0");
                }
                strcat(progNo, archiveMXFInfo.sourceInfaxData.prodCode);
            }
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_PROGRAMME_NUMBER,
                progNo));
            CHK_OFAIL(add_known_source_info(&commonStreamInfo, SRC_INFO_ARCHIVEMXF_PROGRAMME_DURATION,
                convert_prog_duration(archiveMXFInfo.sourceInfaxData.duration, stringBuf)));
        }
        else
        {
            CHK_OFAIL(add_source_info(&commonStreamInfo, "Archive info", "not available"));
        }
    }


    /* complete the input track data and output stream info */

    nextOutputStreamId = 0;
    for (i = 0; i < numTracks; i++)
    {
        MXFTrack* track = get_mxf_track(newSource->mxfReader, i);

        if (track->isVideo)
        {
            CHK_OFAIL(create_output_streams(&newSource->trackData[i], 1, &commonStreamInfo, &nextOutputStreamId));
            outputStream = &newSource->trackData[i].streamData[0];

            outputStream->streamInfo.type = PICTURE_STREAM_TYPE;
            outputStream->streamInfo.frameRate.num = track->video.frameRate.numerator;
            outputStream->streamInfo.frameRate.den = track->video.frameRate.denominator;
            outputStream->streamInfo.width = track->video.frameWidth;
            outputStream->streamInfo.height = track->video.frameHeight;
            outputStream->streamInfo.aspectRatio.num = track->video.aspectRatio.numerator;
            outputStream->streamInfo.aspectRatio.den = track->video.aspectRatio.denominator;
            if (outputStream->streamInfo.aspectRatio.num < 1 || outputStream->streamInfo.aspectRatio.den < 1)
            {
                ml_log_warn("MXF file has unknown aspect ratio. Defaulting to 4:3\n");
                outputStream->streamInfo.aspectRatio.num = 4;
                outputStream->streamInfo.aspectRatio.den = 3;
            }
            if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(SD_Unc_625_50i_422_135_FrameWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(SD_Unc_625_50i_422_135_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(HD_Unc_1080_50i_422_ClipWrapped)))
            {
                CHK_OFAIL(track->video.componentDepth == 8 || track->video.componentDepth == 10);
                if (track->video.componentDepth == 8)
                {
                    outputStream->streamInfo.format = UYVY_FORMAT;
                }
                else
                {
                    CHK_OFAIL(mxf_equals_ul(&track->codecLabel, &MXF_CMDEF_L(UNC_10B_422_INTERLEAVED)));
                    outputStream->streamInfo.format = UYVY_10BIT_FORMAT;
                }
            }
            else if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(IECDV_25_625_50_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(IECDV_25_625_50_FrameWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(IECDV_25_525_60_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(IECDV_25_525_60_FrameWrapped)))
            {
                outputStream->streamInfo.format = DV25_YUV420_FORMAT;
            }
            else if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_25_625_50_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_25_625_50_FrameWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_25_525_60_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_25_525_60_FrameWrapped)))
            {
                outputStream->streamInfo.format = DV25_YUV411_FORMAT;
            }
            else if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_50_625_50_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_50_625_50_FrameWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_50_525_60_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_50_525_60_FrameWrapped)))
            {
                outputStream->streamInfo.format = DV50_FORMAT;
            }
            else if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_50_625_50_defined_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_50_625_50_extended_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_50_625_50_picture_only)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_40_625_50_defined_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_40_625_50_extended_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_40_625_50_picture_only)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_30_625_50_defined_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_30_625_50_extended_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_30_625_50_picture_only)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_50_525_60_defined_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_50_525_60_extended_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_50_525_60_picture_only)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_40_525_60_defined_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_40_525_60_extended_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_40_525_60_picture_only)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_30_525_60_defined_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_30_525_60_extended_template)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(D10_30_525_60_picture_only)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(AvidIMX50_625_50)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(AvidIMX40_625_50)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(AvidIMX30_625_50)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(AvidIMX50_525_60)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(AvidIMX40_525_60)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(AvidIMX30_525_60)))
            {
                outputStream->streamInfo.format = D10_PICTURE_FORMAT;
            }
            else if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(AvidMJPEGClipWrapped)))
            {
                outputStream->streamInfo.format = AVID_MJPEG_FORMAT;
                outputStream->streamInfo.singleField = 1;
            }
            else if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_100_1080_50_I_ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DVBased_100_720_50_P_ClipWrapped)))
            {
                outputStream->streamInfo.format = DV100_FORMAT;
            }
            else if (mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DNxHD1080i120ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DNxHD1080i185ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DNxHD1080p36ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DNxHD720p120ClipWrapped)) ||
                mxf_equals_ul(&track->essenceContainerLabel, &MXF_EC_L(DNxHD720p185ClipWrapped)))
            {
                outputStream->streamInfo.format = AVID_DNxHD_FORMAT;
            }
            else
            {
                outputStream->streamInfo.format = UNKNOWN_FORMAT;
            }
        }
        else /* audio */
        {
            CHK_OFAIL(create_output_streams(&newSource->trackData[i], track->audio.channelCount, &commonStreamInfo, &nextOutputStreamId));

            /* TODO: don't assume PCM */
            for (j = 0; j < (int)track->audio.channelCount; j++)
            {
                outputStream = &newSource->trackData[i].streamData[j];

                outputStream->streamInfo.type = SOUND_STREAM_TYPE;
                outputStream->streamInfo.format = PCM_FORMAT;
                outputStream->streamInfo.samplingRate.num = track->audio.samplingRate.numerator;
                outputStream->streamInfo.samplingRate.den = track->audio.samplingRate.denominator;
                outputStream->streamInfo.numChannels = 1;
                outputStream->streamInfo.bitsPerSample = track->audio.bitsPerSample;
            }
        }
    }

    /* control/playout timecode streams */
    CHK_OFAIL(create_output_streams(&newSource->trackData[numTracks], 1, &commonStreamInfo, &nextOutputStreamId));
    outputStream = &newSource->trackData[numTracks].streamData[0];

    outputStream->streamInfo.type = TIMECODE_STREAM_TYPE;
    outputStream->streamInfo.format = TIMECODE_FORMAT;
    outputStream->streamInfo.timecodeType = CONTROL_TIMECODE_TYPE;
    outputStream->streamInfo.timecodeSubType = NO_TIMECODE_SUBTYPE;


    /* source timecodes */
    for (i = 0; i < numSourceTimecodes; i++)
    {
        CHK_OFAIL(create_output_streams(&newSource->trackData[numTracks + 1 + i], 1, &commonStreamInfo, &nextOutputStreamId));
        outputStream = &newSource->trackData[numTracks + 1 + i].streamData[0];

        timecodeType = get_source_timecode_type(newSource->mxfReader, i);

        outputStream->streamInfo.type = TIMECODE_STREAM_TYPE;
        outputStream->streamInfo.format = TIMECODE_FORMAT;
        outputStream->streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
        if (newSource->isArchiveMXF)
        {
            /* we are only interested in the VITC and LTC source timecodes */
            if (timecodeType == SYSTEM_ITEM_TC_ARRAY_TIMECODE)
            {
                if (!haveArchiveVITC)
                {
                    outputStream->streamInfo.timecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
                    haveArchiveVITC = 1;
                }
                else if (!haveArchiveLTC)
                {
                    outputStream->streamInfo.timecodeSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
                    haveArchiveLTC = 1;
                }
                else
                {
                    /* this is unexpected because a archive MXF file should only have a VITC and LTC timecode */
                    ml_log_warn("Found more than 2 system item timecodes for archive MXF file\n");

                    outputStream->streamInfo.timecodeSubType = NO_TIMECODE_SUBTYPE;

                    /* we disable it because it doesn't apply for archive MXF files */
                    outputStream->isDisabled = 1;
                }
            }
            else
            {
                outputStream->streamInfo.timecodeSubType = NO_TIMECODE_SUBTYPE;

                /* we disable it because it doesn't apply for archive MXF files */
                outputStream->isDisabled = 1;
            }
        }
        else
        {
            outputStream->streamInfo.timecodeSubType = NO_TIMECODE_SUBTYPE;
        }
    }


    clear_stream_info(&commonStreamInfo);

    *source = newSource;
    return 1;

fail:
    clear_stream_info(&commonStreamInfo);
    if (newSource == NULL || newSource->mxfReader == NULL)
    {
        mxf_file_close(&mxfFile);
    }
    mxfs_close(newSource);
    return 0;
}

MediaSource* mxfs_get_media_source(MXFFileSource* source)
{
    return &source->mediaSource;
}

VTRErrorSource* mxfs_get_vtr_error_source(MXFFileSource* source)
{
    return &source->vtrErrorSource;
}

