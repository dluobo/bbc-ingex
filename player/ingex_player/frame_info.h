/*
 * $Id: frame_info.h,v 1.12 2010/01/12 16:32:22 john_f Exp $
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

#ifndef __FRAME_INFO_H__
#define __FRAME_INFO_H__


#include <inttypes.h>

#include "types.h"


#ifdef __cplusplus
extern "C"
{
#endif


#define MAX_SOURCE_INFO_NAME_LEN        15
#define MAX_SOURCE_INFO_VALUE_LEN       30

#define ALL_MARK_TYPE               0xffffffff
#define M0_MARK_TYPE                0x00000001
#define M1_MARK_TYPE                0x00000002
#define M2_MARK_TYPE                0x00000004
#define M3_MARK_TYPE                0x00000008
#define M4_MARK_TYPE                0x00000010
#define M5_MARK_TYPE                0x00000020
#define M6_MARK_TYPE                0x00000040
#define M7_MARK_TYPE                0x00000080
#define M8_MARK_TYPE                0x00000100
#define VTR_ERROR_MARK_TYPE         0x00010000
#define PSE_FAILURE_MARK_TYPE       0x00020000
#define DIGIBETA_DROPOUT_MARK_TYPE  0x00040000

#define CLIP_ID_SIZE                128



typedef enum
{
    UNKNOWN_STREAM_TYPE = 0,
    PICTURE_STREAM_TYPE,
    SOUND_STREAM_TYPE,
    TIMECODE_STREAM_TYPE,
    EVENT_STREAM_TYPE
} StreamType;

typedef enum
{
    UNKNOWN_FORMAT = 0,

    /* Picture */
    UYVY_FORMAT,
    UYVY_10BIT_FORMAT,
    YUV420_FORMAT,
    YUV411_FORMAT,
    YUV422_FORMAT,
    YUV444_FORMAT,
    DV25_YUV420_FORMAT,
    DV25_YUV411_FORMAT,
    DV50_FORMAT,
    DV100_FORMAT,
    D10_PICTURE_FORMAT,
    AVID_MJPEG_FORMAT,
    AVID_DNxHD_FORMAT,

    /* Sound */
    PCM_FORMAT,

    /* Timecode */
    TIMECODE_FORMAT,

    /* Event */
    SOURCE_EVENT_FORMAT

} StreamFormat;

typedef enum
{
    UNKNOWN_TIMECODE_TYPE = 0,
    SYSTEM_TIMECODE_TYPE,
    CONTROL_TIMECODE_TYPE,
    SOURCE_TIMECODE_TYPE
} TimecodeType;

typedef enum
{
    NO_TIMECODE_SUBTYPE = 0,
    VITC_SOURCE_TIMECODE_SUBTYPE,
    LTC_SOURCE_TIMECODE_SUBTYPE,
    DVITC_SOURCE_TIMECODE_SUBTYPE,
    DLTC_SOURCE_TIMECODE_SUBTYPE
} TimecodeSubType;

typedef struct
{
    char* name;
    char* value;
} SourceInfoValue;

typedef enum
{
    SRC_INFO_FILE_NAME = 0,
    SRC_INFO_FILE_TYPE,
    SRC_INFO_FILE_DURATION,

    SRC_INFO_ARCHIVEMXF_ORIG_FILENAME,
    SRC_INFO_ARCHIVEMXF_CREATION_DATE,
    SRC_INFO_ARCHIVEMXF_LTO_SPOOL_NO,
    SRC_INFO_ARCHIVEMXF_SOURCE_SPOOL_NO,
    SRC_INFO_ARCHIVEMXF_SOURCE_ITEM_NO,
    SRC_INFO_ARCHIVEMXF_PROGRAMME_TITLE,
    SRC_INFO_ARCHIVEMXF_EPISODE_TITLE,
    SRC_INFO_ARCHIVEMXF_TX_DATE,
    SRC_INFO_ARCHIVEMXF_PROGRAMME_NUMBER,
    SRC_INFO_ARCHIVEMXF_PROGRAMME_DURATION,

    SRC_INFO_TITLE,

    SRC_INFO_NAME,

    SRC_INFO_UNKNOWN
} SourceInfoName;

typedef struct
{
    StreamType type;

    /* source */
    int sourceId;

    /* general identifier for format */
    StreamFormat format;

    /* video frame rate used for all stream types */
    Rational frameRate;
    int isHardFrameRate;

    /* detailed picture parameters */
    int width;
    int height;
    Rational aspectRatio;
    int singleField;

    /* detailed sound parameters */
    Rational samplingRate;
    int numChannels;
    int bitsPerSample;

    /* detailed timecode parameters */
    TimecodeType timecodeType;
    TimecodeSubType timecodeSubType;

    /* source info */
    SourceInfoValue* sourceInfoValues;
    int numSourceInfoValues;
    int numSourceInfoValuesAlloc;

    /* clip info */
    char clipId[CLIP_ID_SIZE];
} StreamInfo;

typedef struct
{
    int streamId;
    Timecode timecode;
    TimecodeType timecodeType;
    TimecodeSubType timecodeSubType;
} TimecodeInfo;

typedef struct
{
    Rational frameRate;

    int64_t position; /* position in the media source */
    int64_t sourceLength; /* length of media source - 0 <= position < sourceLength */
    int64_t availableSourceLength; /* this will progress from 0 to sourceLength for a file that is still being written to  */
    int64_t startOffset;

    int64_t frameCount; /* count of frames read and accepted */
    int rateControl; /* true if the frame is the next one and the playing speed is normal */

    int reversePlay; /* true if the player is playing in reverse */

    int isRepeat; /* true if frame is a repeat of the last frame */

    int muteAudio; /* true for the first frame when the player starts in paused mode */

    int locked; /* true if player controls are locked */

    int droppedFrame; /* true if a frame was dropped when the player controls are locked */

    /* user marks */
    VTRErrorLevel vtrErrorLevel;
    int isMarked;
    int markType;

    /* all timecodes are only present when all streams in the frame have been read */
    TimecodeInfo timecodes[64];
    int numTimecodes;
} FrameInfo;



const char* get_stream_type_string(StreamType type);
const char* get_stream_format_string(StreamFormat format);

int initialise_stream_info(StreamInfo* streamInfo);
int add_source_info(StreamInfo* streamInfo, const char* name, const char* value);
int add_known_source_info(StreamInfo* streamInfo, SourceInfoName name, const char* value);
int add_filename_source_info(StreamInfo* streamInfo, SourceInfoName name, const char* filename);
int add_timecode_source_info(StreamInfo* streamInfo, SourceInfoName name, int64_t timecode, int timecodeBase);
void clear_stream_info(StreamInfo* streamInfo);
const char* get_source_info_value(const StreamInfo* streamInfo, const char* name);
const char* get_known_source_info_value(const StreamInfo* streamInfo, SourceInfoName name);

int duplicate_stream_info(const StreamInfo* fromStreamInfo, StreamInfo* toStreamInfo);

void set_stream_clip_id(StreamInfo* streamInfo, const char* clipId);

int select_frame_timecode(const FrameInfo* frameInfo, int tcIndex, int tcType, int tcSubType, Timecode* timecode);

int is_pal_frame_rate(const Rational* frameRate);
int is_ntsc_frame_rate(const Rational* frameRate);
int stream_is_pal_frame_rate(const StreamInfo* streamInfo);
int stream_is_ntsc_frame_rate(const StreamInfo* streamInfo);
int frame_is_pal_frame_rate(const FrameInfo* frameInfo);
int frame_is_ntsc_frame_rate(const FrameInfo* frameInfo);

int get_rounded_frame_rate(const Rational* frameRate);


#ifdef __cplusplus
}
#endif


#endif


