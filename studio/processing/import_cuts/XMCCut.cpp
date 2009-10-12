/*
 * $Id: XMCCut.cpp,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Parse Multi-Camera Cut from XML file.
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

#include "XMCCut.h"
#include "XMCFile.h"

using namespace std;
using namespace prodauto;
using namespace xercesc;



XMCCut::XMCCut()
{
    mSelectorIndex = -1;
    mDate = (prodauto::Date){0, 0, 0};
    mPosition = 0;
}

XMCCut::~XMCCut()
{
}

void XMCCut::Parse(DOMElement *element)
{
    mClipName = XMCFile::ParseStringAttr(element, "ClipName");
    mSelectorIndex = XMCFile::ParseIntAttr(element, "SelectorIndex");
    mDate = XMCFile::ParseDateAttr(element, "Date");
    mPosition = XMCFile::ParseInt64Attr(element, "Position");
    mEditRate = XMCFile::ParseRationalAttr(element, "EditRate");
}
