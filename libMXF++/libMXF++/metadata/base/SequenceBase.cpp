/*
 * $Id: SequenceBase.cpp,v 1.2 2009/04/16 17:52:50 john_f Exp $
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


const mxfKey SequenceBase::setKey = MXF_SET_K(Sequence);


SequenceBase::SequenceBase(HeaderMetadata* headerMetadata)
: StructuralComponent(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

SequenceBase::SequenceBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: StructuralComponent(headerMetadata, cMetadataSet)
{}

SequenceBase::~SequenceBase()
{}


std::vector<StructuralComponent*> SequenceBase::getStructuralComponents() const
{
    vector<StructuralComponent*> result;
    auto_ptr<ObjectIterator> iter(getStrongRefArrayItem(&MXF_ITEM_K(Sequence, StructuralComponents)));
    while (iter->next())
    {
        MXFPP_CHECK(dynamic_cast<StructuralComponent*>(iter->get()) != 0);
        result.push_back(dynamic_cast<StructuralComponent*>(iter->get()));
    }
    return result;
}

void SequenceBase::setStructuralComponents(const std::vector<StructuralComponent*>& value)
{
    WrapObjectVectorIterator<StructuralComponent> iter(value);
    setStrongRefArrayItem(&MXF_ITEM_K(Sequence, StructuralComponents), &iter);
}

void SequenceBase::appendStructuralComponents(StructuralComponent* value)
{
    appendStrongRefArrayItem(&MXF_ITEM_K(Sequence, StructuralComponents), value);
}

