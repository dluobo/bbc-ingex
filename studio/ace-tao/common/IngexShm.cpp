/*
 * $Id: IngexShm.cpp,v 1.3 2009/10/12 15:05:38 john_f Exp $
 *
 * Interface for reading audio/video data from shared memory.
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

#include <ace/Time_Value.h>
#include <ace/Thread.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_sys_shm.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_stdio.h>
#include <ace/OS_NS_signal.h>


#include "IngexShm.h"

// static instance pointer
IngexShm * IngexShm::mInstance = 0;

// Local context only
namespace
{
int64_t tv_diff_microsecs(const struct timeval * a, const struct timeval * b)
{
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("a->tv_sec = %d, a->tv_usec = %d\n"), a->tv_sec, a->tv_usec));
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("b->tv_sec = %d, b->tv_usec = %d\n"), b->tv_sec, b->tv_usec));

    //int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
    int64_t diff = b->tv_sec - a->tv_sec;
    diff *= 1000000;
    diff += (b->tv_usec - a->tv_usec);

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("diff = %d\n"), diff));
    return diff;
}
}

ACE_THR_FUNC_RETURN monitor_shm(void * p)
{
    IngexShm * p_ingex_shm = (IngexShm *) p;
    const ACE_Time_Value MONITOR_INTERVAL(1, 0);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Shared memory monitor thread starting.\n")));
    while (p_ingex_shm->mActivated)
    {
        // Check heartbeat
        struct timeval now = ACE_OS::gettimeofday();
        struct timeval heartbeat;
        p_ingex_shm->GetHeartbeat(&heartbeat);
        int64_t diff = tv_diff_microsecs(&heartbeat, &now);
        if (diff > 1000 * 1000)
        {
            p_ingex_shm->Attach();
        }
        ACE_OS::sleep(MONITOR_INTERVAL);
    }

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Shared memory monitor thread exiting.\n")));
    return 0;
}

IngexShm::IngexShm()
: mChannels(0), mAudioTracksPerChannel(0), mpControl(0), /*mTcMode(VITC),*/ mActivated(false), mThreadId(0)
{
    // Start thread to monitor and (re-)connect to shared memory
    mActivated = true;
    ACE_Thread::spawn( monitor_shm, this, THR_NEW_LWP|THR_JOINABLE, &mThreadId );
}

IngexShm::~IngexShm()
{
    // Terminate the monitor thread
    mActivated = false;
    if (mThreadId)
    {
        ACE_Thread::join(mThreadId, 0, 0);
        mThreadId = 0;
    }
}



/**
Attach to shared memory.
*/
void IngexShm::Attach()
{
// The kind of shared memory used by Ingex recorder daemon is not available on Windows
// but with the settings below the code will still compile.
#ifdef WIN32
    const key_t control_shm_key = "control";
    const key_t channel_shm_key[MAX_CHANNELS] = { "channel0", "channel1", "channel2", "channel3" };
    const int shm_flags = 0;
#else
    const key_t control_shm_key = 9;
    const key_t channel_shm_key[MAX_CHANNELS] = { 10, 11, 12, 13, 14, 15, 16, 17 };
    const int shm_flags = SHM_RDONLY;
#endif

    int control_id = ACE_OS::shmget(control_shm_key, sizeof(NexusControl), 0666);
    /*
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        control_id = ACE_OS::shmget(control_shm_key, sizeof(NexusControl), 0666);
        if (control_id != -1)
        {
            break;
        }
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Waiting for shared memory...\n")));
        ACE_OS::sleep(ACE_Time_Value(0, 50 * 1000));
    }
    */

    if (control_id == -1)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to connect to shared memory control buffer!\n")));
    }
    else
    {
        // Shared memory found for control data, attach to it
        mpControl = (NexusControl *)ACE_OS::shmat(control_id, NULL, 0);
        mChannels = mpControl->channels;
        mAudioTracksPerChannel = (mpControl->audio78_offset ? 8 : 4);

        ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("Connected to p_control %@\n  channels=%d elementsize=%d ringlen=%d\n"),
            mpControl,
            mpControl->channels,
            mpControl->elementsize,
            mpControl->ringlen));

        // Get and attach to each video ring buffer
        bool ok = true;
        for (int i = 0; i < mpControl->channels; i++)
        {
            int shm_id = ACE_OS::shmget(channel_shm_key[i], mpControl->elementsize, 0444);
            /*
            for (int attempt = 0; attempt < 5; ++attempt)
            {
                shm_id = ACE_OS::shmget(channel_shm_key[i], mpControl->elementsize, 0444);
                if (shm_id != -1)
                {
                    break;
                }
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Waiting for channel[%d] shared memory...\n"), i));
                ACE_OS::sleep(ACE_Time_Value(0, 50 * 1000));
            }
            */
            if (shm_id == -1)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to connect to shared memory channel[%d]!\n"), i));
                ok = false;
            }
            else
            {
                mRing[i] = (unsigned char *)ACE_OS::shmat(shm_id, NULL, shm_flags);
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Connected to shared memory channel[%d] at %@\n"), i, mRing[i]));
            }
        }
        if (ok)
        {
            ACE_DEBUG((LM_INFO, ACE_TEXT("Connected to shared memory.\n")));
        }
    }
}


int32_t IngexShm::Timecode(unsigned int channel, unsigned int frame)
{
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("Timecode(%d, %d)\n"), channel, frame));
    /*
    NexusTimecode nexus_tc_type;
    switch (mTcMode)
    {
    case LTC:
        nexus_tc_type = NexusTC_LTC;
        break;
    case VITC:
    default:
        nexus_tc_type = NexusTC_VITC;
        break;
    }
    */
    
    // Could check (channel < mChannels) first.
    return (int32_t)nexus_tc(mpControl, mRing, channel, frame, NexusTC_DEFAULT);
}

int32_t IngexShm::CurrentTimecode(unsigned int channel)
{
    // Could check (channel < mChannels) first.
    return Timecode(channel, LastFrame(channel));
}


std::string IngexShm::SourceName(unsigned int channel_i)
{
    if (channel_i < mChannels)
    {
        return mpControl->channel[channel_i].source_name;
    }
    else
    {
        return "";
    }
}

void IngexShm::SourceName(unsigned int channel_i, const std::string & name)
{
    if (channel_i < mChannels)
    {
        nexus_set_source_name(mpControl, channel_i, name.c_str());
    }
}

void IngexShm::GetFrameRate(int & numerator, int & denominator)
{
    numerator = mpControl->frame_rate_numer;
    denominator = mpControl->frame_rate_denom;
}

void IngexShm::GetFrameRate(int & fps, bool & df)
{
    fps = 0;
    df = false;
    if (mpControl)
    {
        int numerator = mpControl->frame_rate_numer;
        int denominator = mpControl->frame_rate_denom;
        if (denominator > 0)
        {
            fps = numerator / denominator;
            if (numerator % denominator > 0)
            {
                df = true;
                ++fps;
            }
        }
    }
}

// Informational updates from Recorder to shared memory
void IngexShm::InfoSetup(std::string name)
{
    // Called once on Recorder startup to register this Recorder in shared mem
    int pid = getpid();
    for (int i = 0; i < MAX_RECORDERS; i++)
    {
        if (mpControl && mpControl->record_info[i].pid == 0)
        {
            // This slot is free, so take it
            memset(&mpControl->record_info[i], 0, sizeof(mpControl->record_info[i]));
            mpControl->record_info[i].pid = pid;
            strncpy(mpControl->record_info[i].name, name.c_str(), sizeof(mpControl->record_info[i].name));
            return;
        }
        // Check if process is dead, if so take over slot
        if (mpControl && ACE_OS::kill(mpControl->record_info[i].pid, 0) == -1 && errno != EPERM)
        {
            memset(&mpControl->record_info[i], 0, sizeof(mpControl->record_info[i]));
            mpControl->record_info[i].pid = pid;
            strncpy(mpControl->record_info[i].name, name.c_str(), sizeof(mpControl->record_info[i].name));
            return;
        }
    }
}

int IngexShm::InfoGetRecIdx(void)
{
    // Find the index into the list of Recorder processes listed in shared mem
    int pid = getpid();
    for (int i=0; i < MAX_RECORDERS; i++) {
        if (mpControl->record_info[i].pid == pid)
            return i;
    }
    return 0;
}

// Reset all enabled flags for this Recorder
void IngexShm::InfoResetChannels()
{
    int recidx = InfoGetRecIdx();
	for (int i=0; i < mpControl->channels; i++) {
		for (int j=0; j < MAX_ENCODES_PER_CHANNEL; j++) {
			mpControl->record_info[recidx].channel[i][j].enabled = 0;
		}
	}
	mpControl->record_info[recidx].quad.enabled = 0;
}

// Reset a single encoding for a single channel
void IngexShm::InfoReset(unsigned int channel, int index, bool quad_video)
{
    int recidx = InfoGetRecIdx();
    if (quad_video)
        memset(&mpControl->record_info[recidx].quad, 0, sizeof(NexusRecordEncodingInfo));
    else
        memset(&mpControl->record_info[recidx].channel[channel][index], 0, sizeof(NexusRecordEncodingInfo));
}

void IngexShm::InfoSetEnabled(unsigned int channel, int index, bool quad_video, bool enabled)
{
    int recidx = InfoGetRecIdx();
    if (quad_video)
        mpControl->record_info[recidx].quad.enabled = enabled ? 1 : 0;
    else
        mpControl->record_info[recidx].channel[channel][index].enabled = enabled ? 1 : 0;
}

void IngexShm::InfoSetRecording(unsigned int channel, int index, bool quad_video, bool recording)
{
    int recidx = InfoGetRecIdx();
    if (quad_video)
    {
        mpControl->record_info[recidx].quad.recording = recording ? 1 : 0;
    }
    else
    {
        mpControl->record_info[recidx].channel[channel][index].recording = recording ? 1 : 0;
    }
}

void IngexShm::InfoSetDesc(unsigned int channel, int index, bool quad_video, const char *fmt, ...)
{
    int recidx = InfoGetRecIdx();
    char buf[sizeof(mpControl->record_info[0].channel[0][0].desc)];
    va_list    ap;
    va_start(ap, fmt);
    ACE_OS::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';

    if (quad_video)
    {
        strncpy(mpControl->record_info[recidx].quad.desc, buf, sizeof(buf));
    }
    else
    {
        strncpy(mpControl->record_info[recidx].channel[channel][index].desc, buf, sizeof(buf));
    }
}

void IngexShm::InfoSetRecordError(unsigned int channel, int index, bool quad_video, const char *fmt, ...)
{
    int recidx = InfoGetRecIdx();
    char buf[sizeof(mpControl->record_info[0].channel[0][0].error)];
    va_list    ap;
    va_start(ap, fmt);
    ACE_OS::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';
    if (buf[strlen(buf)-1] == '\n')    // Remove trailing newline if any
    {
        buf[strlen(buf)-1] = '\0';
    }

    if (quad_video)
    {
        mpControl->record_info[recidx].quad.record_error = 1;
        strncpy(mpControl->record_info[recidx].quad.error, buf, sizeof(buf));
    }
    else
    {
        mpControl->record_info[recidx].channel[channel][index].record_error = 1;
        strncpy(mpControl->record_info[recidx].channel[channel][index].error, buf, sizeof(buf));
    }
}

void IngexShm::InfoSetFramesWritten(unsigned int channel, int index, bool quad_video, int frames_written)
{
    int recidx = InfoGetRecIdx();
    if (quad_video)
        mpControl->record_info[recidx].quad.frames_written = frames_written;
    else
        mpControl->record_info[recidx].channel[channel][index].frames_written = frames_written;
}

void IngexShm::InfoSetFramesDropped(unsigned int channel, int index, bool quad_video, int frames_dropped)
{
    int recidx = InfoGetRecIdx();
    if (quad_video)
        mpControl->record_info[recidx].quad.frames_dropped = frames_dropped;
    else
        mpControl->record_info[recidx].channel[channel][index].frames_dropped = frames_dropped;
}

void IngexShm::InfoSetBacklog(unsigned int channel, int index, bool quad_video, int frames_in_backlog)
{
    int recidx = InfoGetRecIdx();
    if (quad_video)
        mpControl->record_info[recidx].quad.frames_in_backlog = frames_in_backlog;
    else
        mpControl->record_info[recidx].channel[channel][index].frames_in_backlog = frames_in_backlog;
}
