/*
 * $Id: IndexTable.cpp,v 1.2 2009/10/12 15:30:25 philipn Exp $
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
#include <mxf/mxf_avid.h>


using namespace std;
using namespace mxfpp;


bool IndexTableSegment::isIndexTableSegment(const mxfKey* key)
{
    return mxf_is_index_table_segment(key) != 0;
}

IndexTableSegment* IndexTableSegment::read(File* mxfFile, uint64_t segmentLen)
{
    ::MXFIndexTableSegment* cSegment;
    MXFPP_CHECK(mxf_read_index_table_segment(mxfFile->getCFile(), segmentLen, &cSegment));
    return new IndexTableSegment(cSegment);
}


IndexTableSegment::IndexTableSegment()
: _cSegment(0)
{
    MXFPP_CHECK(mxf_create_index_table_segment(&_cSegment));
}

IndexTableSegment::IndexTableSegment(::MXFIndexTableSegment* cSegment)
: _cSegment(cSegment)
{}

IndexTableSegment::~IndexTableSegment()
{
    mxf_free_index_table_segment(&_cSegment);
}


mxfUUID IndexTableSegment::getInstanceUID()
{
    return _cSegment->instanceUID;
}

mxfRational IndexTableSegment::getIndexEditRate()
{
    return _cSegment->indexEditRate;
}

int64_t IndexTableSegment::getIndexStartPosition()
{
    return _cSegment->indexStartPosition;
}

int64_t IndexTableSegment::getIndexDuration()
{
    return _cSegment->indexDuration;
}

uint32_t IndexTableSegment::getEditUnitByteCount()
{
    return _cSegment->editUnitByteCount;
}

uint32_t IndexTableSegment::getIndexSID()
{
    return _cSegment->indexSID;
}

uint32_t IndexTableSegment::getBodySID()
{
    return _cSegment->bodySID;
}

uint8_t IndexTableSegment::getSliceCount()
{
    return _cSegment->sliceCount;
}

uint8_t IndexTableSegment::getPosTableCount()
{
    return _cSegment->posTableCount;
}


void IndexTableSegment::setInstanceUID(mxfUUID value)
{
    _cSegment->instanceUID = value;
}

void IndexTableSegment::setIndexEditRate(mxfRational value)
{
    _cSegment->indexEditRate = value;
}

void IndexTableSegment::setIndexStartPosition(int64_t value)
{
    _cSegment->indexStartPosition = value;
}

void IndexTableSegment::setIndexDuration(int64_t value)
{
    _cSegment->indexDuration = value;
}

void IndexTableSegment::setEditUnitByteCount(uint32_t value)
{
    _cSegment->editUnitByteCount = value;
}

void IndexTableSegment::setIndexSID(uint32_t value)
{
    _cSegment->indexSID = value;
}

void IndexTableSegment::setBodySID(uint32_t value)
{
    _cSegment->bodySID = value;
}

void IndexTableSegment::setSliceCount(uint8_t value)
{
    _cSegment->sliceCount = value;
}

void IndexTableSegment::setPosTableCount(uint8_t value)
{
    _cSegment->posTableCount = value;
}

void IndexTableSegment::appendDeltaEntry(int8_t posTableIndex, uint8_t slice, uint32_t elementData)
{
    MXFPP_CHECK(mxf_default_add_delta_entry(NULL, 0, _cSegment, posTableIndex, slice, elementData));
}

void IndexTableSegment::appendIndexEntry(int8_t temporalOffset, int8_t keyFrameOffset, uint8_t flags, 
    uint64_t streamOffset, const vector<uint32_t>& sliceOffset, const vector<mxfRational>& posTable)
{
    uint32_t* cSliceOffset = 0;
    mxfRational* cPosTable = 0;
    uint8_t i;
    
    try
    {
        if (sliceOffset.size() > 0)
        {
            cSliceOffset = new uint32_t[sliceOffset.size()];
            for (i = 0; i < (uint8_t)sliceOffset.size(); i++)
            {
                cSliceOffset[i] = sliceOffset.at(i);
            }
        }

        if (posTable.size() > 0)
        {
            cPosTable = new mxfRational[posTable.size()];
            for (i = 0; i < (uint8_t)posTable.size(); i++)
            {
                cPosTable[i] = posTable.at(i);
            }
        }
    
        MXFPP_CHECK(mxf_default_add_index_entry(NULL, 0, _cSegment, temporalOffset, keyFrameOffset, flags, streamOffset,
            cSliceOffset, cPosTable));

        delete [] cSliceOffset;
        delete [] cPosTable;
    }
    catch (...)
    {
        delete [] cSliceOffset;
        delete [] cPosTable;
        throw;
    }
}

void IndexTableSegment::write(File* mxfFile, Partition* partition, FillerWriter* filler)
{
    partition->markIndexStart(mxfFile);

    MXFPP_CHECK(mxf_write_index_table_segment(mxfFile->getCFile(), _cSegment));
    if (filler)
    {
        filler->write(mxfFile);
    }
    else
    {
        partition->fillToKag(mxfFile);
    }
    
    partition->markIndexEnd(mxfFile);
}

void IndexTableSegment::writeHeader(File* mxfFile, uint32_t numDeltaEntries, uint32_t numIndexEntries)
{
    MXFPP_CHECK(mxf_write_index_table_segment_header(mxfFile->getCFile(), _cSegment, numDeltaEntries, numIndexEntries));
}

void IndexTableSegment::writeDeltaEntryArrayHeader(File* mxfFile, uint32_t numDeltaEntries)
{
    MXFPP_CHECK(mxf_write_delta_entry_array_header(mxfFile->getCFile(), numDeltaEntries));
}

void IndexTableSegment::writeDeltaEntry(File* mxfFile, int8_t posTableIndex, uint8_t slice, uint32_t elementData)
{
    ::MXFDeltaEntry entry;
    entry.posTableIndex = posTableIndex;
    entry.slice = slice;
    entry.elementData = elementData;
    
    MXFPP_CHECK(mxf_write_delta_entry(mxfFile->getCFile(), &entry));
}

void IndexTableSegment::writeIndexEntryArrayHeader(File* mxfFile, uint8_t sliceCount, uint8_t posTableCount, uint32_t numIndexEntries)
{
    MXFPP_CHECK(mxf_write_index_entry_array_header(mxfFile->getCFile(), sliceCount, posTableCount, numIndexEntries));
}

// TODO: this is not very efficient if sliceOffset or posTable size > 0
void IndexTableSegment::writeIndexEntry(File* mxfFile, int8_t temporalOffset, int8_t keyFrameOffset, uint8_t flags, uint64_t streamOffset, 
    const std::vector<uint32_t>& sliceOffset, const std::vector<mxfRational>& posTable)
{
    uint8_t i;
    MXFIndexEntry entry;
    
    entry.temporalOffset = temporalOffset;
    entry.keyFrameOffset = keyFrameOffset;
    entry.flags = flags;
    entry.streamOffset = streamOffset;
    entry.sliceOffset = 0;
    entry.posTable = 0;
    
    try
    {
        if (sliceOffset.size() > 0)
        {
            entry.sliceOffset = new uint32_t[sliceOffset.size()];
            MXFPP_CHECK(entry.sliceOffset != 0);
            for (i = 0; i < (uint32_t)sliceOffset.size(); i++)
            {
                entry.sliceOffset[i] = sliceOffset.at(i);
            }
        }
        
        if (posTable.size() > 0)
        {
            entry.posTable = new mxfRational[posTable.size()];
            MXFPP_CHECK(entry.posTable != 0);
            for (i = 0; i < (uint32_t)posTable.size(); i++)
            {
                entry.posTable[i] = posTable.at(i);
            }
        }
        
        MXFPP_CHECK(mxf_write_index_entry(mxfFile->getCFile(), (uint8_t)sliceOffset.size(), 
            (uint8_t)posTable.size(), &entry));

        delete [] entry.sliceOffset;
        delete [] entry.posTable;
    }
    catch (...)
    {
        delete [] entry.sliceOffset;
        delete [] entry.posTable;
        throw;
    }
}

void IndexTableSegment::writeAvidIndexEntryArrayHeader(File* mxfFile, uint8_t sliceCount, uint8_t posTableCount, uint32_t numIndexEntries)
{
    MXFPP_CHECK(mxf_avid_write_index_entry_array_header(mxfFile->getCFile(), sliceCount, posTableCount, numIndexEntries));
}

