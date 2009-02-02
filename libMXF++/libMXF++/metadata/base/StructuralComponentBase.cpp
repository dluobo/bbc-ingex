/*
 * $Id: StructuralComponentBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey StructuralComponentBase::setKey = MXF_SET_K(StructuralComponent);


StructuralComponentBase::StructuralComponentBase(HeaderMetadata* headerMetadata)
: InterchangeObject(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

StructuralComponentBase::StructuralComponentBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: InterchangeObject(headerMetadata, cMetadataSet)
{}

StructuralComponentBase::~StructuralComponentBase()
{}


mxfUL StructuralComponentBase::getDataDefinition() const
{
    return getULItem(&MXF_ITEM_K(StructuralComponent, DataDefinition));
}

bool StructuralComponentBase::haveDuration() const
{
    return haveItem(&MXF_ITEM_K(StructuralComponent, Duration));
}

int64_t StructuralComponentBase::getDuration() const
{
    return getLengthItem(&MXF_ITEM_K(StructuralComponent, Duration));
}

void StructuralComponentBase::setDataDefinition(mxfUL value)
{
    setULItem(&MXF_ITEM_K(StructuralComponent, DataDefinition), value);
}

void StructuralComponentBase::setDuration(int64_t value)
{
    setLengthItem(&MXF_ITEM_K(StructuralComponent, Duration), value);
}

