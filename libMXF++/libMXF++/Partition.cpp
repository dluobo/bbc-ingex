/*
 * $Id: Partition.cpp,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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


PositionFillerWriter::PositionFillerWriter(int64_t position)
: _position(position)
{}

PositionFillerWriter::~PositionFillerWriter()
{}

void PositionFillerWriter::write(File* file)
{
    file->fillToPosition(_position);
}



Partition* Partition::read(File* file, const mxfKey* key)
{
    ::MXFPartition* cPartition;
    MXFPP_CHECK(mxf_read_partition(file->getCFile(), key, &cPartition));
    
    return new Partition(cPartition);
}

Partition* Partition::findAndReadHeaderPartition(File* file)
{
    mxfKey key;
    uint8_t llen;
    uint64_t len;

    if (!mxf_read_header_pp_kl(file->getCFile(), &key, &llen, &len))
    {
        return 0;
    }
    
    return read(file, &key);
}


Partition::Partition()
{
    MXFPP_CHECK(mxf_create_partition(&_cPartition));
}

Partition::Partition(::MXFPartition* cPartition)
: _cPartition(cPartition)
{}

Partition::Partition(const Partition& partition)
{
    MXFPP_CHECK(mxf_create_from_partition(partition._cPartition, &_cPartition));
}

Partition::~Partition()
{
    mxf_free_partition(&_cPartition);
}

void Partition::setKey(const mxfKey* key)
{
    _cPartition->key = *key;
}

void Partition::setVersion(uint16_t majorVersion, uint16_t minorVersion)
{
    _cPartition->majorVersion = majorVersion;
    _cPartition->minorVersion = minorVersion;
}

void Partition::setKagSize(uint32_t kagSize)
{
    _cPartition->kagSize = kagSize;
}

void Partition::setThisPartition(uint64_t thisPartition)
{
    _cPartition->thisPartition = thisPartition;
}

void Partition::setPreviousPartition(uint64_t previousPartition)
{
    _cPartition->previousPartition = previousPartition;
}

void Partition::setFooterPartition(uint64_t footerPartition)
{
    _cPartition->footerPartition = footerPartition;
}

void Partition::setHeaderByteCount(uint64_t headerByteCount)
{
    _cPartition->headerByteCount = headerByteCount;
}

void Partition::setIndexByteCount(uint64_t indexByteCount)
{
    _cPartition->indexByteCount = indexByteCount;
}

void Partition::setIndexSID(uint32_t indexSID)
{
    _cPartition->indexSID = indexSID;
}

void Partition::setBodyOffset(uint64_t bodyOffset)
{
    _cPartition->bodyOffset = bodyOffset;
}

void Partition::setBodySID(uint32_t bodySID)
{
    _cPartition->bodySID = bodySID;
}

void Partition::setOperationalPattern(const mxfUL* operationalPattern)
{
    _cPartition->operationalPattern = *operationalPattern;
}

void Partition::addEssenceContainer(const mxfUL* essenceContainer)
{
    MXFPP_CHECK(mxf_append_partition_esscont_label(_cPartition, essenceContainer));
}

const mxfKey* Partition::getKey()
{
    return &_cPartition->key;
}

uint16_t Partition::getMajorVersion()
{
    return _cPartition->majorVersion;
}

uint16_t Partition::getMinorVersion()
{
    return _cPartition->minorVersion;
}

uint32_t Partition::getKagSize()
{
    return _cPartition->kagSize;
}

uint64_t Partition::getThisPartition()
{
    return _cPartition->thisPartition;
}

uint64_t Partition::getPreviousPartition()
{
    return _cPartition->previousPartition;
}

uint64_t Partition::getFooterPartition()
{
    return _cPartition->footerPartition;
}

uint64_t Partition::getHeaderByteCount()
{
    return _cPartition->headerByteCount;
}

uint64_t Partition::getIndexByteCount()
{
    return _cPartition->indexByteCount;
}

uint32_t Partition::getIndexSID()
{
    return _cPartition->indexSID;
}

uint64_t Partition::getBodyOffset()
{
    return _cPartition->bodyOffset;
}

uint32_t Partition::getBodySID()
{
    return _cPartition->bodySID;
}

const mxfUL* Partition::getOperationalPattern()
{
    return &_cPartition->operationalPattern;
}

vector<mxfUL> Partition::getEssenceContainers()
{
    vector<mxfUL> result;
    ::MXFListIterator iter;
    
    mxf_initialise_list_iter(&iter, &_cPartition->essenceContainers);
    
    while (mxf_next_list_iter_element(&iter))
    {
        mxfUL label;
        mxf_get_ul((const uint8_t*)mxf_get_iter_element(&iter), &label);
        result.push_back(label);
    }

    return result;
}




void Partition::markHeaderStart(File* file)
{
    MXFPP_CHECK(mxf_mark_header_start(file->getCFile(), _cPartition));
}

void Partition::markHeaderEnd(File* file)
{
    MXFPP_CHECK(mxf_mark_header_end(file->getCFile(), _cPartition));
}

void Partition::markIndexStart(File* file)
{
    MXFPP_CHECK(mxf_mark_index_start(file->getCFile(), _cPartition));
}

void Partition::markIndexEnd(File* file)
{
    MXFPP_CHECK(mxf_mark_index_end(file->getCFile(), _cPartition));
}

void Partition::write(File* file)
{
    MXFPP_CHECK(mxf_write_partition(file->getCFile(), _cPartition));
    fillToKag(file);
}

void Partition::fillToKag(File* file)
{
    if (_cPartition->kagSize > 0)
    {
        MXFPP_CHECK(mxf_fill_to_kag(file->getCFile(), _cPartition));
    }
}


