/*
 * $Id: PrefaceBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_PREFACE_BASE_H__
#define __MXFPP_PREFACE_BASE_H__



#include <libMXF++/metadata/InterchangeObject.h>


namespace mxfpp
{


class PrefaceBase : public InterchangeObject
{
public:
    friend class MetadataSetFactory<PrefaceBase>;
    static const mxfKey setKey;

public:
    PrefaceBase(HeaderMetadata* headerMetadata);
    virtual ~PrefaceBase();


   // getters

   mxfTimestamp getLastModifiedDate() const;
   uint16_t getVersion() const;
   bool haveObjectModelVersion() const;
   uint32_t getObjectModelVersion() const;
   bool havePrimaryPackage() const;
   GenericPackage* getPrimaryPackage() const;
   std::vector<Identification*> getIdentifications() const;
   ContentStorage* getContentStorage() const;
   mxfUL getOperationalPattern() const;
   std::vector<mxfUL> getEssenceContainers() const;
   std::vector<mxfUL> getDMSchemes() const;


   // setters

   void setLastModifiedDate(mxfTimestamp value);
   void setVersion(uint16_t value);
   void setObjectModelVersion(uint32_t value);
   void setPrimaryPackage(GenericPackage* value);
   void setIdentifications(const std::vector<Identification*>& value);
   void appendIdentifications(Identification* value);
   void setContentStorage(ContentStorage* value);
   void setOperationalPattern(mxfUL value);
   void setEssenceContainers(const std::vector<mxfUL>& value);
   void appendEssenceContainers(mxfUL value);
   void setDMSchemes(const std::vector<mxfUL>& value);
   void appendDMSchemes(mxfUL value);


protected:
    PrefaceBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
