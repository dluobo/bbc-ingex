/*
 * $Id: RGBAEssenceDescriptorBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey RGBAEssenceDescriptorBase::setKey = MXF_SET_K(RGBAEssenceDescriptor);


RGBAEssenceDescriptorBase::RGBAEssenceDescriptorBase(HeaderMetadata* headerMetadata)
: GenericPictureEssenceDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

RGBAEssenceDescriptorBase::RGBAEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: GenericPictureEssenceDescriptor(headerMetadata, cMetadataSet)
{}

RGBAEssenceDescriptorBase::~RGBAEssenceDescriptorBase()
{}


bool RGBAEssenceDescriptorBase::haveComponentMaxRef() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, ComponentMaxRef));
}

uint32_t RGBAEssenceDescriptorBase::getComponentMaxRef() const
{
    return getUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, ComponentMaxRef));
}

bool RGBAEssenceDescriptorBase::haveComponentMinRef() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, ComponentMinRef));
}

uint32_t RGBAEssenceDescriptorBase::getComponentMinRef() const
{
    return getUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, ComponentMinRef));
}

bool RGBAEssenceDescriptorBase::haveAlphaMaxRef() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, AlphaMaxRef));
}

uint32_t RGBAEssenceDescriptorBase::getAlphaMaxRef() const
{
    return getUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, AlphaMaxRef));
}

bool RGBAEssenceDescriptorBase::haveAlphaMinRef() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, AlphaMinRef));
}

uint32_t RGBAEssenceDescriptorBase::getAlphaMinRef() const
{
    return getUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, AlphaMinRef));
}

bool RGBAEssenceDescriptorBase::haveScanningDirection() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, ScanningDirection));
}

uint8_t RGBAEssenceDescriptorBase::getScanningDirection() const
{
    return getUInt8Item(&MXF_ITEM_K(RGBAEssenceDescriptor, ScanningDirection));
}

bool RGBAEssenceDescriptorBase::havePixelLayout() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PixelLayout));
}

std::vector<mxfRGBALayoutComponent> RGBAEssenceDescriptorBase::getPixelLayout() const
{
    return getRGBALayoutComponentArrayItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PixelLayout));
}

bool RGBAEssenceDescriptorBase::havePalette() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, Palette));
}

ByteArray RGBAEssenceDescriptorBase::getPalette() const
{
    return getRawBytesItem(&MXF_ITEM_K(RGBAEssenceDescriptor, Palette));
}

bool RGBAEssenceDescriptorBase::havePaletteLayout() const
{
    return haveItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PaletteLayout));
}

std::vector<mxfRGBALayoutComponent> RGBAEssenceDescriptorBase::getPaletteLayout() const
{
    return getRGBALayoutComponentArrayItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PaletteLayout));
}

void RGBAEssenceDescriptorBase::setComponentMaxRef(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, ComponentMaxRef), value);
}

void RGBAEssenceDescriptorBase::setComponentMinRef(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, ComponentMinRef), value);
}

void RGBAEssenceDescriptorBase::setAlphaMaxRef(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, AlphaMaxRef), value);
}

void RGBAEssenceDescriptorBase::setAlphaMinRef(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(RGBAEssenceDescriptor, AlphaMinRef), value);
}

void RGBAEssenceDescriptorBase::setScanningDirection(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(RGBAEssenceDescriptor, ScanningDirection), value);
}

void RGBAEssenceDescriptorBase::setPixelLayout(const std::vector<mxfRGBALayoutComponent>& value)
{
    setRGBALayoutComponentArrayItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PixelLayout), value);
}

void RGBAEssenceDescriptorBase::appendPixelLayout(mxfRGBALayoutComponent value)
{
    appendRGBALayoutComponentArrayItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PixelLayout), value);
}

void RGBAEssenceDescriptorBase::setPalette(ByteArray value)
{
    setRawBytesItem(&MXF_ITEM_K(RGBAEssenceDescriptor, Palette), value);
}

void RGBAEssenceDescriptorBase::setPaletteLayout(const std::vector<mxfRGBALayoutComponent>& value)
{
    setRGBALayoutComponentArrayItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PaletteLayout), value);
}

void RGBAEssenceDescriptorBase::appendPaletteLayout(mxfRGBALayoutComponent value)
{
    appendRGBALayoutComponentArrayItem(&MXF_ITEM_K(RGBAEssenceDescriptor, PaletteLayout), value);
}

