/*
 * $Id: D10MXFOP1AWriter.h,v 1.1 2010/02/12 13:52:49 philipn Exp $
 *
 * D10 MXF OP-1A writer
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
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

#ifndef __D10_MXF_OP1A_WRITER_H__
#define __D10_MXF_OP1A_WRITER_H__


#include <string>
#include <vector>

#include <libMXF++/MXF.h>

#include "D10ContentPackage.h"


class SetWithDuration;

class D10MXFOP1AWriter
{
public:
    typedef enum
    {
        D10_BIT_RATE_30,
        D10_BIT_RATE_40,
        D10_BIT_RATE_50
    } D10BitRate;

    typedef enum
    {
        D10_SAMPLE_RATE_625_50I,
        D10_SAMPLE_RATE_525_60I,
    } D10SampleRate;

public:
    D10MXFOP1AWriter(std::string filename);
    ~D10MXFOP1AWriter();
    
    // prepare
    void SetSampleRate(D10SampleRate sample_rate); // default D10_SAMPLE_RATE_625_50I
    void SetAudioChannelCount(uint32_t count); // default is 4
    void SetAudioQuantizationBits(uint32_t bits); // default is 24; alternative is 16
    void SetAspectRatio(mxfRational aspect_ratio); // default is 16:9; alternative is 4:3
    void SetStartTimecode(int64_t count, bool drop_frame); // default 0, false
    void SetBitRate(D10BitRate rate, uint32_t encoded_picture_size); // default D10_BIT_RATE_50, 250000
    void PrepareFile();
    
    
    // write content package data
    
    // LTC timecode written to the system item in the essence container
    void SetTimecode(Timecode ltc);
    // SetTimecode using a timecode calculated from the start timecode and duration
    void GenerateTimecode();
    
    // MPEG video data
    void SetVideo(const unsigned char *data, uint32_t size);
    
    // PCM audio data
    // the number of audio samples must follow the 5-sequence count 1602, 1601, 1602, 1601, 1602 when
    // the sample rate is 60i
    uint32_t GetAudioSampleCount();
    void SetAudio(uint32_t channel, const unsigned char *data, uint32_t size);
    
    void WriteContentPackage();
    
    
    // complete writing file
    void CompleteFile();
    
private:
    void CalculateStartPosition();
    uint32_t WriteSystemItem();
    uint32_t WriteAES3AudioElement();

private:
    std::string mFilename;
    D10SampleRate mSampleRate;
    mxfRational mVideoSampleRate;
    uint16_t mRoundedTimecodeBase;
    uint32_t mChannelCount;
    uint32_t mAudioQuantizationBits;
    uint32_t mAudioBytesPerSample;
    mxfRational mAspectRatio;
    int64_t mStartTimecode;
    bool mDropFrameTimecode;
    int64_t mStartPosition;
    D10BitRate mD10BitRate;
    uint32_t mEncodedImageSize;
    uint32_t mMaxEncodedImageSize;
    uint32_t mAES3DataSize;

    uint32_t mSystemItemSize;
    uint32_t mVideoItemSize;
    uint32_t mAudioItemSize;
    mxfUL mEssenceContainerUL;

    mxfpp::File *mMXFFile;
    mxfpp::DataModel *mDataModel;
    mxfpp::HeaderMetadata *mHeaderMetadata;
    mxfpp::IndexTableSegment *mIndexSegment;
    mxfpp::Partition *mHeaderPartition;
    int64_t mHeaderMetadataStartPos;
    std::vector<SetWithDuration*> mSetsWithDuration;
    
    D10ContentPackage mContentPackage;
    DynamicByteArray mAES3Block;
    uint32_t mAudioSequence[5];
    int mAudioSequenceCount;
    int mAudioSequenceIndex;
    
    int64_t mDuration;
};


#endif

