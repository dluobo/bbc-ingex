/*
 * $Id: FileDescriptorBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey FileDescriptorBase::setKey = MXF_SET_K(FileDescriptor);


FileDescriptorBase::FileDescriptorBase(HeaderMetadata* headerMetadata)
: GenericDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

FileDescriptorBase::FileDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: GenericDescriptor(headerMetadata, cMetadataSet)
{}

FileDescriptorBase::~FileDescriptorBase()
{}


bool FileDescriptorBase::haveLinkedTrackID() const
{
    return haveItem(&MXF_ITEM_K(FileDescriptor, LinkedTrackID));
}

uint32_t FileDescriptorBase::getLinkedTrackID() const
{
    return getUInt32Item(&MXF_ITEM_K(FileDescriptor, LinkedTrackID));
}

mxfRational FileDescriptorBase::getSampleRate() const
{
    return getRationalItem(&MXF_ITEM_K(FileDescriptor, SampleRate));
}

bool FileDescriptorBase::haveContainerDuration() const
{
    return haveItem(&MXF_ITEM_K(FileDescriptor, ContainerDuration));
}

int64_t FileDescriptorBase::getContainerDuration() const
{
    return getLengthItem(&MXF_ITEM_K(FileDescriptor, ContainerDuration));
}

mxfUL FileDescriptorBase::getEssenceContainer() const
{
    return getULItem(&MXF_ITEM_K(FileDescriptor, EssenceContainer));
}

bool FileDescriptorBase::haveCodec() const
{
    return haveItem(&MXF_ITEM_K(FileDescriptor, Codec));
}

mxfUL FileDescriptorBase::getCodec() const
{
    return getULItem(&MXF_ITEM_K(FileDescriptor, Codec));
}

void FileDescriptorBase::setLinkedTrackID(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(FileDescriptor, LinkedTrackID), value);
}

void FileDescriptorBase::setSampleRate(mxfRational value)
{
    setRationalItem(&MXF_ITEM_K(FileDescriptor, SampleRate), value);
}

void FileDescriptorBase::setContainerDuration(int64_t value)
{
    setLengthItem(&MXF_ITEM_K(FileDescriptor, ContainerDuration), value);
}

void FileDescriptorBase::setEssenceContainer(mxfUL value)
{
    setULItem(&MXF_ITEM_K(FileDescriptor, EssenceContainer), value);
}

void FileDescriptorBase::setCodec(mxfUL value)
{
    setULItem(&MXF_ITEM_K(FileDescriptor, Codec), value);
}

