/*
 * $Id: XSourceTrackConfig.h,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Read Source Track Config from XML file.
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

#ifndef __X_SOURCE_TRACK_CONFIG_H__
#define __X_SOURCE_TRACK_CONFIG_H__


#include <string>

#include <xercesc/dom/DOMElement.hpp>


class XSourceTrackConfig
{
public:
    XSourceTrackConfig();
    ~XSourceTrackConfig();

    void Parse(xercesc::DOMElement *element);
    
    std::string Name() const { return mName; }
    int Id() const { return mId; }
    int Number() const { return mNumber; }
    int DataDef() const { return mDataDef; }
    
private:
    std::string mName;
    int mId;
    int mNumber;
    int mDataDef;
};

#endif

