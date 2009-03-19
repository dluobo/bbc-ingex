/*
 * $Id: RecordOptions.h,v 1.6 2009/03/19 17:54:48 john_f Exp $
 *
 * Class for channel-specific (i.e. thread-specific) recording data.
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

#ifndef RecordOptions_h
#define RecordOptions_h

#include <string>

#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>

#include "Package.h" // for prodauto::ProjectName

#include "integer_types.h"
#include "recorder_types.h" // for framecount_t
//#include "RecorderSettings.h" // for Wrapping::EnumType

// Description of a particular encoding, to be performed in a thread.
class RecordOptions
{
public:
    RecordOptions();

    int channel_num;
    long source_id; ///< Identifies SourceConfig being encoded
    int index; ///< To distinguish multiple encodings on the same input/channel.
    bool quad; ///< True for encoding from multiple inputs/channels.

    int resolution;
    int file_format;
    std::string dir;
    bool bitc; ///< True for burnt-in timecode

    std::string file_ident;

    framecount_t FramesWritten()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesWrittenMutex);
        return mFramesWritten;
    }
    void FramesWritten(framecount_t f)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesWrittenMutex);
        mFramesWritten = f;
    }
    void IncFramesWritten(framecount_t n = 1)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesWrittenMutex);
        mFramesWritten += n;
    }

    framecount_t FramesDropped()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesDroppedMutex);
        return mFramesDropped;
    }
    void FramesDropped(framecount_t f)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesDroppedMutex);
        mFramesDropped = f;
    }
    void IncFramesDropped(framecount_t n = 1)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesDroppedMutex);
        mFramesDropped += n;
    }

private:
    framecount_t mFramesWritten;        ///< Frames written during capture
    ACE_Thread_Mutex mFramesWrittenMutex;
    framecount_t mFramesDropped;        ///< Frames dropped during capture
    ACE_Thread_Mutex mFramesDroppedMutex;
};


#endif // ifndef RecordOptions_h

