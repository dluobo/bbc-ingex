/*
 * $Id: GenericPackageBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_GENERICPACKAGE_BASE_H__
#define __MXFPP_GENERICPACKAGE_BASE_H__



#include <libMXF++/metadata/InterchangeObject.h>


namespace mxfpp
{


class GenericPackageBase : public InterchangeObject
{
public:
    friend class MetadataSetFactory<GenericPackageBase>;
    static const mxfKey setKey;

public:
    GenericPackageBase(HeaderMetadata* headerMetadata);
    virtual ~GenericPackageBase();


   // getters

   mxfUMID getPackageUID() const;
   bool haveName() const;
   std::string getName() const;
   mxfTimestamp getPackageCreationDate() const;
   mxfTimestamp getPackageModifiedDate() const;
   std::vector<GenericTrack*> getTracks() const;


   // setters

   void setPackageUID(mxfUMID value);
   void setName(std::string value);
   void setPackageCreationDate(mxfTimestamp value);
   void setPackageModifiedDate(mxfTimestamp value);
   void setTracks(const std::vector<GenericTrack*>& value);
   void appendTracks(GenericTrack* value);


protected:
    GenericPackageBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
