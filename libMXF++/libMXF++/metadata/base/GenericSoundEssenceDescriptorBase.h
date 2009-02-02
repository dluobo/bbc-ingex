/*
 * $Id: GenericSoundEssenceDescriptorBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_GENERICSOUNDESSENCEDESCRIPTOR_BASE_H__
#define __MXFPP_GENERICSOUNDESSENCEDESCRIPTOR_BASE_H__



#include <libMXF++/metadata/FileDescriptor.h>


namespace mxfpp
{


class GenericSoundEssenceDescriptorBase : public FileDescriptor
{
public:
    friend class MetadataSetFactory<GenericSoundEssenceDescriptorBase>;
    static const mxfKey setKey;

public:
    GenericSoundEssenceDescriptorBase(HeaderMetadata* headerMetadata);
    virtual ~GenericSoundEssenceDescriptorBase();


   // getters

   bool haveAudioSamplingRate() const;
   mxfRational getAudioSamplingRate() const;
   bool haveLocked() const;
   bool getLocked() const;
   bool haveAudioRefLevel() const;
   int8_t getAudioRefLevel() const;
   bool haveElectroSpatialFormulation() const;
   uint8_t getElectroSpatialFormulation() const;
   bool haveChannelCount() const;
   uint32_t getChannelCount() const;
   bool haveQuantizationBits() const;
   uint32_t getQuantizationBits() const;
   bool haveDialNorm() const;
   int8_t getDialNorm() const;
   bool haveSoundEssenceCompression() const;
   mxfUL getSoundEssenceCompression() const;


   // setters

   void setAudioSamplingRate(mxfRational value);
   void setLocked(bool value);
   void setAudioRefLevel(int8_t value);
   void setElectroSpatialFormulation(uint8_t value);
   void setChannelCount(uint32_t value);
   void setQuantizationBits(uint32_t value);
   void setDialNorm(int8_t value);
   void setSoundEssenceCompression(mxfUL value);


protected:
    GenericSoundEssenceDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
