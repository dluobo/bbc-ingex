/*
 * $Id: OPAtomClipReader.h,v 1.2 2009/10/23 09:05:21 philipn Exp $
 *
 * Read a clip consisting of a set of MXF OP-Atom files
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#ifndef __OPATOM_CLIP_READER_H__
#define __OPATOM_CLIP_READER_H__

#include <vector>
#include <map>

#include "OPAtomTrackReader.h"


class OPAtomClipReader
{
public:
    static std::pair<OPAtomOpenResult, std::string> Open(const std::vector<std::string> &track_filenames,
                                                         OPAtomClipReader **clip_reader);
    static std::string ErrorToString(OPAtomOpenResult result);
    
public:
    ~OPAtomClipReader();

public:
    int64_t GetDuration();
    int64_t DetermineDuration();
    int64_t GetPosition();
    
    const OPAtomContentPackage* Read();
    
    bool Seek(int64_t position);
    
    bool IsEOF();


    const std::vector<OPAtomTrackReader*>& GetTrackReaders() { return mTrackReaders; }

private:
    OPAtomClipReader(const std::vector<OPAtomTrackReader*> &track_readers);

private:
    OPAtomContentPackage mContentPackage;
    
    std::vector<OPAtomTrackReader*> mTrackReaders;
    
    int64_t mPosition;
    int64_t mDuration;
};



#endif

