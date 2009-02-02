/*
 * $Id: DMSourceClipBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <libMXF++/MXF.h>


using namespace std;
using namespace mxfpp;


const mxfKey DMSourceClipBase::setKey = MXF_SET_K(DMSourceClip);


DMSourceClipBase::DMSourceClipBase(HeaderMetadata* headerMetadata)
: SourceClip(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

DMSourceClipBase::DMSourceClipBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: SourceClip(headerMetadata, cMetadataSet)
{}

DMSourceClipBase::~DMSourceClipBase()
{}


bool DMSourceClipBase::haveDMSourceClipTrackIDs() const
{
    return haveItem(&MXF_ITEM_K(DMSourceClip, DMSourceClipTrackIDs));
}

std::vector<uint32_t> DMSourceClipBase::getDMSourceClipTrackIDs() const
{
    return getUInt32ArrayItem(&MXF_ITEM_K(DMSourceClip, DMSourceClipTrackIDs));
}

void DMSourceClipBase::setDMSourceClipTrackIDs(const std::vector<uint32_t>& value)
{
    setUInt32ArrayItem(&MXF_ITEM_K(DMSourceClip, DMSourceClipTrackIDs), value);
}

void DMSourceClipBase::appendDMSourceClipTrackIDs(uint32_t value)
{
    appendUInt32ArrayItem(&MXF_ITEM_K(DMSourceClip, DMSourceClipTrackIDs), value);
}

