/*
 * $Id: SourcePackageBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey SourcePackageBase::setKey = MXF_SET_K(SourcePackage);


SourcePackageBase::SourcePackageBase(HeaderMetadata* headerMetadata)
: GenericPackage(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

SourcePackageBase::SourcePackageBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: GenericPackage(headerMetadata, cMetadataSet)
{}

SourcePackageBase::~SourcePackageBase()
{}


bool SourcePackageBase::haveDescriptor() const
{
    return haveItem(&MXF_ITEM_K(SourcePackage, Descriptor));
}

GenericDescriptor* SourcePackageBase::getDescriptor() const
{
    auto_ptr<MetadataSet> obj(getStrongRefItem(&MXF_ITEM_K(SourcePackage, Descriptor)));
    MXFPP_CHECK(dynamic_cast<GenericDescriptor*>(obj.get()) != 0);
    return dynamic_cast<GenericDescriptor*>(obj.release());
}

void SourcePackageBase::setDescriptor(GenericDescriptor* value)
{
    setStrongRefItem(&MXF_ITEM_K(SourcePackage, Descriptor), value);
}

