/*
 * $Id: XSourceTrackConfig.cpp,v 1.1 2009/10/12 18:47:10 john_f Exp $
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

#include "XSourceTrackConfig.h"
#include "XMCFile.h"

using namespace std;
using namespace xercesc;



XSourceTrackConfig::XSourceTrackConfig()
{
    mId = -1;
    mNumber = -1;
    mDataDef = 0;
}

XSourceTrackConfig::~XSourceTrackConfig()
{
}

void XSourceTrackConfig::Parse(DOMElement *element)
{
    mName = XMCFile::ParseStringAttr(element, "Name");
    mId = XMCFile::ParseIntAttr(element, "Id");
    mNumber = XMCFile::ParseIntAttr(element, "Number");
    mDataDef = XMCFile::ParseDataDefAttr(element, "DataDef");
}

