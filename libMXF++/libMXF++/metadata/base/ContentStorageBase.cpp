/*
 * $Id: ContentStorageBase.cpp,v 1.2 2009/04/16 17:52:50 john_f Exp $
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


const mxfKey ContentStorageBase::setKey = MXF_SET_K(ContentStorage);


ContentStorageBase::ContentStorageBase(HeaderMetadata* headerMetadata)
: InterchangeObject(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

ContentStorageBase::ContentStorageBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: InterchangeObject(headerMetadata, cMetadataSet)
{}

ContentStorageBase::~ContentStorageBase()
{}


std::vector<GenericPackage*> ContentStorageBase::getPackages() const
{
    vector<GenericPackage*> result;
    auto_ptr<ObjectIterator> iter(getStrongRefArrayItem(&MXF_ITEM_K(ContentStorage, Packages)));
    while (iter->next())
    {
        MXFPP_CHECK(dynamic_cast<GenericPackage*>(iter->get()) != 0);
        result.push_back(dynamic_cast<GenericPackage*>(iter->get()));
    }
    return result;
}

bool ContentStorageBase::haveEssenceContainerData() const
{
    return haveItem(&MXF_ITEM_K(ContentStorage, EssenceContainerData));
}

std::vector<EssenceContainerData*> ContentStorageBase::getEssenceContainerData() const
{
    vector<EssenceContainerData*> result;
    auto_ptr<ObjectIterator> iter(getStrongRefArrayItem(&MXF_ITEM_K(ContentStorage, EssenceContainerData)));
    while (iter->next())
    {
        MXFPP_CHECK(dynamic_cast<EssenceContainerData*>(iter->get()) != 0);
        result.push_back(dynamic_cast<EssenceContainerData*>(iter->get()));
    }
    return result;
}

void ContentStorageBase::setPackages(const std::vector<GenericPackage*>& value)
{
    WrapObjectVectorIterator<GenericPackage> iter(value);
    setStrongRefArrayItem(&MXF_ITEM_K(ContentStorage, Packages), &iter);
}

void ContentStorageBase::appendPackages(GenericPackage* value)
{
    appendStrongRefArrayItem(&MXF_ITEM_K(ContentStorage, Packages), value);
}

void ContentStorageBase::setEssenceContainerData(const std::vector<EssenceContainerData*>& value)
{
    WrapObjectVectorIterator<EssenceContainerData> iter(value);
    setStrongRefArrayItem(&MXF_ITEM_K(ContentStorage, EssenceContainerData), &iter);
}

void ContentStorageBase::appendEssenceContainerData(EssenceContainerData* value)
{
    appendStrongRefArrayItem(&MXF_ITEM_K(ContentStorage, EssenceContainerData), value);
}

