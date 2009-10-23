/*
 * $Id: FrameOffsetIndexTable.cpp,v 1.2 2009/10/23 09:05:21 philipn Exp $
 *
 * Basic index table with frame offsets
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include <memory>

#include <libMXF++/MXF.h>

#include <mxf/mxf_avid.h>

#include "FrameOffsetIndexTable.h"


using namespace std;
using namespace mxfpp;


int add_frame_offset_index_entry(void *data, uint32_t num_entries, MXFIndexTableSegment *segment,
                                 int8_t temporal_offset, int8_t key_frame_offset, uint8_t flags,
                                 uint64_t stream_offset, uint32_t *slice_offset, mxfRational *pos_table)
{
    FrameOffsetIndexTableSegment *index_table = static_cast<FrameOffsetIndexTableSegment*>(data);
    
    (void)segment;
    (void)temporal_offset;
    (void)key_frame_offset;
    (void)flags;
    (void)slice_offset;
    (void)pos_table;

    index_table->appendFrameOffset(stream_offset);

    return 1;
}



FrameOffsetIndexTableSegment* FrameOffsetIndexTableSegment::read(File* file, uint64_t segment_len)
{
    auto_ptr<FrameOffsetIndexTableSegment> index_table(new FrameOffsetIndexTableSegment());
    mxf_free_index_table_segment(&index_table->_cSegment);
    
    MXFPP_CHECK(mxf_avid_read_index_table_segment_2(file->getCFile(), segment_len, mxf_default_add_delta_entry, 0,
                                                    add_frame_offset_index_entry, index_table.get(),
                                                    &index_table->_cSegment));
        
    return index_table.release();
}



FrameOffsetIndexTableSegment::FrameOffsetIndexTableSegment()
: IndexTableSegment()
{
    setIndexDuration(-1);
}

FrameOffsetIndexTableSegment::~FrameOffsetIndexTableSegment()
{
}

bool FrameOffsetIndexTableSegment::haveFrameOffset(int64_t position)
{
    return position < (int64_t)mFrameOffsets.size();
}

int64_t FrameOffsetIndexTableSegment::getFrameOffset(int64_t position)
{
    MXFPP_CHECK(position < (int64_t)mFrameOffsets.size());
    
    return mFrameOffsets[position];
}

bool FrameOffsetIndexTableSegment::getLastIndexOffset(int64_t *offset, int64_t *position)
{
    if (mFrameOffsets.empty())
        return false;
    
    *position = mFrameOffsets.size() - 1;
    *offset = mFrameOffsets[*position];
    
    return true;
}

int64_t FrameOffsetIndexTableSegment::getDuration()
{
    if (getIndexDuration() >= 0)
        return getIndexDuration();
    
    // -1 because the last entry is the last frame's end offset
    return mFrameOffsets.size() - 1;
}

void FrameOffsetIndexTableSegment::appendFrameOffset(int64_t offset)
{
    if (mFrameOffsets.size() == mFrameOffsets.capacity())
        mFrameOffsets.reserve(mFrameOffsets.capacity() + 8192);
    
    return mFrameOffsets.push_back(offset);
}

