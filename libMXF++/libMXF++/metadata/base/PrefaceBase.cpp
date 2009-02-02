/*
 * $Id: PrefaceBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey PrefaceBase::setKey = MXF_SET_K(Preface);


PrefaceBase::PrefaceBase(HeaderMetadata* headerMetadata)
: InterchangeObject(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

PrefaceBase::PrefaceBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: InterchangeObject(headerMetadata, cMetadataSet)
{}

PrefaceBase::~PrefaceBase()
{}


mxfTimestamp PrefaceBase::getLastModifiedDate() const
{
    return getTimestampItem(&MXF_ITEM_K(Preface, LastModifiedDate));
}

uint16_t PrefaceBase::getVersion() const
{
    return getVersionTypeItem(&MXF_ITEM_K(Preface, Version));
}

bool PrefaceBase::haveObjectModelVersion() const
{
    return haveItem(&MXF_ITEM_K(Preface, ObjectModelVersion));
}

uint32_t PrefaceBase::getObjectModelVersion() const
{
    return getUInt32Item(&MXF_ITEM_K(Preface, ObjectModelVersion));
}

bool PrefaceBase::havePrimaryPackage() const
{
    return haveItem(&MXF_ITEM_K(Preface, PrimaryPackage));
}

GenericPackage* PrefaceBase::getPrimaryPackage() const
{
    auto_ptr<MetadataSet> obj(getWeakRefItem(&MXF_ITEM_K(Preface, PrimaryPackage)));
    MXFPP_CHECK(dynamic_cast<GenericPackage*>(obj.get()) != 0);
    return dynamic_cast<GenericPackage*>(obj.release());
}

std::vector<Identification*> PrefaceBase::getIdentifications() const
{
    vector<Identification*> result;
    auto_ptr<ObjectIterator> iter(getStrongRefArrayItem(&MXF_ITEM_K(Preface, Identifications)));
    while (iter->next())
    {
        MXFPP_CHECK(dynamic_cast<Identification*>(iter->get()) != 0);
        result.push_back(dynamic_cast<Identification*>(iter->get()));
    }
    return result;
}

ContentStorage* PrefaceBase::getContentStorage() const
{
    auto_ptr<MetadataSet> obj(getStrongRefItem(&MXF_ITEM_K(Preface, ContentStorage)));
    MXFPP_CHECK(dynamic_cast<ContentStorage*>(obj.get()) != 0);
    return dynamic_cast<ContentStorage*>(obj.release());
}

mxfUL PrefaceBase::getOperationalPattern() const
{
    return getULItem(&MXF_ITEM_K(Preface, OperationalPattern));
}

std::vector<mxfUL> PrefaceBase::getEssenceContainers() const
{
    return getULArrayItem(&MXF_ITEM_K(Preface, EssenceContainers));
}

std::vector<mxfUL> PrefaceBase::getDMSchemes() const
{
    return getULArrayItem(&MXF_ITEM_K(Preface, DMSchemes));
}

void PrefaceBase::setLastModifiedDate(mxfTimestamp value)
{
    setTimestampItem(&MXF_ITEM_K(Preface, LastModifiedDate), value);
}

void PrefaceBase::setVersion(uint16_t value)
{
    setVersionTypeItem(&MXF_ITEM_K(Preface, Version), value);
}

void PrefaceBase::setObjectModelVersion(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(Preface, ObjectModelVersion), value);
}

void PrefaceBase::setPrimaryPackage(GenericPackage* value)
{
    setWeakRefItem(&MXF_ITEM_K(Preface, PrimaryPackage), value);
}

void PrefaceBase::setIdentifications(const std::vector<Identification*>& value)
{
    WrapObjectVectorIterator<Identification> iter(value);
    setStrongRefArrayItem(&MXF_ITEM_K(Preface, Identifications), &iter);
}

void PrefaceBase::appendIdentifications(Identification* value)
{
    appendStrongRefArrayItem(&MXF_ITEM_K(Preface, Identifications), value);
}

void PrefaceBase::setContentStorage(ContentStorage* value)
{
    setStrongRefItem(&MXF_ITEM_K(Preface, ContentStorage), value);
}

void PrefaceBase::setOperationalPattern(mxfUL value)
{
    setULItem(&MXF_ITEM_K(Preface, OperationalPattern), value);
}

void PrefaceBase::setEssenceContainers(const std::vector<mxfUL>& value)
{
    setULArrayItem(&MXF_ITEM_K(Preface, EssenceContainers), value);
}

void PrefaceBase::appendEssenceContainers(mxfUL value)
{
    appendULArrayItem(&MXF_ITEM_K(Preface, EssenceContainers), value);
}

void PrefaceBase::setDMSchemes(const std::vector<mxfUL>& value)
{
    setULArrayItem(&MXF_ITEM_K(Preface, DMSchemes), value);
}

void PrefaceBase::appendDMSchemes(mxfUL value)
{
    appendULArrayItem(&MXF_ITEM_K(Preface, DMSchemes), value);
}

