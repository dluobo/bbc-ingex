/*
 * $Id: EncodeFrameBuffer.h,v 1.2 2010/01/14 14:07:13 john_f Exp $
 *
 * Buffer to handle video/audio data during encoding process.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#ifndef EncodeFrameBuffer_h
#define EncodeFrameBuffer_h

#include <vector>
#include <map>
#include <ace/Thread_Mutex.h>

class EncodeFrameTrack
{
public:
    EncodeFrameTrack();
    ~EncodeFrameTrack();
    void Init(void * data, size_t size, bool copy, bool del, bool coded,
                int frame_index, int * p_frame_index);
    void * Data() { return mData; }
    size_t Size() { return mSize; }
    void Coded(bool b) { mCoded = b; }
    bool Coded() const { return mCoded; }
    int FrameIndex() { return mFrameIndex; }
    bool Valid();
    void Error(bool err) { mError = err; }
    bool Error() { return mError; }
private:
    void * mData;
    size_t mSize;
    bool mDel;
    bool mCoded;
    int mFrameIndex;
    int * mpFrameIndex; // For checking data still valid
    bool mError;
};

class EncodeFrame
{
public:
    //EncodeFrame();
    //~EncodeFrame();
    EncodeFrameTrack & Track(unsigned int track_index) { return mTracks[track_index]; }
    bool IsCoded() const;
    //void * TrackData(unsigned int trk);
    //size_t TrackSize(unsigned int trk);
private:
    std::map<unsigned int, EncodeFrameTrack> mTracks; // key is index of track
};

/**
A buffer of EncodeFrames, implemented as a map.
*/
class EncodeFrameBuffer
{
public:
    EncodeFrame & Frame(unsigned int frame_index);
    void EraseFrame(unsigned int index);
    size_t QueueSize();
    size_t CodedSize();
private:
    std::map<unsigned int, EncodeFrame> mFrameBuffer;
    ACE_Thread_Mutex mFrameBufferMutex; // mutex protects the map
};

#endif // ifndef CodedFrameBuffer_h

