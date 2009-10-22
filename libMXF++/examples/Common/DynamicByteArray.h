/*
 * $Id: DynamicByteArray.h,v 1.3 2009/10/22 16:35:50 philipn Exp $
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
 
#ifndef __DYNAMIC_BYTE_ARRAY_H__
#define __DYNAMIC_BYTE_ARRAY_H__



class DynamicByteArray
{
public:
    DynamicByteArray();
    DynamicByteArray(uint32_t size);
    ~DynamicByteArray();

    void setAllocIncrement(uint32_t increment);
    
    unsigned char* getBytes() const;
    uint32_t getSize() const;

    void append(unsigned char* bytes, uint32_t size);
    unsigned char* getBytesAvailable() const;
    uint32_t getSizeAvailable() const;
    void setSize(uint32_t size);
    
    void setCopy(unsigned char* bytes, uint32_t size);
    
    void grow(uint32_t size);
    void allocate(uint32_t size);
    void minAllocate(uint32_t min_size);
    void reallocate(uint32_t size);

    void clear();

    unsigned char& operator[](uint32_t index) const;
    
private:
    unsigned char* _bytes;
    uint32_t _size;
    bool _isCopy;
    uint32_t _allocatedSize;
    uint32_t _increment;
};



#endif

