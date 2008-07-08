/*
 * $Id: ByteArray.h,v 1.1 2008/07/08 16:23:31 philipn Exp $
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
 
#ifndef __RECORDER_BYTE_ARRAY_H__
#define __RECORDER_BYTE_ARRAY_H__


#include <inttypes.h>


namespace rec
{

class ByteArray
{
public:
    ByteArray();
    ByteArray(uint32_t size);
    ~ByteArray();

    void setAllocIncrement(uint32_t increment);
    
    unsigned char* getBytes() const;
    uint32_t getSize() const;

    void append(const unsigned char* bytes, uint32_t size);
    unsigned char* getBytesAvailable() const;
    uint32_t getSizeAvailable() const;
    void setSize(uint32_t size);
    
    void setCopy(unsigned char* bytes, uint32_t size);
    
    void grow(uint32_t size);
    void allocate(uint32_t size);
    void reallocate(uint32_t size);

    void clear();

    
private:
    unsigned char* _bytes;
    uint32_t _size;
    bool _isCopy;
    uint32_t _allocatedSize;
    uint32_t _increment;
};


};


#endif

