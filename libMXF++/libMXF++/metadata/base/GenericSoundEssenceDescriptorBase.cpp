/*
 * $Id: GenericSoundEssenceDescriptorBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey GenericSoundEssenceDescriptorBase::setKey = MXF_SET_K(GenericSoundEssenceDescriptor);


GenericSoundEssenceDescriptorBase::GenericSoundEssenceDescriptorBase(HeaderMetadata* headerMetadata)
: FileDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

GenericSoundEssenceDescriptorBase::GenericSoundEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: FileDescriptor(headerMetadata, cMetadataSet)
{}

GenericSoundEssenceDescriptorBase::~GenericSoundEssenceDescriptorBase()
{}


bool GenericSoundEssenceDescriptorBase::haveAudioSamplingRate() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, AudioSamplingRate));
}

mxfRational GenericSoundEssenceDescriptorBase::getAudioSamplingRate() const
{
    return getRationalItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, AudioSamplingRate));
}

bool GenericSoundEssenceDescriptorBase::haveLocked() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, Locked));
}

bool GenericSoundEssenceDescriptorBase::getLocked() const
{
    return getBooleanItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, Locked));
}

bool GenericSoundEssenceDescriptorBase::haveAudioRefLevel() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, AudioRefLevel));
}

int8_t GenericSoundEssenceDescriptorBase::getAudioRefLevel() const
{
    return getInt8Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, AudioRefLevel));
}

bool GenericSoundEssenceDescriptorBase::haveElectroSpatialFormulation() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, ElectroSpatialFormulation));
}

uint8_t GenericSoundEssenceDescriptorBase::getElectroSpatialFormulation() const
{
    return getUInt8Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, ElectroSpatialFormulation));
}

bool GenericSoundEssenceDescriptorBase::haveChannelCount() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, ChannelCount));
}

uint32_t GenericSoundEssenceDescriptorBase::getChannelCount() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, ChannelCount));
}

bool GenericSoundEssenceDescriptorBase::haveQuantizationBits() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, QuantizationBits));
}

uint32_t GenericSoundEssenceDescriptorBase::getQuantizationBits() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, QuantizationBits));
}

bool GenericSoundEssenceDescriptorBase::haveDialNorm() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, DialNorm));
}

int8_t GenericSoundEssenceDescriptorBase::getDialNorm() const
{
    return getInt8Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, DialNorm));
}

bool GenericSoundEssenceDescriptorBase::haveSoundEssenceCompression() const
{
    return haveItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, SoundEssenceCompression));
}

mxfUL GenericSoundEssenceDescriptorBase::getSoundEssenceCompression() const
{
    return getULItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, SoundEssenceCompression));
}

void GenericSoundEssenceDescriptorBase::setAudioSamplingRate(mxfRational value)
{
    setRationalItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, AudioSamplingRate), value);
}

void GenericSoundEssenceDescriptorBase::setLocked(bool value)
{
    setBooleanItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, Locked), value);
}

void GenericSoundEssenceDescriptorBase::setAudioRefLevel(int8_t value)
{
    setInt8Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, AudioRefLevel), value);
}

void GenericSoundEssenceDescriptorBase::setElectroSpatialFormulation(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, ElectroSpatialFormulation), value);
}

void GenericSoundEssenceDescriptorBase::setChannelCount(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, ChannelCount), value);
}

void GenericSoundEssenceDescriptorBase::setQuantizationBits(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, QuantizationBits), value);
}

void GenericSoundEssenceDescriptorBase::setDialNorm(int8_t value)
{
    setInt8Item(&MXF_ITEM_K(GenericSoundEssenceDescriptor, DialNorm), value);
}

void GenericSoundEssenceDescriptorBase::setSoundEssenceCompression(mxfUL value)
{
    setULItem(&MXF_ITEM_K(GenericSoundEssenceDescriptor, SoundEssenceCompression), value);
}

