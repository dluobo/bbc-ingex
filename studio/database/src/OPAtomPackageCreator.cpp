/*
 * $Id: OPAtomPackageCreator.cpp,v 1.1 2009/10/12 15:54:33 philipn Exp $
 *
 * OP-Atom package group creator
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

#include "OPAtomPackageCreator.h"
#include "ProdAutoException.h"
#include "Logging.h"
#include "Utilities.h"

using namespace std;
using namespace prodauto;



string OPAtomPackageCreator::CreateFileLocation(string prefix, string suffix,
                                                int data_def, uint32_t track_id, uint32_t track_number)
{
    char complete_suffix[64];
    
    sprintf(complete_suffix, "_%d_%s%d%s", track_id, (data_def == PICTURE_DATA_DEFINITION ? "v" : "a"), track_number,
            suffix.c_str());
    
    return prefix + complete_suffix;
}



OPAtomPackageCreator::OPAtomPackageCreator(bool is_pal_project)
: RecorderPackageCreator(is_pal_project, OPERATIONAL_PATTERN_ATOM)
{
}

OPAtomPackageCreator::~OPAtomPackageCreator()
{
}

void OPAtomPackageCreator::CreatePackageGroup(SourceConfig *source_config, vector<bool> enabled_tracks,
                                              uint32_t umid_gen_offset)
{
    PA_ASSERT(source_config->trackConfigs.size() == enabled_tracks.size());
    
    // allow reuse of this object to create packages
    ClearPackages();
    
    Timestamp creation_date = GetCreationDate();
    
    // create the material package
    mMaterialPackage = new MaterialPackage();
    mMaterialPackage->uid = generateUMID(umid_gen_offset);
    mMaterialPackage->name = GetClipName();
    mMaterialPackage->creationDate = creation_date;
    mMaterialPackage->projectName = mProjectName;
    mMaterialPackage->op = OPERATIONAL_PATTERN_ATOM;
    mMaterialPackage->addUserComments(mUserComments);
    mMaterialPackage->addUserComment(AVID_UC_SOURCE_NAME, source_config->name, STATIC_COMMENT_POSITION, 0);
    
    CreateTapeSourcePackage(source_config);
    
    uint32_t track_id = FIRST_MATERIAL_TRACK_ID;
    size_t i;
    for (i = 0; i < source_config->trackConfigs.size(); i++) {
        if (!enabled_tracks[i])
            continue;
        
        SourceTrackConfig *track_config = source_config->trackConfigs[i];
        
        // create the file source package
        SourcePackage *fsp = new SourcePackage();
        mFileSourcePackages.push_back(fsp);
        fsp->uid = generateUMID(umid_gen_offset);
        fsp->name = "";
        fsp->creationDate = creation_date;
        fsp->projectName = mProjectName;
        fsp->sourceConfigName = source_config->name;
        FileEssenceDescriptor *descriptor = new FileEssenceDescriptor();
        fsp->descriptor = descriptor;
        descriptor->fileFormat = mFileFormat;
        descriptor->fileLocation = CreateFileLocation(track_config->dataDef, track_id, track_config->number);
        if (track_config->dataDef == PICTURE_DATA_DEFINITION) {
            descriptor->imageAspectRatio = mImageAspectRatio;
            descriptor->videoResolutionID = mVideoResolutionId;
        } else { // SOUND_DATA_DEFINITION
            descriptor->audioQuantizationBits = mAudioQuantizationBits;
        }
        
        // create a track in the file source package
        Track *fsp_track = new Track();
        fsp->tracks.push_back(fsp_track);
        fsp_track->id = 1;
        fsp_track->number = 0; // not relevant for file packages
        fsp_track->name = CreateTrackName(track_config->dataDef, track_config->number);
        if (track_config->dataDef == PICTURE_DATA_DEFINITION)
            fsp_track->editRate = mProjectEditRate;
        else
            fsp_track->editRate = g_audioEditRate;
        fsp_track->dataDef = track_config->dataDef;
        fsp_track->origin = 0;
        fsp_track->sourceClip = new SourceClip();
        fsp_track->sourceClip->position = convertPosition(mStartPosition, mProjectEditRate, fsp_track->editRate);
        fsp_track->sourceClip->length = 0; // updated when writing is completed
        fsp_track->sourceClip->sourcePackageUID = mTapeSourcePackage->uid;
        fsp_track->sourceClip->sourceTrackID = track_config->id;

        
        // create a track in the material package
        Track *mp_track = new Track();
        mMaterialPackage->tracks.push_back(mp_track);
        mp_track->id = track_id;
        mp_track->number = track_config->number;
        mp_track->name = CreateTrackName(track_config->dataDef, track_config->number);
        if (track_config->dataDef == PICTURE_DATA_DEFINITION)
            mp_track->editRate = mProjectEditRate;
        else
            mp_track->editRate = g_audioEditRate;
        mp_track->dataDef = track_config->dataDef;
        mp_track->origin = 0;
        mp_track->sourceClip = new SourceClip();
        mp_track->sourceClip->position = 0;
        mp_track->sourceClip->length = 0; // updated when writing is completed
        mp_track->sourceClip->sourcePackageUID = fsp->uid;
        mp_track->sourceClip->sourceTrackID = fsp_track->id;

        mTrackMap[track_config->id] = track_id;
        
        track_id++;
    }
}

string OPAtomPackageCreator::CreateFileLocation(int data_def, uint32_t track_id, uint32_t track_number)
{
    return CreateFileLocation(mFileLocationPrefix, GetFileLocationSuffix(data_def), data_def, track_id, track_number);
}

