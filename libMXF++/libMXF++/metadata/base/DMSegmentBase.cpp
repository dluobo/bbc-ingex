/*
 * $Id: DMSegmentBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey DMSegmentBase::setKey = MXF_SET_K(DMSegment);


DMSegmentBase::DMSegmentBase(HeaderMetadata* headerMetadata)
: StructuralComponent(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

DMSegmentBase::DMSegmentBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: StructuralComponent(headerMetadata, cMetadataSet)
{}

DMSegmentBase::~DMSegmentBase()
{}


int64_t DMSegmentBase::getEventStartPosition() const
{
    return getPositionItem(&MXF_ITEM_K(DMSegment, EventStartPosition));
}

bool DMSegmentBase::haveEventComment() const
{
    return haveItem(&MXF_ITEM_K(DMSegment, EventComment));
}

std::string DMSegmentBase::getEventComment() const
{
    return getStringItem(&MXF_ITEM_K(DMSegment, EventComment));
}

bool DMSegmentBase::haveTrackIDs() const
{
    return haveItem(&MXF_ITEM_K(DMSegment, TrackIDs));
}

std::vector<uint32_t> DMSegmentBase::getTrackIDs() const
{
    return getUInt32ArrayItem(&MXF_ITEM_K(DMSegment, TrackIDs));
}

bool DMSegmentBase::haveDMFramework() const
{
    return haveItem(&MXF_ITEM_K(DMSegment, DMFramework));
}

DMFramework* DMSegmentBase::getDMFramework() const
{
    auto_ptr<MetadataSet> obj(getStrongRefItem(&MXF_ITEM_K(DMSegment, DMFramework)));
    MXFPP_CHECK(dynamic_cast<DMFramework*>(obj.get()) != 0);
    return dynamic_cast<DMFramework*>(obj.release());
}

void DMSegmentBase::setEventStartPosition(int64_t value)
{
    setPositionItem(&MXF_ITEM_K(DMSegment, EventStartPosition), value);
}

void DMSegmentBase::setEventComment(std::string value)
{
    setStringItem(&MXF_ITEM_K(DMSegment, EventComment), value);
}

void DMSegmentBase::setTrackIDs(const std::vector<uint32_t>& value)
{
    setUInt32ArrayItem(&MXF_ITEM_K(DMSegment, TrackIDs), value);
}

void DMSegmentBase::appendTrackIDs(uint32_t value)
{
    appendUInt32ArrayItem(&MXF_ITEM_K(DMSegment, TrackIDs), value);
}

void DMSegmentBase::setDMFramework(DMFramework* value)
{
    setStrongRefItem(&MXF_ITEM_K(DMSegment, DMFramework), value);
}

