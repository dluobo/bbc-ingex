/*
 * $Id: D10ContentPackage.cpp,v 1.1 2010/02/12 13:52:49 philipn Exp $
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


D10ContentPackage::D10ContentPackage()
{
    Reset();
}

D10ContentPackage::~D10ContentPackage()
{
}

void D10ContentPackage::Reset()
{
    mLTC.hour = 0;
    mLTC.min = 0;
    mLTC.sec = 0;
    mLTC.frame = 0;
    
    mVideoBytes.setSize(0);
    uint32_t i;
    for (i = 0; i < MAX_CP_AUDIO_TRACKS; i++)
        mAudioBytes[i].setSize(0);
}

bool D10ContentPackage::IsComplete(uint32_t num_audio_tracks)
{
    if (mVideoBytes.getSize() == 0)
        return false;
    
    uint32_t i;
    for (i = 0; i < num_audio_tracks; i++) {
        if (mAudioBytes[i].getSize() == 0)
            return false;
    }
    
    return true;
}

