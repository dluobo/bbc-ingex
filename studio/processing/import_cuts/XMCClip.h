/*
 * $Id: XMCClip.h,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Parse Multi-Camera Clip from XML file.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#ifndef __X_MC_CLIP_H__
#define __X_MC_CLIP_H__


#include <map>
#include <string>

#include <xercesc/dom/DOMElement.hpp>

#include "XMCTrack.h"


class XMCClip
{
public:
    XMCClip();
    ~XMCClip();

    void Parse(xercesc::DOMElement *element);
    
    std::string Name() const { return mName; }
    std::map<int, XMCTrack*>& Tracks() { return mTracks; }
    
private:
    std::string mName;
    std::map<int, XMCTrack*> mTracks;
};




#endif
