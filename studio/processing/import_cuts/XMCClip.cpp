/*
 * $Id: XMCClip.cpp,v 1.1 2009/10/12 18:47:10 john_f Exp $
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

#include <memory>

#include <XmlTools.h>

#include "XMCClip.h"
#include "XMCFile.h"
#include "ICException.h"

using namespace std;
using namespace xercesc;



XMCClip::XMCClip()
{
}

XMCClip::~XMCClip()
{
    map<int, XMCTrack*>::iterator iter;
    for (iter = mTracks.begin(); iter != mTracks.end(); iter++)
        delete iter->second;
}

void XMCClip::Parse(DOMElement *element)
{
    mName = XMCFile::ParseStringAttr(element, "Name");

    // McTrack child elements
    DOMNodeList *children = element->getChildNodes();
    const XMLSize_t children_count = children->getLength();
    XMLSize_t i;
    for (i = 0; i < children_count; i++) {
        DOMNode *node = children->item(i);
        
        if (node->getNodeType() && node->getNodeType() == DOMNode::ELEMENT_NODE) {
            DOMElement *element = dynamic_cast<xercesc::DOMElement*>(node);
            
            if (XMLString::equals(element->getTagName(), Xml("McTrack"))) {
                auto_ptr<XMCTrack> mc_track(new XMCTrack());
                mc_track->Parse(element);
                
                if (!mTracks.insert(make_pair(mc_track->Index(), mc_track.get())).second)
                    throw ICException("McTrack index is not unique");
                mc_track.release();
            }
        }
    }
}
