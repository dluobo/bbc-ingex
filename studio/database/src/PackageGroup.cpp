/*
 * $Id: PackageGroup.cpp,v 1.1 2009/10/12 15:54:33 philipn Exp $
 *
 * Package group
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

#include <stdio.h>
#include <errno.h>

#include "PackageGroup.h"
#include "Database.h"
#include "Utilities.h"
#include "ProdAutoException.h"
#include "Logging.h"

using namespace std;
using namespace prodauto;


PackageGroup::PackageGroup(bool is_pal_project, int op)
{
    mPALProject = is_pal_project;
    mOP = op;
    mProjectEditRate = (is_pal_project ? g_palEditRate : g_ntscEditRate);
    mMaterialPackage = 0;
    mTapeSourcePackage = 0;
    mOwnTapeSourcePackage = false;

}

PackageGroup::~PackageGroup()
{
    ClearPackages();
}

void PackageGroup::UpdateUserComments(vector<UserComment> &user_comments)
{
    mMaterialPackage->addUserComments(user_comments);
}

void PackageGroup::UpdateProjectName(ProjectName project_name)
{
    mMaterialPackage->projectName = project_name;
}

void PackageGroup::UpdateDuration(int64_t duration)
{
    size_t i;
    for (i = 0; i < mMaterialPackage->tracks.size(); i++) {
        Track *mp_track = mMaterialPackage->tracks[i];

        mp_track->sourceClip->length = convertPosition(duration, mProjectEditRate, mp_track->editRate);
    
        Track *fsp_track = GetFileSourceTrack(mp_track->id);
        fsp_track->sourceClip->length = convertPosition(duration, mProjectEditRate, fsp_track->editRate);
    }
}

void PackageGroup::UpdateDuration(uint32_t mp_track_id, int64_t duration)
{
    Track *mp_track = mMaterialPackage->getTrack(mp_track_id);
    PA_ASSERT(mp_track);
    
    mp_track->sourceClip->length = convertPosition(duration, mProjectEditRate, mp_track->editRate);
    
    Track *fsp_track = GetFileSourceTrack(mp_track_id);
    fsp_track->sourceClip->length = convertPosition(duration, mProjectEditRate, fsp_track->editRate);
}

void PackageGroup::UpdateFileLocation(uint32_t mp_track_id, string file_location)
{
    SourcePackage *fsp = GetFileSourcePackage(mp_track_id);
    
    PA_ASSERT(fsp->descriptor->getType() == FILE_ESSENCE_DESC_TYPE);
    FileEssenceDescriptor *file_descriptor = dynamic_cast<FileEssenceDescriptor*>(fsp->descriptor);
    file_descriptor->fileLocation = file_location;
}

void PackageGroup::UpdateFileLocation(string file_location)
{
    PA_ASSERT(mFileSourcePackages.size() == 1);
    SourcePackage *fsp = mFileSourcePackages[0];
    
    PA_ASSERT(fsp->descriptor->getType() == FILE_ESSENCE_DESC_TYPE);
    FileEssenceDescriptor *file_descriptor = dynamic_cast<FileEssenceDescriptor*>(fsp->descriptor);
    file_descriptor->fileLocation = file_location;
}

SourcePackage* PackageGroup::GetFileSourcePackage()
{
    PA_ASSERT(mOP == OPERATIONAL_PATTERN_1A);

    if (mFileSourcePackages.size() != 1)
        return 0;
    
    return mFileSourcePackages[0];
}

SourcePackage* PackageGroup::GetFileSourcePackage(uint32_t mp_track_id)
{
    Track *mp_track = mMaterialPackage->getTrack(mp_track_id);
    PA_ASSERT(mp_track);
    
    SourcePackage *fsp = 0;
    size_t i;
    for (i = 0; i < mFileSourcePackages.size(); i++) {
        if (mp_track->sourceClip->sourcePackageUID == mFileSourcePackages[i]->uid) {
            fsp = mFileSourcePackages[i];
            break;
        }
    }
    PA_ASSERT(fsp);
    
    return fsp;
}

Track* PackageGroup::GetFileSourceTrack(uint32_t mp_track_id)
{
    Track *mp_track = mMaterialPackage->getTrack(mp_track_id);
    PA_ASSERT(mp_track);
    
    SourcePackage *fsp = 0;
    size_t i;
    for (i = 0; i < mFileSourcePackages.size(); i++) {
        if (mp_track->sourceClip->sourcePackageUID == mFileSourcePackages[i]->uid) {
            fsp = mFileSourcePackages[i];
            break;
        }
    }
    PA_ASSERT(fsp);
    
    Track *fsp_track = fsp->getTrack(mp_track->sourceClip->sourceTrackID);
    PA_ASSERT(fsp_track);
    
    return fsp_track;
}

string PackageGroup::GetFileLocation(uint32_t mp_track_id)
{
    SourcePackage *fsp = GetFileSourcePackage(mp_track_id);
    
    PA_ASSERT(fsp->descriptor->getType() == FILE_ESSENCE_DESC_TYPE);
    FileEssenceDescriptor *file_descriptor = dynamic_cast<FileEssenceDescriptor*>(fsp->descriptor);
    return file_descriptor->fileLocation;
}

string PackageGroup::GetFileLocation()
{
    PA_ASSERT(mOP == OPERATIONAL_PATTERN_1A);

    SourcePackage *fsp = mFileSourcePackages[0];
    
    PA_ASSERT(fsp->descriptor->getType() == FILE_ESSENCE_DESC_TYPE);
    FileEssenceDescriptor *file_descriptor = dynamic_cast<FileEssenceDescriptor*>(fsp->descriptor);
    return file_descriptor->fileLocation;
}

string PackageGroup::CreateFileLocation(string directory, string file_path)
{
    // strip the directory path
    string filename = file_path;
    size_t sep_index;
#if defined(_WIN32)
    if ((sep_index = file_path.rfind("\\")) != string::npos ||
        (sep_index = file_path.rfind(":")) != string::npos)
#else
    if ((sep_index = file_path.rfind("/")) != string::npos)
#endif
    {
        filename = file_path.substr(sep_index + 1);
    }
    
    if (directory.empty())
        return filename;

#if defined(_WIN32)
    return directory + "\\" + filename;
#else
    return directory + "/" + filename;
#endif
}

void PackageGroup::RelocateFile(uint32_t mp_track_id, string target_directory)
{
    string from = GetFileLocation(mp_track_id);
    string to = CreateFileLocation(target_directory, from);
    
    if (from == to)
        return;
    
    if (rename(from.c_str(), to.c_str()) != 0) {
        PA_LOGTHROW(ProdAutoException, ("Failed to rename file from '%s' to '%s': %s\n",
                                         from.c_str(), to.c_str(), strerror(errno)));
    }
    
    UpdateFileLocation(mp_track_id, to);
}

void PackageGroup::RelocateFile(string target_directory)
{
    PA_ASSERT(mOP == OPERATIONAL_PATTERN_1A);
    
    string from = GetFileLocation();
    string to = CreateFileLocation(target_directory, from);
    
    if (from == to)
        return;
    
    if (rename(from.c_str(), to.c_str()) != 0) {
        PA_LOGTHROW(ProdAutoException, ("Failed to rename file from '%s' to '%s': %s\n",
                                         from.c_str(), to.c_str(), strerror(errno)));
    }
    
    UpdateFileLocation(to);
}

void PackageGroup::SaveToDatabase()
{
    Database *database = Database::getInstance();
    auto_ptr<Transaction> transaction(database->getTransaction("SavePackageGroup"));

    if (!mTapeSourcePackage->isPersistent())
        database->savePackage(mTapeSourcePackage, transaction.get());

    size_t i;
    for (i = 0; i < mFileSourcePackages.size(); i++)
        database->savePackage(mFileSourcePackages[i], transaction.get());
    
    database->savePackage(mMaterialPackage, transaction.get());
    
    transaction->commit();
}

void PackageGroup::SetMaterialPackage(MaterialPackage *material_package)
{
    if (mMaterialPackage)
        delete mMaterialPackage;
    mMaterialPackage = material_package;
}

void PackageGroup::AppendFileSourcePackage(SourcePackage *file_source_package)
{
    if (mOP == OPERATIONAL_PATTERN_1A)
        ClearFileSourcePackages();
    mFileSourcePackages.push_back(file_source_package);
}

void PackageGroup::ClearFileSourcePackages()
{
    size_t i;
    for (i = 0; i < mFileSourcePackages.size(); i++)
        delete mFileSourcePackages[i];
    mFileSourcePackages.clear();
}

void PackageGroup::SetTapeSourcePackage(SourcePackage *tape_source_package, bool take_ownership)
{
    if (mOwnTapeSourcePackage)
        delete mTapeSourcePackage;
    mTapeSourcePackage = tape_source_package;
    mOwnTapeSourcePackage = take_ownership;
}

void PackageGroup::ClearPackages()
{
    delete mMaterialPackage;
    mMaterialPackage = 0;

    size_t i;
    for (i = 0; i < mFileSourcePackages.size(); i++)
        delete mFileSourcePackages[i];
    mFileSourcePackages.clear();

    if (mOwnTapeSourcePackage)
        delete mTapeSourcePackage;
    mTapeSourcePackage = 0;
    mOwnTapeSourcePackage = false;
}

