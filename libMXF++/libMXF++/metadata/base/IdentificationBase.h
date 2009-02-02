/*
 * $Id: IdentificationBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_IDENTIFICATION_BASE_H__
#define __MXFPP_IDENTIFICATION_BASE_H__



#include <libMXF++/metadata/InterchangeObject.h>


namespace mxfpp
{


class IdentificationBase : public InterchangeObject
{
public:
    friend class MetadataSetFactory<IdentificationBase>;
    static const mxfKey setKey;

public:
    IdentificationBase(HeaderMetadata* headerMetadata);
    virtual ~IdentificationBase();


   // getters

   mxfUUID getThisGenerationUID() const;
   std::string getCompanyName() const;
   std::string getProductName() const;
   bool haveProductVersion() const;
   mxfProductVersion getProductVersion() const;
   std::string getVersionString() const;
   mxfUUID getProductUID() const;
   mxfTimestamp getModificationDate() const;
   bool haveToolkitVersion() const;
   mxfProductVersion getToolkitVersion() const;
   bool havePlatform() const;
   std::string getPlatform() const;


   // setters

   void setThisGenerationUID(mxfUUID value);
   void setCompanyName(std::string value);
   void setProductName(std::string value);
   void setProductVersion(mxfProductVersion value);
   void setVersionString(std::string value);
   void setProductUID(mxfUUID value);
   void setModificationDate(mxfTimestamp value);
   void setToolkitVersion(mxfProductVersion value);
   void setPlatform(std::string value);


protected:
    IdentificationBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
