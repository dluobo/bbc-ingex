/*
 * $Id: Timecode.h,v 1.3 2009/06/12 17:50:00 john_f Exp $
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

#ifndef Timecode_h
#define Timecode_h

#include <sys/timeb.h>
#include <string>

class Duration
{
public:
    Duration(int frames = 0, int fps = 25, bool df = false);
    int FrameCount() { return mFrameCount; }
    const char * Text() const { return mText; }
private:
// fundamental data
    int mFrameCount;
    int mFramesPerSecond;
    bool mDropFrame;

// derived data
    int mHours;
    int mMinutes;
    int mSeconds;
    int mFrames;
    char mText[16];

// methods
    void UpdateHoursMinsEtc();
    void UpdateText();
};

class Timecode
{
public:
// enum
    //enum Mode { TC25, TC30, TC30DF };

// constructors
    Timecode(int frames_since_midnight = 0, int fps = 25, bool df = false);
    Timecode(int hr, int min, int sec, int frame, int fps = 25, bool df = false);
    Timecode(const char * s, int fps = 25, bool df = false);
// copy constructor
    Timecode(const Timecode & tc);
// assignment
    Timecode & operator=(const Timecode &);
    Timecode & operator=(int frames_since_midnight);

// methods
    const char * Text() const { return mText; }
    const char * TextNoSeparators() const { return mTextNoSeparators; }
    int FramesSinceMidnight() const { return mFramesSinceMidnight; }
    int FramesPerSecond() const { return mFramesPerSecond; }
    bool DropFrame() const { return mDropFrame; }
    int EditRateNumerator() const;
    int EditRateDenominator() const;

// operator overload
    friend Timecode operator+(const Timecode & lhs, const int & frames);
    friend Duration operator-(const Timecode & lhs, const Timecode & rhs);
    //friend void operator+=(Timecode & lhs, const Timecode & rhs);
    //friend void operator-=(Timecode & lhs, const Timecode & rhs);
    void operator+=(int frames);

private:
// fundamental data
    int mFramesSinceMidnight;
    int mFramesPerSecond;
    bool mDropFrame;

// derived data
    int mHours;
    int mMinutes;
    int mSeconds;
    int mFrames;
    char mText[16];
    char mTextNoSeparators[16];

// methods
    void UpdateFramesSinceMidnight();
    void UpdateHoursMinsEtc();
    void UpdateText();
};

#endif //#ifndef Timecode_h

