/*
 * $Id: PCMEssenceParser.h,v 1.1 2009/10/22 16:37:31 philipn Exp $
 *
 * Parse raw PCM essence data
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

#ifndef __PCM_ESSENCE_PARSER_H__
#define __PCM_ESSENCE_PARSER_H__

#include "RawEssenceParser.h"


class PCMEssenceParser : public RawEssenceParser
{
public:
    PCMEssenceParser(mxfpp::File *file, int64_t essence_length, mxfUL essence_label, mxfRational edit_rate,
                     const mxfpp::FileDescriptor *file_descriptor);
    virtual ~PCMEssenceParser();

public:
    // from RawEssenceParser
    virtual bool Read(DynamicByteArray *data, uint32_t *num_samples);
    virtual bool Seek(int64_t position);
    virtual int64_t DetermineDuration();
    
private:
    uint32_t mFrameSizeSequence[5];
    int mSequenceLen;
    uint32_t mFrameSequenceSize;
    unsigned int mBytesPerSample;
};


#endif

