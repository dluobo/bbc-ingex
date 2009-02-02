/*
 * $Id: WaveAudioDescriptorBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey WaveAudioDescriptorBase::setKey = MXF_SET_K(WaveAudioDescriptor);


WaveAudioDescriptorBase::WaveAudioDescriptorBase(HeaderMetadata* headerMetadata)
: GenericSoundEssenceDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

WaveAudioDescriptorBase::WaveAudioDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: GenericSoundEssenceDescriptor(headerMetadata, cMetadataSet)
{}

WaveAudioDescriptorBase::~WaveAudioDescriptorBase()
{}


uint16_t WaveAudioDescriptorBase::getBlockAlign() const
{
    return getUInt16Item(&MXF_ITEM_K(WaveAudioDescriptor, BlockAlign));
}

bool WaveAudioDescriptorBase::haveSequenceOffset() const
{
    return haveItem(&MXF_ITEM_K(WaveAudioDescriptor, SequenceOffset));
}

uint8_t WaveAudioDescriptorBase::getSequenceOffset() const
{
    return getUInt8Item(&MXF_ITEM_K(WaveAudioDescriptor, SequenceOffset));
}

uint32_t WaveAudioDescriptorBase::getAvgBps() const
{
    return getUInt32Item(&MXF_ITEM_K(WaveAudioDescriptor, AvgBps));
}

void WaveAudioDescriptorBase::setBlockAlign(uint16_t value)
{
    setUInt16Item(&MXF_ITEM_K(WaveAudioDescriptor, BlockAlign), value);
}

void WaveAudioDescriptorBase::setSequenceOffset(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(WaveAudioDescriptor, SequenceOffset), value);
}

void WaveAudioDescriptorBase::setAvgBps(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(WaveAudioDescriptor, AvgBps), value);
}

