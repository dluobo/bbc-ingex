/*
 * $Id: RecordedSource.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to represent a source recorded during a take.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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

#ifndef RecordedSource_h
#define RecordedSource_h

#include <vector>

#include "Source.h"
#include "Clip.h"

/**
For each source that you recorded during a take, there will
be a number of clips.
*/
class RecordedSource
{
public:
    ::Source & Source() { return mSource; }
    const ::Source & Source() const { return mSource; }

    int ClipCount() const { return mClips.size(); }
    ::Clip & Clip(int i) { return mClips[i]; }
    const ::Clip & Clip(int i) const { return mClips[i]; }
    void AddClip(const ::Clip & clip) { mClips.push_back(clip); }

private:
    ::Source mSource;
    std::vector<::Clip> mClips;
};

#endif #ifndef RecordedSource_h