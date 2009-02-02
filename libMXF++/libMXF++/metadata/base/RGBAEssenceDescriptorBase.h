/*
 * $Id: RGBAEssenceDescriptorBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_RGBAESSENCEDESCRIPTOR_BASE_H__
#define __MXFPP_RGBAESSENCEDESCRIPTOR_BASE_H__



#include <libMXF++/metadata/GenericPictureEssenceDescriptor.h>


namespace mxfpp
{


class RGBAEssenceDescriptorBase : public GenericPictureEssenceDescriptor
{
public:
    friend class MetadataSetFactory<RGBAEssenceDescriptorBase>;
    static const mxfKey setKey;

public:
    RGBAEssenceDescriptorBase(HeaderMetadata* headerMetadata);
    virtual ~RGBAEssenceDescriptorBase();


   // getters

   bool haveComponentMaxRef() const;
   uint32_t getComponentMaxRef() const;
   bool haveComponentMinRef() const;
   uint32_t getComponentMinRef() const;
   bool haveAlphaMaxRef() const;
   uint32_t getAlphaMaxRef() const;
   bool haveAlphaMinRef() const;
   uint32_t getAlphaMinRef() const;
   bool haveScanningDirection() const;
   uint8_t getScanningDirection() const;
   bool havePixelLayout() const;
   std::vector<mxfRGBALayoutComponent> getPixelLayout() const;
   bool havePalette() const;
   ByteArray getPalette() const;
   bool havePaletteLayout() const;
   std::vector<mxfRGBALayoutComponent> getPaletteLayout() const;


   // setters

   void setComponentMaxRef(uint32_t value);
   void setComponentMinRef(uint32_t value);
   void setAlphaMaxRef(uint32_t value);
   void setAlphaMinRef(uint32_t value);
   void setScanningDirection(uint8_t value);
   void setPixelLayout(const std::vector<mxfRGBALayoutComponent>& value);
   void appendPixelLayout(mxfRGBALayoutComponent value);
   void setPalette(ByteArray value);
   void setPaletteLayout(const std::vector<mxfRGBALayoutComponent>& value);
   void appendPaletteLayout(mxfRGBALayoutComponent value);


protected:
    RGBAEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
