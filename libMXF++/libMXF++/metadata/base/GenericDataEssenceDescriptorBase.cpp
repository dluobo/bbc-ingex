/*
 * $Id: GenericDataEssenceDescriptorBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey GenericDataEssenceDescriptorBase::setKey = MXF_SET_K(GenericDataEssenceDescriptor);


GenericDataEssenceDescriptorBase::GenericDataEssenceDescriptorBase(HeaderMetadata* headerMetadata)
: FileDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

GenericDataEssenceDescriptorBase::GenericDataEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: FileDescriptor(headerMetadata, cMetadataSet)
{}

GenericDataEssenceDescriptorBase::~GenericDataEssenceDescriptorBase()
{}


bool GenericDataEssenceDescriptorBase::haveDataEssenceCoding() const
{
    return haveItem(&MXF_ITEM_K(GenericDataEssenceDescriptor, DataEssenceCoding));
}

mxfUL GenericDataEssenceDescriptorBase::getDataEssenceCoding() const
{
    return getULItem(&MXF_ITEM_K(GenericDataEssenceDescriptor, DataEssenceCoding));
}

void GenericDataEssenceDescriptorBase::setDataEssenceCoding(mxfUL value)
{
    setULItem(&MXF_ITEM_K(GenericDataEssenceDescriptor, DataEssenceCoding), value);
}

