/*
 * $Id: RecoverClip.cpp,v 1.1 2009/11/17 16:26:35 john_f Exp $
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

#define __STDC_FORMAT_MACROS    1

#include "RecoverClip.h"

#include <libMXF++/MXF.h>
#include <OPAtomClipReader.h>

#include <MXFOPAtomWriter.h>
#include <ProdAutoException.h>
#include <Logging.h>

using namespace std;
using namespace prodauto;



RecoverClip::RecoverClip(const IngexMXFInfo *info)
{
    mPackageGroup = info->getPackageGroup()->Clone();
}

RecoverClip::~RecoverClip()
{
    delete mPackageGroup;
}

bool RecoverClip::IsSameClip(const IngexMXFInfo *info)
{
    return info->getPackageGroup()->GetMaterialPackage()->uid == mPackageGroup->GetMaterialPackage()->uid;
}

void RecoverClip::MergeIn(const IngexMXFInfo *info)
{
    PA_ASSERT(IsSameClip(info));
    // assert that either both have the same tape source package or both have no tape source package
    PA_ASSERT((!info->getPackageGroup()->GetTapeSourcePackage() && !mPackageGroup->GetTapeSourcePackage()) ||
              (info->getPackageGroup()->GetTapeSourcePackage() && mPackageGroup->GetTapeSourcePackage()));
    PA_ASSERT(!info->getPackageGroup()->GetTapeSourcePackage() ||
              info->getPackageGroup()->GetTapeSourcePackage()->uid == mPackageGroup->GetTapeSourcePackage()->uid);
    
    // copy in non-duplicate file source packages
    vector<prodauto::SourcePackage*> &from_file_packages = info->getPackageGroup()->GetFileSourcePackages();
    vector<prodauto::SourcePackage*> &to_file_packages = mPackageGroup->GetFileSourcePackages();
    size_t i, j;
    for (i = 0; i < from_file_packages.size(); i++) {
        for (j = 0; j < to_file_packages.size(); j++) {
            if (from_file_packages[i]->uid == to_file_packages[j]->uid)
                break;
        }

        if (j == to_file_packages.size()) {
            mPackageGroup->AppendFileSourcePackage(
                dynamic_cast<prodauto::SourcePackage*>(from_file_packages[i]->clone()));
        } else {
            printf("Warning: ignoring duplicate file '%s'\n", info->getFilename().c_str());
        }
    }
    
}

bool RecoverClip::IsComplete()
{
    prodauto::MaterialPackage *material_package = mPackageGroup->GetMaterialPackage();
    size_t i;
    for (i = 0; i < material_package->tracks.size(); i++) {
        if (!mPackageGroup->HaveFileSourcePackage(material_package->tracks[i]->id))
            return false;
    }
    
    return true;
}

vector<uint32_t> RecoverClip::GetMissingTracks()
{
    vector<uint32_t> missing_tracks;
    
    prodauto::MaterialPackage *material_package = mPackageGroup->GetMaterialPackage();
    size_t i;
    for (i = 0; i < material_package->tracks.size(); i++) {
        if (!mPackageGroup->HaveFileSourcePackage(material_package->tracks[i]->id))
            missing_tracks.push_back(material_package->tracks[i]->id);
    }
    
    return missing_tracks;
}

bool RecoverClip::Recover(string create_dir, string dest_dir, string fail_dir, string output_prefix,
                          bool remove_files_on_error, bool simulate)
{
    // collect file locations for the available source packages because they will be modified below
    vector<string> file_locations;
    prodauto::MaterialPackage *material_package = mPackageGroup->GetMaterialPackage();
    size_t i;
    for (i = 0; i < material_package->tracks.size(); i++) {
        if (mPackageGroup->HaveFileSourcePackage(material_package->tracks[i]->id))
            file_locations.push_back(mPackageGroup->GetFileLocation(material_package->tracks[i]->id));
    }
    
    printf("Recover %d of %d files in clip '%s':\n", file_locations.size(), material_package->tracks.size(),
           mPackageGroup->GetMaterialPackage()->name.c_str());
    for (i = 0; i < file_locations.size(); i++)
        printf("    '%s'\n", file_locations[i].c_str());
    

    // open a clip reader with the given file locations
    OPAtomClipReader *clip_reader;
    pair<OPAtomOpenResult, string> result = OPAtomClipReader::Open(file_locations, &clip_reader);
    if (result.first != OP_ATOM_SUCCESS) {
        fprintf(stderr, "Failed to open file '%s' in clip '%s': %s\n", result.second.c_str(),
                mPackageGroup->GetMaterialPackage()->name.c_str(),
                OPAtomClipReader::ErrorToString(result.first).c_str());
        return false;
    }
    
    // update the file locations with a new prefix
    mPackageGroup->UpdateAllFileLocations(output_prefix);
    
    int64_t num_samples = 0;
    prodauto::MXFOPAtomWriter writer;
    const OPAtomContentPackage *content_package;
    try
    {
        writer.SetCreatingDirectory(create_dir);
        writer.SetDestinationDirectory(dest_dir);
        writer.SetFailureDirectory(fail_dir);

        if (!simulate)
            writer.PrepareToWrite(mPackageGroup, false);
        
        while (true) {
            // read content package
            content_package = clip_reader->Read();
            if (!content_package) {
                if (!clip_reader->IsEOF()) {
                    fprintf(stderr, "Failed to read content package\n");
                    throw false;
                }
                break;
            }
            
            // write content package essence data
            if (!simulate) {
                const OPAtomContentElement *element;
                for (i = 0; i < content_package->NumEssenceData(); i++) {
                    element = content_package->GetEssenceDataI(i);
                    writer.WriteSamples(element->GetMaterialTrackId(), element->GetNumSamples(),
                                        element->GetBytes(), element->GetSize());
                }
            }
            
            num_samples++;
        }
        
        if (!simulate)
            writer.CompleteWriting(false);
        
        printf("Success: recovered %"PRId64" samples\n", num_samples);
    }
    catch (...)
    {
        fprintf(stderr, "Failed to complete recovery\n");
        if (!simulate)
            writer.AbortWriting(remove_files_on_error);
        return false;
    }
    
    delete clip_reader;
    
    return true;
}

