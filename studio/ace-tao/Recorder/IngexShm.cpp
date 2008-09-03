/*
 * $Id: IngexShm.cpp,v 1.5 2008/09/03 14:09:05 john_f Exp $
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

#include <ace/OS_NS_unistd.h>
#include <ace/Time_Value.h>
#include <ace/OS_NS_sys_shm.h>
#include <ace/Log_Msg.h>

#include "IngexShm.h"

// static instance pointer
IngexShm * IngexShm::mInstance = 0;

IngexShm::IngexShm(void)
: mChannels(0), mAudioTracksPerChannel(0), mTcMode(VITC)
{
}

IngexShm::~IngexShm(void)
{
}

/**
Attach to shared memory.
@return The number of buffers (channels) attached to.
*/
int IngexShm::Init()
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

    int control_id;
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

    if (control_id == -1)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to connect to shared memory!\n")));
        return 0;
    }

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

    // Attach to each video ring buffer
    for (int i = 0; i < mpControl->channels; i++)
    {
        int shm_id;
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
        mRing[i] = (unsigned char *)ACE_OS::shmat(shm_id, NULL, shm_flags);
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Connected to channel[%d] %@\n"), i, mRing[i]));
    }

    return mChannels;
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
        // Set name
        strncpy( mpControl->channel[channel_i].source_name, name.c_str(),
            sizeof(mpControl->channel[channel_i].source_name));
        // Signal the change
#ifndef _MSC_VER
        PTHREAD_MUTEX_LOCK(&mpControl->m_source_name_update)
#endif
        ++mpControl->source_name_update;
#ifndef _MSC_VER
        PTHREAD_MUTEX_UNLOCK(&mpControl->m_source_name_update)
#endif
    }
}

// Informational updates from Recorder to shared memory
void IngexShm::InfoSetup(std::string name)
{
    // Called once on Recorder startup to register this Recorder in shared mem
    int pid = getpid();
    for (int i=0; i < MAX_RECORDERS; i++) {
        if (mpControl->record_info[i].pid == 0) {
            // This slot is free, so take it
            memset(&mpControl->record_info[i], 0, sizeof(mpControl->record_info[i]));
            mpControl->record_info[i].pid = pid;
            strncpy(mpControl->record_info[i].name, name.c_str(), sizeof(mpControl->record_info[i].name));
            return;
        }
        // Check if process is dead, if so take over slot
        if (kill(mpControl->record_info[i].pid, 0) == -1 && errno != EPERM) {
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
        mpControl->record_info[recidx].quad.recording = recording ? 1 : 0;
    else
        mpControl->record_info[recidx].channel[channel][index].recording = recording ? 1 : 0;
}

void IngexShm::InfoSetDesc(unsigned int channel, int index, bool quad_video, const char *fmt, ...)
{
    int recidx = InfoGetRecIdx();
    char buf[sizeof(mpControl->record_info[0].channel[0][0].desc)];
    va_list    ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';

    if (quad_video) {
        strncpy(mpControl->record_info[recidx].quad.desc, buf, sizeof(buf));
    }
    else {
        strncpy(mpControl->record_info[recidx].channel[channel][index].desc, buf, sizeof(buf));
    }
}

void IngexShm::InfoSetRecordError(unsigned int channel, int index, bool quad_video, const char *fmt, ...)
{
    int recidx = InfoGetRecIdx();
    char buf[sizeof(mpControl->record_info[0].channel[0][0].error)];
    va_list    ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';
    if (buf[strlen(buf)-1] == '\n')    // Remove trailing newline if any
        buf[strlen(buf)-1] = '\0';

    if (quad_video) {
        mpControl->record_info[recidx].quad.record_error = 1;
        strncpy(mpControl->record_info[recidx].quad.error, buf, sizeof(buf));
    }
    else {
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
