/*
 * $Id: ByteArray.cpp,v 1.1 2008/07/08 16:23:25 philipn Exp $
 *
 * Manages a dynamic array of bytes
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#include "ByteArray.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Logging.h"


using namespace std;
using namespace rec;



ByteArray::ByteArray()
: _bytes(0), _size(0), _isCopy(false), _allocatedSize(0), _increment(256)
{}

ByteArray::ByteArray(uint32_t size)
: _bytes(0), _size(0), _isCopy(false), _allocatedSize(0), _increment(256)
{
    allocate(size);
}

ByteArray::~ByteArray()
{
    if (!_isCopy)
    {
        SAFE_ARRAY_DELETE(_bytes);
    }
}

void ByteArray::setAllocIncrement(uint32_t increment)
{
    _increment = increment;
}

unsigned char* ByteArray::getBytes() const
{
    return _bytes;
}

uint32_t ByteArray::getSize() const
{
    return _size;
}

void ByteArray::append(const unsigned char* bytes, uint32_t size)
{
    REC_ASSERT(!_isCopy);
    
    if (size == 0)
    {
        return;
    }
    
    grow(_size + size - _allocatedSize);

    memcpy(_bytes + _size, bytes, size);
    _size += size;
}

unsigned char* ByteArray::getBytesAvailable() const
{
    REC_ASSERT(!_isCopy);
    
    if (_size == _allocatedSize)
    {
        return NULL;
    }
    
    return _bytes + _size;
}

uint32_t ByteArray::getSizeAvailable() const
{
    return _allocatedSize - _size;
}

void ByteArray::setSize(uint32_t size)
{
    if (size > _allocatedSize)
    {
        REC_LOGTHROW(("Cannot set byte array size > allocated size"));
    }
    
    _size = size;
}

void ByteArray::setCopy(unsigned char* bytes, uint32_t size)
{
    clear();
    
    _bytes = bytes;
    _size = size;
    _allocatedSize = 0;
    _isCopy = 1;
}

void ByteArray::grow(uint32_t size)
{
    REC_ASSERT(!_isCopy);
    
    if (_size + size > _allocatedSize)
    {
        reallocate(_size + size);
    }
}

void ByteArray::allocate(uint32_t size)
{
    REC_ASSERT(!_isCopy);
    
    if (_allocatedSize >= size)
    {
        return;
    }
    
    SAFE_ARRAY_DELETE(_bytes);
    _size = 0;
    _allocatedSize = 0;

    _bytes = new unsigned char[size];
    memset(_bytes, 0, size);
    _allocatedSize = size;
}    

void ByteArray::reallocate(uint32_t size)
{
    REC_ASSERT(!_isCopy);
    
    if (_allocatedSize >= size)
    {
        return;
    }
    
    unsigned char* newBytes = new unsigned char[size];
    memset(newBytes, 0, size);
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

void ByteArray::clear()
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



