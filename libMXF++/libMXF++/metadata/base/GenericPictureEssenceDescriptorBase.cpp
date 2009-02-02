/*
 * $Id: GenericPictureEssenceDescriptorBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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


const mxfKey GenericPictureEssenceDescriptorBase::setKey = MXF_SET_K(GenericPictureEssenceDescriptor);


GenericPictureEssenceDescriptorBase::GenericPictureEssenceDescriptorBase(HeaderMetadata* headerMetadata)
: FileDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

GenericPictureEssenceDescriptorBase::GenericPictureEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: FileDescriptor(headerMetadata, cMetadataSet)
{}

GenericPictureEssenceDescriptorBase::~GenericPictureEssenceDescriptorBase()
{}


bool GenericPictureEssenceDescriptorBase::haveSignalStandard() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SignalStandard));
}

uint8_t GenericPictureEssenceDescriptorBase::getSignalStandard() const
{
    return getUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SignalStandard));
}

bool GenericPictureEssenceDescriptorBase::haveFrameLayout() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, FrameLayout));
}

uint8_t GenericPictureEssenceDescriptorBase::getFrameLayout() const
{
    return getUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, FrameLayout));
}

bool GenericPictureEssenceDescriptorBase::haveStoredWidth() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredWidth));
}

uint32_t GenericPictureEssenceDescriptorBase::getStoredWidth() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredWidth));
}

bool GenericPictureEssenceDescriptorBase::haveStoredHeight() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredHeight));
}

uint32_t GenericPictureEssenceDescriptorBase::getStoredHeight() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredHeight));
}

bool GenericPictureEssenceDescriptorBase::haveStoredF2Offset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredF2Offset));
}

int32_t GenericPictureEssenceDescriptorBase::getStoredF2Offset() const
{
    return getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredF2Offset));
}

bool GenericPictureEssenceDescriptorBase::haveSampledWidth() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledWidth));
}

uint32_t GenericPictureEssenceDescriptorBase::getSampledWidth() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledWidth));
}

bool GenericPictureEssenceDescriptorBase::haveSampledHeight() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledHeight));
}

uint32_t GenericPictureEssenceDescriptorBase::getSampledHeight() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledHeight));
}

bool GenericPictureEssenceDescriptorBase::haveSampledXOffset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledXOffset));
}

int32_t GenericPictureEssenceDescriptorBase::getSampledXOffset() const
{
    return getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledXOffset));
}

bool GenericPictureEssenceDescriptorBase::haveSampledYOffset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledYOffset));
}

int32_t GenericPictureEssenceDescriptorBase::getSampledYOffset() const
{
    return getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledYOffset));
}

bool GenericPictureEssenceDescriptorBase::haveDisplayHeight() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayHeight));
}

uint32_t GenericPictureEssenceDescriptorBase::getDisplayHeight() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayHeight));
}

bool GenericPictureEssenceDescriptorBase::haveDisplayWidth() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayWidth));
}

uint32_t GenericPictureEssenceDescriptorBase::getDisplayWidth() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayWidth));
}

bool GenericPictureEssenceDescriptorBase::haveDisplayXOffset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayXOffset));
}

int32_t GenericPictureEssenceDescriptorBase::getDisplayXOffset() const
{
    return getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayXOffset));
}

bool GenericPictureEssenceDescriptorBase::haveDisplayYOffset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayYOffset));
}

int32_t GenericPictureEssenceDescriptorBase::getDisplayYOffset() const
{
    return getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayYOffset));
}

bool GenericPictureEssenceDescriptorBase::haveDisplayF2Offset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayF2Offset));
}

int32_t GenericPictureEssenceDescriptorBase::getDisplayF2Offset() const
{
    return getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayF2Offset));
}

bool GenericPictureEssenceDescriptorBase::haveAspectRatio() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, AspectRatio));
}

mxfRational GenericPictureEssenceDescriptorBase::getAspectRatio() const
{
    return getRationalItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, AspectRatio));
}

bool GenericPictureEssenceDescriptorBase::haveActiveFormatDescriptor() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ActiveFormatDescriptor));
}

uint8_t GenericPictureEssenceDescriptorBase::getActiveFormatDescriptor() const
{
    return getUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ActiveFormatDescriptor));
}

bool GenericPictureEssenceDescriptorBase::haveVideoLineMap() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, VideoLineMap));
}

std::vector<int32_t> GenericPictureEssenceDescriptorBase::getVideoLineMap() const
{
    return getInt32ArrayItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, VideoLineMap));
}

bool GenericPictureEssenceDescriptorBase::haveAlphaTransparency() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, AlphaTransparency));
}

uint8_t GenericPictureEssenceDescriptorBase::getAlphaTransparency() const
{
    return getUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, AlphaTransparency));
}

bool GenericPictureEssenceDescriptorBase::haveCaptureGamma() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, CaptureGamma));
}

mxfUL GenericPictureEssenceDescriptorBase::getCaptureGamma() const
{
    return getULItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, CaptureGamma));
}

bool GenericPictureEssenceDescriptorBase::haveImageAlignmentOffset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageAlignmentOffset));
}

uint32_t GenericPictureEssenceDescriptorBase::getImageAlignmentOffset() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageAlignmentOffset));
}

bool GenericPictureEssenceDescriptorBase::haveImageStartOffset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageStartOffset));
}

uint32_t GenericPictureEssenceDescriptorBase::getImageStartOffset() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageStartOffset));
}

bool GenericPictureEssenceDescriptorBase::haveImageEndOffset() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageEndOffset));
}

uint32_t GenericPictureEssenceDescriptorBase::getImageEndOffset() const
{
    return getUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageEndOffset));
}

bool GenericPictureEssenceDescriptorBase::haveFieldDominance() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, FieldDominance));
}

uint8_t GenericPictureEssenceDescriptorBase::getFieldDominance() const
{
    return getUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, FieldDominance));
}

bool GenericPictureEssenceDescriptorBase::havePictureEssenceCoding() const
{
    return haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, PictureEssenceCoding));
}

mxfUL GenericPictureEssenceDescriptorBase::getPictureEssenceCoding() const
{
    return getULItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, PictureEssenceCoding));
}

void GenericPictureEssenceDescriptorBase::setSignalStandard(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SignalStandard), value);
}

void GenericPictureEssenceDescriptorBase::setFrameLayout(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, FrameLayout), value);
}

void GenericPictureEssenceDescriptorBase::setStoredWidth(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredWidth), value);
}

void GenericPictureEssenceDescriptorBase::setStoredHeight(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredHeight), value);
}

void GenericPictureEssenceDescriptorBase::setStoredF2Offset(int32_t value)
{
    setInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, StoredF2Offset), value);
}

void GenericPictureEssenceDescriptorBase::setSampledWidth(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledWidth), value);
}

void GenericPictureEssenceDescriptorBase::setSampledHeight(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledHeight), value);
}

void GenericPictureEssenceDescriptorBase::setSampledXOffset(int32_t value)
{
    setInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledXOffset), value);
}

void GenericPictureEssenceDescriptorBase::setSampledYOffset(int32_t value)
{
    setInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, SampledYOffset), value);
}

void GenericPictureEssenceDescriptorBase::setDisplayHeight(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayHeight), value);
}

void GenericPictureEssenceDescriptorBase::setDisplayWidth(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayWidth), value);
}

void GenericPictureEssenceDescriptorBase::setDisplayXOffset(int32_t value)
{
    setInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayXOffset), value);
}

void GenericPictureEssenceDescriptorBase::setDisplayYOffset(int32_t value)
{
    setInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayYOffset), value);
}

void GenericPictureEssenceDescriptorBase::setDisplayF2Offset(int32_t value)
{
    setInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, DisplayF2Offset), value);
}

void GenericPictureEssenceDescriptorBase::setAspectRatio(mxfRational value)
{
    setRationalItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, AspectRatio), value);
}

void GenericPictureEssenceDescriptorBase::setActiveFormatDescriptor(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ActiveFormatDescriptor), value);
}

void GenericPictureEssenceDescriptorBase::setVideoLineMap(const std::vector<int32_t>& value)
{
    setInt32ArrayItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, VideoLineMap), value);
}

void GenericPictureEssenceDescriptorBase::appendVideoLineMap(int32_t value)
{
    appendInt32ArrayItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, VideoLineMap), value);
}

void GenericPictureEssenceDescriptorBase::setAlphaTransparency(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, AlphaTransparency), value);
}

void GenericPictureEssenceDescriptorBase::setCaptureGamma(mxfUL value)
{
    setULItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, CaptureGamma), value);
}

void GenericPictureEssenceDescriptorBase::setImageAlignmentOffset(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageAlignmentOffset), value);
}

void GenericPictureEssenceDescriptorBase::setImageStartOffset(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageStartOffset), value);
}

void GenericPictureEssenceDescriptorBase::setImageEndOffset(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ImageEndOffset), value);
}

void GenericPictureEssenceDescriptorBase::setFieldDominance(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor, FieldDominance), value);
}

void GenericPictureEssenceDescriptorBase::setPictureEssenceCoding(mxfUL value)
{
    setULItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, PictureEssenceCoding), value);
}

