/*
 * $Id: DynamicByteArray.cpp,v 1.5 2010/02/12 13:52:47 philipn Exp $
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
 
#include "CommonTypes.h"
#include "DynamicByteArray.h"
#include <libMXF++/MXFException.h>
#include <cstring>


using namespace std;
using namespace mxfpp;



DynamicByteArray::DynamicByteArray()
: _bytes(0), _size(0), _isCopy(false), _allocatedSize(0), _increment(256)
{}

DynamicByteArray::DynamicByteArray(uint32_t size)
: _bytes(0), _size(0), _isCopy(false), _allocatedSize(0), _increment(256)
{
    allocate(size);
}

DynamicByteArray::~DynamicByteArray()
{
    if (!_isCopy)
    {
        SAFE_ARRAY_DELETE(_bytes);
    }
}

void DynamicByteArray::setAllocIncrement(uint32_t increment)
{
    _increment = increment;
}

unsigned char* DynamicByteArray::getBytes() const
{
    return _bytes;
}

uint32_t DynamicByteArray::getSize() const
{
    return _size;
}

void DynamicByteArray::setBytes(const unsigned char* bytes, uint32_t size)
{
    setSize(0);
    append(bytes, size);
}

void DynamicByteArray::append(const unsigned char* bytes, uint32_t size)
{
    if (size == 0)
    {
        return;
    }
    
    if (_isCopy)
    {
        throw MXFException("Cannot append byte array data to a copy\n");
    }
    
    grow(size);

    memcpy(_bytes + _size, bytes, size);
    _size += size;
}

void DynamicByteArray::appendZeros(uint32_t size)
{
    if (size == 0)
    {
        return;
    }
    
    if (_isCopy)
    {
        throw MXFException("Cannot append byte array data to a copy\n");
    }
    
    grow(size);

    memset(_bytes + _size, 0, size);
    _size += size;
}

unsigned char* DynamicByteArray::getBytesAvailable() const
{
    if (_isCopy)
    {
        throw MXFException("Cannot get available byte array data for a copy\n");
    }
    
    if (_size == _allocatedSize)
    {
        return NULL;
    }
    
    return _bytes + _size;
}

uint32_t DynamicByteArray::getSizeAvailable() const
{
    return _allocatedSize - _size;
}

void DynamicByteArray::setSize(uint32_t size)
{
    if (size > _allocatedSize)
    {
        throw MXFException("Cannot set byte array size > allocated size\n");
    }
    
    _size = size;
}

void DynamicByteArray::setCopy(unsigned char* bytes, uint32_t size)
{
    clear();
    
    _bytes = bytes;
    _size = size;
    _allocatedSize = 0;
    _isCopy = 1;
}

void DynamicByteArray::grow(uint32_t size)
{
    if (_isCopy)
    {
        throw MXFException("Cannot grow a byte array that is a copy\n");
    }
    
    if (_size + size > _allocatedSize)
    {
        reallocate(_size + size);
    }
}

void DynamicByteArray::allocate(uint32_t size)
{
    if (_isCopy)
    {
        throw MXFException("Cannot allocate a byte array that is a copy\n");
    }

    SAFE_ARRAY_DELETE(_bytes);
    _size = 0;
    _allocatedSize = 0;

    _bytes = new unsigned char[size];
    _allocatedSize = size;
}    

void DynamicByteArray::minAllocate(uint32_t min_size)
{
    if (_allocatedSize < min_size)
        allocate(min_size);
}

void DynamicByteArray::reallocate(uint32_t size)
{
    unsigned char* newBytes;
    
    if (_isCopy)
    {
        throw MXFException("Cannot reallocate a byte array that is a copy\n");
    }

    newBytes = new unsigned char[size];
    if (_size > 0)
    {
        memcpy(newBytes, _bytes, _size);
    }

    SAFE_ARRAY_DELETE(_bytes);
    _bytes = newBytes;
    _allocatedSize = size;
    
    if (size < _size)
    {
        _size = size;
    }
}    

void DynamicByteArray::clear()
{
    if (_isCopy)
    {
        _bytes = 0;
        _size = 0;
        _allocatedSize = 0;
        _isCopy = false;
    }
    else
    {
        SAFE_ARRAY_DELETE(_bytes);
        _size = 0;
        _allocatedSize = 0;
    }
}

unsigned char& DynamicByteArray::operator[](uint32_t index) const
{
    return _bytes[index];
}


