/*
 * $Id: IdentificationBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey IdentificationBase::setKey = MXF_SET_K(Identification);


IdentificationBase::IdentificationBase(HeaderMetadata* headerMetadata)
: InterchangeObject(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

IdentificationBase::IdentificationBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: InterchangeObject(headerMetadata, cMetadataSet)
{}

IdentificationBase::~IdentificationBase()
{}


mxfUUID IdentificationBase::getThisGenerationUID() const
{
    return getUUIDItem(&MXF_ITEM_K(Identification, ThisGenerationUID));
}

std::string IdentificationBase::getCompanyName() const
{
    return getStringItem(&MXF_ITEM_K(Identification, CompanyName));
}

std::string IdentificationBase::getProductName() const
{
    return getStringItem(&MXF_ITEM_K(Identification, ProductName));
}

bool IdentificationBase::haveProductVersion() const
{
    return haveItem(&MXF_ITEM_K(Identification, ProductVersion));
}

mxfProductVersion IdentificationBase::getProductVersion() const
{
    return getProductVersionItem(&MXF_ITEM_K(Identification, ProductVersion));
}

std::string IdentificationBase::getVersionString() const
{
    return getStringItem(&MXF_ITEM_K(Identification, VersionString));
}

mxfUUID IdentificationBase::getProductUID() const
{
    return getUUIDItem(&MXF_ITEM_K(Identification, ProductUID));
}

mxfTimestamp IdentificationBase::getModificationDate() const
{
    return getTimestampItem(&MXF_ITEM_K(Identification, ModificationDate));
}

bool IdentificationBase::haveToolkitVersion() const
{
    return haveItem(&MXF_ITEM_K(Identification, ToolkitVersion));
}

mxfProductVersion IdentificationBase::getToolkitVersion() const
{
    return getProductVersionItem(&MXF_ITEM_K(Identification, ToolkitVersion));
}

bool IdentificationBase::havePlatform() const
{
    return haveItem(&MXF_ITEM_K(Identification, Platform));
}

std::string IdentificationBase::getPlatform() const
{
    return getStringItem(&MXF_ITEM_K(Identification, Platform));
}

void IdentificationBase::setThisGenerationUID(mxfUUID value)
{
    setUUIDItem(&MXF_ITEM_K(Identification, ThisGenerationUID), value);
}

void IdentificationBase::setCompanyName(std::string value)
{
    setStringItem(&MXF_ITEM_K(Identification, CompanyName), value);
}

void IdentificationBase::setProductName(std::string value)
{
    setStringItem(&MXF_ITEM_K(Identification, ProductName), value);
}

void IdentificationBase::setProductVersion(mxfProductVersion value)
{
    setProductVersionItem(&MXF_ITEM_K(Identification, ProductVersion), value);
}

void IdentificationBase::setVersionString(std::string value)
{
    setStringItem(&MXF_ITEM_K(Identification, VersionString), value);
}

void IdentificationBase::setProductUID(mxfUUID value)
{
    setUUIDItem(&MXF_ITEM_K(Identification, ProductUID), value);
}

void IdentificationBase::setModificationDate(mxfTimestamp value)
{
    setTimestampItem(&MXF_ITEM_K(Identification, ModificationDate), value);
}

void IdentificationBase::setToolkitVersion(mxfProductVersion value)
{
    setProductVersionItem(&MXF_ITEM_K(Identification, ToolkitVersion), value);
}

void IdentificationBase::setPlatform(std::string value)
{
    setStringItem(&MXF_ITEM_K(Identification, Platform), value);
}

