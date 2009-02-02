/*
 * $Id: MetadataSet.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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

#ifndef __MXFPP_METADATA_SET_H__
#define __MXFPP_METADATA_SET_H__


#include <map>
#include <vector>


namespace mxfpp
{


typedef struct
{
    uint8_t* data;
    uint16_t length;
} ByteArray;


class MetadataSet;

class ObjectIterator
{
public:
    virtual ~ObjectIterator() {};
    
    virtual bool next() = 0;
    virtual MetadataSet* get() = 0;
    virtual uint32_t size() = 0;
};

template <typename Obj>
class WrapObjectVectorIterator : public ObjectIterator
{
public:
    WrapObjectVectorIterator(const std::vector<Obj*>& value)
    : _value(value), _size(0), _pos(((uint32_t)-1))
    {
        _size = (uint32_t)value.size();
    }
    
    virtual ~WrapObjectVectorIterator() {}    
    
    
    virtual bool next()
    {
        if (_pos < _size)
        {
            _pos++;
        }
        return _pos < _size;
    }
    
    virtual MetadataSet* get()
    {
        if (_pos == ((uint32_t)-1))
        {
            throw MXFException("Must call next() before calling get() in object iterator");        
        }
        else if (_pos >= _size)
        {
            throw MXFException("Object iterator has no more objects to return");        
        }
        return _value.at(_pos);
    }
    
    virtual uint32_t size()
    {
        return _size;
    }
    
private:
    const std::vector<Obj*>& _value;
    uint32_t _size;
    uint32_t _pos;
};


class HeaderMetadata;

class MetadataSet
{
public:
    friend class HeaderMetadata; 
    
public:
    MetadataSet(const MetadataSet& set);
    virtual ~MetadataSet();

    bool haveItem(const mxfKey* itemKey) const;
    
    ByteArray getRawBytesItem(const mxfKey* itemKey) const;
    
    uint8_t getUInt8Item(const mxfKey* itemKey) const;
    uint16_t getUInt16Item(const mxfKey* itemKey) const;
    uint32_t getUInt32Item(const mxfKey* itemKey) const;
    uint64_t getUInt64Item(const mxfKey* itemKey) const;
    int8_t getInt8Item(const mxfKey* itemKey) const;
    int16_t getInt16Item(const mxfKey* itemKey) const;
    int32_t getInt32Item(const mxfKey* itemKey) const;
    int64_t getInt64Item(const mxfKey* itemKey) const;
    mxfVersionType getVersionTypeItem(const mxfKey* itemKey) const;
    mxfUUID getUUIDItem(const mxfKey* itemKey) const;
    mxfUL getULItem(const mxfKey* itemKey) const;
    mxfAUID getAUIDItem(const mxfKey* itemKey) const;
    mxfUMID getUMIDItem(const mxfKey* itemKey) const;
    mxfTimestamp getTimestampItem(const mxfKey* itemKey) const;
    int64_t getLengthItem(const mxfKey* itemKey) const;
    mxfRational getRationalItem(const mxfKey* itemKey) const;
    int64_t getPositionItem(const mxfKey* itemKey) const;
    bool getBooleanItem(const mxfKey* itemKey) const;
    mxfProductVersion getProductVersionItem(const mxfKey* itemKey) const;
    mxfRGBALayoutComponent getRGBALayoutComponentItem(const mxfKey* itemKey) const;
    std::string getStringItem(const mxfKey* itemKey) const;
    MetadataSet* getStrongRefItem(const mxfKey* itemKey) const;
    MetadataSet* getWeakRefItem(const mxfKey* itemKey) const;

    std::vector<uint8_t> getUInt8ArrayItem(const mxfKey* itemKey) const;
    std::vector<uint16_t> getUInt16ArrayItem(const mxfKey* itemKey) const;
    std::vector<uint32_t> getUInt32ArrayItem(const mxfKey* itemKey) const;
    std::vector<uint64_t> getUInt64ArrayItem(const mxfKey* itemKey) const;
    std::vector<int8_t> getInt8ArrayItem(const mxfKey* itemKey) const;
    std::vector<int16_t> getInt16ArrayItem(const mxfKey* itemKey) const;
    std::vector<int32_t> getInt32ArrayItem(const mxfKey* itemKey) const;
    std::vector<int64_t> getInt64ArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfVersionType> getVersionTypeArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfUUID> getUUIDArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfUL> getULArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfAUID> getAUIDArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfUMID> getUMIDArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfTimestamp> getTimestampArrayItem(const mxfKey* itemKey) const;
    std::vector<int64_t> getLengthArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfRational> getRationalArrayItem(const mxfKey* itemKey) const;
    std::vector<int64_t> getPositionArrayItem(const mxfKey* itemKey) const;
    std::vector<bool> getBooleanArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfProductVersion> getProductVersionArrayItem(const mxfKey* itemKey) const;
    std::vector<mxfRGBALayoutComponent> getRGBALayoutComponentArrayItem(const mxfKey* itemKey) const;

    ObjectIterator* getStrongRefArrayItem(const mxfKey* itemKey) const;
    ObjectIterator* getWeakRefArrayItem(const mxfKey* itemKey) const;
    

    void setRawBytesItem(const mxfKey* itemKey, ByteArray value);
    
    void setUInt8Item(const mxfKey* itemKey, uint8_t value); 
    void setUInt16Item(const mxfKey* itemKey, uint16_t value); 
    void setUInt32Item(const mxfKey* itemKey, uint32_t value); 
    void setUInt64Item(const mxfKey* itemKey, uint64_t value); 
    void setInt8Item(const mxfKey* itemKey, int8_t value); 
    void setInt16Item(const mxfKey* itemKey, int16_t value); 
    void setInt32Item(const mxfKey* itemKey, int32_t value); 
    void setInt64Item(const mxfKey* itemKey, int64_t value); 
    void setVersionTypeItem(const mxfKey* itemKey, mxfVersionType value); 
    void setUUIDItem(const mxfKey* itemKey, mxfUUID value); 
    void setULItem(const mxfKey* itemKey, mxfUL value); 
    void setAUIDItem(const mxfKey* itemKey, mxfAUID value); 
    void setUMIDItem(const mxfKey* itemKey, mxfUMID value); 
    void setTimestampItem(const mxfKey* itemKey, mxfTimestamp value); 
    void setLengthItem(const mxfKey* itemKey, int64_t value); 
    void setRationalItem(const mxfKey* itemKey, mxfRational value); 
    void setPositionItem(const mxfKey* itemKey, int64_t value); 
    void setBooleanItem(const mxfKey* itemKey, bool value); 
    void setProductVersionItem(const mxfKey* itemKey, mxfProductVersion value); 
    void setRGBALayoutComponentItem(const mxfKey* itemKey, mxfRGBALayoutComponent value); 
    void setStringItem(const mxfKey* itemKey, std::string value); 
    void setStrongRefItem(const mxfKey* itemKey, MetadataSet* value); 
    void setWeakRefItem(const mxfKey* itemKey, MetadataSet* value); 
    
    void setUInt8ArrayItem(const mxfKey* itemKey, const std::vector<uint8_t>& value); 
    void setUInt16ArrayItem(const mxfKey* itemKey, const std::vector<uint16_t>& value); 
    void setUInt32ArrayItem(const mxfKey* itemKey, const std::vector<uint32_t>& value); 
    void setUInt64ArrayItem(const mxfKey* itemKey, const std::vector<uint64_t>& value); 
    void setInt8ArrayItem(const mxfKey* itemKey, const std::vector<int8_t>& value); 
    void setInt16ArrayItem(const mxfKey* itemKey, const std::vector<int16_t>& value); 
    void setInt32ArrayItem(const mxfKey* itemKey, const std::vector<int32_t>& value); 
    void setInt64ArrayItem(const mxfKey* itemKey, const std::vector<int64_t>& value); 
    void setVersionTypeArrayItem(const mxfKey* itemKey, const std::vector<mxfVersionType>& value); 
    void setUUIDArrayItem(const mxfKey* itemKey, const std::vector<mxfUUID>& value); 
    void setULArrayItem(const mxfKey* itemKey, const std::vector<mxfUL>& value); 
    void setAUIDArrayItem(const mxfKey* itemKey, const std::vector<mxfAUID>& value); 
    void setUMIDArrayItem(const mxfKey* itemKey, const std::vector<mxfUMID>& value); 
    void setTimestampArrayItem(const mxfKey* itemKey, const std::vector<mxfTimestamp>& value); 
    void setLengthArrayItem(const mxfKey* itemKey, const std::vector<int64_t>& value); 
    void setRationalArrayItem(const mxfKey* itemKey, const std::vector<mxfRational>& value); 
    void setPositionArrayItem(const mxfKey* itemKey, const std::vector<int64_t>& value); 
    void setBooleanArrayItem(const mxfKey* itemKey, const std::vector<bool>& value); 
    void setProductVersionArrayItem(const mxfKey* itemKey, const std::vector<mxfProductVersion>& value); 
    void setRGBALayoutComponentArrayItem(const mxfKey* itemKey, const std::vector<mxfRGBALayoutComponent>& value); 

    void setStrongRefArrayItem(const mxfKey* itemKey, ObjectIterator* iter); 
    void setWeakRefArrayItem(const mxfKey* itemKey, ObjectIterator* iter); 
    
    void appendUInt8ArrayItem(const mxfKey* itemKey, uint8_t value); 
    void appendUInt16ArrayItem(const mxfKey* itemKey, uint16_t value); 
    void appendUInt32ArrayItem(const mxfKey* itemKey, uint32_t value); 
    void appendUInt64ArrayItem(const mxfKey* itemKey, uint64_t value); 
    void appendInt8ArrayItem(const mxfKey* itemKey, int8_t value); 
    void appendInt16ArrayItem(const mxfKey* itemKey, int16_t value); 
    void appendInt32ArrayItem(const mxfKey* itemKey, int32_t value); 
    void appendInt64ArrayItem(const mxfKey* itemKey, int64_t value); 
    void appendVersionTypeArrayItem(const mxfKey* itemKey, mxfVersionType value); 
    void appendUUIDArrayItem(const mxfKey* itemKey, mxfUUID value); 
    void appendULArrayItem(const mxfKey* itemKey, mxfUL value); 
    void appendAUIDArrayItem(const mxfKey* itemKey, mxfAUID value); 
    void appendUMIDArrayItem(const mxfKey* itemKey, mxfUMID value); 
    void appendTimestampArrayItem(const mxfKey* itemKey, mxfTimestamp value); 
    void appendLengthArrayItem(const mxfKey* itemKey, int64_t value); 
    void appendRationalArrayItem(const mxfKey* itemKey, mxfRational value); 
    void appendPositionArrayItem(const mxfKey* itemKey, int64_t value); 
    void appendBooleanArrayItem(const mxfKey* itemKey, bool value); 
    void appendProductVersionArrayItem(const mxfKey* itemKey, mxfProductVersion value); 
    void appendRGBALayoutComponentArrayItem(const mxfKey* itemKey, mxfRGBALayoutComponent value); 

    void appendStrongRefArrayItem(const mxfKey* itemKey, MetadataSet* value); 
    void appendWeakRefArrayItem(const mxfKey* itemKey, MetadataSet* value); 
    

    // Avid extensions
    void attachAvidAttribute(std::string name, std::string value);
    void attachAvidUserComment(std::string name, std::string value);
    
    
    
    ::MXFMetadataSet* getCMetadataSet() const { return _cMetadataSet; }
    
protected:
    MetadataSet(HeaderMetadata* headerMetadata, ::MXFMetadataSet* metadataSet);

    HeaderMetadata* _headerMetadata;
    ::MXFMetadataSet* _cMetadataSet;
};


class AbsMetadataSetFactory
{
public:
    virtual ~AbsMetadataSetFactory() {};
    
    virtual MetadataSet* create(HeaderMetadata* headerMetadata, ::MXFMetadataSet* metadataSet) = 0;
};

template<class MetadataSetType>
class MetadataSetFactory : public AbsMetadataSetFactory
{
public:
    virtual MetadataSet* create(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
    {
        return new MetadataSetType(headerMetadata, cMetadataSet);
    }
};




};


#endif

