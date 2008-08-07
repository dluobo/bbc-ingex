/*
 * $Id: RecordingDevice.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to manage communication with a recorder.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
 * All Rights Reserved.
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

#include <ace/OS_NS_string.h>

#include <CorbaUtil.h>

#include "RecordingDevice.h"

#include "Timecode.h"

const int pre_roll_frames = 0; //100; // 4s
const int post_roll_frames = 75; // 3s

RecordingDevice::ReturnCode RecordingDevice::ResolveDevice()
{
    ReturnCode result = OK;
    if(mName.empty())
    {
        result = NO_DEVICE;
    }
    mTrackCount = 0;

    CosNaming::Name name;
    name.length(3); 
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("RecordingDevices");
    name[2].id = CORBA::string_dup(mName.c_str());

    CORBA::Object_var obj;
    if(OK == result)
    {
        obj = CorbaUtil::Instance()->ResolveObject(name);
    }

    if(CORBA::is_nil(obj))
    {
        result = NO_DEVICE;
    }

    if(OK == result)
    {
        try
        {
            mRecorder = ProdAuto::Recorder::_narrow(obj);
        }
        catch(const CORBA::Exception &)
        {
            result = NO_DEVICE;
        }
    }

    // Find out how many recording tracks there are
    ProdAuto::TrackList_var track_list;
    if(OK == result)
    {
        try
        {
            track_list = mRecorder->Tracks();
            mTrackCount = track_list->length();
        }
        catch(const CORBA::Exception &)
        {
            result = NO_DEVICE;
        }
    }

    return result;
}

/*
RecordingDevice::ReturnCode RecordingDevice::SetDirectory(const char * subdir)
{
    // Set sub-directory for recordings based on Series/Programme
    bool ok_so_far = true;
    if(ok_so_far && !CORBA::is_nil(mRecorder))
    {
        try
        {
            mRecorder->SubDirectory(subdir);
            mSubDir = subdir;
        }
        catch(const CORBA::Exception &)
        {
            ok_so_far = false;
        }
    }

    return (ok_so_far ? OK : NO_DEVICE);
}
*/


/**
Get remote device to start recording.
For the moment, we enable all tracks.
*/
RecordingDevice::ReturnCode RecordingDevice::StartRecording(const Timecode & tc,
                                                            const std::string & description)
{
    bool ok_so_far = true;
    ReturnCode return_code = OK;
    mHasRecorded = false;

    ProdAuto::MxfTimecode start_tc_mxf;
#if 1
    // Normal operation
    start_tc_mxf.undefined = false;
#else
    // Crash record for testing
    start_tc_mxf.undefined = true;
#endif
    start_tc_mxf.samples = tc.FramesSinceMidnight();

    ProdAuto::MxfDuration pre_roll_mxf;
    pre_roll_mxf.undefined = false;
    pre_roll_mxf.samples = pre_roll_frames;

    if(CORBA::is_nil(mRecorder))
    {
        ok_so_far = false;
        return_code = NO_DEVICE;
    }

    if(ok_so_far)
    {
        // Sort out parameters to be passed
        CORBA::BooleanSeq rec_enable;
        rec_enable.length(mTrackCount);
        //::ProdAuto::StringArray tape_number;
        //::ProdAuto::StringList_var files;
        for(int i = 0; i < mTrackCount; ++i)
        {
            //tape_number[i] = CORBA::string_dup(tape[i]);
            rec_enable[i] = 1;
        }
        ProdAuto::Recorder::ReturnCode result;
        CORBA::StringSeq tapes;

        // Attempt operation on remote recorder
        try
        {
            result = mRecorder->Start(
                start_tc_mxf,
                pre_roll_mxf,
                rec_enable,
                false
                );
            if(ProdAuto::Recorder::SUCCESS == result)
            {
                return_code = RecordingDevice::OK;
            }
            else
            {
                return_code = RecordingDevice::FAILED;
            }
        }
        catch (const CORBA::Exception &)
        {
            result = ProdAuto::Recorder::FAILURE;
            return_code = RecordingDevice::NO_DEVICE;
        }


        if(ProdAuto::Recorder::SUCCESS == result)
        {
            //start_tc -= pre_roll_dur;
            //mInTime = start_tc.Text(); // store the start timecode
            //mDuration = "";
            //for(CORBA::ULong i = 0; i < files->length(); ++i)
            //{
            //  mFilename[i] = (const char *) files[i];
            //}
            mIsRecording = true;
        }
        else
        {
            ok_so_far = false;
        }
    }

    return return_code;
}


RecordingDevice::ReturnCode RecordingDevice::StopRecording(const Timecode & tc)
{
    bool ok_so_far = true;
    ReturnCode return_code = OK;

    if(CORBA::is_nil(mRecorder))
    {
        ok_so_far = false;
        return_code = NO_DEVICE;
    }

    if (ok_so_far)
    {
        // Sort out parameters to be passed
        ProdAuto::MxfTimecode stop_tc_mxf;
        stop_tc_mxf.undefined = false;
        stop_tc_mxf.samples = tc.FramesSinceMidnight();

        //::Duration post_roll_dur;
        //post_roll_dur.TotalFrames(post_roll_frames);
        ProdAuto::MxfDuration post_roll_mxf;
        post_roll_mxf.undefined = false;
        post_roll_mxf.samples = post_roll_frames;


        ::ProdAuto::Recorder::ReturnCode result;
        CORBA::StringSeq_var files;
        ::ProdAuto::LocatorSeq locators;
        try
        {
            result = mRecorder->Stop(stop_tc_mxf, post_roll_mxf, "", "", locators, files.out());
        }
        catch (const CORBA::Exception &)
        {
            result = ProdAuto::Recorder::FAILURE;
        }

        if(ProdAuto::Recorder::SUCCESS == result)
        {
            //stop_tc += post_roll_dur;
            //mOutTime = stop_tc.Text(); // store the stop timecode

            //::Duration dur_tc;
            //dur_tc = Timecode(mOutTime.c_str()) - Timecode(mInTime.c_str());
            //mDuration = dur_tc.Text(); // store the duration

            // need to store paths
            //for(int i = 0; i < SDI_RECORDER_CHANNELS; ++i)
            //{
                //mFilename[i] = path.in()[i].in();
            //}

            mIsRecording = false;
            mHasRecorded = true;
        }
        else
        {
            ok_so_far = false;
            return_code = FAILED;
        }
    }

    return return_code;
}

void RecordingDevice::GetVideoTracks(std::vector<std::string> & tracks)
{
    tracks.clear();
    bool ok_so_far = true;
    ProdAuto::TrackList_var idl_tracks;
    try
    {
        idl_tracks = mRecorder->Tracks();
    }
    catch (const CORBA::Exception &)
    {
        ok_so_far = false;
    }
    if(ok_so_far)
    {
        for(CORBA::ULong i = 0; i < idl_tracks->length(); ++i)
        {
            ProdAuto::Track & idl_track = idl_tracks[i];
            if(idl_track.type == ProdAuto::VIDEO)
            {
                std::string name;
                name += idl_track.src.package_name;
                name += " - ";
                name += idl_track.src.track_name;
                tracks.push_back(name);
            }
        }
    }
}
