/*
 * $Id: IndexTable.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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

#ifndef __MXFPP_INDEX_TABLE_H__
#define __MXFPP_INDEX_TABLE_H__

#include <vector>


namespace mxfpp
{

class File;

class IndexTableSegment
{
public:
    static bool isIndexTableSegment(const mxfKey* key);
    static IndexTableSegment* read(File* mxfFile, uint64_t segmentLen);

    
    IndexTableSegment();
    IndexTableSegment(::MXFIndexTableSegment* cSegment);
    virtual ~IndexTableSegment();
    
    mxfUUID getInstanceUID();
    mxfRational getIndexEditRate();
    int64_t getIndexStartPosition();
    int64_t getIndexDuration();
    uint32_t getEditUnitByteCount();
    uint32_t getIndexSID();
    uint32_t getBodySID();
    uint8_t getSliceCount();
    uint8_t getPosTableCount();
    // deltaEntryArray
    // indexEntryArray
    
    void setInstanceUID(mxfUUID value);
    void setIndexEditRate(mxfRational value);
    void setIndexStartPosition(int64_t value);
    void setIndexDuration(int64_t value);
    void setEditUnitByteCount(uint32_t value);
    void setIndexSID(uint32_t value);
    void setBodySID(uint32_t value);
    void setSliceCount(uint8_t value);
    void setPosTableCount(uint8_t value);
    
    void appendDeltaEntry(int8_t posTableIndex, uint8_t slice, uint32_t elementData);
    void appendIndexEntry(int8_t temporalOffset, int8_t keyFrameOffset, uint8_t flags, uint64_t streamOffset, 
        const std::vector<uint32_t>& sliceOffset, const std::vector<mxfRational>& posTable);

    
    void write(File* mxfFile, Partition* partition, FillerWriter* filler);
    
    void writeHeader(File* mxfFile, uint32_t numDeltaEntries, uint32_t numIndexEntries);
    void writeDeltaEntryArrayHeader(File* mxfFile, uint32_t numDeltaEntries);
    void writeDeltaEntry(File* mxfFile, int8_t posTableIndex, uint8_t slice, uint32_t elementData);
    void writeIndexEntryArrayHeader(File* mxfFile, uint8_t sliceCount, uint8_t posTableCount, uint32_t numIndexEntries);
    void writeIndexEntry(File* mxfFile, int8_t temporalOffset, int8_t keyFrameOffset, uint8_t flags, uint64_t streamOffset, 
        const std::vector<uint32_t>& sliceOffset, const std::vector<mxfRational>& posTable);
    
    void writeAvidIndexEntryArrayHeader(File* mxfFile, uint8_t sliceCount, uint8_t posTableCount, uint32_t numIndexEntries);
    
    ::MXFIndexTableSegment* getCIndexTableSegment() const { return _cSegment; }

private:
    ::MXFIndexTableSegment* _cSegment;
};



};


#endif

