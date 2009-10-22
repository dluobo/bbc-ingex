/*
 * $Id: PCMEssenceParser.cpp,v 1.1 2009/10/22 16:37:31 philipn Exp $
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

#include <libMXF++/MXF.h>

#include "PCMEssenceParser.h"

using namespace std;
using namespace mxfpp;



PCMEssenceParser::PCMEssenceParser(File *file, int64_t essence_length, mxfUL essence_label, mxfRational edit_rate,
                                   const FileDescriptor *file_descriptor)
: RawEssenceParser(file, essence_length, essence_label)
{
    // only support 625 or 525 line frame rates
    MXFPP_ASSERT((edit_rate.numerator == 25 && edit_rate.denominator == 1) ||
                 (edit_rate.numerator == 30000 && edit_rate.denominator == 1001));
                 
    const GenericSoundEssenceDescriptor *sound_descriptor = 
        dynamic_cast<const GenericSoundEssenceDescriptor*>(file_descriptor);
    if (!sound_descriptor)
        throw MXFException("FileDescriptor for PCM audio is not a GenericSoundEssenceDescriptor");
    
    if (sound_descriptor->haveAudioSamplingRate()) {
        mxfRational sampling_rate = sound_descriptor->getAudioSamplingRate();
        // only support 48kHz
        MXFPP_ASSERT(sampling_rate.numerator == 48000 && sampling_rate.denominator == 1);
    }
    
    if (!sound_descriptor->haveQuantizationBits())
        throw MXFException("Sound descriptor does not specify quantization bits");
    if (!sound_descriptor->haveChannelCount())
        throw MXFException("Sound descriptor does not specify channel count");
    
    mBytesPerSample = sound_descriptor->getChannelCount() * ((sound_descriptor->getQuantizationBits() + 7) / 8);
    
    if (edit_rate.numerator == 25 && edit_rate.denominator == 1) {
        mSequenceLen = 1;
        mFrameSizeSequence[0] = 1920 * mBytesPerSample;
        mFrameSequenceSize = mFrameSizeSequence[0];
    } else { // 30000/1001
        mSequenceLen = 5;
        mFrameSizeSequence[0] = 1602 * mBytesPerSample;
        mFrameSizeSequence[1] = 1601 * mBytesPerSample;
        mFrameSizeSequence[2] = 1602 * mBytesPerSample;
        mFrameSizeSequence[3] = 1601 * mBytesPerSample;
        mFrameSizeSequence[4] = 1602 * mBytesPerSample;
        mFrameSequenceSize = (1602 * 3 + 1601 * 2) * mBytesPerSample;
    }
    
    DetermineDuration();
}

PCMEssenceParser::~PCMEssenceParser()
{
}

bool PCMEssenceParser::Read(DynamicByteArray *data, uint32_t *num_samples)
{
    if (IsEOF())
        return false;

    uint32_t frame_size = (uint32_t)(mFrameSizeSequence[mPosition % mSequenceLen]);

    data->allocate(frame_size);
    uint32_t count = mFile->read(data->getBytes(), frame_size);
    if (count != frame_size) {
        mFile->seek(-count, SEEK_CUR);
        mDuration = mPosition;
        return false;
    }
    
    data->setSize(frame_size);
    *num_samples = frame_size / mBytesPerSample;
    
    mEssenceOffset += frame_size;
    mPosition++;

    return true;
}

bool PCMEssenceParser::Seek(int64_t position)
{
    if (position >= mDuration)
        return false;
    
    // whole sequence offset
    int64_t offset = (position / mSequenceLen) * mFrameSequenceSize;
    
    // partial sequence offset
    int remainder = (int)(position % mSequenceLen);
    int i;
    for (i = 0; i < remainder; i++)
        offset += mFrameSizeSequence[i];
    
    mFile->seek(mEssenceStartOffset + offset, SEEK_SET);
    
    mEssenceOffset = offset;
    mPosition = position;
    
    return true;
}

int64_t PCMEssenceParser::DetermineDuration()
{
    if (mDuration >= 0)
        return mDuration;

    int64_t file_size = mFile->size();

    int64_t essence_length = file_size - mEssenceStartOffset;
    if (mEssenceLength > 0 && essence_length > mEssenceLength)
        essence_length = mEssenceLength;
        
    // whole sequences
    mDuration = mSequenceLen * (essence_length / mFrameSequenceSize);
        
    // partial sequence
    int64_t remainder = essence_length - mDuration * mFrameSequenceSize / mSequenceLen;
    int i;
    for (i = 0; i < mSequenceLen; i++) {
        remainder -= mFrameSizeSequence[i];
        if (remainder < 0)
            break;
        mDuration++;
    }
    
    return mDuration;
}

