/*
 * $Id: File.h,v 1.3 2010/02/12 13:52:49 philipn Exp $
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

#ifndef __MXFPP_FILE_H__
#define __MXFPP_FILE_H__

#include <string>
#include <vector>

#include <libMXF++/Partition.h>


namespace mxfpp
{


class File
{
public:
    static File* openNew(std::string filename);
    static File* openRead(std::string filename);
    static File* openModify(std::string filename);

    ~File();
    
    void setMinLLen(uint8_t llen);
    Partition& createPartition();
    
    void writeRIP();
    void updatePartitions();
    
    Partition& getPartition(int index);
    
    void readK(mxfKey* key);
    void readL(uint8_t* llen, uint64_t* len);
    void readKL(mxfKey* key, uint8_t* llen, uint64_t* len);
    void readNextNonFillerKL(mxfKey* key, uint8_t* llen, uint64_t *len);

    uint32_t read(unsigned char* data, uint32_t count);
    int64_t tell();
    void seek(int64_t position, int whence);
    void skip(uint64_t len);
    int64_t size();
    bool eof();

    
    uint32_t write(unsigned char* data, uint32_t count);
    
    void writeUInt8(uint8_t value);
    void writeUInt16(uint16_t value);
    void writeUInt32(uint32_t value);
    void writeUInt64(uint64_t value);
    void writeInt8(int8_t value);
    void writeInt16(int16_t value);
    void writeInt32(int32_t value);
    void writeInt64(int64_t value);
    void writeUL(const mxfUL* ul);
    
    void writeKL(const mxfKey* key, uint64_t len);
    void writeFixedL(uint8_t llen, uint64_t len);
    void writeFixedKL(const mxfKey* key, uint8_t llen, uint64_t len);
    
    void writeBatchHeader(uint32_t len, uint32_t eleLen);
    void writeArrayHeader(uint32_t len, uint32_t eleLen);
    
    void fillToPosition(uint64_t position);
    void writeFill(uint32_t size);

    
    ::MXFFile* getCFile() const { return _cFile; }

private:
    File(::MXFFile* _cFile);
    
    std::vector<Partition*> _partitions;
    
    ::MXFFile* _cFile;
};

};




#endif 


