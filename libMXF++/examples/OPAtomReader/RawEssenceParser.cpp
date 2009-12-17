/*
 * $Id: RawEssenceParser.cpp,v 1.3 2009/12/17 16:36:43 john_f Exp $
 *
 * Parse raw essence data
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

#include <libMXF++/MXF.h>

#include <mxf/mxf_labels_and_keys.h>
#include <mxf/mxf_avid_labels_and_keys.h>

#include "RawEssenceParser.h"
#include "FixedSizeEssenceParser.h"
#include "VariableSizeEssenceParser.h"
#include "PCMEssenceParser.h"

using namespace std;
using namespace mxfpp;



typedef enum
{
    FIXED_FRAME_SIZE_PARSER,
    PCM_PARSER,
    VARIABLE_FRAME_SIZE_PARSER
} ParserType;

typedef struct
{
    const mxfUL essence_label;
    ParserType parser_type;
    uint32_t fixed_frame_size;
} SupportedFormat;

const SupportedFormat SUPPORTED_FORMATS[] =
{
    {MXF_EC_L(IECDV_25_525_60_ClipWrapped),         FIXED_FRAME_SIZE_PARSER,    120000},
    {MXF_EC_L(IECDV_25_625_50_ClipWrapped),         FIXED_FRAME_SIZE_PARSER,    144000},
    {MXF_EC_L(DVBased_25_525_60_ClipWrapped),       FIXED_FRAME_SIZE_PARSER,    120000},
    {MXF_EC_L(DVBased_25_625_50_ClipWrapped),       FIXED_FRAME_SIZE_PARSER,    144000},
    {MXF_EC_L(DVBased_50_525_60_ClipWrapped),       FIXED_FRAME_SIZE_PARSER,    240000},
    {MXF_EC_L(DVBased_50_625_50_ClipWrapped),       FIXED_FRAME_SIZE_PARSER,    288000},
    {MXF_EC_L(DVBased_100_1080_50_I_ClipWrapped),   FIXED_FRAME_SIZE_PARSER,    576000},
    {MXF_EC_L(DVBased_100_720_50_P_ClipWrapped),    FIXED_FRAME_SIZE_PARSER,    288000},
    {MXF_EC_L(DNxHD1080p36ClipWrapped),             FIXED_FRAME_SIZE_PARSER,    188416},
    {MXF_EC_L(DNxHD1080i120ClipWrapped),            FIXED_FRAME_SIZE_PARSER,    606208},
    {MXF_EC_L(DNxHD1080p120ClipWrapped),            FIXED_FRAME_SIZE_PARSER,    606208},
    {MXF_EC_L(DNxHD1080i185ClipWrapped),            FIXED_FRAME_SIZE_PARSER,    917504},
    {MXF_EC_L(DNxHD1080p185ClipWrapped),            FIXED_FRAME_SIZE_PARSER,    917504},
    {MXF_EC_L(DNxHD1080i185XClipWrapped),           FIXED_FRAME_SIZE_PARSER,    917504},
    {MXF_EC_L(DNxHD1080p185XClipWrapped),           FIXED_FRAME_SIZE_PARSER,    917504},
    {MXF_EC_L(DNxHD720p120ClipWrapped),             FIXED_FRAME_SIZE_PARSER,    303104},
    {MXF_EC_L(DNxHD720p185ClipWrapped),             FIXED_FRAME_SIZE_PARSER,    458752},
    {MXF_EC_L(SD_Unc_625_50i_422_135_ClipWrapped),  FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(HD_Unc_1080_50i_422_ClipWrapped),     FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(AvidIMX30_625_50),                    FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(AvidIMX40_625_50),                    FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(AvidIMX50_625_50),                    FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(AvidIMX30_525_60),                    FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(AvidIMX40_525_60),                    FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(AvidIMX50_525_60),                    FIXED_FRAME_SIZE_PARSER,         0},
    {MXF_EC_L(AvidMJPEGClipWrapped),                VARIABLE_FRAME_SIZE_PARSER,      0},
    {MXF_EC_L(BWFClipWrapped),                      PCM_PARSER,                      0},
    {MXF_EC_L(AES3ClipWrapped),                     PCM_PARSER,                      0},
};

#define NUM_SUPPORTED_FORMATS   (sizeof(SUPPORTED_FORMATS) / sizeof(SupportedFormat))




RawEssenceParser* RawEssenceParser::Create(File *file, int64_t essence_length, mxfUL essence_label,
                                           const FileDescriptor *file_descriptor, mxfRational edit_rate,
                                           uint32_t frame_size, FrameOffsetIndexTableSegment *index_table)
{
    int64_t fixed_frame_size;
    size_t i;
    for (i = 0; i < NUM_SUPPORTED_FORMATS; i++) {
        if (mxf_equals_ul(&essence_label, &SUPPORTED_FORMATS[i].essence_label)) {
            switch (SUPPORTED_FORMATS[i].parser_type)
            {
                case FIXED_FRAME_SIZE_PARSER:
                {
                    fixed_frame_size = SUPPORTED_FORMATS[i].fixed_frame_size;
                    if (fixed_frame_size == 0)
                        fixed_frame_size = frame_size;
                    return new FixedSizeEssenceParser(file, essence_length, essence_label, file_descriptor,
                                                      fixed_frame_size);
                }
                case PCM_PARSER:
                {
                    // TODO: edit rate passed in should be the video edit rate
                    mxfRational edit_rate = (mxfRational){25, 1};
                    return new PCMEssenceParser(file, essence_length, essence_label, edit_rate, file_descriptor);
                }
                case VARIABLE_FRAME_SIZE_PARSER:
                {
                    return new VariableSizeEssenceParser(file, essence_length, essence_label, edit_rate,
                                                         file_descriptor, index_table);
                }
            }
        }
    }
    
    return 0;
}


RawEssenceParser::RawEssenceParser(File *file, int64_t essence_length, mxfUL essence_label)
{
    mFile = file;
    mEssenceLength = essence_length;
    mEssenceLabel = essence_label;
    mPosition = 0;
    mDuration = -1;
    mEssenceOffset = 0;
    
    mEssenceStartOffset = mFile->tell();
}

RawEssenceParser::~RawEssenceParser()
{
    delete mFile;
}

bool RawEssenceParser::IsEOF()
{
    return mDuration >= 0 && mPosition >= mDuration;
}

