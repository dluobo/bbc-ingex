/*
 * $Id: XSourceConfig.cpp,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Parse Source Config from XML file.
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

#include "XSourceConfig.h"
#include "XMCFile.h"
#include "ICException.h"

using namespace std;
using namespace xercesc;



XSourceConfig::XSourceConfig()
{
}

XSourceConfig::~XSourceConfig()
{
    map<int, XSourceTrackConfig*>::iterator iter;
    for (iter = mTracks.begin(); iter != mTracks.end(); iter++)
        delete iter->second;
}

void XSourceConfig::Parse(DOMElement *element)
{
    mName = XMCFile::ParseStringAttr(element, "Name");
    mLocationName = XMCFile::ParseStringAttr(element, "LocationName");
    
    // SourceTrackConfig child elements
    DOMNodeList *children = element->getChildNodes();
    const XMLSize_t children_count = children->getLength();
    XMLSize_t i;
    for (i = 0; i < children_count; i++) {
        DOMNode *node = children->item(i);
        
        if (node->getNodeType() && node->getNodeType() == DOMNode::ELEMENT_NODE) {
            DOMElement *element = dynamic_cast<xercesc::DOMElement*>(node);
            
            if (XMLString::equals(element->getTagName(), Xml("SourceTrackConfig"))) {
                auto_ptr<XSourceTrackConfig> st_config(new XSourceTrackConfig());
                st_config->Parse(element);
                
                if (!mTracks.insert(make_pair(st_config->Id(), st_config.get())).second)
                    throw ICException("SourceTrackConfig Index is not unique");
                st_config.release();
            }
        }
    }
}
