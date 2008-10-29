/*
 * $Id: frame_info.c,v 1.5 2008/10/29 17:47:41 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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

#include "frame_info.h"
#include "logging.h"
#include "macros.h"


static const char* g_streamTypeStrings[] = 
{
    "unknown",
    "picture",
    "sound",
    "timecode",
    "event"
};

static const char* g_streamFormatStrings[] = 
{
    "unknown",
    "uyvy",
    "uyvy10bit",
    "yuv420",
    "yuv411",
    "yuv422",
    "yuv444",
    "dv25_420",
    "dv25_411",
    "dv50",
    "d10_picture",
    "avid_mjpeg",
    "avid_dnxhd",
    "pcm",
    "timecode",
    "source_event"
};

/* keep length <= MAX_SOURCE_INFO_NAME_LEN */
static const char* g_sourceInfoNames[] = 
{ 
    "Filename", 
    "File type",
    "File duration", 
    
    "Orig filename", 
    "Creation date", 
    "LTO spool no.", 
    "D3 spool no.", 
    "D3 item no.", 
    "Prog title", 
    "Episode title", 
    "Tx date", 
    "Prog number", 
    "Prog duration", 
    
    "Title",
    
    "Name",
    
    "Unknown"
};



const char* get_stream_type_string(StreamType type)
{
    if (type >= sizeof(g_streamTypeStrings) / sizeof(char*))
    {
        assert(type >= sizeof(g_streamTypeStrings) / sizeof(char*));
        ml_log_error("unknown stream type %d\n", type);
        return g_streamTypeStrings[0];
    }
    
    return g_streamTypeStrings[type];
}

const char* get_stream_format_string(StreamFormat format)
{
    if (format >= sizeof(g_streamFormatStrings) / sizeof(char*))
    {
        assert(format >= sizeof(g_streamFormatStrings) / sizeof(char*));
        ml_log_error("unknown stream format %d\n", format);
        return g_streamFormatStrings[0];
    }
    
    return g_streamFormatStrings[format];
}


int initialise_stream_info(StreamInfo* streamInfo)
{
    memset(streamInfo, 0, sizeof(StreamInfo));
    
    CALLOC_ORET(streamInfo->sourceInfoValues, SourceInfoValue, 32);
    streamInfo->numSourceInfoValuesAlloc = 32;
    
    return 1;
}

int add_source_info(StreamInfo* streamInfo, const char* name, const char* value)
{
    SourceInfoValue* newArray = NULL;
    char* newName = NULL;
    char* newValue = NULL;
    
    /* copy new name-value pair */
    if (name == NULL)
    {
        MALLOC_OFAIL(newName, char, 1);
        newName[0] = '\0';
    }
    else
    {
        MALLOC_OFAIL(newName, char, MAX_SOURCE_INFO_NAME_LEN + 1);
        strncpy(newName, name, MAX_SOURCE_INFO_NAME_LEN);
        newName[MAX_SOURCE_INFO_NAME_LEN] = '\0';
    }
    if (value == NULL)
    {
        MALLOC_OFAIL(newValue, char, 1);
        newValue[0] = '\0';
    }
    else
    {
        MALLOC_OFAIL(newValue, char, MAX_SOURCE_INFO_VALUE_LEN + 1);
        strncpy(newValue, value, MAX_SOURCE_INFO_VALUE_LEN);
        newValue[MAX_SOURCE_INFO_VALUE_LEN] = '\0';
    }
    
    if (streamInfo->numSourceInfoValues + 1 > streamInfo->numSourceInfoValuesAlloc)
    {
        /* allocate new array and copy over old values */
        CALLOC_OFAIL(newArray, SourceInfoValue, streamInfo->numSourceInfoValuesAlloc + 32);
        memcpy(newArray, streamInfo->sourceInfoValues, streamInfo->numSourceInfoValuesAlloc * sizeof(SourceInfoValue));
        
        SAFE_FREE(&streamInfo->sourceInfoValues);
        streamInfo->sourceInfoValues = newArray;
        newArray = NULL;
        streamInfo->numSourceInfoValuesAlloc += 32;
    }

    streamInfo->numSourceInfoValues++;
    streamInfo->sourceInfoValues[streamInfo->numSourceInfoValues - 1].name = newName;
    newName = NULL;
    streamInfo->sourceInfoValues[streamInfo->numSourceInfoValues - 1].value = newValue;
    newValue = NULL;
    
    return 1;
    
fail:
    SAFE_FREE(&newName);
    SAFE_FREE(&newValue);
    SAFE_FREE(&newArray);
    return 0;
}

int add_known_source_info(StreamInfo* streamInfo, SourceInfoName name, const char* value)
{
    if (name > sizeof(g_sourceInfoNames) / sizeof(char*))
    {
        ml_log_warn("Unknown name enum %d\n", name);
        return add_source_info(streamInfo, g_sourceInfoNames[SRC_INFO_UNKNOWN], value);
    }
    else
    {
        return add_source_info(streamInfo, g_sourceInfoNames[name], value);
    }
}

int add_filename_source_info(StreamInfo* streamInfo, SourceInfoName name, const char* filename)
{
    char limitedFilename[MAX_SOURCE_INFO_VALUE_LEN + 1];
    size_t len;
    
    len = strlen(filename);
    if (len > MAX_SOURCE_INFO_VALUE_LEN)
    {
        sprintf(limitedFilename, "...%s", &filename[len - (MAX_SOURCE_INFO_VALUE_LEN - 3)]);
    }
    else
    {
        sprintf(limitedFilename, "%s", filename);
    }
    
    return add_known_source_info(streamInfo, name, limitedFilename);
}

int add_timecode_source_info(StreamInfo* streamInfo, SourceInfoName name, int64_t timecode, int timecodeBase)
{
    char timecodeStr[MAX_SOURCE_INFO_VALUE_LEN + 1];
    
    if (timecode < 0)
    {
        sprintf(timecodeStr, "?"); 
    }
    else
    {
        sprintf(timecodeStr, "%02"PFi64":%02"PFi64":%02"PFi64":%02"PFi64, 
            timecode / (60 * 60 * timecodeBase),
            (timecode % (60 * 60 * timecodeBase)) / (60 * timecodeBase),
            ((timecode % (60 * 60 * timecodeBase)) % (60 * timecodeBase)) / timecodeBase,
            ((timecode % (60 * 60 * timecodeBase)) % (60 * timecodeBase)) % timecodeBase);
    }

    return add_known_source_info(streamInfo, name, timecodeStr);
}


void clear_stream_info(StreamInfo* streamInfo)
{
    int i;
    
    if (streamInfo == NULL)
    {
        return;
    }
    
    for (i = 0; i < streamInfo->numSourceInfoValues; i++)
    {
        SAFE_FREE(&streamInfo->sourceInfoValues[i].name);
        SAFE_FREE(&streamInfo->sourceInfoValues[i].value);
    }
    SAFE_FREE(&streamInfo->sourceInfoValues);

    memset(streamInfo, 0, sizeof(StreamInfo));
}

const char* get_source_info_value(const StreamInfo* streamInfo, const char* name)
{
    int i;
    for (i = 0; i < streamInfo->numSourceInfoValues; i++)
    {
       if (strcmp(name, streamInfo->sourceInfoValues[i].name) == 0)
       {
           return streamInfo->sourceInfoValues[i].value;
       }
    }
    
    return NULL;
}

const char* get_known_source_info_value(const StreamInfo* streamInfo, SourceInfoName name)
{
    int i;
    
    if (name > sizeof(g_sourceInfoNames) / sizeof(char*))
    {
        assert(name <= sizeof(g_sourceInfoNames) / sizeof(char*));
        ml_log_warn("Unknown name enum %d\n", name);
        return NULL;
    }
    
    for (i = 0; i < streamInfo->numSourceInfoValues; i++)
    {
       if (strcmp(g_sourceInfoNames[name], streamInfo->sourceInfoValues[i].name) == 0)
       {
           return streamInfo->sourceInfoValues[i].value;
       }
    }
   
    return NULL;
}


int duplicate_stream_info(const StreamInfo* fromStreamInfo, StreamInfo* toStreamInfo)
{
    int i;
    
    *toStreamInfo = *fromStreamInfo;
    
    toStreamInfo->sourceInfoValues = NULL;
    toStreamInfo->numSourceInfoValues = 0;
    toStreamInfo->numSourceInfoValuesAlloc = 0;
    for (i = 0; i < fromStreamInfo->numSourceInfoValues; i++)
    {
        CHK_ORET(add_source_info(toStreamInfo, fromStreamInfo->sourceInfoValues[i].name, fromStreamInfo->sourceInfoValues[i].value));
    }
    
    return 1;
}


void set_stream_clip_id(StreamInfo* streamInfo, const char* clipId)
{
    if (clipId == NULL)
    {
        streamInfo->clipId[0] = '\0';
        return;
    }
    
    strncpy(streamInfo->clipId, clipId, sizeof(streamInfo->clipId));
    streamInfo->clipId[sizeof(streamInfo->clipId) - 1] = '\0';
}


int select_frame_timecode(const FrameInfo* frameInfo, int tcIndex, int tcType, int tcSubType, Timecode* timecode)
{
    int typedIndex = 0;
    int index = 0;
    int i;
    for (i = 0; i < frameInfo->numTimecodes; i++)
    {
        if ((tcIndex < 0 || tcIndex == typedIndex) &&
            (tcType < 0 || frameInfo->timecodes[i].timecodeType == (TimecodeType)tcType) &&
            (tcSubType < 0 || frameInfo->timecodes[i].timecodeSubType == (TimecodeSubType)tcSubType))
        {
            *timecode = frameInfo->timecodes[i].timecode;
            return 1;
        }
        else if ((tcType < 0 || frameInfo->timecodes[i].timecodeType == (TimecodeType)tcType) &&
            (tcSubType < 0 || frameInfo->timecodes[i].timecodeSubType == (TimecodeSubType)tcSubType))
        {
            typedIndex++;
        }
        index++;
    }
    
    return 0;
}

