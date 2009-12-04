/*
 * $Id: MXFOPAtomWriter.cpp,v 1.3 2009/12/04 18:28:34 john_f Exp $
 *
 * MXF OP-Atom writer
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

#include <string.h>
#include <memory>

#include "write_avid_mxf.h"

#include "MXFOPAtomWriter.h"
#include "MXFWriterException.h"

#include <Logging.h>
#include <Utilities.h>

using namespace std;
using namespace prodauto;


#define CHECK(cmd) \
    if (!(cmd)) \
    { \
        PA_LOGTHROW(MXFWriterException, ("'%s' failed\n", #cmd)); \
    }


static void convert_umid(const prodauto::UMID &in, mxfUMID &out)
{
    memcpy(&out, &in, 32);
}

static void convert_timestamp(const prodauto::Timestamp &in, mxfTimestamp &out)
{
    memcpy(&out, &in, 8);
}

static void convert_rational(const prodauto::Rational &in, mxfRational &out)
{
    memcpy(&out, &in, 8);
}

static AvidRGBColor convert_comment_color(int in)
{
    switch (in)
    {
        case USER_COMMENT_WHITE_COLOUR:
            return AVID_WHITE;
        case USER_COMMENT_RED_COLOUR:
            return AVID_RED;
        case USER_COMMENT_YELLOW_COLOUR:
            return AVID_YELLOW;
        case USER_COMMENT_GREEN_COLOUR:
            return AVID_GREEN;
        case USER_COMMENT_CYAN_COLOUR:
            return AVID_CYAN;
        case USER_COMMENT_BLUE_COLOUR:
            return AVID_BLUE;
        case USER_COMMENT_MAGENTA_COLOUR:
            return AVID_MAGENTA;
        case USER_COMMENT_BLACK_COLOUR:
            return AVID_BLACK;
        default:
            return AVID_RED;
    }
}




MXFOPAtomWriter::MXFOPAtomWriter()
: MXFWriter()
{
    mPackageGroup = 0;
    mOwnPackageGroup = false;
    mPackageDefinitions = 0;
    mClipWriter = 0;
}

MXFOPAtomWriter::~MXFOPAtomWriter()
{
    ResetWriter();
}

void MXFOPAtomWriter::PrepareToWrite(PackageGroup *package_group, bool take_ownership)
{
    PA_ASSERT(package_group->GetOP() == OPERATIONAL_PATTERN_ATOM);
    
    ResetWriter();
    
    mPackageGroup = package_group;
    mOwnPackageGroup = take_ownership;
    
    CreatePackageDefinitions();
    
    ProjectFormat project_format;
    mxfRational project_edit_rate;
    if (package_group->IsPALProject()) {
        project_format = PAL_25i;
        project_edit_rate = (mxfRational){25, 1};
    } else {
        project_format = NTSC_30i;
        project_edit_rate = (mxfRational){30000, 1001};
    }
    
    CHECK(create_clip_writer(mPackageGroup->GetMaterialPackage()->projectName.name.c_str(), project_format,
                             project_edit_rate, false, false, mPackageDefinitions, &mClipWriter));

    // update the file locations in the prodauto file source packages
    MXFListIterator mp_track_iter;
    mxf_initialise_list_iter(&mp_track_iter, &mPackageDefinitions->materialPackage->tracks);
    while (mxf_next_list_iter_element(&mp_track_iter))
    {
        ::Track *mp_track = (::Track*)mxf_get_iter_element(&mp_track_iter);
        
        // find the corresponding file source package and track
        ::Package *fs_package = 0;
        MXFListIterator fsp_iter;
        mxf_initialise_list_iter(&fsp_iter, &mPackageDefinitions->fileSourcePackages);
        while (mxf_next_list_iter_element(&fsp_iter))
        {
            ::Package *package = (::Package*)mxf_get_iter_element(&fsp_iter);
            
            if (memcmp(&package->uid, &mp_track->sourcePackageUID, sizeof(package->uid)) == 0) {
                fs_package = package;
                break;
            }
        }
        // file source package could be null if the prodauto package_group didn't include a file source package
        // for this material package track
        
        if (fs_package)
            mPackageGroup->UpdateFileLocation(mp_track->id, fs_package->filename);
        else
            Logging::warning("Missing file source package for material track %d\n", mp_track->id);
    }
}

void MXFOPAtomWriter::WriteSamples(uint32_t mp_track_id, uint32_t num_samples, const uint8_t *data, uint32_t data_size)
{
    CHECK(write_samples(mClipWriter, mp_track_id, num_samples, data, data_size));
}

void MXFOPAtomWriter::StartSampleData(uint32_t mp_track_id)
{
    CHECK(start_write_samples(mClipWriter, mp_track_id));
}

void MXFOPAtomWriter::WriteSampleData(uint32_t mp_track_id, const uint8_t *data, uint32_t data_size)
{
    CHECK(write_sample_data(mClipWriter, mp_track_id, data, data_size));
}

void MXFOPAtomWriter::EndSampleData(uint32_t mp_track_id, uint32_t num_samples)
{
    CHECK(end_write_samples(mClipWriter, mp_track_id, num_samples));
}

void MXFOPAtomWriter::CompleteWriting(bool save_to_database)
{
    try
    {
        CreatePackageDefinitions();

        // record the durations before the mClipWriter is deleted when completing
        uint32_t mp_track_id;
        size_t i;
        for (i = 0; i < mPackageGroup->GetMaterialPackage()->tracks.size(); i++) {
            mp_track_id = mPackageGroup->GetMaterialPackage()->tracks[i]->id;
            if (mPackageGroup->HaveFileSourcePackage(mp_track_id))
                mPackageGroup->UpdateDuration(mp_track_id, GetDuration(mp_track_id));
        }
        
        CHECK(update_and_complete_writing(&mClipWriter, mPackageDefinitions,
                                          mPackageGroup->GetMaterialPackage()->projectName.name.c_str()));
        
        // rename the files to the destination directory
        for (i = 0; i < mPackageGroup->GetMaterialPackage()->tracks.size(); i++) {
            mp_track_id = mPackageGroup->GetMaterialPackage()->tracks[i]->id;
            if (mPackageGroup->HaveFileSourcePackage(mp_track_id))
                mPackageGroup->RelocateFile(mp_track_id, mDestinationDirectory);
        }
        
        // save the packages to the database
        if (save_to_database)
            mPackageGroup->SaveToDatabase();
    }
    catch (...)
    {
        try
        {
            // abort writing if not completed
            if (mClipWriter)
                abort_writing(&mClipWriter, false);
            
            // rename the files to the failure directory
            uint32_t mp_track_id;
            size_t i;
            for (i = 0; i < mPackageGroup->GetMaterialPackage()->tracks.size(); i++) {
                mp_track_id = mPackageGroup->GetMaterialPackage()->tracks[i]->id;
                if (mPackageGroup->HaveFileSourcePackage(mp_track_id))
                    mPackageGroup->RelocateFile(mp_track_id, mFailureDirectory);
            }
        }
        catch (...)
        {
            // give up
        }
        
        mClipWriter = 0;

        throw;
    }
}

void MXFOPAtomWriter::AbortWriting(bool delete_files)
{
    try
    {
        if (mClipWriter)
            abort_writing(&mClipWriter, delete_files);

        if (!delete_files) {
            // rename the files to the failure directory
            uint32_t mp_track_id;
            size_t i;
            for (i = 0; i < mPackageGroup->GetMaterialPackage()->tracks.size(); i++) {
                mp_track_id = mPackageGroup->GetMaterialPackage()->tracks[i]->id;
                if (mPackageGroup->HaveFileSourcePackage(mp_track_id))
                    mPackageGroup->RelocateFile(mp_track_id, mFailureDirectory);
            }
        }
    }
    catch (...)
    {
        // give up
    }

    mClipWriter = 0;
}

void MXFOPAtomWriter::CreatePackageDefinitions()
{
    mxfUMID umid;
    mxfTimestamp timestamp;
    mxfRational edit_rate;
    size_t i;
    
    if (mPackageDefinitions) {
        free_package_definitions(&mPackageDefinitions);
        mPackageDefinitions = 0;
    }

    if (mPackageGroup->IsPALProject())
        convert_rational(g_palEditRate, edit_rate);
    else
        convert_rational(g_ntscEditRate, edit_rate);

    CHECK(create_package_definitions(&mPackageDefinitions, &edit_rate));

    // map the tape source package
    if (mPackageGroup->GetTapeSourcePackage()) {
        convert_umid(mPackageGroup->GetTapeSourcePackage()->uid, umid);
        convert_timestamp(mPackageGroup->GetTapeSourcePackage()->creationDate, timestamp);
        CHECK(create_tape_source_package(mPackageDefinitions, &umid, mPackageGroup->GetTapeSourcePackage()->name.c_str(),
                                         &timestamp));
    
        for (i = 0; i < mPackageGroup->GetTapeSourcePackage()->tracks.size(); i++) {
            prodauto::Track *track = mPackageGroup->GetTapeSourcePackage()->tracks[i];
    
            ::Track *mxf_track;
            convert_umid(track->sourceClip->sourcePackageUID, umid);
            convert_rational(track->editRate, edit_rate);
            CHECK(create_track(mPackageDefinitions->tapeSourcePackage, track->id,
                               track->number, track->name.c_str(), track->dataDef == PICTURE_DATA_DEFINITION,
                               &edit_rate, &umid, track->sourceClip->sourceTrackID,
                               track->sourceClip->position, track->sourceClip->length, 0,
                               &mxf_track));
        }
    }

    // map the material package
    convert_umid(mPackageGroup->GetMaterialPackage()->uid, umid);
    convert_timestamp(mPackageGroup->GetMaterialPackage()->creationDate, timestamp);
    CHECK(create_material_package(mPackageDefinitions, &umid, mPackageGroup->GetMaterialPackage()->name.c_str(),
                                  &timestamp));

    for (i = 0; i < mPackageGroup->GetMaterialPackage()->tracks.size(); i++) {
        prodauto::Track *track = mPackageGroup->GetMaterialPackage()->tracks[i];

        ::Track *mxf_track;
        convert_umid(track->sourceClip->sourcePackageUID, umid);
        convert_rational(track->editRate, edit_rate);
        CHECK(create_track(mPackageDefinitions->materialPackage, track->id,
                           track->number, track->name.c_str(), track->dataDef == PICTURE_DATA_DEFINITION,
                           &edit_rate, &umid, track->sourceClip->sourceTrackID,
                           track->sourceClip->position, track->sourceClip->length, 0,
                           &mxf_track));
    }

    // map the user comments in the material package
    vector<UserComment> user_comments = mPackageGroup->GetMaterialPackage()->getUserComments();
    for (i = 0; i < user_comments.size(); i++) {
        if (user_comments[i].name.empty())
            continue;

        if (user_comments[i].position < 0) { // static comment
            CHECK(set_user_comment(mPackageDefinitions, user_comments[i].name.c_str(), user_comments[i].value.c_str()));
        } else { // comment with position - a locator with comment
            CHECK(add_locator(mPackageDefinitions, user_comments[i].position, user_comments[i].value.c_str(),
                              convert_comment_color(user_comments[i].colour)));
        }
    }


    // map the file packages
    for (i = 0; i < mPackageGroup->GetFileSourcePackages().size(); i++) {
        SourcePackage *fsp = mPackageGroup->GetFileSourcePackages()[i];

        PA_ASSERT(fsp->tracks.size() == 1);
        prodauto::Track *track = fsp->tracks[0];

        FileEssenceDescriptor *descriptor = dynamic_cast<FileEssenceDescriptor*>(fsp->descriptor);
        PA_ASSERT(descriptor != 0);

        descriptor->fileLocation = mPackageGroup->CreateFileLocation(mCreatingDirectory, descriptor->fileLocation);


        EssenceType essence_type;
        EssenceInfo essence_info;
        init_essence_info(&essence_info);
        if (track->dataDef == PICTURE_DATA_DEFINITION)  {
            convert_rational(descriptor->imageAspectRatio, essence_info.imageAspectRatio);
            
            switch (descriptor->videoResolutionID)
            {
                case UNC_MATERIAL_RESOLUTION:
                    essence_type = UncUYVY;
                    break;
                case DV25_MATERIAL_RESOLUTION:
                    essence_type = IECDV25;
                    break;
                case DV50_MATERIAL_RESOLUTION:
                    essence_type = DVBased50;
                    break;
                case DVCPROHD_MATERIAL_RESOLUTION:
                    essence_type = DV1080i50;
                    break;
                case MJPEG21_MATERIAL_RESOLUTION:
                    essence_info.mjpegResolution = Res21;
                    essence_type = AvidMJPEG;
                    break;
                case MJPEG31_MATERIAL_RESOLUTION:
                    essence_info.mjpegResolution = Res31;
                    essence_type = AvidMJPEG;
                    break;
                case MJPEG101_MATERIAL_RESOLUTION:
                    essence_info.mjpegResolution = Res101;
                    essence_type = AvidMJPEG;
                    break;
                case MJPEG151S_MATERIAL_RESOLUTION:
                    essence_info.mjpegResolution = Res151s;
                    essence_type = AvidMJPEG;
                    break;
                case MJPEG201_MATERIAL_RESOLUTION:
                    essence_info.mjpegResolution = Res201;
                    essence_type = AvidMJPEG;
                    break;
                case MJPEG101M_MATERIAL_RESOLUTION:
                    essence_info.mjpegResolution = Res101m;
                    essence_type = AvidMJPEG;
                    break;
                case IMX30_MATERIAL_RESOLUTION:
                    PA_ASSERT(mPackageGroup->IsPALProject()); // NTSC not yet supported
                    essence_info.imxFrameSize = 150000; // max PAL frame size
                    essence_type = IMX30;
                    break;
                case IMX40_MATERIAL_RESOLUTION:
                    PA_ASSERT(mPackageGroup->IsPALProject()); // NTSC not yet supported
                    essence_info.imxFrameSize = 200000; // max PAL frame size
                    essence_type = IMX40;
                    break;
                case IMX50_MATERIAL_RESOLUTION:
                    PA_ASSERT(mPackageGroup->IsPALProject()); // NTSC not yet supported
                    essence_info.imxFrameSize = 250000; // max PAL frame size
                    essence_type = IMX50;
                    break;
                case DNX36p_MATERIAL_RESOLUTION:
                    essence_type = DNxHD1080p36;
                    break;
                case DNX120p_MATERIAL_RESOLUTION:
                    essence_type = DNxHD1080p120;
                    break;
                case DNX185p_MATERIAL_RESOLUTION:
                    essence_type = DNxHD1080p185;
                    break;
                case DNX120i_MATERIAL_RESOLUTION:
                    essence_type = DNxHD1080i120;
                    break;
                case DNX185i_MATERIAL_RESOLUTION:
                    essence_type = DNxHD1080i185;
                    break;

                default:
                    PA_LOGTHROW(MXFWriterException, ("Unsupported resolution %d", descriptor->videoResolutionID));
                    break;
            }
        } else {
            essence_info.pcmBitsPerSample = descriptor->audioQuantizationBits;
            essence_info.locked = 1;
            essence_type = PCM;
        }

        ::Package *mxf_package;
        convert_umid(fsp->uid, umid);
        convert_timestamp(fsp->creationDate, timestamp);
        CHECK(create_file_source_package(mPackageDefinitions, &umid, fsp->name.c_str(), &timestamp,
                                         descriptor->fileLocation.c_str(), essence_type, &essence_info,
                                         &mxf_package));


        ::Track *mxf_track;
        convert_umid(track->sourceClip->sourcePackageUID, umid);
        convert_rational(track->editRate, edit_rate);
        CHECK(create_track(mxf_package, track->id, 0 /* number is ignored */, track->name.c_str(),
                           track->dataDef == PICTURE_DATA_DEFINITION, &edit_rate, &umid,
                           track->sourceClip->sourceTrackID, track->sourceClip->position,
                           track->sourceClip->length, 0, &mxf_track));
    }
}

int64_t MXFOPAtomWriter::GetDuration(uint32_t mp_track_id)
{
    PA_ASSERT(mClipWriter);
    
    prodauto::Track *track = mPackageGroup->GetFileSourceTrack(mp_track_id);
    
    // num samples is in terms of the file source package track edit rate
    int64_t num_samples;
    CHECK(get_num_samples(mClipWriter, mp_track_id, &num_samples));
        
    return convertPosition(num_samples, track->editRate, mPackageGroup->GetProjectEditRate());
}

void MXFOPAtomWriter::ResetWriter()
{
    if (mClipWriter) {
        Logging::warning("MXF OP-Atom writing was not completed or aborted - aborting without deleting the files\n");
        try
        {
            AbortWriting(false);
        }
        catch (...)
        {
        }
        
        mClipWriter = 0;
    }
    
    if (mPackageDefinitions) {
        free_package_definitions(&mPackageDefinitions);
        mPackageDefinitions = 0;
    }

    if (mPackageGroup) {
        if (mOwnPackageGroup)
            delete mPackageGroup;
        mPackageGroup = 0;
        mOwnPackageGroup = false;
    }
}

