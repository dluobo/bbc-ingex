/*
 * $Id: HeaderMetadata.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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

#ifndef __MXFPP_HEADERMETADATA_H__
#define __MXFPP_HEADERMETADATA_H__

#include <map>

#include <libMXF++/File.h>
#include <libMXF++/DataModel.h>


namespace mxfpp
{

class Preface;
class HeaderMetadata;


class HeaderMetadata
{
public:
    friend class MetadataSet; // allow metadata sets to remove themselves from the _objectDirectory 

public:
    static bool isHeaderMetadata(const mxfKey* key);

    HeaderMetadata(DataModel* dataModel);
    virtual ~HeaderMetadata();
    
    static mxfProductVersion getToolkitVersion();
    static std::string getPlatform();
    
    void registerObjectFactory(const mxfKey* key, AbsMetadataSetFactory* factory);
    
    
    virtual void read(File* file, Partition* partition, const mxfKey* key, uint8_t llen, uint64_t len);

    virtual void write(File* file, Partition* partition, FillerWriter* filler);
    

    Preface* getPreface();
    

    void add(MetadataSet* set);
    
    MetadataSet* wrap(::MXFMetadataSet* cMetadataSet);
    ::MXFMetadataSet* createCSet(const mxfKey* key);
    MetadataSet* createAndWrap(const mxfKey* key);
    
    ::MXFHeaderMetadata* getCHeaderMetadata() const { return _cHeaderMetadata; }
    
private:
    void initialiseObjectFactory();
    void remove(MetadataSet* set);
    
    std::map<mxfKey, AbsMetadataSetFactory*> _objectFactory;
    
    ::MXFHeaderMetadata* _cHeaderMetadata;
    std::map<mxfUUID, MetadataSet*> _objectDirectory;
    bool _busyDestructing;
};



};



#endif

