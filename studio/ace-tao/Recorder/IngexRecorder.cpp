/*
 * $Id: IngexRecorder.cpp,v 1.6 2008/09/04 15:38:44 john_f Exp $
 *
 * Class to manage an individual recording.
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


#include <iostream>
#include <cstdio>
#include <sstream>

#include <ace/OS_NS_unistd.h>
#include <ace/Time_Value.h>
#include <ace/Thread.h>
#include <ace/OS_NS_sys_shm.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_time.h>
#include <ace/OS_NS_string.h>

#include "IngexRecorder.h"
#include "RecorderSettings.h"
#include "Timecode.h"
#include "DateTime.h"
#include "FileUtils.h"

#include <DBException.h>

#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))


/**
Constructor clears all member data.
The name parameter is used when reading config from database.
*/
IngexRecorder::IngexRecorder(RecorderImpl * impl, unsigned int index)
: mpCompletionCallback(0), mpImpl(impl), mTargetDuration(0),
  mRecordingOK(true), mIndex(index), mDroppedFrames(false)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::IngexRecorder()\n")));

    if (mpImpl)
    {
        mFps = mpImpl->Fps();
        mDf = mpImpl->Df();
    }
}

/**
Destructor
*/
IngexRecorder::~IngexRecorder()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::~IngexRecorder()\n")));

    for (std::vector<ThreadParam>::iterator
        it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
    {
        delete it->p_opt;
    }
}

void IngexRecorder::Setup(
                framecount_t start_timecode,
                const std::vector<bool> & channel_enables,
                const std::vector<bool> & track_enables,
                const char * project)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Setup()\n")));

    // Get ProjectName associated with supplied name.
    prodauto::ProjectName project_name;
    GetProjectFromDb(project, project_name);

    // Store project name and recording description
    mProjectName = project_name;

    // Get current recorder settings
    RecorderSettings * settings = RecorderSettings::Instance();
    settings->Update(mpImpl->Recorder());

    switch (settings->timecode_mode)
    {
    case LTC_PARAMETER_VALUE:
        IngexShm::Instance()->TcMode(IngexShm::LTC);
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("LTC mode\n")));
        break;
    case VITC_PARAMETER_VALUE:
        IngexShm::Instance()->TcMode(IngexShm::VITC);
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("VITC mode\n")));
        break;
    default:
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Unexpected timecode mode\n")));
        break;
    }

    //unsigned int n_channels = IngexShm::Instance()->Channels();
    // Tracks per channel
    // e.g. 5 (1 video, 4 audio) or 9 (1 video, 8 audio)
    // A bit dodgy as different inputs on a recorder can have
    // different numbers of tracks.
    mTracksPerChannel = track_enables.size() / channel_enables.size();

    // Set up enables
    mChannelEnable = channel_enables;
    mTrackEnable = track_enables;
    int first_enabled_channel = -1;
    for (unsigned int i = 0; first_enabled_channel < 0 && i < mChannelEnable.size(); ++i)
    {
        if (mChannelEnable[i])
        {
            first_enabled_channel = i;
        }
    }

    // Create any needed paths.
    // NB. Paths need to be same as those used in recorder_fucntions.cpp
    for (std::vector<EncodeParams>::const_iterator it = settings->encodings.begin();
        it != settings->encodings.end(); ++it)
    {
        FileUtils::CreatePath(it->dir);
        // For MXF we also use "creating" and "failures" directories
        if (it->file_format == MXF_FILE_FORMAT_TYPE)
        {
            std::ostringstream creating_path;
            creating_path << it->dir << '/' << settings->mxf_subdir_creating;
            FileUtils::CreatePath(creating_path.str());
            std::ostringstream failures_path;
            failures_path << it->dir << '/' << settings->mxf_subdir_failures;
            FileUtils::CreatePath(failures_path.str());
        }
    }

    // Set up encoding threads
    int encoding_i = 0;
    for (std::vector<EncodeParams>::const_iterator it = settings->encodings.begin();
        it != settings->encodings.end(); ++it, ++encoding_i)
    {
        if (it->source == Input::NORMAL)
        {
            for (unsigned int i = 0; i < mChannelEnable.size(); i++)
            {
                if (mChannelEnable[i])
                {
                    ThreadParam tp;
                    tp.p_rec = this;

                    tp.p_opt = new RecordOptions;
                    tp.p_opt->channel_num = i;
                    tp.p_opt->index = encoding_i;
                    
                    tp.p_opt->resolution = it->resolution;
                    tp.p_opt->file_format = it->file_format;
                    tp.p_opt->bitc = it->bitc;
                    tp.p_opt->dir = it->dir;

                    mThreadParams.push_back(tp);
                }
            }
        }
        else if (it->source == Input::QUAD)
        {
            ThreadParam tp;
            tp.p_rec = this;

            tp.p_opt = new RecordOptions;
            tp.p_opt->channel_num = first_enabled_channel;
            tp.p_opt->index = 0;
            tp.p_opt->quad = true;
            tp.p_opt->resolution = it->resolution;
            tp.p_opt->file_format = it->file_format;
            tp.p_opt->bitc = it->bitc;
            tp.p_opt->dir = it->dir;

            mThreadParams.push_back(tp);
        }
    }
#if 0
// For test only, add a further encoding
    {
        ThreadParam tp;
        tp.p_rec = this;

        tp.p_opt = new RecordOptions;
        tp.p_opt->channel_num = 0;
        tp.p_opt->index = 2;
        
        tp.p_opt->resolution = 8;
        tp.p_opt->file_format = MXF_FILE_FORMAT_TYPE;
        tp.p_opt->bitc = true;
        tp.p_opt->dir = "/video/mxf_offline";

        mThreadParams.push_back(tp);
    }
#endif

    // Create and store filenames based on source, target timecode and date.
    ::Timecode tc(start_timecode, mFps, mDf);
    std::string date = DateTime::DateNoSeparators();
    const char * tcode = tc.TextNoSeparators();

    // We also include project name to help with copying to project-based
    // directories on a file-server.
    // NB. need to remove unsuitable characters such as space or ampersand)

    // Set filename stems in RecordOptions.
    for (std::vector<ThreadParam>::iterator
        it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
    {
        const char * src_name = (it->p_opt->quad ? QUAD_NAME : SOURCE_NAME[it->p_opt->channel_num]);
        std::ostringstream ident;
        ident << date << "_" << tcode
            << "_" << project
            << "_" << mpImpl->Name()
            << "_" << src_name
            << "_" << it->p_opt->index;
        it->p_opt->file_ident = ident.str();
    }

}


/**
Prepare for a recording.  Search for target timecode and set some of the RecordOptions
*/
bool IngexRecorder::CheckStartTimecode(
                std::vector<bool> & channel_enables,
                framecount_t & start_timecode,
                framecount_t pre_roll,
                bool crash_record)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::CheckStartTimecode()\n")));

    unsigned int n_channels = IngexShm::Instance()->Channels();

    framecount_t target_tc;

    // If crash record, search across all channels for the minimum current (lastframe) timecode
    if (crash_record)
    {
        //int tc[MAX_CHANNELS];
        struct { int framecount; bool valid; } tc[MAX_CHANNELS];
        for (unsigned int i = 0; i < MAX_CHANNELS; ++i)
        {
            tc[i].valid = false;
        }

        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Crash record:\n")));
        for (unsigned int channel_i = 0; channel_i < channel_enables.size(); channel_i++)
        {
            if (channel_enables[channel_i] && IngexShm::Instance()->SignalPresent(channel_i))
            {
                tc[channel_i].framecount = IngexShm::Instance()->CurrentTimecode(channel_i);
                tc[channel_i].valid = true;

                ACE_DEBUG((LM_DEBUG, ACE_TEXT("    tc[%d]=%C\n"),
                    channel_i, Timecode(tc[channel_i].framecount, mFps, mDf).Text()));
            }
        }

        int max_tc = 0;
        int min_tc = INT_MAX;
        bool tc_valid = false;
        for (unsigned int channel_i = 0; channel_i < n_channels; channel_i++)
        {
            if (tc[channel_i].valid)
            {
                max_tc = max(max_tc, tc[channel_i].framecount);
                min_tc = min(min_tc, tc[channel_i].framecount);
                tc_valid = true;
            }
        }

        if (!tc_valid)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("    No channels enabled!\n")));
        }

        target_tc = min_tc;
        //target_tc = tc[1]; // just for testing
    }
    else
    {
        target_tc = start_timecode;
    }

    // Include pre-roll
    if (target_tc < pre_roll)
    {
        target_tc += 24 * 60 * 60 * 25;
    }
    target_tc -= pre_roll;

    // NB. Should be keeping target_tc and pre-roll separate in case of discontinuous timecode.
    // i.e. find target_tc and then step back by pre-roll.

    // Start timecode for record threads.
    // Also used for setting stop duration (not the ideal way to do it).
    mStartTimecode = target_tc;

    // Passing back the actual start_timecode.
    start_timecode = target_tc;

    // local vars
    bool found_all_target = true;
    unsigned int search_limit;
    const int ring_length = IngexShm::Instance()->RingLength();
    if (ring_length > SEARCH_GUARD)
    {
        search_limit = ring_length - SEARCH_GUARD;
    }
    else
    {
        search_limit = 1;
    }

    // Search for desired timecode across all target sources
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Searching for timecode %C\n"), Timecode(target_tc, mFps, mDf).Text()));
    for (unsigned int channel_i = 0; channel_i < n_channels; channel_i++)
    {
        if (! channel_enables[channel_i])
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Channel %d not enabled\n"), channel_i));
            continue;
        }

        int lastframe = IngexShm::Instance()->LastFrame(channel_i);

        bool found_target = false;
        int first_tc_seen = 0;
        int last_tc_seen = 0;

        // Find target timecode.
        for (unsigned int i = 0; !found_target && i < search_limit; ++i)
        {
            // read timecode value
			int frame = lastframe - i;
			if (frame < 0)
			{
				frame += ring_length;
            }
            int tc = IngexShm::Instance()->Timecode(channel_i, frame);

            if (i == 0)
            {
                first_tc_seen = tc;
            }
            last_tc_seen = tc;

            if (tc == target_tc)
            {
                mStartFrame[channel_i] = lastframe - i;
                found_target = true;

                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Found channel%d lf=%6d lf-i=%8d tc=%C\n"),
                    channel_i, lastframe, lastframe - i, Timecode(tc, mFps, mDf).Text()));
            }
            else if (i == 0 && target_tc > tc && target_tc - tc < 5)
            {
                // Target is slightly in the future.  We predict the start frame.
                mStartFrame[channel_i] = frame + target_tc - tc;
                found_target = true;

                ACE_DEBUG((LM_WARNING, ACE_TEXT("Target timecode in future for channel[%d]: target=%C, most_recent=%C\n"),
                    channel_i,
                    Timecode(target_tc, mFps, mDf).Text(),
                    Timecode(first_tc_seen, mFps, mDf).Text()
                    ));
            }
        }

        if (! found_target)
        {
            ACE_DEBUG((LM_ERROR, "channel[%d] Target tc %C not found, buffer %C - %C\n",
                channel_i,
                Timecode(target_tc, mFps, mDf).Text(),
                Timecode(last_tc_seen, mFps, mDf).Text(),
                Timecode(first_tc_seen, mFps, mDf).Text()
            ));

            if (IngexShm::Instance()->SignalPresent(channel_i))
            {
                found_all_target = false;
            }
            else
            {
                // As there is no signal at this input we won't
                // prevent recording from starting but will
                // simply disable this input.
                ACE_DEBUG((LM_ERROR,
                    ACE_TEXT("This channel has no signal present so will be disabled for the current recording.\n")
                    ));
                channel_enables[channel_i] = false;
            }
        }
    } // for channel_i


    // Return
    if (!found_all_target)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not start record - not all target timecodes found\n")));
        return false;
    }
    else
    {
        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::PrepareStart() returning true\n")));
        return true;
    }
}

bool IngexRecorder::Start()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Start()\n")));

    // Now start all record threads, including quad
    bool result = false;
    for (std::vector<ThreadParam>::iterator
        it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
    {
        // Select which encoding function to invoke
        //int channelnum = it->p_opt->channel_num;
        //ACE_THR_FUNC fn = (channelnum == QUAD_SOURCE ? start_quad_thread : start_record_thread);
        ACE_THR_FUNC fn = start_record_thread;

        // and start in new thread.
        ACE_thread_t thr;
        int err = ACE_Thread::spawn( fn, &(*it),
                      THR_NEW_LWP|THR_JOINABLE, &thr );
        const char * src_name = (it->p_opt->quad ? QUAD_NAME : SOURCE_NAME[it->p_opt->channel_num]);
        int index = it->p_opt->index;
        if (err == 0)
        {
            it->id = thr;
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Created %C record thread %d id %x\n"),
                src_name, index, thr));
            // If at least one thread starts, we return true as the thread will
            // trigger deletion of this IngexRecorder when it exits.
            result = true;
        }
        else
        {
            it->id = 0;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to create %C record thread %d\n"),
                src_name, index));
        }
    }


    // Create a thread to manage termination of recording threads
    if (result)
    {
        int err = ACE_Thread::spawn( manage_record_thread, this,
                                THR_NEW_LWP|THR_DETACHED, &mManageThreadId );
        if (err != 0)
        {
            ACE_DEBUG((LM_ERROR,
                ACE_TEXT("Failed to create termination managing thread\n")));
        }
    }

    return result;
}


bool IngexRecorder::Stop( framecount_t & stop_timecode,
                framecount_t post_roll,
                const char * description,
                const std::vector<Locator> & locs)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Stop(%C, %d)\n"),
        Timecode(stop_timecode, mFps, mDf).Text(), post_roll));

    mUserComments.clear();
    mUserComments.push_back(
        prodauto::UserComment(AVID_UC_DESCRIPTION_NAME, description, STATIC_COMMENT_POSITION, 0));

    // Store locators
    for (std::vector<Locator>::const_iterator it = locs.begin(); it != locs.end(); ++it)
    {
        int64_t position = it->timecode - mStartTimecode;
        mUserComments.push_back(
            prodauto::UserComment(POSITIONED_COMMENT_NAME, it->comment, position, it->colour));
    }

    // Really want to pass stop_timecode and post_roll to the record threads
    // or do some kind of "prepare_stop"
    // but for now will set an absolute duration.

    //post_roll = 0;  //tmp test

    framecount_t capture_length = 0;
    if (stop_timecode < 0)
    {
        // Stop timecode is "now".
        // Find longest current recorded duration and add post_roll.

        for (std::vector<ThreadParam>::iterator
            it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
        {
            framecount_t frames_written = it->p_opt->FramesWritten();
            framecount_t frames_dropped = it->p_opt->FramesDropped();
            framecount_t frames_total = frames_written + frames_dropped;

            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C frames total = %d (written = %d, dropped = %d)\n"),
                SOURCE_NAME[it->p_opt->channel_num], frames_total, frames_written, frames_dropped));

            if (capture_length < frames_total)
            {
                capture_length = frames_total;
            }
        }
        capture_length += post_roll;
    }
    else
    {
        // Calculate capture length.
        capture_length = stop_timecode - mStartTimecode + post_roll;
    }

    // Return the expected "out time" of the recording
    stop_timecode = mStartTimecode + capture_length;

    // guard against crazy capture lengths
    if (capture_length <= 0)
    {
        // force immediate-ish finish of recording
        // true duration will be updated from frames_written variable
        ACE_DEBUG((LM_ERROR, ACE_TEXT("recorder_stop: crazy duration of %d passed - stopping recording immediately by\n"
                "              signalling duration of 1.\n"), capture_length));
        capture_length = 1;
    }

    // Now stop record by signalling the duration in frames
    TargetDuration(capture_length);
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Target duration set to %d\n"), capture_length));


    return true;
}

framecount_t IngexRecorder::OutTime()
{
    framecount_t d = TargetDuration();
    if (d > 0)
    {
        return mStartTimecode + d;
    }
    else
    {
        return d;
    }
}


#if 0
/**
Write a metadata file alongside the media files.
This is maintained for compatibility with an earlier version of the recorder
but is not ideal as this file can become inconsistent with the "true" metadata.
Can perhaps be used to determine when the recorded files are complete.
*/
bool IngexRecorder::WriteMetadataFile(const char * meta_name)
{
    FILE *fp_meta = NULL;
    char tmp_meta_name[FILENAME_MAX];
    strcpy(tmp_meta_name, meta_name);
    strcat(tmp_meta_name, ".tmp");

    if ((fp_meta = fopen(tmp_meta_name, "wb")) == NULL)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open for write: %C %p\n"), tmp_meta_name, ACE_TEXT("fopen")));
        return false;
    }
    fprintf(fp_meta, "path=%s\n", meta_name);
    //fprintf(fp_meta, "a_tape=");
    //for (int i = 0; i < mChannels; i++)
    //{
    //  fprintf(fp_meta, "%s%s", record_opt[i].tapename.c_str(), i == MAX_CHANNELS - 1 ? "\n" : ",");
    //}
    fprintf(fp_meta, "enabled=");
    for (int i = 0; i < MAX_RECORD; i++)
    {
        fprintf(fp_meta, "%d%s", record_opt[i].enabled, i == MAX_RECORD - 1 ? "\n" : ",");
    }
    fprintf(fp_meta, "start_tc=%d\n", mStartTimecode);
    fprintf(fp_meta, "start_tc_str=%s\n", Timecode(mStartTimecode, mFps, mDf).Text());
    fprintf(fp_meta, "capture_length=%d\n", record_opt[0].FramesWritten());
#if 0
    fprintf(fp_meta, "a_start_tc=");
    for (int i = 0; i < mChannels + 1; i++)
    {
        fprintf(fp_meta, "%d%s", record_opt[i].start_tc, i == mChannels ? "\n" : " ");
    }
    fprintf(fp_meta, "a_start_tc_str=");
    for (int i = 0; i < mChannels + 1; i++)
    {
        fprintf(fp_meta, "%s%s", framesToStr(record_opt[i].start_tc, tmpstr), i == mChannels ? "\n" : " ");
    }
    fprintf(fp_meta, "a_duration=");
    for (int i = 0; i < mChannels + 1; i++)
    {
        fprintf(fp_meta, "%d%s", record_opt[i].FramesWritten(), i == mChannels ? "\n" : " ");
    }
    fprintf(fp_meta, "description=%s\n", ""); // not necessarily known at this time
#endif
    fclose(fp_meta);

    if (rename(tmp_meta_name, meta_name) != 0)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT( "Could not rename %s to %s %p\n"), tmp_meta_name, meta_name, ACE_TEXT("rename")));
        perror("rename");
        return false;
    }

    return true;
}
#endif

bool IngexRecorder::GetProjectFromDb(const std::string & name, prodauto::ProjectName & project_name)
{
    // Note that we are assuming that Database::initialise() has already
    // been called somewhere.

    // Get ProjectName
    bool result = false;
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
        if (db)
        {
            project_name = db->loadOrCreateProjectName(name);
            result = true;
        }
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    return result;
}

void IngexRecorder::GetMetadata(prodauto::ProjectName & project_name, std::vector<prodauto::UserComment> & user_comments)
{
    ACE_Guard<ACE_Thread_Mutex> guard(mMetadataMutex);

    project_name = mProjectName;
    user_comments = mUserComments;
}


