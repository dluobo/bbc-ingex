/*
 * $Id: RecorderPackageCreator.h,v 1.1 2009/10/12 15:54:33 philipn Exp $
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

#ifndef __PRODAUTO_RECORDER_PACKAGE_CREATOR_H__
#define __PRODAUTO_RECORDER_PACKAGE_CREATOR_H__

#include <string>
#include <vector>
#include <map>

#include "PackageGroup.h"
#include "SourceConfig.h"

#define DISABLED_MATERIAL_TRACK_ID      0
#define FIRST_MATERIAL_TRACK_ID         1


namespace prodauto
{

class RecorderPackageCreator : public PackageGroup
{
public:
    RecorderPackageCreator(bool is_pal_project, int op);
    virtual ~RecorderPackageCreator();

    // settings
    void SetFileFormat(int file_format); // default: MXF_FILE_FORMAT_TYPE
    void SetCreationDate(Timestamp creation_date); // default: now
    void SetStartPosition(int64_t position); // default: 0
    void SetUserComments(std::vector<UserComment> &user_comments); // default: empty
    void SetProjectName(ProjectName project_name); // default: ""
    void SetFileLocationPrefix(std::string prefix); // default: "ingex"
    void SetVideoFileLocationSuffix(std::string suffix); // default: depends on file format
    void SetAudioFileLocationSuffix(std::string suffix); // default: depends on file format
    void SetClipName(std::string clip_name); // default: file location prefix minus the directory path
    void SetImageAspectRatio(Rational image_aspect_ratio); // default: 16:9
    void SetVideoResolutionID(int video_resolution_id); // default: UNC_MATERIAL_RESOLUTION
    void SetAudioQuantBits(uint32_t bits); // default: 16
    
public:
    // create the package group from a source config with the given tracks enabled
    // note: enabled_tracks.size() shall equal source_config->trackConfigs.size()
    virtual void CreatePackageGroup(SourceConfig *source_config, std::vector<bool> enabled_tracks,
                                    uint32_t umid_gen_offset) = 0;

public:
    // map source config tracks to material package tracks
    // if the source config track was not enabled then 0 is returned (the first material package track id equals 1)
    uint32_t GetMaterialTrackId(uint32_t source_config_track_id);

protected:
    // overide from PackageGroup
    virtual void ClearPackages();
    
protected:
    std::string GetClipName();
    std::string GetFileLocationSuffix();
    std::string GetFileLocationSuffix(int data_def);
    Timestamp GetCreationDate();
    void SetOrigin();
    
    std::string CreateTrackName(int data_def, uint32_t track_number);

    void CreateTapeSourcePackage(SourceConfig *source_config);
    
protected:
    int mFileFormat;
    Timestamp mCreationDate;
    int64_t mStartPosition;
    int64_t mOrigin;
    std::vector<UserComment> mUserComments;
    ProjectName mProjectName;
    std::string mFileLocationPrefix;
    std::string mVideoFileLocationSuffix;
    std::string mAudioFileLocationSuffix;
    std::string mClipName;
    Rational mImageAspectRatio;
    int mVideoResolutionId;
    uint32_t mAudioQuantizationBits;

    std::map<uint32_t, uint32_t> mTrackMap;
};


};


#endif

