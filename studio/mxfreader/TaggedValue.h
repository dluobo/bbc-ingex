/*
 * $Id: TaggedValue.h,v 1.1 2009/09/18 17:29:30 john_f Exp $
 *
 * Extract information from Ingex MXF files.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __TAGGED_VALUE_H__
#define __TAGGED_VALUE_H__


#include <libMXF++/metadata/InterchangeObject.h>


class TaggedValue : public mxfpp::InterchangeObject
{
public:
    static void registerObjectFactory(mxfpp::HeaderMetadata *header_metadata);

public:
    static const mxfKey set_key;

public:
    friend class mxfpp::MetadataSetFactory<TaggedValue>;

public:
    TaggedValue(mxfpp::HeaderMetadata *header_metadata);
    virtual ~TaggedValue();


   // getters
   std::string getName();
   bool isStringValue();
   std::string getStringValue();
   

   // setters
   void setName(std::string value);
   //void setStringValue(std::string value);

protected:
    TaggedValue(mxfpp::HeaderMetadata *header_metadata, ::MXFMetadataSet* c_metadata_set);
};



#endif
