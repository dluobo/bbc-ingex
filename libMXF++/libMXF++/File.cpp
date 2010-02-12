/*
 * $Id: File.cpp,v 1.3 2010/02/12 13:52:49 philipn Exp $
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


// Utility class to hold a ::MXFPartitions list
class PartitionList
{
public:
    PartitionList()
    {
        mxf_initialise_list(&_cPartitions, NULL);
    }
    
    PartitionList(vector<Partition*>& partitions)
    {
        mxf_initialise_list(&_cPartitions, NULL);
        
        vector<Partition*>::const_iterator iter;
        for (iter = partitions.begin(); iter != partitions.end(); iter++)
        {
            MXFPP_CHECK(mxf_append_partition(&_cPartitions, (*iter)->getCPartition()));
        }
    }
    
    ~PartitionList()
    {
        mxf_clear_list(&_cPartitions);
    }
    
    ::MXFFilePartitions* getList()
    {
        return &_cPartitions;
    }
    
private:
    ::MXFFilePartitions _cPartitions;
};



File* File::openNew(string filename)
{
    ::MXFFile* cFile;
    MXFPP_CHECK(mxf_disk_file_open_new(filename.c_str(), &cFile));
    
    return new File(cFile);
}

File* File::openRead(string filename)
{
    ::MXFFile* cFile;
    MXFPP_CHECK(mxf_disk_file_open_read(filename.c_str(), &cFile));
    
    return new File(cFile);
}

File* File::openModify(string filename)
{
    ::MXFFile* cFile;
    MXFPP_CHECK(mxf_disk_file_open_modify(filename.c_str(), &cFile));
    
    return new File(cFile);
}


File::File(::MXFFile* cFile)
: _cFile(cFile)
{}

File::~File()
{
    mxf_file_close(&_cFile);

    vector<Partition*>::iterator iter;
    for (iter = _partitions.begin(); iter != _partitions.end(); iter++)
    {
        delete *iter;
    }
}

void File::setMinLLen(uint8_t llen)
{
    mxf_file_set_min_llen(_cFile, llen);
}

Partition& File::createPartition()
{
    Partition* previousPartition = 0;
    Partition* partition = 0;
    
    if (_partitions.size() > 0)
    {
        previousPartition = _partitions.back();
    }
    
    _partitions.push_back(new Partition());
    partition = _partitions.back();
    if (previousPartition)
    {
        MXFPP_CHECK(mxf_initialise_with_partition(previousPartition->getCPartition(), partition->getCPartition()));
    }
    
    return (*_partitions.back());
}

void File::writeRIP()
{
    PartitionList partitionList(_partitions);
    
    MXFPP_CHECK(mxf_write_rip(_cFile, partitionList.getList()));
}

void File::updatePartitions()
{
    PartitionList partitionList(_partitions);
    
    MXFPP_CHECK(mxf_update_partitions(_cFile, partitionList.getList()));
}

Partition& File::getPartition(int index)
{
    MXFPP_CHECK(index >= 0 && index < (int)_partitions.size());
    return *_partitions.at(index);
}

void File::readK(mxfKey* key)
{
    MXFPP_CHECK(mxf_read_k(_cFile, key));
}

void File::readL(uint8_t* llen, uint64_t* len)
{
    MXFPP_CHECK(mxf_read_l(_cFile, llen, len));
}

void File::readKL(mxfKey* key, uint8_t* llen, uint64_t* len)
{
    MXFPP_CHECK(mxf_read_kl(_cFile, key, llen, len));
}

void File::readNextNonFillerKL(mxfKey* key, uint8_t* llen, uint64_t *len)
{
    MXFPP_CHECK(mxf_read_next_nonfiller_kl(_cFile, key, llen, len));
}

uint32_t File::read(unsigned char* data, uint32_t count)
{
    return mxf_file_read(_cFile, data, count);
}

int64_t File::tell()
{
    return mxf_file_tell(_cFile);
}

void File::seek(int64_t position, int whence)
{
    MXFPP_CHECK(mxf_file_seek(_cFile, position, whence));
}

void File::skip(uint64_t len)
{
    MXFPP_CHECK(mxf_skip(_cFile, len));
}

int64_t File::size()
{
    return mxf_file_size(_cFile);
}

bool File::eof()
{
    return mxf_file_eof(_cFile);
}


uint32_t File::write(unsigned char* data, uint32_t count)
{
    return mxf_file_write(_cFile, data, count);
}

void File::writeUInt8(uint8_t value)
{
    MXFPP_CHECK(mxf_write_uint8(_cFile, value));
}

void File::writeUInt16(uint16_t value)
{
    MXFPP_CHECK(mxf_write_uint16(_cFile, value));
}

void File::writeUInt32(uint32_t value)
{
    MXFPP_CHECK(mxf_write_uint32(_cFile, value));
}

void File::writeUInt64(uint64_t value)
{
    MXFPP_CHECK(mxf_write_uint64(_cFile, value));
}

void File::writeInt8(int8_t value)
{
    MXFPP_CHECK(mxf_write_int8(_cFile, value));
}

void File::writeInt16(int16_t value)
{
    MXFPP_CHECK(mxf_write_int16(_cFile, value));
}

void File::writeInt32(int32_t value)
{
    MXFPP_CHECK(mxf_write_int32(_cFile, value));
}

void File::writeInt64(int64_t value)
{
    MXFPP_CHECK(mxf_write_int64(_cFile, value));
}

void File::writeUL(const mxfUL* ul)
{
    MXFPP_CHECK(mxf_write_ul(_cFile, ul));
}

void File::writeKL(const mxfKey* key, uint64_t len)
{
    MXFPP_CHECK(mxf_write_kl(_cFile, key, len));
}

void File::writeFixedL(uint8_t llen, uint64_t len)
{
    MXFPP_CHECK(mxf_write_fixed_l(_cFile, llen, len));
}

void File::writeFixedKL(const mxfKey* key, uint8_t llen, uint64_t len)
{
    MXFPP_CHECK(mxf_write_fixed_kl(_cFile, key, llen, len));
}

void File::writeBatchHeader(uint32_t len, uint32_t eleLen)
{
    MXFPP_CHECK(mxf_write_batch_header(_cFile, len, eleLen));
}

void File::writeArrayHeader(uint32_t len, uint32_t eleLen)
{
    MXFPP_CHECK(mxf_write_array_header(_cFile, len, eleLen));
}

void File::fillToPosition(uint64_t position)
{
    MXFPP_CHECK(mxf_fill_to_position(_cFile, position));
}

void File::writeFill(uint32_t size)
{
    MXFPP_CHECK(mxf_write_fill(_cFile, size));
}



