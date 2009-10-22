/*
 * $Id: RawEssenceParser.h,v 1.1 2009/10/22 16:37:31 philipn Exp $
 *
 * Parse raw essence data
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

#ifndef __RAW_ESSENCE_PARSER_H__
#define __RAW_ESSENCE_PARSER_H__

#include <vector>

#include "DynamicByteArray.h"
#include "FrameOffsetIndexTable.h"

namespace mxfpp
{
class File;
class FileDescriptor;
};


class RawEssenceParser
{
public:
    static RawEssenceParser* Create(mxfpp::File *file, int64_t essence_length, mxfUL essence_label,
                                    const mxfpp::FileDescriptor *file_descriptor, mxfRational edit_rate,
                                    uint32_t frame_size, FrameOffsetIndexTableSegment *index_table);

public:
    virtual ~RawEssenceParser();

    mxfUL GetEssenceContainerLabel() { return mEssenceLabel; }
    int64_t GetDuration() { return mDuration; }
    int64_t GetPosition() { return mPosition; }
    
    int64_t GetEssenceOffset() { return mEssenceOffset; }

    bool IsEOF();

public:
    virtual bool Read(DynamicByteArray *data, uint32_t *num_samples) = 0;
    virtual bool Seek(int64_t position) = 0;
    virtual int64_t DetermineDuration() = 0;
    
protected:
    RawEssenceParser(mxfpp::File *file, int64_t essence_length, mxfUL essence_label);

protected:
    mxfpp::File *mFile;
    int64_t mEssenceLength;
    mxfUL mEssenceLabel;
    int64_t mEssenceStartOffset;
    
    int64_t mPosition;
    int64_t mDuration;
    int64_t mEssenceOffset;
};


#endif

