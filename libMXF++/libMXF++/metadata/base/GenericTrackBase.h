/*
 * $Id: GenericTrackBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_GENERICTRACK_BASE_H__
#define __MXFPP_GENERICTRACK_BASE_H__



#include <libMXF++/metadata/InterchangeObject.h>


namespace mxfpp
{


class GenericTrackBase : public InterchangeObject
{
public:
    friend class MetadataSetFactory<GenericTrackBase>;
    static const mxfKey setKey;

public:
    GenericTrackBase(HeaderMetadata* headerMetadata);
    virtual ~GenericTrackBase();


   // getters

   bool haveTrackID() const;
   uint32_t getTrackID() const;
   uint32_t getTrackNumber() const;
   bool haveTrackName() const;
   std::string getTrackName() const;
   StructuralComponent* getSequence() const;


   // setters

   void setTrackID(uint32_t value);
   void setTrackNumber(uint32_t value);
   void setTrackName(std::string value);
   void setSequence(StructuralComponent* value);


protected:
    GenericTrackBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
