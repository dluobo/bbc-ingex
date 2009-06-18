/*
 * $Id: MPEGVideoDescriptorBase.h,v 1.1 2009/06/18 12:08:16 philipn Exp $
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

#ifndef __MXFPP_MPEGVIDEODESCRIPTOR_BASE_H__
#define __MXFPP_MPEGVIDEODESCRIPTOR_BASE_H__



#include <libMXF++/metadata/CDCIEssenceDescriptor.h>


namespace mxfpp
{


class MPEGVideoDescriptorBase : public CDCIEssenceDescriptor
{
public:
    friend class MetadataSetFactory<MPEGVideoDescriptorBase>;
    static const mxfKey setKey;

public:
    MPEGVideoDescriptorBase(HeaderMetadata* headerMetadata);
    virtual ~MPEGVideoDescriptorBase();


   // getters

   bool haveSingleSequence() const;
   bool getSingleSequence() const;
   bool haveConstantBFrames() const;
   bool getConstantBFrames() const;
   bool haveCodedContentType() const;
   uint8_t getCodedContentType() const;
   bool haveLowDelay() const;
   bool getLowDelay() const;
   bool haveClosedGOP() const;
   bool getClosedGOP() const;
   bool haveIdenticalGOP() const;
   bool getIdenticalGOP() const;
   bool haveMaxGOP() const;
   uint16_t getMaxGOP() const;
   bool haveBPictureCount() const;
   uint16_t getBPictureCount() const;
   bool haveBitRate() const;
   uint32_t getBitRate() const;
   bool haveProfileAndLevel() const;
   uint8_t getProfileAndLevel() const;


   // setters

   void setSingleSequence(bool value);
   void setConstantBFrames(bool value);
   void setCodedContentType(uint8_t value);
   void setLowDelay(bool value);
   void setClosedGOP(bool value);
   void setIdenticalGOP(bool value);
   void setMaxGOP(uint16_t value);
   void setBPictureCount(uint16_t value);
   void setBitRate(uint32_t value);
   void setProfileAndLevel(uint8_t value);


protected:
    MPEGVideoDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
