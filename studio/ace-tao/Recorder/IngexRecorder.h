/*
 * $Id: IngexRecorder.h,v 1.6 2008/09/04 15:38:44 john_f Exp $
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

#ifndef IngexRecorder_h
#define IngexRecorder_h

#include <string>
#include <list>

#include <ace/Basic_Types.h>
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>

// Wrapper for shared memory API.
// Via this, MAX_CHANNELS is #defined
#include "IngexShm.h"  

// Assuming here that MAX_CHANNELS == 8
const char * const SOURCE_NAME[] = {
    "input0", "input1", "input2", "input3", "input4", "input5", "input6", "input7" };
const char * const QUAD_NAME = "quad";


// This interval ensures that timecode found during CheckStartTimecode() is still
// available in buffer for Start().
const int SEARCH_GUARD = 5;

#include "Database.h"  // For Recorder
#include "RecordOptions.h"
#include "recorder_types.h" // for framecount_t
#include "RecorderImpl.h"

class IngexRecorder;

/**
Data passed when invoking record thread.
*/
//typedef struct {
//    //int         invoke_channel;
//    IngexRecorder * p_rec;
//    RecordOptions * p_opt;
//} ThreadInvokeArg;

struct ThreadParam
{
    ACE_thread_t id;
    IngexRecorder * p_rec;
    RecordOptions * p_opt;
};


// Thread functions
ACE_THR_FUNC_RETURN start_record_thread(void *p_arg);
ACE_THR_FUNC_RETURN start_quad_thread(void *p_arg);
ACE_THR_FUNC_RETURN manage_record_thread(void *p_arg);

struct Locator
{
    std::string comment;
    int colour;
    int64_t timecode;
};

/**
Low-level interface to Ingex recorder.
Make one instance of this class for each recording.
Within that recording there will be a number of threads dealing with
different sources etc.
*/
class IngexRecorder
{

// Per-recording functions
public:
    IngexRecorder(RecorderImpl * impl, unsigned int index);
    ~IngexRecorder();

    void Setup( framecount_t start_timecode,
                const std::vector<bool> & channel_enables,
                const std::vector<bool> & track_enables,
                const char * project);

    bool CheckStartTimecode(
                std::vector<bool> & channel_enables,
                framecount_t & start_timecode,
                framecount_t pre_roll,
                bool crash_record);

    bool Start(void);

    bool Stop(framecount_t & stop_timecode,
                framecount_t post_roll,
                const char * description,
                const std::vector<Locator> & locs);

    void SetCompletionCallback(void(*p_fn)(IngexRecorder *)) { mpCompletionCallback = p_fn; }

    bool WriteMetadataFile(const char * meta_name);

    void NoteFailure()
    {
        // Should maybe use a mutex
        mRecordingOK = false;
    }

    void NoteDroppedFrames()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mDroppedFramesMutex);
        mDroppedFrames = true;
    }

    bool DroppedFrames()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mDroppedFramesMutex);
        return mDroppedFrames;
    }

    framecount_t OutTime();

    framecount_t TargetDuration()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mDurationMutex);
        return mTargetDuration;
    }
    void TargetDuration(framecount_t d)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mDurationMutex);
        mTargetDuration = d;
    }

private:
    // Current RecorderConfig
    // May be better to return a unique ptr for thread safety
    // - can't do that as tape names are only stored in the instance
    //prodauto::Recorder * Recorder() const { return mRecorder; }
    prodauto::Recorder * Recorder() const { return mpImpl->Recorder(); }

    bool GetProjectFromDb(const std::string & name, prodauto::ProjectName & project_name);
    void DoCompletionCallback() { if(mpCompletionCallback) (*mpCompletionCallback)(this); }

    // Get copies of metadata in thread-safe manner
    void GetMetadata(prodauto::ProjectName & project_name, std::vector<prodauto::UserComment> & user_comments);

// Per-recording data
private:
    std::string mName;
    framecount_t mStartTimecode;
    int mFps;
    bool mDf;

    // These are used to write metadata file on completion of recording
    char mVideoPath[FILENAME_MAX];
    char mDvdPath[FILENAME_MAX];

    void (* mpCompletionCallback)(IngexRecorder *);

    ACE_thread_t mManageThreadId;

// The RecorderImpl which holds source configs etc.
    RecorderImpl * mpImpl;

// Duration
    framecount_t mTargetDuration;           ///< To signal when capture should stop
    ACE_Thread_Mutex mDurationMutex;

// Per-channel data
// Enables
    //bool mChannelEnable[MAX_CHANNELS];
    //bool mTrackEnable[MAX_CHANNELS * 5];
    std::vector<bool> mChannelEnable;
    std::vector<bool> mTrackEnable;
    unsigned int mTracksPerChannel;

public:
// MXF filenames
    std::string mFileNames[MAX_CHANNELS * 5];

    bool mRecordingOK;
    unsigned int mIndex;

    //RecordOptions   record_opt[MAX_RECORD];
private:
    ACE_Thread_Mutex mDroppedFramesMutex;
    bool mDroppedFrames;
    int mStartFrame[MAX_CHANNELS];

// Recording metadata
    ACE_Thread_Mutex mMetadataMutex;
    prodauto::ProjectName mProjectName; ///< Avid project name.
    std::vector<prodauto::UserComment> mUserComments;

// Per-thread data
    std::vector<ThreadParam> mThreadParams;

    //ThreadInvokeArg invoke_arg[MAX_RECORD];
    //std::vector<ACE_thread_t> mThreadIds;

// Thread functions are friends
    friend ACE_THR_FUNC_RETURN start_record_thread(void *p_arg);
    //friend ACE_THR_FUNC_RETURN start_quad_thread(void *p_arg);
    friend ACE_THR_FUNC_RETURN manage_record_thread(void *p_arg);

};

#endif // ifndef IngexRecorder_h

