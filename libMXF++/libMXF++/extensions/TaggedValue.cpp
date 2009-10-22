/*
 * $Id: TaggedValue.cpp,v 1.1 2009/10/22 16:37:31 philipn Exp $
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

#include <libMXF++/MXF.h>
#include <mxf/mxf_avid.h>

#include "TaggedValue.h"

#include <cstring>
#include <cstdlib>

using namespace std;
using namespace mxfpp;


// prefix is 0x42 ('B') for big endian or 0x4c ('L') for little endian, followed by half-swapped key for String type
static const unsigned char g_prefix_be[17] =
    {0x42, 0x01, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0e, 0x2b, 0x34, 0x01, 0x04, 0x01, 0x01};
static const unsigned char g_prefix_le[17] =
    {0x4C, 0x00, 0x02, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0e, 0x2b, 0x34, 0x01, 0x04, 0x01, 0x01};



const mxfKey TaggedValue::set_key = MXF_SET_K(TaggedValue);

void TaggedValue::registerObjectFactory(HeaderMetadata *header_metadata)
{
    header_metadata->registerObjectFactory(&set_key, new MetadataSetFactory<TaggedValue>());
}



TaggedValue::TaggedValue(HeaderMetadata* header_metadata)
: InterchangeObject(header_metadata, header_metadata->createCSet(&set_key))
{
    header_metadata->add(this);
}

TaggedValue::TaggedValue(HeaderMetadata *header_metadata, ::MXFMetadataSet *c_metadata_set)
: InterchangeObject(header_metadata, c_metadata_set)
{}

TaggedValue::~TaggedValue()
{}

string TaggedValue::getName()
{
    return getStringItem(&MXF_ITEM_K(TaggedValue, Name));
}

bool TaggedValue::isStringValue()
{
    ByteArray bytes;
    bool is_big_endian;

    bytes = getRawBytesItem(&MXF_ITEM_K(TaggedValue, Value));
    
    if (bytes.length <= sizeof(g_prefix_be) || (bytes.data[0] != 0x42 && bytes.data[0] != 0x4C))
    {
        return false;
    }

    is_big_endian = (bytes.data[0] == 0x42);

    if ((is_big_endian && memcmp(&bytes.data[1], &g_prefix_be[1], sizeof(g_prefix_be) - 1) != 0) ||
        (!is_big_endian && memcmp(&bytes.data[1], &g_prefix_le[1], sizeof(g_prefix_le) - 1) != 0))
    {
        return false;
    }
    
    return true;
}

string TaggedValue::getStringValue()
{
    ByteArray bytes;
    bool is_big_endian;
    mxfUTF16Char *utf16_value = 0;
    char* utf8_value = 0;
    size_t utf8_size;
    uint16_t i;
    uint8_t *value_ptr;
    uint16_t str_size;
    string result;
    
    MXFPP_CHECK(isStringValue());

    try {
        bytes = getRawBytesItem(&MXF_ITEM_K(TaggedValue, Value));
        is_big_endian = (bytes.data[0] == 0x42);

        str_size = (bytes.length - sizeof(g_prefix_be)) / 2;
        if (str_size == 0 ||
            (bytes.data[sizeof(g_prefix_be)] == 0 && bytes.data[sizeof(g_prefix_be) + 1] == 0))
        {
            return "";
        }
        
        utf16_value = new mxfUTF16Char[str_size + 1];

        value_ptr = &bytes.data[sizeof(g_prefix_be)];
        for (i = 0; i < str_size; i++) {
            if (is_big_endian)
                utf16_value[i] = ((*value_ptr) << 8) | (*(value_ptr + 1));
            else
                utf16_value[i] = (*value_ptr) | ((*(value_ptr + 1)) << 8);

            if (utf16_value[i] == 0)
                break;

            value_ptr += 2;
        }
        utf16_value[str_size] = 0;

        utf8_size = wcstombs(0, utf16_value, 0);
        MXFPP_CHECK(utf8_size != (size_t)(-1));
        utf8_size += 1;
        utf8_value = new char[utf8_size];
        wcstombs(utf8_value, utf16_value, utf8_size);
        
        result = utf8_value;
        
        delete [] utf16_value;
        utf16_value = 0;
        delete [] utf8_value;
        utf8_value = 0;

        return result;
        
    } catch (...) {
        delete [] utf16_value;
        delete [] utf8_value;
        throw;
    }
}

void TaggedValue::setName(string value)
{
    setStringItem(&MXF_ITEM_K(TaggedValue, Name), value);
}

