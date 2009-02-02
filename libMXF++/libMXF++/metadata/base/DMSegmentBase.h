/*
 * $Id: DMSegmentBase.h,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
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

#ifndef __MXFPP_DMSEGMENT_BASE_H__
#define __MXFPP_DMSEGMENT_BASE_H__



#include <libMXF++/metadata/StructuralComponent.h>


namespace mxfpp
{


class DMSegmentBase : public StructuralComponent
{
public:
    friend class MetadataSetFactory<DMSegmentBase>;
    static const mxfKey setKey;

public:
    DMSegmentBase(HeaderMetadata* headerMetadata);
    virtual ~DMSegmentBase();


   // getters

   int64_t getEventStartPosition() const;
   bool haveEventComment() const;
   std::string getEventComment() const;
   bool haveTrackIDs() const;
   std::vector<uint32_t> getTrackIDs() const;
   bool haveDMFramework() const;
   DMFramework* getDMFramework() const;


   // setters

   void setEventStartPosition(int64_t value);
   void setEventComment(std::string value);
   void setTrackIDs(const std::vector<uint32_t>& value);
   void appendTrackIDs(uint32_t value);
   void setDMFramework(DMFramework* value);


protected:
    DMSegmentBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
