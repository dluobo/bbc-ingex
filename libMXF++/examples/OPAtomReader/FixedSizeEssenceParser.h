/*
 * $Id: FixedSizeEssenceParser.h,v 1.2 2009/10/23 09:05:21 philipn Exp $
 *
 * Parse raw essence data with fixed frame size
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

#ifndef __FIXED_SIZE_ESSENCE_PARSER_H__
#define __FIXED_SIZE_ESSENCE_PARSER_H__

#include "RawEssenceParser.h"


class FixedSizeEssenceParser : public RawEssenceParser
{
public:
    FixedSizeEssenceParser(mxfpp::File *file, int64_t essence_length, mxfUL essence_label,
                           const mxfpp::FileDescriptor *file_descriptor, uint32_t frame_size);
    virtual ~FixedSizeEssenceParser();

public:
    // from RawEssenceParser
    virtual bool Read(DynamicByteArray *data, uint32_t *num_samples);
    virtual bool Seek(int64_t position);
    virtual int64_t DetermineDuration();

private:
    bool DetermineFrameSize();
    bool DetermineMPEGFrameSize();
    void DetermineUncFrameSize(const mxfpp::FileDescriptor *file_descriptor);

private:
    uint32_t mFrameSize;
};



#endif

