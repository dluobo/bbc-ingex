/*
 * $Id: IngexRecorderImpl.cpp,v 1.5 2008/09/03 14:09:05 john_f Exp $
 *
 * Servant class for Recorder.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
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

#include <ace/OS_NS_time.h>
#include <ace/OS_NS_unistd.h>

#include <sstream>

#include <Database.h>
#include <DBException.h>

#include "IngexShm.h"
#include "RecorderSettings.h"
#include "IngexRecorderImpl.h"
#include "FileUtils.h"
#include "Timecode.h"
#include "DateTime.h"

IngexRecorderImpl * IngexRecorderImpl::mInstance = 0;

// To be called on completion of recording
void recording_completed(IngexRecorder * rec)
{
#if 0
    // Test - keep recording in chunks
    const framecount_t chunk_size = 60 * 25; // 1 min
    framecount_t restart = rec->OutTime();
    // make new out time a round figure
    framecount_t out = restart + chunk_size;
    out = (out / chunk_size) * chunk_size;
    // set duration for this chunk
    rec->TargetDuration(out - restart);
    rec->Start();
#else
    // notify observers
    IngexRecorderImpl::Instance()->NotifyCompletion(rec);

    // destroy recorder
    delete rec;
#endif
}

// Implementation skeleton constructor
// Note that CopyManager mode is set here.
IngexRecorderImpl::IngexRecorderImpl (void)
: mRecording(false), mRecordingIndex(1), mCopyManager(CopyMode::NEW)
{
    mInstance = this;
}

// Initialise the recorder
bool IngexRecorderImpl::Init(std::string name, std::string db_user, std::string db_pw)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Init(%C, %C, %C)\n"),
        name.c_str(), db_user.c_str(), db_pw.c_str()));

    // Shared memory initialisation
    bool result = IngexShm::Instance()->Init();
    if (result == false)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Shared Memory init failed!\n")));
    }

    // Base class initialisation
    // Each channel has 1 video and 4 or 8 audio tracks
    const unsigned int max_inputs = IngexShm::Instance()->Channels();
    const unsigned int max_tracks_per_input = 1 + IngexShm::Instance()->AudioTracksPerChannel();
    RecorderImpl::Init(name, db_user, db_pw, max_inputs, max_tracks_per_input);


    // Store video source names in shared memory
    UpdateShmSourceNames();

    // Setup pre-roll
    mMaxPreRoll.undefined = false;
    mMaxPreRoll.edit_rate = EDIT_RATE;
    if (IngexShm::Instance()->RingLength() > (SEARCH_GUARD + 1))
    {
        mMaxPreRoll.samples = IngexShm::Instance()->RingLength() - (SEARCH_GUARD + 1);
    }
    else
    {
        mMaxPreRoll.samples = 0;
    }


    // Setup post-roll
    mMaxPostRoll.undefined = true; // no limit to post-roll
    mMaxPostRoll.edit_rate = EDIT_RATE;
    mMaxPostRoll.samples = 0;

    RecorderSettings * settings = RecorderSettings::Instance();
    settings->Update(mRecorder.get());

    mCopyManager.RecorderName(name);

    // Register this Recorder in the monitoring area of shared memory
    // and initialise encoding enabled/disabled flags for all channels and encodings
    IngexShm::Instance()->InfoSetup(mRecorder->name);
    for (unsigned int channel_i = 0; channel_i < IngexShm::Instance()->Channels(); ++channel_i)
    {
        int enc_idx = 0;
        for (std::vector<EncodeParams>::const_iterator it = settings->encodings.begin(); it != settings->encodings.end(); ++it, ++enc_idx)
        {
            bool quad_video = it->source == Input::QUAD;
            IngexShm::Instance()->InfoSetEnabled(channel_i, enc_idx, quad_video, true);
            IngexShm::Instance()->InfoSetDesc(channel_i, enc_idx, quad_video,
                "%s%s %s",
                DatabaseEnums::Instance()->ResolutionName(it->resolution).c_str(),
                //(settings->ResolutionName(it->resolution)),
                quad_video ? "(quad)" : "",
                it->file_format == MXF_FILE_FORMAT_TYPE ? "MXF" : "OTHER"
                );

        }
    }


    // Set timecode mode
    switch (settings->timecode_mode)
    {
    case LTC_PARAMETER_VALUE:
        IngexShm::Instance()->TcMode(IngexShm::LTC);
        break;
    case VITC_PARAMETER_VALUE:
        IngexShm::Instance()->TcMode(IngexShm::VITC);
        break;
    default:
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Unexpected timecode mode\n")));
        break;
    }

    // Start copy process
    this->StartCopying(0);

    char buf[256];
    if (ACE_OS::hostname(buf, 256) == 0)
    {
        mHostname = buf;
    }

    return result;
}

// Implementation skeleton destructor
IngexRecorderImpl::~IngexRecorderImpl (void)
{
    IngexShm::Destroy();
    prodauto::Database::close();
}

char * IngexRecorderImpl::RecordingFormat (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return CORBA::string_dup("Ingex recorder");
}

::ProdAuto::TrackStatusList * IngexRecorderImpl::TracksStatus (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // Update member with current timecode and status
    for (unsigned int channel_i = 0; channel_i < IngexShm::Instance()->Channels(); ++channel_i)
    {
        bool signal = IngexShm::Instance()->SignalPresent(channel_i);
        framecount_t timecode = IngexShm::Instance()->CurrentTimecode(channel_i);

        // We know each input has mMaxTracksPerInput, see comments
        // in RecorderImpl::UpdateSources()
        for (unsigned int j = 0; j < mMaxTracksPerInput; ++j)
        {
            CORBA::ULong track_i = mMaxTracksPerInput * channel_i + j;
            if (track_i < mTracksStatus->length())
            {
                ProdAuto::TrackStatus & ts = mTracksStatus->operator[](track_i);

                ts.signal_present = signal;
                if (timecode >= 0)
                {
                    ts.timecode.undefined = 0;
                    ts.timecode.samples = timecode;
                }
                else
                {
                    ts.timecode.undefined = 1;
                    ts.timecode.samples = 0;
                }
                //ACE_DEBUG((LM_INFO, ACE_TEXT("Track %d: %C\n"), track_i, (ts.rec ? "recording" : "not recording")));
            }
        }
    }

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Start (
    ::ProdAuto::MxfTimecode & start_timecode,
    const ::ProdAuto::MxfDuration & pre_roll,
    const ::CORBA::BooleanSeq & rec_enable,
    const char * project,
    ::CORBA::Boolean test_only
  )
  throw (
    ::CORBA::SystemException
  )
{
    Timecode start_tc(start_timecode.undefined ? 0 : start_timecode.samples);
    framecount_t pre = (pre_roll.undefined ? 0 : pre_roll.samples);
    ACE_DEBUG((LM_INFO, ACE_TEXT("IngexRecorderImpl::Start(), tc %C, pre-roll %d, time %C\n"),
        start_tc.Text(), pre, DateTime::Timecode().c_str()));

    // Enforce start-stop-start sequence
    if (mRecording)
    {
        // Immediate stop
        DoStop(-1, 0);
    }

    // Clear previous monitoring data for enabled channels
    IngexShm::Instance()->InfoResetChannels();

    // Stop any copy process to reduce disc activity.
    mCopyManager.StopCopying(mRecordingIndex);

    // Make sure we are up-to-date with source config and
    // settings from database.
    // Also happens when controller requests tracks.
    // By updating again here, the controller may get
    // out of sync if track config changes.  However, the
    // alternative (not updating here) would mean you rely
    // on controller disconnecting and reconnecting to
    // force an update.
    UpdateFromDatabase();

    // Translate enables to per-track and per-channel.
    std::vector<bool> channel_enables;
    std::vector<bool> track_enables;
    for (unsigned int channel_i = 0; channel_i < mMaxInputs; ++channel_i)
    {
        // Start with channel enables false.
        bool ce = false;

        // Go through tracks
        for (unsigned int j = 0; j < mMaxTracksPerInput; ++j)
        {
            CORBA::ULong track_i = channel_i * mMaxTracksPerInput + j;

            bool te = false;
            if (track_i < mTracks->length() && mTracks[track_i].has_source)
            {
                te = rec_enable[track_i];
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %2d %C\n"),
                    track_i, te ? "enabled" : "not enabled"));
            }
            else
            {
                // If no source connected, track is left disabled.
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %2d not connected\n"),
                    track_i));
            }
            track_enables.push_back(te);

            if (te)
            {
                // If one track enabled, enable channel.
                ce = true;
            }
        }

        // Set channel enable
        channel_enables.push_back(ce);
    }

    // Make a new IngexRecorder to manage this recording.
    mpIngexRecorder = new IngexRecorder(this, mRecordingIndex++);

    // Set-up callback for completion of recording
    mpIngexRecorder->SetCompletionCallback(&recording_completed);

    // Determine start timecode or crash-record
    framecount_t start;
    bool crash;
    if (start_timecode.undefined)
    {
        start = 0;
        crash = true;
    }
    else
    {
        start = start_timecode.samples;
        crash = false;
    }

    // Check for start timecode.
    // This may modify the channel_enable array.
    bool ok = mpIngexRecorder->CheckStartTimecode(
        channel_enables,
        start,
        pre_roll.undefined ? 0 : pre_roll.samples,
        crash);

    // Set return value for actual start timecode
    start_timecode.undefined = false;
    start_timecode.edit_rate = EDIT_RATE;
    start_timecode.samples = start;

    // Setup IngexRecorder
    mpIngexRecorder->Setup(
        start,
        channel_enables,
        track_enables,
        project);

    // Start
    if (!test_only)
    {
        if (ok)
        {
            ok = mpIngexRecorder->Start();
            mRecording = true;
        }

        // Set status info
        if (ok)
        {
            mStatusDist.SendStatus("event", "recording starting");

            for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
            {
                ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
                ts.rec = rec_enable[i];
            }
        }
    }

    if (!ok || test_only)
    {
        delete mpIngexRecorder;
        mpIngexRecorder = 0;
    }

    // Return
    return (ok ? ProdAuto::Recorder::SUCCESS : ProdAuto::Recorder::FAILURE);
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Stop (
    ::ProdAuto::MxfTimecode & mxf_stop_timecode,
    const ::ProdAuto::MxfDuration & mxf_post_roll,
    const char * description,
    const ::ProdAuto::LocatorSeq & locators,
    ::CORBA::StringSeq_out files
  )
  throw (
    ::CORBA::SystemException
  )
{
    Timecode stop_tc(mxf_stop_timecode.undefined ? 0 : mxf_stop_timecode.samples);
    framecount_t post = (mxf_post_roll.undefined ? 0 : mxf_post_roll.samples);
    ACE_DEBUG((LM_INFO, ACE_TEXT("IngexRecorderImpl::Stop(), tc %C, post-roll %d, time %C\n"),
        stop_tc.Text(), post, DateTime::Timecode().c_str()));

    // Translate parameters.
    framecount_t stop_timecode = (mxf_stop_timecode.undefined ? -1 : mxf_stop_timecode.samples);

    framecount_t post_roll = (mxf_post_roll.undefined ? 0 : mxf_post_roll.samples);

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Stop(%d, %d)\n"),
    //    stop_timecode, post_roll));

    // Create out parameter
    files = new ::CORBA::StringSeq;
    files->length(mTracks->length());

    // Translate the locators
    std::vector<Locator> locs;
    for (CORBA::ULong i = 0; i < locators.length(); ++i)
    {
        Locator lo;
        lo.comment = locators[i].comment;
        lo.colour = TranslateLocatorColour(locators[i].colour);
        lo.timecode = locators[i].timecode.samples;
        locs.push_back(lo);
    }

    // Tell recorder when to stop.
    if (mpIngexRecorder)
    {
        mpIngexRecorder->Stop(stop_timecode, post_roll,
            description,
            locs);

        // Return the expected "out time"
        mxf_stop_timecode.undefined = false;
        mxf_stop_timecode.edit_rate = EDIT_RATE;
        mxf_stop_timecode.samples = stop_timecode;

        // Return the filenames
        for (CORBA::ULong track_i = 0; track_i < mTracks->length(); ++track_i)
        {
            std::string name = mpIngexRecorder->mFileNames[track_i];
            if (!name.empty())
            {
                name = "/" + mHostname + name;
            }
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %d \"%C\"\n"), track_i, name.c_str()));
            files->operator[](track_i) = name.c_str();
        }
    }
    mpIngexRecorder = 0;  // It will be deleted when it signals completion

    // Update status to "not recording".
    mRecording = false;
    for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
    }

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

/**
Simplified Stop without returning filenames etc.
*/
void IngexRecorderImpl::DoStop(framecount_t timecode, framecount_t post_roll)
{
    // Tell recorder when to stop.
    std::vector<Locator> locs;
    if (mpIngexRecorder)
    {
        mpIngexRecorder->Stop(timecode, post_roll, "recording halted", locs);
    }
    mpIngexRecorder = 0;  // It will be deleted when it signals completion

    // Update status to "not recording".
    mRecording = false;
    for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
    }
}

void IngexRecorderImpl::NotifyCompletion(IngexRecorder * rec)
{
    ACE_DEBUG((LM_INFO, "IngexRecorderImpl::NotifyCompletion()\n"));

    if (!rec->mRecordingOK)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Recording failed!\n")));
        mStatusDist.SendStatus("error", "recording failed");
    }
    else if (rec->DroppedFrames())
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Recording dropped frames!\n")));
        mStatusDist.SendStatus("error", "recording dropped frames");
    }
    else
    {
        mStatusDist.SendStatus("event", "recording completed");
    }

    this->StartCopying(rec->mIndex);
}

void IngexRecorderImpl::StartCopying(unsigned int index)
{
    mCopyManager.Command(RecorderSettings::Instance()->copy_command);
    mCopyManager.ClearSrcDest();
    std::vector<EncodeParams> & encodings = RecorderSettings::Instance()->encodings;
    for (std::vector<EncodeParams>::const_iterator it = encodings.begin(); it != encodings.end(); ++it)
    {
        mCopyManager.AddSrcDest(it->dir, it->dest);
    }

    mCopyManager.StartCopying(index);
}

void IngexRecorderImpl::UpdateShmSourceNames()
{
    try
    {
        prodauto::RecorderConfig * rc = 0;
        if (mRecorder.get() && mRecorder->hasConfig())
        {
            rc = mRecorder->getConfig();
        }
        if (rc)
        {
            const unsigned int n_inputs = ACE_MIN((unsigned int)rc->recorderInputConfigs.size(), mMaxInputs);

            for (unsigned int i = 0; i < n_inputs; ++i)
            {
                prodauto::RecorderInputConfig * ric = rc->getInputConfig(i + 1);

                prodauto::RecorderInputTrackConfig * ritc = 0;
                if (ric)
                {
                    ritc = ric->trackConfigs[0]; // video track
                }
                prodauto::SourceConfig * sc = 0;
                if (ritc)
                {
                    sc = ritc->sourceConfig;
                }
                if (sc)
                {
                    // Store name in shared memory
                    //IngexShm::Instance()->SourceName(i, sc->getSourcePackage()->name);
                    IngexShm::Instance()->SourceName(i, sc->name);
                }
            } // for
        }
    } // try
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }
}

int IngexRecorderImpl::TranslateLocatorColour(ProdAuto::LocatorColour::EnumType e)
{
    // database values are defined in DatabaseEnums.h

    int i;
    switch (e)
    {
    case ProdAuto::LocatorColour::WHITE:
        i = USER_COMMENT_WHITE_COLOUR;
        break;
    case ProdAuto::LocatorColour::RED:
        i = USER_COMMENT_RED_COLOUR;
        break;
    case ProdAuto::LocatorColour::YELLOW:
        i = USER_COMMENT_YELLOW_COLOUR;
        break;
    case ProdAuto::LocatorColour::GREEN:
        i = USER_COMMENT_GREEN_COLOUR;
        break;
    case ProdAuto::LocatorColour::CYAN:
        i = USER_COMMENT_CYAN_COLOUR;
        break;
    case ProdAuto::LocatorColour::BLUE:
        i = USER_COMMENT_BLUE_COLOUR;
        break;
    case ProdAuto::LocatorColour::MAGENTA:
        i = USER_COMMENT_MAGENTA_COLOUR;
        break;
    case ProdAuto::LocatorColour::BLACK:
        i = USER_COMMENT_BLACK_COLOUR;
        break;
    default:
        i = USER_COMMENT_RED_COLOUR;
        break;
    }

    return i;
}


