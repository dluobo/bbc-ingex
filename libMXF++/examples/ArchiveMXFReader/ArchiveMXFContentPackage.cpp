/*
 * $Id: ArchiveMXFContentPackage.cpp,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include "CommonTypes.h"
#include <libMXF++/MXFException.h>

#include "ArchiveMXFContentPackage.h"


using namespace std;
using namespace mxfpp;


ArchiveMXFContentPackage::ArchiveMXFContentPackage()
: _position(-1)
{}

ArchiveMXFContentPackage::~ArchiveMXFContentPackage()
{}

int ArchiveMXFContentPackage::getNumAudioTracks() const
{
    return _numAudioTracks;
}

int64_t ArchiveMXFContentPackage::getPosition() const
{
    return _position;
}

Timecode ArchiveMXFContentPackage::getVITC() const
{
    return _vitc;
}

Timecode ArchiveMXFContentPackage::getLTC() const
{
    return _ltc;
}

const DynamicByteArray* ArchiveMXFContentPackage::getVideo() const
{
    return &_videoBytes;
}

const DynamicByteArray* ArchiveMXFContentPackage::getAudio(int index) const
{
    if (index < 0 || index > MAX_CP_AUDIO_TRACKS)
    {
        throw MXFException("Audio index %d is out of range\n", index);
    }
    
    return &_audioBytes[index];
}


