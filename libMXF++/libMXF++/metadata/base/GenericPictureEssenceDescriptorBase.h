/*
 * $Id: GenericPictureEssenceDescriptorBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_GENERICPICTUREESSENCEDESCRIPTOR_BASE_H__
#define __MXFPP_GENERICPICTUREESSENCEDESCRIPTOR_BASE_H__



#include <libMXF++/metadata/FileDescriptor.h>


namespace mxfpp
{


class GenericPictureEssenceDescriptorBase : public FileDescriptor
{
public:
    friend class MetadataSetFactory<GenericPictureEssenceDescriptorBase>;
    static const mxfKey setKey;

public:
    GenericPictureEssenceDescriptorBase(HeaderMetadata* headerMetadata);
    virtual ~GenericPictureEssenceDescriptorBase();


   // getters

   bool haveSignalStandard() const;
   uint8_t getSignalStandard() const;
   bool haveFrameLayout() const;
   uint8_t getFrameLayout() const;
   bool haveStoredWidth() const;
   uint32_t getStoredWidth() const;
   bool haveStoredHeight() const;
   uint32_t getStoredHeight() const;
   bool haveStoredF2Offset() const;
   int32_t getStoredF2Offset() const;
   bool haveSampledWidth() const;
   uint32_t getSampledWidth() const;
   bool haveSampledHeight() const;
   uint32_t getSampledHeight() const;
   bool haveSampledXOffset() const;
   int32_t getSampledXOffset() const;
   bool haveSampledYOffset() const;
   int32_t getSampledYOffset() const;
   bool haveDisplayHeight() const;
   uint32_t getDisplayHeight() const;
   bool haveDisplayWidth() const;
   uint32_t getDisplayWidth() const;
   bool haveDisplayXOffset() const;
   int32_t getDisplayXOffset() const;
   bool haveDisplayYOffset() const;
   int32_t getDisplayYOffset() const;
   bool haveDisplayF2Offset() const;
   int32_t getDisplayF2Offset() const;
   bool haveAspectRatio() const;
   mxfRational getAspectRatio() const;
   bool haveActiveFormatDescriptor() const;
   uint8_t getActiveFormatDescriptor() const;
   bool haveVideoLineMap() const;
   std::vector<int32_t> getVideoLineMap() const;
   bool haveAlphaTransparency() const;
   uint8_t getAlphaTransparency() const;
   bool haveCaptureGamma() const;
   mxfUL getCaptureGamma() const;
   bool haveImageAlignmentOffset() const;
   uint32_t getImageAlignmentOffset() const;
   bool haveImageStartOffset() const;
   uint32_t getImageStartOffset() const;
   bool haveImageEndOffset() const;
   uint32_t getImageEndOffset() const;
   bool haveFieldDominance() const;
   uint8_t getFieldDominance() const;
   bool havePictureEssenceCoding() const;
   mxfUL getPictureEssenceCoding() const;


   // setters

   void setSignalStandard(uint8_t value);
   void setFrameLayout(uint8_t value);
   void setStoredWidth(uint32_t value);
   void setStoredHeight(uint32_t value);
   void setStoredF2Offset(int32_t value);
   void setSampledWidth(uint32_t value);
   void setSampledHeight(uint32_t value);
   void setSampledXOffset(int32_t value);
   void setSampledYOffset(int32_t value);
   void setDisplayHeight(uint32_t value);
   void setDisplayWidth(uint32_t value);
   void setDisplayXOffset(int32_t value);
   void setDisplayYOffset(int32_t value);
   void setDisplayF2Offset(int32_t value);
   void setAspectRatio(mxfRational value);
   void setActiveFormatDescriptor(uint8_t value);
   void setVideoLineMap(const std::vector<int32_t>& value);
   void appendVideoLineMap(int32_t value);
   void setAlphaTransparency(uint8_t value);
   void setCaptureGamma(mxfUL value);
   void setImageAlignmentOffset(uint32_t value);
   void setImageStartOffset(uint32_t value);
   void setImageEndOffset(uint32_t value);
   void setFieldDominance(uint8_t value);
   void setPictureEssenceCoding(mxfUL value);


protected:
    GenericPictureEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
