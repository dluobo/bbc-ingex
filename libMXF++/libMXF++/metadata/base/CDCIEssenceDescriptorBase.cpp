/*
 * $Id: CDCIEssenceDescriptorBase.cpp,v 1.1 2009/02/02 05:14:34 stuart_hc Exp $
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


const mxfKey CDCIEssenceDescriptorBase::setKey = MXF_SET_K(CDCIEssenceDescriptor);


CDCIEssenceDescriptorBase::CDCIEssenceDescriptorBase(HeaderMetadata* headerMetadata)
: GenericPictureEssenceDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

CDCIEssenceDescriptorBase::CDCIEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: GenericPictureEssenceDescriptor(headerMetadata, cMetadataSet)
{}

CDCIEssenceDescriptorBase::~CDCIEssenceDescriptorBase()
{}


bool CDCIEssenceDescriptorBase::haveComponentDepth() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, ComponentDepth));
}

uint32_t CDCIEssenceDescriptorBase::getComponentDepth() const
{
    return getUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, ComponentDepth));
}

bool CDCIEssenceDescriptorBase::haveHorizontalSubsampling() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, HorizontalSubsampling));
}

uint32_t CDCIEssenceDescriptorBase::getHorizontalSubsampling() const
{
    return getUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, HorizontalSubsampling));
}

bool CDCIEssenceDescriptorBase::haveVerticalSubsampling() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, VerticalSubsampling));
}

uint32_t CDCIEssenceDescriptorBase::getVerticalSubsampling() const
{
    return getUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, VerticalSubsampling));
}

bool CDCIEssenceDescriptorBase::haveColorSiting() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, ColorSiting));
}

uint8_t CDCIEssenceDescriptorBase::getColorSiting() const
{
    return getUInt8Item(&MXF_ITEM_K(CDCIEssenceDescriptor, ColorSiting));
}

bool CDCIEssenceDescriptorBase::haveReversedByteOrder() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, ReversedByteOrder));
}

bool CDCIEssenceDescriptorBase::getReversedByteOrder() const
{
    return getBooleanItem(&MXF_ITEM_K(CDCIEssenceDescriptor, ReversedByteOrder));
}

bool CDCIEssenceDescriptorBase::havePaddingBits() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, PaddingBits));
}

int16_t CDCIEssenceDescriptorBase::getPaddingBits() const
{
    return getInt16Item(&MXF_ITEM_K(CDCIEssenceDescriptor, PaddingBits));
}

bool CDCIEssenceDescriptorBase::haveAlphaSampleDepth() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, AlphaSampleDepth));
}

uint32_t CDCIEssenceDescriptorBase::getAlphaSampleDepth() const
{
    return getUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, AlphaSampleDepth));
}

bool CDCIEssenceDescriptorBase::haveBlackRefLevel() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, BlackRefLevel));
}

uint32_t CDCIEssenceDescriptorBase::getBlackRefLevel() const
{
    return getUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, BlackRefLevel));
}

bool CDCIEssenceDescriptorBase::haveWhiteReflevel() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, WhiteReflevel));
}

uint32_t CDCIEssenceDescriptorBase::getWhiteReflevel() const
{
    return getUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, WhiteReflevel));
}

bool CDCIEssenceDescriptorBase::haveColorRange() const
{
    return haveItem(&MXF_ITEM_K(CDCIEssenceDescriptor, ColorRange));
}

uint32_t CDCIEssenceDescriptorBase::getColorRange() const
{
    return getUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, ColorRange));
}

void CDCIEssenceDescriptorBase::setComponentDepth(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, ComponentDepth), value);
}

void CDCIEssenceDescriptorBase::setHorizontalSubsampling(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, HorizontalSubsampling), value);
}

void CDCIEssenceDescriptorBase::setVerticalSubsampling(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, VerticalSubsampling), value);
}

void CDCIEssenceDescriptorBase::setColorSiting(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(CDCIEssenceDescriptor, ColorSiting), value);
}

void CDCIEssenceDescriptorBase::setReversedByteOrder(bool value)
{
    setBooleanItem(&MXF_ITEM_K(CDCIEssenceDescriptor, ReversedByteOrder), value);
}

void CDCIEssenceDescriptorBase::setPaddingBits(int16_t value)
{
    setInt16Item(&MXF_ITEM_K(CDCIEssenceDescriptor, PaddingBits), value);
}

void CDCIEssenceDescriptorBase::setAlphaSampleDepth(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, AlphaSampleDepth), value);
}

void CDCIEssenceDescriptorBase::setBlackRefLevel(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, BlackRefLevel), value);
}

void CDCIEssenceDescriptorBase::setWhiteReflevel(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, WhiteReflevel), value);
}

void CDCIEssenceDescriptorBase::setColorRange(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(CDCIEssenceDescriptor, ColorRange), value);
}

