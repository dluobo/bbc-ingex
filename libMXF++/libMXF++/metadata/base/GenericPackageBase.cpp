/*
 * $Id: GenericPackageBase.cpp,v 1.2 2009/04/16 17:52:50 john_f Exp $
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


const mxfKey GenericPackageBase::setKey = MXF_SET_K(GenericPackage);


GenericPackageBase::GenericPackageBase(HeaderMetadata* headerMetadata)
: InterchangeObject(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

GenericPackageBase::GenericPackageBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: InterchangeObject(headerMetadata, cMetadataSet)
{}

GenericPackageBase::~GenericPackageBase()
{}


mxfUMID GenericPackageBase::getPackageUID() const
{
    return getUMIDItem(&MXF_ITEM_K(GenericPackage, PackageUID));
}

bool GenericPackageBase::haveName() const
{
    return haveItem(&MXF_ITEM_K(GenericPackage, Name));
}

std::string GenericPackageBase::getName() const
{
    return getStringItem(&MXF_ITEM_K(GenericPackage, Name));
}

mxfTimestamp GenericPackageBase::getPackageCreationDate() const
{
    return getTimestampItem(&MXF_ITEM_K(GenericPackage, PackageCreationDate));
}

mxfTimestamp GenericPackageBase::getPackageModifiedDate() const
{
    return getTimestampItem(&MXF_ITEM_K(GenericPackage, PackageModifiedDate));
}

std::vector<GenericTrack*> GenericPackageBase::getTracks() const
{
    vector<GenericTrack*> result;
    auto_ptr<ObjectIterator> iter(getStrongRefArrayItem(&MXF_ITEM_K(GenericPackage, Tracks)));
    while (iter->next())
    {
        MXFPP_CHECK(dynamic_cast<GenericTrack*>(iter->get()) != 0);
        result.push_back(dynamic_cast<GenericTrack*>(iter->get()));
    }
    return result;
}

void GenericPackageBase::setPackageUID(mxfUMID value)
{
    setUMIDItem(&MXF_ITEM_K(GenericPackage, PackageUID), value);
}

void GenericPackageBase::setName(std::string value)
{
    setStringItem(&MXF_ITEM_K(GenericPackage, Name), value);
}

void GenericPackageBase::setPackageCreationDate(mxfTimestamp value)
{
    setTimestampItem(&MXF_ITEM_K(GenericPackage, PackageCreationDate), value);
}

void GenericPackageBase::setPackageModifiedDate(mxfTimestamp value)
{
    setTimestampItem(&MXF_ITEM_K(GenericPackage, PackageModifiedDate), value);
}

void GenericPackageBase::setTracks(const std::vector<GenericTrack*>& value)
{
    WrapObjectVectorIterator<GenericTrack> iter(value);
    setStrongRefArrayItem(&MXF_ITEM_K(GenericPackage, Tracks), &iter);
}

void GenericPackageBase::appendTracks(GenericTrack* value)
{
    appendStrongRefArrayItem(&MXF_ITEM_K(GenericPackage, Tracks), value);
}

