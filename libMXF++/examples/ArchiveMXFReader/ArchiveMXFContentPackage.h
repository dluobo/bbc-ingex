/*
 * $Id: ArchiveMXFContentPackage.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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
 
#ifndef __MXFPP_ARCHIVE_MXF_CONTENT_PACKAGE_H__
#define __MXFPP_ARCHIVE_MXF_CONTENT_PACKAGE_H__


#include "DynamicByteArray.h"


#define MAX_CP_AUDIO_TRACKS     8


namespace mxfpp
{


class ArchiveMXFReader;

class ArchiveMXFContentPackage
{
public:
    friend class ArchiveMXFReader;
    
public:
    ArchiveMXFContentPackage();
    virtual ~ArchiveMXFContentPackage();

    int getNumAudioTracks() const;
    
    int64_t getPosition() const;
    
    Timecode getVITC() const;
    Timecode getLTC() const;
    
    const DynamicByteArray* getVideo() const;
    const DynamicByteArray* getAudio(int index) const;
    
private:
    int _numAudioTracks;

    int64_t _position;
    
    int32_t _size;
    
    Timecode _vitc;
    Timecode _ltc;
    
    DynamicByteArray _videoBytes;
    DynamicByteArray _audioBytes[MAX_CP_AUDIO_TRACKS];
};



};


#endif

