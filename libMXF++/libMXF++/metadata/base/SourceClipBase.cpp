/*
 * $Id: SourceClipBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey SourceClipBase::setKey = MXF_SET_K(SourceClip);


SourceClipBase::SourceClipBase(HeaderMetadata* headerMetadata)
: StructuralComponent(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

SourceClipBase::SourceClipBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: StructuralComponent(headerMetadata, cMetadataSet)
{}

SourceClipBase::~SourceClipBase()
{}


int64_t SourceClipBase::getStartPosition() const
{
    return getPositionItem(&MXF_ITEM_K(SourceClip, StartPosition));
}

mxfUMID SourceClipBase::getSourcePackageID() const
{
    return getUMIDItem(&MXF_ITEM_K(SourceClip, SourcePackageID));
}

uint32_t SourceClipBase::getSourceTrackID() const
{
    return getUInt32Item(&MXF_ITEM_K(SourceClip, SourceTrackID));
}

void SourceClipBase::setStartPosition(int64_t value)
{
    setPositionItem(&MXF_ITEM_K(SourceClip, StartPosition), value);
}

void SourceClipBase::setSourcePackageID(mxfUMID value)
{
    setUMIDItem(&MXF_ITEM_K(SourceClip, SourcePackageID), value);
}

void SourceClipBase::setSourceTrackID(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(SourceClip, SourceTrackID), value);
}

