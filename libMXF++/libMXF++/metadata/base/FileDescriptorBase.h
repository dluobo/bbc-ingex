/*
 * $Id: FileDescriptorBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_FILEDESCRIPTOR_BASE_H__
#define __MXFPP_FILEDESCRIPTOR_BASE_H__



#include <libMXF++/metadata/GenericDescriptor.h>


namespace mxfpp
{


class FileDescriptorBase : public GenericDescriptor
{
public:
    friend class MetadataSetFactory<FileDescriptorBase>;
    static const mxfKey setKey;

public:
    FileDescriptorBase(HeaderMetadata* headerMetadata);
    virtual ~FileDescriptorBase();


   // getters

   bool haveLinkedTrackID() const;
   uint32_t getLinkedTrackID() const;
   mxfRational getSampleRate() const;
   bool haveContainerDuration() const;
   int64_t getContainerDuration() const;
   mxfUL getEssenceContainer() const;
   bool haveCodec() const;
   mxfUL getCodec() const;


   // setters

   void setLinkedTrackID(uint32_t value);
   void setSampleRate(mxfRational value);
   void setContainerDuration(int64_t value);
   void setEssenceContainer(mxfUL value);
   void setCodec(mxfUL value);


protected:
    FileDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
