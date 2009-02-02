/*
 * $Id: CDCIEssenceDescriptorBase.h,v 1.1 2009/02/02 05:14:34 stuart_hc Exp $
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

#ifndef __MXFPP_CDCIESSENCEDESCRIPTOR_BASE_H__
#define __MXFPP_CDCIESSENCEDESCRIPTOR_BASE_H__



#include <libMXF++/metadata/GenericPictureEssenceDescriptor.h>


namespace mxfpp
{


class CDCIEssenceDescriptorBase : public GenericPictureEssenceDescriptor
{
public:
    friend class MetadataSetFactory<CDCIEssenceDescriptorBase>;
    static const mxfKey setKey;

public:
    CDCIEssenceDescriptorBase(HeaderMetadata* headerMetadata);
    virtual ~CDCIEssenceDescriptorBase();


   // getters

   bool haveComponentDepth() const;
   uint32_t getComponentDepth() const;
   bool haveHorizontalSubsampling() const;
   uint32_t getHorizontalSubsampling() const;
   bool haveVerticalSubsampling() const;
   uint32_t getVerticalSubsampling() const;
   bool haveColorSiting() const;
   uint8_t getColorSiting() const;
   bool haveReversedByteOrder() const;
   bool getReversedByteOrder() const;
   bool havePaddingBits() const;
   int16_t getPaddingBits() const;
   bool haveAlphaSampleDepth() const;
   uint32_t getAlphaSampleDepth() const;
   bool haveBlackRefLevel() const;
   uint32_t getBlackRefLevel() const;
   bool haveWhiteReflevel() const;
   uint32_t getWhiteReflevel() const;
   bool haveColorRange() const;
   uint32_t getColorRange() const;


   // setters

   void setComponentDepth(uint32_t value);
   void setHorizontalSubsampling(uint32_t value);
   void setVerticalSubsampling(uint32_t value);
   void setColorSiting(uint8_t value);
   void setReversedByteOrder(bool value);
   void setPaddingBits(int16_t value);
   void setAlphaSampleDepth(uint32_t value);
   void setBlackRefLevel(uint32_t value);
   void setWhiteReflevel(uint32_t value);
   void setColorRange(uint32_t value);


protected:
    CDCIEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
