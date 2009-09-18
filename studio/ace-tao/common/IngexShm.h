/*
 * $Id: IngexShm.h,v 1.2 2009/09/18 15:50:18 john_f Exp $
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

#ifndef IngexShm_h
#define IngexShm_h

#include <ace/Thread.h>
#include <string>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

//#include <ace/Log_Msg.h>
#include "integer_types.h"

#include "nexus_control.h"

/**
C++ API for the shared memory used to pass video/audio
data into the recorder.
*/
class IngexShm
{
public:
  // Singelton accessor
    static IngexShm * Instance()
    {
        if (mInstance == 0)  // is it the first call?
        {  
            mInstance = new IngexShm; // create sole instance
        }
        return mInstance; // address of sole instance
    }
  // Singleton destroy
    static void Destroy() { delete mInstance; mInstance = 0; }

    ~IngexShm();

    void Attach();
    unsigned int Channels() { return mChannels; }
    unsigned int AudioTracksPerChannel() { return mAudioTracksPerChannel; }
    int RingLength() { if (mpControl) return mpControl->ringlen; else return 0; }
    int LastFrame(unsigned int channel_i)
    {
        int frame = 0;
        if (channel_i < mChannels)
        {
#ifndef _MSC_VER
            PTHREAD_MUTEX_LOCK(&mpControl->channel[channel_i].m_lastframe)
#endif
            frame = mpControl->channel[channel_i].lastframe;
#ifndef _MSC_VER
            PTHREAD_MUTEX_UNLOCK(&mpControl->channel[channel_i].m_lastframe)
#endif
        }
        if (frame < 0)
        {
            frame = 0;
        }
        return frame;
    }

    std::string SourceName(unsigned int channel_i);
    void SourceName(unsigned int channel_i, const std::string & name);

    enum TcEnum { LTC, VITC };

    void TcMode(TcEnum mode) { mTcMode = mode; }

    int FrameRateNumerator() { if (mpControl) return mpControl->frame_rate_numer; else return 0; }
    int FrameRateDenominator() { if (mpControl) return mpControl->frame_rate_denom; else return 0; }
    void GetFrameRate(int & numerator, int & denominator);
    void GetFrameRate(int & fps, bool & df);

    // In funtions below, you could check (channel < mChannels) first.

    int32_t Timecode(unsigned int channel, unsigned int frame)
    {
        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("Timecode(%d, %d)\n"), channel, frame));
        uint8_t * p_tc = mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen)
            + (mTcMode == LTC ? mpControl->ltc_offset : mpControl->vitc_offset);
        return * (int32_t *) p_tc;
    }
    int32_t CurrentTimecode(unsigned int channel)
    {
        return Timecode(channel, LastFrame(channel));
    }

    bool SignalPresent(unsigned int channel)
    {
        uint8_t * p_ok = mRing[channel]
            + mpControl->elementsize * (LastFrame(channel) % mpControl->ringlen)
            + mpControl->signal_ok_offset;
        return 0 != * (int32_t *) p_ok;
    }

    unsigned int Width() { return mpControl->width; }
    unsigned int Height() { return mpControl->height; }
    unsigned int PrimaryWidth() { return mpControl->width; }
    unsigned int PrimaryHeight() { return mpControl->height; }
    unsigned int SecondaryWidth() { return mpControl->sec_width; }
    unsigned int SecondaryHeight() { return mpControl->sec_height; }
    bool Interlace() { return true; } // not yet supported

    int FrameNumber(unsigned int channel, unsigned int frame)
    {
        void * p_framenumber = mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->frame_number_offset;
        return * (int *) p_framenumber;
    }

    int FrameNumberOffset() { return mpControl->frame_number_offset; }
    int SecondaryVideoOffset() { return mpControl->sec_video_offset; }

    uint8_t * pVideoPri(int channel, int frame)
    {
        return mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen);
    }
    //int sizeVideo422() { return mpControl->width*mpControl->height*2; }

    uint8_t * pVideoSec(unsigned int channel, unsigned int frame)
    {
        return mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->sec_video_offset;
    }
    //unsigned int sizeVideoSec() { return  mpControl->width*mpControl->height*3/2; }

    int32_t * pAudio12(unsigned int channel, unsigned int frame)
    {
        return (int32_t *)
            (mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio12_offset);
    }

    int32_t * pAudio34(unsigned int channel, unsigned int frame)
    {
        return  (int32_t *)
            (mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio34_offset);
    }

    int32_t * pAudio56(unsigned int channel, unsigned int frame)
    {
        return (int32_t *)
            (mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio56_offset);
    }

    int32_t * pAudio78(unsigned int channel, unsigned int frame)
    {
        return  (int32_t *)
            (mRing[channel] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio78_offset);
    }

    CaptureFormat PrimaryCaptureFormat()
    {
        if (mpControl)
        {
            return mpControl->pri_video_format;
        }
        else
        {
            return FormatNone;
        }
    }

    CaptureFormat SecondaryCaptureFormat()
    {
        if (mpControl)
        {
            return mpControl->sec_video_format;
        }
        else
        {
            return FormatNone;
        }
    }

    void GetHeartbeat(struct timeval * tv)
    {
        if (mpControl)
        {
            *tv = mpControl->owner_heartbeat;
        }
        else
        {
            tv->tv_sec = 0;
            tv->tv_usec = 0;
        }
    }

    // Informational updates from Recorder to shared memory
    void InfoSetup(std::string name);
    int InfoGetRecIdx(void);
    void InfoResetChannels(void);
    void InfoReset(unsigned int channel, int index, bool quad_video);
    void InfoSetEnabled(unsigned int channel, int index, bool quad_video, bool enabled);
    void InfoSetRecording(unsigned int channel, int index, bool quad_video, bool recording);
    void InfoSetDesc(unsigned int channel, int index, bool quad_video, const char *fmt, ...);
    void InfoSetRecordError(unsigned int channel, int index, bool quad_video, const char *fmt, ...);
    void InfoSetFramesWritten(unsigned int channel, int index, bool quad_video, int frames_written);
    void InfoSetFramesDropped(unsigned int channel, int index, bool quad_video, int frames_dropped);
    void InfoSetBacklog(unsigned int channel, int index, bool quad_video, int frames_in_backlog);


protected:
    // Protect constructors to force use of Instance()
    IngexShm();
    IngexShm(const IngexShm &);
    IngexShm & operator= (const IngexShm &);

private:
    unsigned int mChannels;
    unsigned int mAudioTracksPerChannel;
    uint8_t * mRing[MAX_CHANNELS];
    NexusControl * mpControl;
    TcEnum mTcMode;
    // static instance pointer
    static IngexShm * mInstance;
    // thread management
    bool mActivated;
    ACE_thread_t mThreadId;

// friends
    friend ACE_THR_FUNC_RETURN monitor_shm(void * p);
};

#endif //#ifndef IngexShm_h

