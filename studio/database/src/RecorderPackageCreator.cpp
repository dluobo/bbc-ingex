/*
 * $Id: RecorderPackageCreator.cpp,v 1.1 2009/10/12 15:54:33 philipn Exp $
 *
 * Recorder package group creator
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "RecorderPackageCreator.h"
#include "Utilities.h"
#include "ProdAutoException.h"
#include "Logging.h"

using namespace std;
using namespace prodauto;



RecorderPackageCreator::RecorderPackageCreator(bool is_pal_project, int op)
: PackageGroup(is_pal_project, op)
{
    mFileFormat = MXF_FILE_FORMAT_TYPE;
    mCreationDate = g_nullTimestamp;
    mStartPosition = 0;
    mOrigin = 0;
    mFileLocationPrefix = "ingex";
    mImageAspectRatio = g_16x9ImageAspect;
    mVideoResolutionId = UNC_MATERIAL_RESOLUTION;
    mAudioQuantizationBits = 16;
}

RecorderPackageCreator::~RecorderPackageCreator()
{
    ClearPackages();
}

void RecorderPackageCreator::SetFileFormat(int file_format)
{
    mFileFormat = file_format;
}

void RecorderPackageCreator::SetCreationDate(Timestamp creation_date)
{
    mCreationDate = creation_date;
}

void RecorderPackageCreator::SetStartPosition(int64_t position)
{
    mStartPosition = position;

    SetOrigin();
}

void RecorderPackageCreator::SetUserComments(vector<UserComment> &user_comments)
{
    mUserComments = user_comments;
}

void RecorderPackageCreator::SetProjectName(ProjectName project_name)
{
    mProjectName = project_name;
}

void RecorderPackageCreator::SetFileLocationPrefix(string prefix)
{
    mFileLocationPrefix = prefix;
}

void RecorderPackageCreator::SetVideoFileLocationSuffix(string suffix)
{
    mVideoFileLocationSuffix = suffix;
}

void RecorderPackageCreator::SetAudioFileLocationSuffix(string suffix)
{
    mAudioFileLocationSuffix = suffix;
}

void RecorderPackageCreator::SetClipName(string clip_name)
{
    mClipName = clip_name;
}

void RecorderPackageCreator::SetImageAspectRatio(Rational image_aspect_ratio)
{
    mImageAspectRatio = image_aspect_ratio;
}

void RecorderPackageCreator::SetVideoResolutionID(int video_resolution_id)
{
    mVideoResolutionId = video_resolution_id;
}

void RecorderPackageCreator::SetAudioQuantBits(uint32_t bits)
{
    mAudioQuantizationBits = bits;
}

uint32_t RecorderPackageCreator::GetMaterialTrackId(uint32_t source_config_track_id)
{
    map<uint32_t, uint32_t>::iterator result = mTrackMap.find(source_config_track_id);
    if (result == mTrackMap.end())
        return DISABLED_MATERIAL_TRACK_ID;
    
    return result->second;
}

void RecorderPackageCreator::ClearPackages()
{
    PackageGroup::ClearPackages();
    
    mTrackMap.clear();
}

string RecorderPackageCreator::GetClipName()
{
    if (!mClipName.empty())
        return mClipName;
    
    // default clip name is the file location prefix minus the directory path
    string clip_name = mFileLocationPrefix;
    size_t sep_index;
#if defined(_WIN32)
    if ((sep_index = mFileLocationPrefix.rfind("\\")) != string::npos ||
        (sep_index = mFileLocationPrefix.rfind(":")) != string::npos)
#else
    if ((sep_index = mFileLocationPrefix.rfind("/")) != string::npos)
#endif
    {
        clip_name = mFileLocationPrefix.substr(sep_index + 1);
    }
    
    return clip_name;
}

string RecorderPackageCreator::GetFileLocationSuffix()
{
    switch (mFileFormat)
    {
        case MXF_FILE_FORMAT_TYPE:
            return ".mxf";
        case MOV_FILE_FORMAT_TYPE:
            return ".mov";
        case MPG_FILE_FORMAT_TYPE:
            return ".mpg";
        case RAW_FILE_FORMAT_TYPE:
        default:
            return ".raw";
    }
}

string RecorderPackageCreator::GetFileLocationSuffix(int data_def)
{
    if (data_def == PICTURE_DATA_DEFINITION && !mVideoFileLocationSuffix.empty())
        return mVideoFileLocationSuffix;
    else if (data_def == SOUND_DATA_DEFINITION && !mAudioFileLocationSuffix.empty())
        return mAudioFileLocationSuffix;
    else
        return GetFileLocationSuffix();
}

Timestamp RecorderPackageCreator::GetCreationDate()
{
    if (mCreationDate != g_nullTimestamp)
        return mCreationDate;
    
    return generateTimestampNow();
}

void RecorderPackageCreator::SetOrigin()
{
    double frame_rate = mProjectEditRate.numerator / (double)mProjectEditRate.denominator;

    time_t seconds = (time_t)(mStartPosition / frame_rate + 0.5);
    
    // if the start position is <= 24 hours then it must be a time-of-day timecode
    if (seconds < 24 * 60 * 60) {
        mOrigin = 0;
        return;
    }

    // TODO
    mOrigin = 0;
    return;
    
#if 0
    // calculate the origin needed to adjust the start position to a time-of-day timecode
    
    struct tm gmt;
    memset(&gmt, 0, sizeof(gmt));
    
#if defined(_MSC_VER)
    const struct tm *gmt_ptr = gmtime(&seconds);
    if (!gmt_ptr)
        PA_LOGTHROW(ProdAutoException, ("gmtime failed"));
    gmt = *gmt_ptr;
#else
    if (!gmtime_r(&seconds, &gmt))
        PA_LOGTHROW(ProdAutoException, ("gmtime failed"));
#endif

    // start of the day
    gmt.tm_hour = 0;
    gmt.tm_min = 0;
    gmt.tm_sec = 0;

    mOrigin = mktime(&gmt);
#endif
}

string RecorderPackageCreator::CreateTrackName(int data_def, uint32_t track_number)
{
    char name[32];
    
    if (data_def == PICTURE_DATA_DEFINITION)
        sprintf(name, "V%d", track_number);
    else
        sprintf(name, "A%d", track_number);
    
    return name;
}

void RecorderPackageCreator::CreateTapeSourcePackage(SourceConfig *source_config)
{
    // a tape source package shall have a source package
    mTapeSourcePackage = source_config->getSourcePackage();
    PA_ASSERT(mTapeSourcePackage);
    mOwnTapeSourcePackage = false;
    
    // TODO: adjust the track origins if neccessary and in some cases a new source package must be created if
    // the start timecode extends beyond the length of the tracks
}

