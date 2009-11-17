/*
 * $Id: Timecode.cpp,v 1.6 2009/11/17 16:29:16 john_f Exp $
 *
 * Class to hold a Timecode
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
#include <cstring>
#include "Timecode.h"

// constants for 30 fps drop frame
const unsigned int DF_FRAMES_PER_MINUTE       = 60 * 30 - 2;                    // 1798
const unsigned int DF_FRAMES_PER_TEN_MINUTE   = 10 * DF_FRAMES_PER_MINUTE + 2;  // 17982
const unsigned int DF_FRAMES_PER_HOUR         = 6 * DF_FRAMES_PER_TEN_MINUTE;   // 107892


// constructor - timecode defaults to zero, mode defaults to TC25
Timecode::Timecode(int frames_since_midnight, int fps, bool df)
: mFramesSinceMidnight(frames_since_midnight), mFramesPerSecond(fps), mDropFrame(df)
{
    // Guard against garbage data
    if (mFramesSinceMidnight < 0)
    {
        mFramesSinceMidnight = 0;
    }

    // Drop frame only supported for 30 fps
    if (df && fps != 30)
    {
        mDropFrame = false;
    }
    UpdateHoursMinsEtc();
    UpdateText();
}

// constructor - mode defaults to TC25
Timecode::Timecode(int hr, int min, int sec, int frame, int fps, bool df)
: mFramesPerSecond(fps), mDropFrame(df), mHours(hr), mMinutes(min), mSeconds(sec), mFrames(frame)
{
    // Drop frame only supported for 30 fps
    if (df && fps != 30)
    {
        mDropFrame = false;
    }
    UpdateFramesSinceMidnight();
    UpdateText();
}

// constructor - from text - mode defaults to TC25
Timecode::Timecode(const char * s, int fps, bool df)
: mFramesPerSecond(fps), mDropFrame(df)
{
    bool ok = false;
    if (11 == strlen(s))
    {
        if (4 == sscanf(s,"%2d:%2d:%2d:%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            ok = true;
        }
        else if (4 == sscanf(s,"%2d:%2d:%2d.%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            ok = true;
        }
        else if (4 == sscanf(s,"%2d:%2d:%2d;%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            mDropFrame = true;
            ok = true;
        }
    }
    else if (8 == strlen(s))
    {
        if (3 == sscanf(s,"%2d:%2d:%2d", &mHours, &mMinutes, &mSeconds))
        {
            mFrames = 0;
            ok = true;
        }
    }

     // Drop frame only supported for 30 fps
    if (mDropFrame && mFramesPerSecond != 30)
    {
        mDropFrame = false;
    }

    if (!ok)
    {
        mHours = 0;
        mMinutes = 0;
        mSeconds = 0;
        mFrames = 0;
    }

    UpdateFramesSinceMidnight();
    UpdateText();
}

// Copy constructor
Timecode::Timecode(const Timecode & tc)
{
    mFramesPerSecond = tc.mFramesPerSecond;
    mDropFrame = tc.mDropFrame;
    mFramesSinceMidnight = tc.mFramesSinceMidnight;
    mHours = tc.mHours;
    mMinutes = tc.mMinutes;
    mSeconds = tc.mSeconds;
    mFrames = tc.mFrames;

    UpdateText();
}

Timecode & Timecode::operator=(const Timecode & tc)
{
    if (this != &tc)
    {
        mFramesPerSecond = tc.mFramesPerSecond;
        mDropFrame = tc.mDropFrame;
        mFramesSinceMidnight = tc.mFramesSinceMidnight;
        mHours = tc.mHours;
        mMinutes = tc.mMinutes;
        mSeconds = tc.mSeconds;
        mFrames = tc.mFrames;
        UpdateText();
    }
    return *this;
}

Timecode & Timecode::operator=(int frames_since_midnight)
{
    // Guard against garbage data
    if (frames_since_midnight < 0)
    {
        frames_since_midnight = 0;
    }
    mFramesSinceMidnight = frames_since_midnight;
    UpdateHoursMinsEtc();
    UpdateText();
    return *this;
}

void Timecode::UpdateHoursMinsEtc()
{
    // Guard against garbage data
    if (mFramesSinceMidnight < 0)
    {
        mFramesSinceMidnight = 0;
    }

    int frames = mFramesSinceMidnight;

    if (!mDropFrame)
    {
        mHours = frames / (mFramesPerSecond * 60 * 60);
        frames %= (mFramesPerSecond * 60 * 60);
        mMinutes = frames / (mFramesPerSecond * 60);
        frames %= (mFramesPerSecond * 60);
        mSeconds = frames / mFramesPerSecond;
        mFrames = frames % mFramesPerSecond;
    }
    else
    {
        mHours = frames / DF_FRAMES_PER_HOUR;
        frames %= DF_FRAMES_PER_HOUR;

        int ten_minute = frames / DF_FRAMES_PER_TEN_MINUTE;
        frames %= DF_FRAMES_PER_TEN_MINUTE;

        // must adjust frame count to make minutes calculation work
        // calculations from here on in just assume that within the 10 minute cycle
        // there are only DF_FRAMES_PER_MINUTE (1798) frames per minute - even for the first minute
        // in the ten minute cycle. Hence we decrement the frame count by 2 to get the minutes count
        // So for the first two frames of the ten minute cycle we get a negative frames number

        frames -= 2;

        int unit_minute = frames / DF_FRAMES_PER_MINUTE;
        frames %= DF_FRAMES_PER_MINUTE;
        mMinutes = ten_minute * 10 + unit_minute;

        // frames now contains frame in minute @ 1798 frames per minute
        // put the 2 frame adjustment back in to get the correct frame count
        // For the first two frames of the ten minute cycle, frames is negative and this
        // adjustment makes it non-negative. For other minutes in the cycle the frames count
        // goes from 0 upwards, thus this adjusment gives the required 2 frame offset

        frames += 2;

        mSeconds = frames / mFramesPerSecond;
        mFrames = frames % mFramesPerSecond;
    }

    // Prevent hours exceeding one day
    mHours %= 24;
}

void Timecode::UpdateFramesSinceMidnight()
{
    if (!mDropFrame)
    {
        mFramesSinceMidnight = mFrames
            + mSeconds * mFramesPerSecond
            + mMinutes * 60 * mFramesPerSecond
            + mHours * 60 * 60 * mFramesPerSecond;
    }
    else
    {
        mFramesSinceMidnight = mFrames
            + mSeconds * mFramesPerSecond
            + (mMinutes % 10) * DF_FRAMES_PER_MINUTE
            + (mMinutes / 10) * DF_FRAMES_PER_TEN_MINUTE
            + mHours * DF_FRAMES_PER_HOUR;
    }
}

/**
This method is used to update text representation of timecode whenever numerical
values change.
Unfortunately you can't produce the text in Text() function because it would not
then be const.
*/
void Timecode::UpdateText()
{
    // Full text representation
    if (mDropFrame)
    {
        sprintf(mText,"%02d:%02d:%02d;%02d", mHours, mMinutes, mSeconds, mFrames);
    }
    else
    {
        sprintf(mText,"%02d:%02d:%02d:%02d", mHours, mMinutes, mSeconds, mFrames);
    }

    // No separators version
    sprintf(mTextNoSeparators,"%02d%02d%02d%02d", mHours, mMinutes, mSeconds, mFrames);
}

/**
To help with conversion to MXF-style timecode.
*/
int Timecode::EditRateNumerator() const
{
    int num = 0;
    if (!mDropFrame)
    {
        num = mFramesPerSecond;
    }
    else
    {
        num = mFramesPerSecond * 1000;
    }
    return num;
}

/**
To help with conversion to MXF-style timecode.
*/
int Timecode::EditRateDenominator() const
{
    int denom = 0;
    if (!mDropFrame)
    {
        denom = 1;
    }
    else
    {
        denom = 1001;
    }
    return denom;
}

/**
Add a number of frames.
*/
void Timecode::operator+=(int frames)
{
    mFramesSinceMidnight += frames;
    UpdateHoursMinsEtc();
    UpdateText();
}



Duration::Duration(int frames, int fps, bool df)
: mFrameCount(frames), mFramesPerSecond(fps), mDropFrame(df)
{
    UpdateHoursMinsEtc();
    UpdateText();
}

void Duration::UpdateHoursMinsEtc()
{
    int frames = mFrameCount;

    if (!mDropFrame)
    {
        mHours = frames / (mFramesPerSecond * 60 * 60);
        frames %= (mFramesPerSecond * 60 * 60);
        mMinutes = frames / (mFramesPerSecond * 60);
        frames %= (mFramesPerSecond * 60);
        mSeconds = frames / mFramesPerSecond;
        mFrames = frames % mFramesPerSecond;
    }
    else
    {
        mHours = frames / DF_FRAMES_PER_HOUR;
        frames %= DF_FRAMES_PER_HOUR;

        int ten_minute = frames / DF_FRAMES_PER_TEN_MINUTE;
        frames %= DF_FRAMES_PER_TEN_MINUTE;

        // must adjust frame count to make minutes calculation work
        // calculations from here on in just assume that within the 10 minute cycle
        // there are only DF_FRAMES_PER_MINUTE (1798) frames per minute - even for the first minute
        // in the ten minute cycle. Hence we decrement the frame count by 2 to get the minutes count
        // So for the first two frames of the ten minute cycle we get a negative frames number

        frames -= 2;

        int unit_minute = frames / DF_FRAMES_PER_MINUTE;
        frames %= DF_FRAMES_PER_MINUTE;
        mMinutes = ten_minute * 10 + unit_minute;

        // frames now contains frame in minute @ 1798 frames per minute
        // put the 2 frame adjustment back in to get the correct frame count
        // For the first two frames of the ten minute cycle, frames is negative and this
        // adjustment makes it non-negative. For other minutes in the cycle the frames count
        // goes from 0 upwards, thus this adjusment gives the required 2 frame offset

        frames += 2;

        mSeconds = frames / mFramesPerSecond;
        mFrames = frames % mFramesPerSecond;
    }
}

/**
This method is used to update text representation of duration whenever numerical
values change.
*/
void Duration::UpdateText()
{
#if 1
    // Full string including frames
    if (mDropFrame)
    {
        sprintf(mText,"%02d:%02d:%02d;%02d", mHours, mMinutes, mSeconds, mFrames);
    }
    else
    {
        sprintf(mText,"%02d:%02d:%02d:%02d", mHours, mMinutes, mSeconds, mFrames);
    }
#else
    // Shorter version without frames
    if (mHours == 0)
    {
        sprintf(mText,"%02d:%02d", mMinutes, mSeconds);
    }
    else
    {
        sprintf(mText,"%d:%02d:%02d", mHours, mMinutes, mSeconds);
    }
#endif
}

/**
Get difference between two Timecodes.
*/
Duration operator-(const Timecode & lhs, const Timecode & rhs)
{
    // NB. Should check for mode compatibility
    return Duration(lhs.FramesSinceMidnight() - rhs.FramesSinceMidnight(), lhs.mFramesPerSecond, lhs.mDropFrame);
}

/**
Add a number of frames to a timecode.
*/
Timecode operator+(const Timecode & lhs, const int & frames)
{
    return Timecode(lhs.mFramesSinceMidnight + frames);
}



