/*
 * $Id: MPEGVideoDescriptorBase.cpp,v 1.1 2009/06/18 12:08:16 philipn Exp $
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


const mxfKey MPEGVideoDescriptorBase::setKey = MXF_SET_K(MPEGVideoDescriptor);


MPEGVideoDescriptorBase::MPEGVideoDescriptorBase(HeaderMetadata* headerMetadata)
: CDCIEssenceDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

MPEGVideoDescriptorBase::MPEGVideoDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: CDCIEssenceDescriptor(headerMetadata, cMetadataSet)
{}

MPEGVideoDescriptorBase::~MPEGVideoDescriptorBase()
{}


bool MPEGVideoDescriptorBase::haveSingleSequence() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, SingleSequence));
}

bool MPEGVideoDescriptorBase::getSingleSequence() const
{
    return getBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, SingleSequence));
}

bool MPEGVideoDescriptorBase::haveConstantBFrames() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, ConstantBFrames));
}

bool MPEGVideoDescriptorBase::getConstantBFrames() const
{
    return getBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, ConstantBFrames));
}

bool MPEGVideoDescriptorBase::haveCodedContentType() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, CodedContentType));
}

uint8_t MPEGVideoDescriptorBase::getCodedContentType() const
{
    return getUInt8Item(&MXF_ITEM_K(MPEGVideoDescriptor, CodedContentType));
}

bool MPEGVideoDescriptorBase::haveLowDelay() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, LowDelay));
}

bool MPEGVideoDescriptorBase::getLowDelay() const
{
    return getBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, LowDelay));
}

bool MPEGVideoDescriptorBase::haveClosedGOP() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, ClosedGOP));
}

bool MPEGVideoDescriptorBase::getClosedGOP() const
{
    return getBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, ClosedGOP));
}

bool MPEGVideoDescriptorBase::haveIdenticalGOP() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, IdenticalGOP));
}

bool MPEGVideoDescriptorBase::getIdenticalGOP() const
{
    return getBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, IdenticalGOP));
}

bool MPEGVideoDescriptorBase::haveMaxGOP() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, MaxGOP));
}

uint16_t MPEGVideoDescriptorBase::getMaxGOP() const
{
    return getUInt16Item(&MXF_ITEM_K(MPEGVideoDescriptor, MaxGOP));
}

bool MPEGVideoDescriptorBase::haveBPictureCount() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, BPictureCount));
}

uint16_t MPEGVideoDescriptorBase::getBPictureCount() const
{
    return getUInt16Item(&MXF_ITEM_K(MPEGVideoDescriptor, BPictureCount));
}

bool MPEGVideoDescriptorBase::haveBitRate() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, BitRate));
}

uint32_t MPEGVideoDescriptorBase::getBitRate() const
{
    return getUInt32Item(&MXF_ITEM_K(MPEGVideoDescriptor, BitRate));
}

bool MPEGVideoDescriptorBase::haveProfileAndLevel() const
{
    return haveItem(&MXF_ITEM_K(MPEGVideoDescriptor, ProfileAndLevel));
}

uint8_t MPEGVideoDescriptorBase::getProfileAndLevel() const
{
    return getUInt8Item(&MXF_ITEM_K(MPEGVideoDescriptor, ProfileAndLevel));
}

void MPEGVideoDescriptorBase::setSingleSequence(bool value)
{
    setBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, SingleSequence), value);
}

void MPEGVideoDescriptorBase::setConstantBFrames(bool value)
{
    setBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, ConstantBFrames), value);
}

void MPEGVideoDescriptorBase::setCodedContentType(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(MPEGVideoDescriptor, CodedContentType), value);
}

void MPEGVideoDescriptorBase::setLowDelay(bool value)
{
    setBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, LowDelay), value);
}

void MPEGVideoDescriptorBase::setClosedGOP(bool value)
{
    setBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, ClosedGOP), value);
}

void MPEGVideoDescriptorBase::setIdenticalGOP(bool value)
{
    setBooleanItem(&MXF_ITEM_K(MPEGVideoDescriptor, IdenticalGOP), value);
}

void MPEGVideoDescriptorBase::setMaxGOP(uint16_t value)
{
    setUInt16Item(&MXF_ITEM_K(MPEGVideoDescriptor, MaxGOP), value);
}

void MPEGVideoDescriptorBase::setBPictureCount(uint16_t value)
{
    setUInt16Item(&MXF_ITEM_K(MPEGVideoDescriptor, BPictureCount), value);
}

void MPEGVideoDescriptorBase::setBitRate(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(MPEGVideoDescriptor, BitRate), value);
}

void MPEGVideoDescriptorBase::setProfileAndLevel(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(MPEGVideoDescriptor, ProfileAndLevel), value);
}

