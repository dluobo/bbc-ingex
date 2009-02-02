/*
 * $Id: HeaderMetadata.cpp,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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

#include <libMXF++/MXF.h>

using namespace std;
using namespace mxfpp;


static mxfProductVersion g_libMXFPPToolkitVersion = {0, 1, 0, 0, 0};



bool operator < (const mxfKey& left, const mxfKey& right)
{
    return memcmp(&left, &right, sizeof(mxfKey)) < 0;
}

bool operator < (const mxfUUID& left, const mxfUUID& right)
{
    return memcmp(&left, &right, sizeof(mxfUUID)) < 0;
}


bool HeaderMetadata::isHeaderMetadata(const mxfKey* key)
{
    return mxf_is_header_metadata(key) != 0;
}



HeaderMetadata::HeaderMetadata(DataModel* dataModel)
{
    initialiseObjectFactory();
    MXFPP_CHECK(mxf_create_header_metadata(&_cHeaderMetadata, dataModel->getCDataModel()));
}

HeaderMetadata::~HeaderMetadata()
{
    map<mxfKey, AbsMetadataSetFactory*>::iterator iter1;
    for (iter1 = _objectFactory.begin(); iter1 != _objectFactory.end(); iter1++)
    {
        delete (*iter1).second;
    }

    map<mxfUUID, MetadataSet*>::iterator iter2;
    for (iter2 = _objectDirectory.begin(); iter2 != _objectDirectory.end(); iter2++)
    {
        (*iter2).second->_headerMetadata = 0; // break containment link
        delete (*iter2).second;
    }

    mxf_free_header_metadata(&_cHeaderMetadata);
}

mxfProductVersion HeaderMetadata::getToolkitVersion()
{
    return g_libMXFPPToolkitVersion;
}

string HeaderMetadata::getPlatform()
{
    return mxf_get_platform_string();
}


void HeaderMetadata::registerObjectFactory(const mxfKey* key, AbsMetadataSetFactory* factory)
{
    pair<map<mxfKey, AbsMetadataSetFactory*>::iterator, bool> result = 
        _objectFactory.insert(pair<mxfKey, AbsMetadataSetFactory*>(*key, factory));
    if (result.second == false)
    {
        // replace existing factory with new one
        _objectFactory.erase(result.first);
        _objectFactory.insert(pair<mxfKey, AbsMetadataSetFactory*>(*key, factory));
    }
}

void HeaderMetadata::read(File* file, Partition* partition, const mxfKey* key, 
    uint8_t llen, uint64_t len)
{
    MXFPP_CHECK(mxf_read_header_metadata(file->getCFile(), _cHeaderMetadata, 
        partition->getCPartition()->headerByteCount, key, llen, len));
}

void HeaderMetadata::write(File* file, Partition* partition, FillerWriter* filler)
{
    partition->markHeaderStart(file);
    
    MXFPP_CHECK(mxf_write_header_metadata(file->getCFile(), _cHeaderMetadata));
    if (filler)
    {
        filler->write(file);
    }

    partition->markHeaderEnd(file);
}

Preface* HeaderMetadata::getPreface()
{
    ::MXFMetadataSet* cSet;
    if (!mxf_find_singular_set_by_key(_cHeaderMetadata, &MXF_SET_K(Preface), &cSet))
    {
        throw MXFException("Header metadata is missing a Preface object");
    }
    
    return dynamic_cast<Preface*>(wrap(cSet));
}

void HeaderMetadata::add(MetadataSet* set)
{
    _objectDirectory.insert(pair<mxfUUID, MetadataSet*>(set->getCMetadataSet()->instanceUID, set));
}

MetadataSet* HeaderMetadata::wrap(::MXFMetadataSet* cMetadataSet)
{
    MXFPP_CHECK(cMetadataSet->headerMetadata == _cHeaderMetadata);
    MXFPP_CHECK(!mxf_equals_uuid(&cMetadataSet->instanceUID, &g_Null_UUID));
    
    MetadataSet* set = 0;
    
    map<mxfUUID, MetadataSet*>::const_iterator objIter;
    objIter = _objectDirectory.find(cMetadataSet->instanceUID);
    if (objIter != _objectDirectory.end())
    {
        set = (*objIter).second;
        
        if (cMetadataSet != set->getCMetadataSet())
        {
            mxf_log(MXF_WLOG, "Metadata set with same instance UUID found when creating "
                "C++ object. Changing wrapped C metadata set.");
            set->_cMetadataSet = cMetadataSet;
        }
    }
    else
    {
        map<mxfKey, AbsMetadataSetFactory*>::iterator iter;
    
        ::MXFSetDef* setDef = 0;
        MXFPP_CHECK(mxf_find_set_def(_cHeaderMetadata->dataModel, &cMetadataSet->key, &setDef));
            
        while (setDef != 0)
        {
            iter = _objectFactory.find(setDef->key);
            if (iter != _objectFactory.end())
            {
                set = (*iter).second->create(this, cMetadataSet);
                break;
            }
            else
            {
                setDef = setDef->parentSetDef;
            }
        }
        
        if (set == 0)
        {
            // shouldn't be here if every class is a sub-class of interchange object
            // and libMXF ignores sets with unknown defs
            throw MXFException("Could not create C++ object for metadata set");
        }
 
        add(set);
    }
    
    return set;
}
    
::MXFMetadataSet* HeaderMetadata::createCSet(const mxfKey* key)
{
    ::MXFMetadataSet* cMetadataSet;
    
    MXFPP_CHECK(mxf_create_set(_cHeaderMetadata, key, &cMetadataSet));

    return cMetadataSet;
}

MetadataSet* HeaderMetadata::createAndWrap(const mxfKey* key)
{
    return wrap(createCSet(key));
}

void HeaderMetadata::initialiseObjectFactory()
{
#define REGISTER_CLASS(className) \
    _objectFactory.insert(pair<mxfKey, AbsMetadataSetFactory*>( \
        className::setKey, new MetadataSetFactory<className>()));
        

    REGISTER_CLASS(InterchangeObject);
    REGISTER_CLASS(Preface);
    REGISTER_CLASS(Identification);
    REGISTER_CLASS(ContentStorage);
    REGISTER_CLASS(EssenceContainerData);
    REGISTER_CLASS(GenericPackage);
    REGISTER_CLASS(Locator);
    REGISTER_CLASS(NetworkLocator);
    REGISTER_CLASS(TextLocator);
    REGISTER_CLASS(GenericTrack);
    REGISTER_CLASS(StaticTrack);
    REGISTER_CLASS(Track);
    REGISTER_CLASS(EventTrack);
    REGISTER_CLASS(StructuralComponent);
    REGISTER_CLASS(Sequence);
    REGISTER_CLASS(TimecodeComponent);
    REGISTER_CLASS(SourceClip);
    REGISTER_CLASS(DMSegment);
    REGISTER_CLASS(DMSourceClip);
    REGISTER_CLASS(MaterialPackage);
    REGISTER_CLASS(SourcePackage);
    REGISTER_CLASS(GenericDescriptor);
    REGISTER_CLASS(FileDescriptor);
    REGISTER_CLASS(GenericPictureEssenceDescriptor);
    REGISTER_CLASS(CDCIEssenceDescriptor);
    REGISTER_CLASS(RGBAEssenceDescriptor);
    REGISTER_CLASS(GenericSoundEssenceDescriptor);
    REGISTER_CLASS(GenericDataEssenceDescriptor);
    REGISTER_CLASS(MultipleDescriptor);
    REGISTER_CLASS(WaveAudioDescriptor);
    REGISTER_CLASS(AES3AudioDescriptor);
    REGISTER_CLASS(DMFramework);
    REGISTER_CLASS(DMSet);
    
    // Add new classes here
    
}

void HeaderMetadata::remove(MetadataSet* set)
{
    map<mxfUUID, MetadataSet*>::iterator objIter;
    objIter = _objectDirectory.find(set->getCMetadataSet()->instanceUID);
    if (objIter != _objectDirectory.end())
    {
        _objectDirectory.erase(objIter);
    }   
    // TODO: throw exception or log warning if set not in there?
}

    
    
