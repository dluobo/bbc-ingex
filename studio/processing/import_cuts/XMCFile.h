/*
 * $Id: XMCFile.h,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Parse Multi-Camera cuts file.
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

#ifndef __X_MC_FILE_H__
#define __X_MC_FILE_H__


#include <set>
#include <string>
#include <map>
#include <vector>

#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/dom/DOMElement.hpp>

#include <DataTypes.h>

#include "XLocation.h"
#include "XSourceConfig.h"
#include "XMCClip.h"
#include "XMCCut.h"



class XMCFile
{
public:
    static std::string ParseStringAttr(const xercesc::DOMElement *element, std::string attr_name);
    static int ParseIntAttr(const xercesc::DOMElement *element, std::string attr_name);
    static prodauto::Date ParseDateAttr(const xercesc::DOMElement *element, std::string attr_name);
    static int64_t ParseInt64Attr(const xercesc::DOMElement *element, std::string attr_name);
    static prodauto::Rational ParseRationalAttr(const xercesc::DOMElement *element, std::string attr_name);
    static int ParseDataDefAttr(const xercesc::DOMElement *element, std::string attr_name);
    
public:
    XMCFile();
    ~XMCFile();
    
    bool Parse(std::string filename);
    
    std::set<XLocation*, XLocationComparator>& Locations() { return mLocations; }
    std::map<std::string, XSourceConfig*>& SourceConfigs() { return mSourceConfigs; }
    std::map<std::string, XMCClip*>& Clips() { return mClips; }
    std::vector<XMCCut*>& Cuts() { return mCuts; }

private:
    void Clear();
    
private:
    xercesc::XercesDOMParser *mParser;

    std::set<XLocation*, XLocationComparator> mLocations;
    std::map<std::string, XSourceConfig*> mSourceConfigs;
    std::map<std::string, XMCClip*> mClips;
    std::vector<XMCCut*> mCuts;
};


#endif
