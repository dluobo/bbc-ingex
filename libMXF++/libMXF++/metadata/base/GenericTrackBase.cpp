/*
 * $Id: GenericTrackBase.cpp,v 1.2 2009/04/16 17:52:50 john_f Exp $
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
#include <memory>


using namespace std;
using namespace mxfpp;


const mxfKey GenericTrackBase::setKey = MXF_SET_K(GenericTrack);


GenericTrackBase::GenericTrackBase(HeaderMetadata* headerMetadata)
: InterchangeObject(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

GenericTrackBase::GenericTrackBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: InterchangeObject(headerMetadata, cMetadataSet)
{}

GenericTrackBase::~GenericTrackBase()
{}


bool GenericTrackBase::haveTrackID() const
{
    return haveItem(&MXF_ITEM_K(GenericTrack, TrackID));
}

uint32_t GenericTrackBase::getTrackID() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericTrack, TrackID));
}

uint32_t GenericTrackBase::getTrackNumber() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericTrack, TrackNumber));
}

bool GenericTrackBase::haveTrackName() const
{
    return haveItem(&MXF_ITEM_K(GenericTrack, TrackName));
}

std::string GenericTrackBase::getTrackName() const
{
    return getStringItem(&MXF_ITEM_K(GenericTrack, TrackName));
}

StructuralComponent* GenericTrackBase::getSequence() const
{
    auto_ptr<MetadataSet> obj(getStrongRefItem(&MXF_ITEM_K(GenericTrack, Sequence)));
    MXFPP_CHECK(dynamic_cast<StructuralComponent*>(obj.get()) != 0);
    return dynamic_cast<StructuralComponent*>(obj.release());
}

void GenericTrackBase::setTrackID(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericTrack, TrackID), value);
}

void GenericTrackBase::setTrackNumber(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericTrack, TrackNumber), value);
}

void GenericTrackBase::setTrackName(std::string value)
{
    setStringItem(&MXF_ITEM_K(GenericTrack, TrackName), value);
}

void GenericTrackBase::setSequence(StructuralComponent* value)
{
    setStrongRefItem(&MXF_ITEM_K(GenericTrack, Sequence), value);
}

