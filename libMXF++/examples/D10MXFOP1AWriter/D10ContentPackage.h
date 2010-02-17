/*
 * $Id: D10ContentPackage.h,v 1.2 2010/02/17 16:04:24 philipn Exp $
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

#ifndef __D10_CONTENT_PACKAGE_H__
#define __D10_CONTENT_PACKAGE_H__

#include "CommonTypes.h"
#include "DynamicByteArray.h"

#define MAX_CP_AUDIO_TRACKS     8



class D10ContentPackage
{
public:
    virtual Timecode GetUserTimecode() const = 0;

    virtual const unsigned char* GetVideo() const = 0;
    virtual unsigned int GetVideoSize() const = 0;
    
    virtual uint32_t GetNumAudioTracks() const = 0;
    virtual const unsigned char* GetAudio(int index) const = 0;
    virtual unsigned int GetAudioSize() const = 0;
};


class D10ContentPackageInt : public D10ContentPackage
{
public:
    D10ContentPackageInt();
    ~D10ContentPackageInt();

    void Reset();
    bool IsComplete(uint32_t num_audio_tracks) const;
    
public:
    // from D10ContentPackage
    virtual Timecode GetUserTimecode() const;
    virtual const unsigned char* GetVideo() const;
    virtual unsigned int GetVideoSize() const;
    virtual uint32_t GetNumAudioTracks() const;
    virtual const unsigned char* GetAudio(int index) const;
    virtual unsigned int GetAudioSize() const;
    
public:
    Timecode mUserTimecode;
    DynamicByteArray mVideoBytes;
    DynamicByteArray mAudioBytes[MAX_CP_AUDIO_TRACKS];
};


#endif

