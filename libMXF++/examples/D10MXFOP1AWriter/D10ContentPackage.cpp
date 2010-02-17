/*
 * $Id: D10ContentPackage.cpp,v 1.2 2010/02/17 16:04:23 philipn Exp $
 *
 * D10 MXF OP-1A content package
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "CommonTypes.h"
#include <libMXF++/MXFException.h>

#include "D10ContentPackage.h"

using namespace std;
using namespace mxfpp;


D10ContentPackageInt::D10ContentPackageInt()
{
    Reset();
}

D10ContentPackageInt::~D10ContentPackageInt()
{
}

void D10ContentPackageInt::Reset()
{
    mUserTimecode.hour = 0;
    mUserTimecode.min = 0;
    mUserTimecode.sec = 0;
    mUserTimecode.frame = 0;
    
    mVideoBytes.setSize(0);
    uint32_t i;
    for (i = 0; i < MAX_CP_AUDIO_TRACKS; i++)
        mAudioBytes[i].setSize(0);
}

bool D10ContentPackageInt::IsComplete(uint32_t num_audio_tracks) const
{
    if (mVideoBytes.getSize() == 0)
        return false;
    
    return GetNumAudioTracks() >= num_audio_tracks;
}

Timecode D10ContentPackageInt::GetUserTimecode() const
{
    return mUserTimecode;
}

const unsigned char* D10ContentPackageInt::GetVideo() const
{
    return mVideoBytes.getBytes();
}

unsigned int D10ContentPackageInt::GetVideoSize() const
{
    return mVideoBytes.getSize();
}

uint32_t D10ContentPackageInt::GetNumAudioTracks() const
{
    uint32_t i;
    for (i = 0; i < MAX_CP_AUDIO_TRACKS; i++) {
        if (mAudioBytes[i].getSize() == 0)
            break;
    }

    return i;
}

const unsigned char* D10ContentPackageInt::GetAudio(int index) const
{
    MXFPP_ASSERT(index >= 0 && index < MAX_CP_AUDIO_TRACKS);
    
    return mAudioBytes[index].getBytes();
}

unsigned int D10ContentPackageInt::GetAudioSize() const
{
    return mAudioBytes[0].getSize();
}

