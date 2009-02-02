/*
 * $Id: TimecodeComponentBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey TimecodeComponentBase::setKey = MXF_SET_K(TimecodeComponent);


TimecodeComponentBase::TimecodeComponentBase(HeaderMetadata* headerMetadata)
: StructuralComponent(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

TimecodeComponentBase::TimecodeComponentBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: StructuralComponent(headerMetadata, cMetadataSet)
{}

TimecodeComponentBase::~TimecodeComponentBase()
{}


uint16_t TimecodeComponentBase::getRoundedTimecodeBase() const
{
    return getUInt16Item(&MXF_ITEM_K(TimecodeComponent, RoundedTimecodeBase));
}

int64_t TimecodeComponentBase::getStartTimecode() const
{
    return getPositionItem(&MXF_ITEM_K(TimecodeComponent, StartTimecode));
}

bool TimecodeComponentBase::getDropFrame() const
{
    return getBooleanItem(&MXF_ITEM_K(TimecodeComponent, DropFrame));
}

void TimecodeComponentBase::setRoundedTimecodeBase(uint16_t value)
{
    setUInt16Item(&MXF_ITEM_K(TimecodeComponent, RoundedTimecodeBase), value);
}

void TimecodeComponentBase::setStartTimecode(int64_t value)
{
    setPositionItem(&MXF_ITEM_K(TimecodeComponent, StartTimecode), value);
}

void TimecodeComponentBase::setDropFrame(bool value)
{
    setBooleanItem(&MXF_ITEM_K(TimecodeComponent, DropFrame), value);
}

