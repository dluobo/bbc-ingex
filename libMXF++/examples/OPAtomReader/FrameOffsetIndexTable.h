/*
 * $Id: FrameOffsetIndexTable.h,v 1.2 2009/10/23 09:05:21 philipn Exp $
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

#ifndef __MXFPP_FRAME_OFFSET_INDEX_TABLE_H__
#define __MXFPP_FRAME_OFFSET_INDEX_TABLE_H__

#include <vector>

#include "libMXF++/IndexTable.h"


class FrameOffsetIndexTableSegment : public mxfpp::IndexTableSegment
{
public:
    static FrameOffsetIndexTableSegment* read(mxfpp::File *file, uint64_t segment_len);

public:
    FrameOffsetIndexTableSegment();
    virtual ~FrameOffsetIndexTableSegment();
    
    bool haveFrameOffset(int64_t position);
    int64_t getFrameOffset(int64_t position);
    
    bool getLastIndexOffset(int64_t *offset, int64_t *position);
    
    int64_t getDuration();
    
    void appendFrameOffset(int64_t offset);
    
private:
    std::vector<int64_t> mFrameOffsets;
};


#endif

