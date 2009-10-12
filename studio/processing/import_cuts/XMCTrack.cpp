/*
 * $Id: XMCTrack.cpp,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Parse Multi-Camera Track from XML file.
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

#include "XMCTrack.h"
#include "XMCFile.h"
#include "ICException.h"

using namespace std;
using namespace xercesc;



XMCTrack::XMCTrack()
{
    mIndex = -1;
    mNumber = -1;
}

XMCTrack::~XMCTrack()
{
    map<int, XMCSelector*>::iterator iter;
    for (iter = mSelectors.begin(); iter != mSelectors.end(); iter++)
        delete iter->second;
}

void XMCTrack::Parse(DOMElement *element)
{
    mIndex = XMCFile::ParseIntAttr(element, "Index");
    mNumber = XMCFile::ParseIntAttr(element, "Number");

    // McSelector child elements
    DOMNodeList *children = element->getChildNodes();
    const XMLSize_t children_count = children->getLength();
    XMLSize_t i;
    for (i = 0; i < children_count; i++) {
        DOMNode *node = children->item(i);
        
        if (node->getNodeType() && node->getNodeType() == DOMNode::ELEMENT_NODE) {
            DOMElement *element = dynamic_cast<xercesc::DOMElement*>(node);
            
            if (XMLString::equals(element->getTagName(), Xml("McSelector"))) {
                auto_ptr<XMCSelector> mc_selector(new XMCSelector());
                mc_selector->Parse(element);
                
                if (!mSelectors.insert(make_pair(mc_selector->Index(), mc_selector.get())).second)
                    throw ICException("McSelector index is not unique");
                mc_selector.release();
            }
        }
    }
}

