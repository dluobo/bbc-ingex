/*
 * $Id: OPAtomClipReader.cpp,v 1.2 2009/10/23 09:05:21 philipn Exp $
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

#include <libMXF++/MXF.h>

#include "OPAtomClipReader.h"

using namespace std;
using namespace mxfpp;



pair<OPAtomOpenResult, string> OPAtomClipReader::Open(const vector<std::string> &track_filenames,
                                                      OPAtomClipReader **clip_reader)
{
    MXFPP_ASSERT(!track_filenames.empty());
    
    vector<OPAtomTrackReader*> track_readers;
    string filename;
    try
    {
        OPAtomOpenResult result;
        OPAtomTrackReader *track_reader;
        size_t i;
        for (i = 0; i < track_filenames.size(); i++) {
            filename = track_filenames[i];
            
            result = OPAtomTrackReader::Open(filename, &track_reader);
            if (result != OP_ATOM_SUCCESS)
                throw result;
            track_readers.push_back(track_reader);
        }
        
        *clip_reader = new OPAtomClipReader(track_readers);
        
        return make_pair(OP_ATOM_SUCCESS, "");
    }
    catch (const OPAtomOpenResult &ex)
    {
        size_t i;
        for (i = 0; i < track_readers.size(); i++)
            delete track_readers[i];
     
        return make_pair(ex, filename);
    }
    catch (...)
    {
        size_t i;
        for (i = 0; i < track_readers.size(); i++)
            delete track_readers[i];
     
        return make_pair(OP_ATOM_FAIL, filename);
    }
}

string OPAtomClipReader::ErrorToString(OPAtomOpenResult result)
{
    return OPAtomTrackReader::ErrorToString(result);
}



OPAtomClipReader::OPAtomClipReader(const std::vector<OPAtomTrackReader*> &track_readers)
{
    MXFPP_ASSERT(!track_readers.empty());
    
    mTrackReaders = track_readers;
    mPosition = 0;
    mDuration = -1;
    
    GetDuration();
}

OPAtomClipReader::~OPAtomClipReader()
{
    size_t i;
    for (i = 0; i < mTrackReaders.size(); i++)
        delete mTrackReaders[i];
}

int64_t OPAtomClipReader::GetDuration()
{
    if (mDuration >= 0)
        return mDuration;
    
    int64_t min_duration = -1;
    int64_t duration = -1;
    size_t i;
    for (i = 0; i < mTrackReaders.size(); i++) {
        duration = mTrackReaders[i]->GetDuration();
        if (duration < 0) {
            min_duration = -1;
            break;
        }
        
        if (min_duration < 0 || duration < min_duration)
            min_duration = duration;
    }
    
    mDuration = min_duration;
    
    return mDuration;
}

int64_t OPAtomClipReader::DetermineDuration()
{
    int64_t min_duration = -1;
    int64_t duration = -1;
    size_t i;
    for (i = 0; i < mTrackReaders.size(); i++) {
        duration = mTrackReaders[i]->DetermineDuration();
        if (duration < 0) {
            min_duration = -1;
            break;
        }
        
        if (min_duration < 0 || duration < min_duration)
            min_duration = duration;
    }
    
    mDuration = min_duration;
    
    return mDuration;
}

int64_t OPAtomClipReader::GetPosition()
{
    return mPosition;
}

const OPAtomContentPackage* OPAtomClipReader::Read()
{
    int64_t prev_position = GetPosition();
    
    size_t i;
    for (i = 0; i < mTrackReaders.size(); i++) {
        if (!mTrackReaders[i]->Read(&mContentPackage)) {
            size_t j;
            for (j = 0; j <= i; j++)
                mTrackReaders[j]->Seek(prev_position);
            return 0;
        }
    }
    
    mPosition++;
    
    return &mContentPackage;
}

bool OPAtomClipReader::Seek(int64_t position)
{
    int64_t prev_position = GetPosition();
    
    size_t i;
    for (i = 0; i < mTrackReaders.size(); i++) {
        if (!mTrackReaders[i]->Seek(position)) {
            size_t j;
            for (j = 0; j <= i; j++)
                mTrackReaders[j]->Seek(prev_position);
            return false;
        }
    }
    
    mPosition = position;
    
    return true;
}

bool OPAtomClipReader::IsEOF()
{
    size_t i;
    for (i = 0; i < mTrackReaders.size(); i++) {
        if (mTrackReaders[i]->IsEOF())
            return true;
    }
    
    return false;
}

