/*
 * $Id: GenericDescriptorBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_GENERICDESCRIPTOR_BASE_H__
#define __MXFPP_GENERICDESCRIPTOR_BASE_H__



#include <libMXF++/metadata/InterchangeObject.h>


namespace mxfpp
{


class GenericDescriptorBase : public InterchangeObject
{
public:
    friend class MetadataSetFactory<GenericDescriptorBase>;
    static const mxfKey setKey;

public:
    GenericDescriptorBase(HeaderMetadata* headerMetadata);
    virtual ~GenericDescriptorBase();


   // getters

   bool haveLocators() const;
   std::vector<Locator*> getLocators() const;


   // setters

   void setLocators(const std::vector<Locator*>& value);
   void appendLocators(Locator* value);


protected:
    GenericDescriptorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
