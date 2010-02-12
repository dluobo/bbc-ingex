/*
 * $Id: D10MXFOP1AWriter.cpp,v 1.1 2010/02/12 13:52:49 philipn Exp $
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

#include <cstring>

#include "D10MXFOP1AWriter.h"
#include "libMXF++/MXFException.h"

using namespace std;
using namespace mxfpp;


static const char *COMPANY_NAME = "BBC";
static const char *PRODUCT_NAME = "Ingex";
static const char *VERSION_STRING = "0.1";
static const mxfUUID PRODUCT_UID =
    {0x93, 0xa1, 0xba, 0xae, 0x41, 0xc5, 0x4a, 0x16, 0x8b, 0x2c, 0x42, 0xe7, 0x23, 0x0d, 0x0a, 0x4e};
    
static const uint8_t LLEN = 4;
static const uint32_t KAG_SIZE = 0x200;
static const mxfRational AUDIO_SAMPLING_RATE = {48000, 1};

static const mxfKey VIDEO_ELEMENT_KEY = MXF_D10_PICTURE_EE_K(0x00);
static const mxfKey AUDIO_ELEMENT_KEY = MXF_D10_SOUND_EE_K(0x00);

static const mxfKey MXF_EE_K(EmptyPackageMetadataSet) = MXF_SDTI_CP_PACKAGE_METADATA_KEY(0x00);
static const uint32_t SYSTEM_ITEM_METADATA_PACK_SIZE = 7 + 16 + 17 + 17;



static void convert_timecode_to_12m(Timecode *t, bool drop_frame, unsigned char *t12m)
{
    // the format follows the specification of the TimecodeArray property
    // defined in SMPTE 405M, table 2, which follows section 8.2 of SMPTE 331M 
    // The Binary Group Data is not used and is set to 0
    
    memset(t12m, 0, 8);
    if (drop_frame)
        t12m[0] = 0x40; // set drop-frame flag
    t12m[0] |= ((t->frame % 10) & 0x0f) | (((t->frame / 10) & 0x3) << 4);
    t12m[1] = ((t->sec % 10) & 0x0f) | (((t->sec / 10) & 0x7) << 4);
    t12m[2] = ((t->min % 10) & 0x0f) | (((t->min / 10) & 0x7) << 4);
    t12m[3] = ((t->hour % 10) & 0x0f) | (((t->hour / 10) & 0x3) << 4);
    
}

static string get_track_name(string prefix, int number)
{
    char buf[16];
    sprintf(buf, "%d", number);
    return prefix.append(buf);
}

static uint32_t get_kag_aligned_size(uint32_t data_size)
{
    // assuming the partition pack is aligned to the kag working from the first byte of the file
    
    uint32_t fill_size = KAG_SIZE - (data_size % KAG_SIZE);
    if (fill_size < mxfKey_extlen + LLEN)
        fill_size += KAG_SIZE;
    
    return data_size + fill_size;
}



class SetWithDuration
{
public:
    virtual ~SetWithDuration() {}
    
    virtual void UpdateDuration(int64_t duration) = 0;
};

class StructComponentSet : public SetWithDuration
{
public:
    StructComponentSet(StructuralComponent *component)
    {
        mComponent = component;
    }
    
    virtual ~StructComponentSet()
    {}
    
    virtual void UpdateDuration(int64_t duration)
    {
        mComponent->setDuration(duration);
    }
    
private:
    StructuralComponent *mComponent;
};

class FileDescriptorSet : public SetWithDuration
{
public:
    FileDescriptorSet(FileDescriptor *descriptor)
    {
        mDescriptor = descriptor;
    }
    
    virtual ~FileDescriptorSet()
    {}
    
    virtual void UpdateDuration(int64_t duration)
    {
        mDescriptor->setContainerDuration(duration);
    }
    
private:
    FileDescriptor *mDescriptor;
};





D10MXFOP1AWriter::D10MXFOP1AWriter(string filename)
{
    mFilename = filename;
    SetSampleRate(D10_SAMPLE_RATE_625_50I);
    SetAudioChannelCount(4);
    SetAudioQuantizationBits(24);
    SetAspectRatio((mxfRational){16, 9});
    SetStartTimecode(0, false);
    mStartPosition = 0; // calculated in PrepareFile()
    SetBitRate(D10_BIT_RATE_50, mMaxEncodedImageSize);
    
    mSystemItemSize = 0;
    mVideoItemSize = 0;
    mAudioItemSize = 0;
    
    mMXFFile = 0;
    mDataModel = 0;
    mHeaderMetadata = 0;
    mIndexSegment = 0;
    mHeaderPartition = 0;
    mHeaderMetadataStartPos = 0;
    
    mAES3Block.allocate(1920 * 4 * 8 + 4); // max size required
    
    mDuration = 0;
}

D10MXFOP1AWriter::~D10MXFOP1AWriter()
{
    size_t i;
    for (i = 0; i < mSetsWithDuration.size(); i++)
        delete mSetsWithDuration[i];
    
    delete mMXFFile;
    delete mDataModel;
    delete mHeaderMetadata;
    delete mIndexSegment;
    // mHeaderPartition is owned by mMXFFile
}

void D10MXFOP1AWriter::SetSampleRate(D10SampleRate sample_rate)
{
    mSampleRate = sample_rate;
    
    if (sample_rate == D10_SAMPLE_RATE_625_50I) {
        mMaxEncodedImageSize = 250000;
        mVideoSampleRate = (mxfRational){25, 1};
        mRoundedTimecodeBase = 25;
        mAudioSequenceCount = 1;
        mAudioSequenceIndex = 0;
        mAudioSequence[0] = 1920;
    } else {
        mMaxEncodedImageSize = 208541;
        mVideoSampleRate = (mxfRational){30000, 1001};
        mRoundedTimecodeBase = 30;
        mAudioSequenceCount = 5;
        mAudioSequenceIndex = 0;
        mAudioSequence[0] = 1602;
        mAudioSequence[1] = 1601;
        mAudioSequence[2] = 1602;
        mAudioSequence[3] = 1601;
        mAudioSequence[4] = 1602;
    }
}

void D10MXFOP1AWriter::SetAudioChannelCount(uint32_t count)
{
    MXFPP_CHECK(count < MAX_CP_AUDIO_TRACKS);
    mChannelCount = count;
}

void D10MXFOP1AWriter::SetAudioQuantizationBits(uint32_t bits)
{
    MXFPP_CHECK(bits <= 24);
    mAudioQuantizationBits = bits;
    mAudioBytesPerSample = (mAudioQuantizationBits + 7) / 8;
}

void D10MXFOP1AWriter::SetAspectRatio(mxfRational aspect_ratio)
{
    MXFPP_CHECK((aspect_ratio.numerator == 4 && aspect_ratio.denominator == 3) ||
                (aspect_ratio.numerator == 16 && aspect_ratio.denominator == 9));
    mAspectRatio = aspect_ratio;
}

void D10MXFOP1AWriter::SetStartTimecode(int64_t count, bool drop_frame)
{
    MXFPP_CHECK(count >= 0);
    mStartTimecode = count;
    mDropFrameTimecode = drop_frame;
}

void D10MXFOP1AWriter::SetBitRate(D10BitRate rate, uint32_t encoded_image_size)
{
    MXFPP_CHECK(encoded_image_size <= mMaxEncodedImageSize);
    mD10BitRate = rate;
    mEncodedImageSize = encoded_image_size;
}

void D10MXFOP1AWriter::PrepareFile()
{
    mxfTimestamp now;
    mxfUMID file_package_uid;
    mxfUMID material_package_uid;
    mxfUUID uuid;
    mxfUL picture_essence_coding_ul = g_Null_UL;
    uint32_t i;
    
    
    // inits
    
    CalculateStartPosition();
    
    mxf_get_timestamp_now(&now);
    mxf_generate_umid(&file_package_uid);
    mxf_generate_umid(&material_package_uid);
    if (mSampleRate == D10_SAMPLE_RATE_625_50I) {
        switch (mD10BitRate)
        {
            case D10_BIT_RATE_30:
                mEssenceContainerUL = MXF_EC_L(D10_30_625_50_defined_template);
                picture_essence_coding_ul = MXF_CMDEF_L(D10_30_625_50);
                break;
            case D10_BIT_RATE_40:
                mEssenceContainerUL = MXF_EC_L(D10_40_625_50_defined_template);
                picture_essence_coding_ul = MXF_CMDEF_L(D10_40_625_50);
                break;
            case D10_BIT_RATE_50:
                mEssenceContainerUL = MXF_EC_L(D10_50_625_50_defined_template);
                picture_essence_coding_ul = MXF_CMDEF_L(D10_50_625_50);
                break;
        }
    } else {
        switch (mD10BitRate)
        {
            case D10_BIT_RATE_30:
                mEssenceContainerUL = MXF_EC_L(D10_30_525_60_defined_template);
                picture_essence_coding_ul = MXF_CMDEF_L(D10_30_525_60);
                break;
            case D10_BIT_RATE_40:
                mEssenceContainerUL = MXF_EC_L(D10_40_525_60_defined_template);
                picture_essence_coding_ul = MXF_CMDEF_L(D10_40_525_60);
                break;
            case D10_BIT_RATE_50:
                mEssenceContainerUL = MXF_EC_L(D10_50_525_60_defined_template);
                picture_essence_coding_ul = MXF_CMDEF_L(D10_50_525_60);
                break;
        }
    }
    mSystemItemSize = get_kag_aligned_size(mxfKey_extlen + LLEN + SYSTEM_ITEM_METADATA_PACK_SIZE + mxfKey_extlen + LLEN);
    mVideoItemSize = get_kag_aligned_size(mxfKey_extlen + LLEN + mEncodedImageSize);
    if (mSampleRate == D10_SAMPLE_RATE_625_50I) {
        mAudioItemSize = get_kag_aligned_size(mxfKey_extlen + LLEN + 1920 * 4 * 8 + 4);
    } else {
        mAudioItemSize = get_kag_aligned_size(mxfKey_extlen + LLEN + 1602 * 4 * 8 + 4);
        // check the fill item fits with the smaller audio frame size
        MXFPP_ASSERT(1601 * 4 * 8 + 4 <= mAudioItemSize - mxfKey_extlen - LLEN);
    }
    
    
    // open file
    
    mMXFFile = File::openNew(mFilename);
    
    
    // set minimum llen
    
    mMXFFile->setMinLLen(LLEN);
    
    
    // write the header partition pack
    
    mHeaderPartition = &(mMXFFile->createPartition());
    mHeaderPartition->setKey(&MXF_PP_K(ClosedComplete, Header));
    mHeaderPartition->setBodySID(1);
    mHeaderPartition->setIndexSID(2);
    mHeaderPartition->setKagSize(KAG_SIZE);
    mHeaderPartition->setOperationalPattern(&MXF_OP_L(1a, qq09));
    mHeaderPartition->addEssenceContainer(&mEssenceContainerUL);
    mHeaderPartition->write(mMXFFile);
    mHeaderPartition->fillToKag(mMXFFile);
    
    
    // create the header metadata
    
    mDataModel = new DataModel();
    mHeaderMetadata = new HeaderMetadata(mDataModel);

    // Preface
    Preface *preface = new Preface(mHeaderMetadata);
    preface->setLastModifiedDate(now);
    preface->setVersion(258);
    preface->setOperationalPattern(MXF_OP_L(1a, qq09));
    preface->appendEssenceContainers(mEssenceContainerUL);
    
    // Preface - Identification
    Identification *ident = new Identification(mHeaderMetadata);
    preface->appendIdentifications(ident);
    ident->initialise(COMPANY_NAME, PRODUCT_NAME, VERSION_STRING, PRODUCT_UID);
    
    // Preface - ContentStorage
    ContentStorage* content_storage = new ContentStorage(mHeaderMetadata);
    preface->setContentStorage(content_storage);
    
    // Preface - ContentStorage - EssenceContainerData
    EssenceContainerData *ess_container_data = new EssenceContainerData(mHeaderMetadata);
    content_storage->appendEssenceContainerData(ess_container_data);
    ess_container_data->setLinkedPackageUID(file_package_uid);
    ess_container_data->setBodySID(1);
    ess_container_data->setIndexSID(2);
    
    
    // Preface - ContentStorage - MaterialPackage
    MaterialPackage *material_package = new MaterialPackage(mHeaderMetadata);
    content_storage->appendPackages(material_package);
    material_package->setPackageUID(material_package_uid);
    material_package->setPackageCreationDate(now);
    material_package->setPackageModifiedDate(now);

    // Preface - ContentStorage - MaterialPackage - Timecode Track
    Track *timecode_track = new Track(mHeaderMetadata);
    material_package->appendTracks(timecode_track);
    timecode_track->setTrackName("TC1");
    timecode_track->setTrackID(1);
    timecode_track->setTrackNumber(0);
    timecode_track->setEditRate(mVideoSampleRate);
    timecode_track->setOrigin(0);

    // Preface - ContentStorage - MaterialPackage - Timecode Track - Sequence
    Sequence *sequence = new Sequence(mHeaderMetadata);
    timecode_track->setSequence(sequence);
    sequence->setDataDefinition(MXF_DDEF_L(Timecode));
    sequence->setDuration(-1); // updated when writing completed
    mSetsWithDuration.push_back(new StructComponentSet(sequence));

    // Preface - ContentStorage - MaterialPackage - Timecode Track - TimecodeComponent
    TimecodeComponent *timecode_component = new TimecodeComponent(mHeaderMetadata);
    sequence->appendStructuralComponents(timecode_component);
    timecode_component->setDataDefinition(MXF_DDEF_L(Timecode));
    timecode_component->setDuration(-1); // updated when writing completed
    timecode_component->setRoundedTimecodeBase(mRoundedTimecodeBase);
    if (mSampleRate == D10_SAMPLE_RATE_625_50I)
        timecode_component->setDropFrame(false);
    else
        timecode_component->setDropFrame(mDropFrameTimecode);
    timecode_component->setStartTimecode(mStartTimecode);
    mSetsWithDuration.push_back(new StructComponentSet(timecode_component));

    // Preface - ContentStorage - MaterialPackage - Timeline Tracks
    // video track and audio track
    for (i = 0; i < 2; i++)
    {
        bool is_picture = (i == 0);
        
        // Preface - ContentStorage - MaterialPackage - Timeline Track
        Track *track = new Track(mHeaderMetadata);
        material_package->appendTracks(track);
        track->setTrackName(is_picture ? "V1" : get_track_name("A", i));
        track->setTrackID(i + 2);
        track->setTrackNumber(0);
        track->setEditRate(mVideoSampleRate);
        track->setOrigin(0);
    
        // Preface - ContentStorage - MaterialPackage - Timeline Track - Sequence
        Sequence *sequence = new Sequence(mHeaderMetadata);
        track->setSequence(sequence);
        sequence->setDataDefinition(is_picture ? MXF_DDEF_L(Picture) : MXF_DDEF_L(Sound));
        sequence->setDuration(-1); // updated when writing completed
        mSetsWithDuration.push_back(new StructComponentSet(sequence));

        // Preface - ContentStorage - MaterialPackage - Timeline Track - Sequence - SourceClip
        SourceClip *source_clip = new SourceClip(mHeaderMetadata);
        sequence->appendStructuralComponents(source_clip);
        source_clip->setDataDefinition(is_picture ? MXF_DDEF_L(Picture) : MXF_DDEF_L(Sound));
        source_clip->setDuration(-1); // updated when writing completed
        source_clip->setStartPosition(0);
        source_clip->setSourceTrackID(i + 2);
        source_clip->setSourcePackageID(file_package_uid);
        mSetsWithDuration.push_back(new StructComponentSet(source_clip));
    }


    // Preface - ContentStorage - SourcePackage
    SourcePackage *file_source_package = new SourcePackage(mHeaderMetadata);
    content_storage->appendPackages(file_source_package);
    file_source_package->setPackageUID(file_package_uid);
    file_source_package->setPackageCreationDate(now);
    file_source_package->setPackageModifiedDate(now);

    // Preface - ContentStorage - SourcePackage - Timecode Track
    timecode_track = new Track(mHeaderMetadata);
    file_source_package->appendTracks(timecode_track);
    timecode_track->setTrackName("TC1");
    timecode_track->setTrackID(1);
    timecode_track->setTrackNumber(0);
    timecode_track->setEditRate(mVideoSampleRate);
    timecode_track->setOrigin(0);

    // Preface - ContentStorage - SourcePackage - Timecode Track - Sequence
    sequence = new Sequence(mHeaderMetadata);
    timecode_track->setSequence(sequence);
    sequence->setDataDefinition(MXF_DDEF_L(Timecode));
    sequence->setDuration(-1); // updated when writing completed
    mSetsWithDuration.push_back(new StructComponentSet(sequence));

    // Preface - ContentStorage - MaterialPackage - Timecode Track - TimecodeComponent
    timecode_component = new TimecodeComponent(mHeaderMetadata);
    sequence->appendStructuralComponents(timecode_component);
    timecode_component->setDataDefinition(MXF_DDEF_L(Timecode));
    timecode_component->setDuration(-1); // updated when writing completed
    timecode_component->setRoundedTimecodeBase(mRoundedTimecodeBase);
    if (mSampleRate == D10_SAMPLE_RATE_625_50I)
        timecode_component->setDropFrame(false);
    else
        timecode_component->setDropFrame(mDropFrameTimecode);
    timecode_component->setStartTimecode(mStartTimecode);
    mSetsWithDuration.push_back(new StructComponentSet(timecode_component));

    // Preface - ContentStorage - SourcePackage - Timeline Tracks
    // video track followed by audio track
    for (i = 0; i < 2; i++)
    {
        bool is_picture = (i == 0);
        
        // Preface - ContentStorage - SourcePackage - Timeline Track
        Track *track = new Track(mHeaderMetadata);
        file_source_package->appendTracks(track);
        track->setTrackName(is_picture ? "V1" : get_track_name("A", i));
        track->setTrackID(i + 2);
        if (is_picture)
            track->setTrackNumber(MXF_D10_PICTURE_TRACK_NUM(0x00));
        else
            track->setTrackNumber(MXF_D10_SOUND_TRACK_NUM(0x00));
        track->setEditRate(mVideoSampleRate);
        track->setOrigin(0);
    
        // Preface - ContentStorage - SourcePackage - Timeline Track - Sequence
        Sequence *sequence = new Sequence(mHeaderMetadata);
        track->setSequence(sequence);
        sequence->setDataDefinition(is_picture ? MXF_DDEF_L(Picture) : MXF_DDEF_L(Sound));
        sequence->setDuration(-1); // updated when writing completed
        mSetsWithDuration.push_back(new StructComponentSet(sequence));

        // Preface - ContentStorage - SourcePackage - Timeline Track - Sequence - SourceClip
        SourceClip *source_clip = new SourceClip(mHeaderMetadata);
        sequence->appendStructuralComponents(source_clip);
        source_clip->setDataDefinition(is_picture ? MXF_DDEF_L(Picture) : MXF_DDEF_L(Sound));
        source_clip->setDuration(-1); // updated when writing completed
        source_clip->setStartPosition(0);
        source_clip->setSourceTrackID(0);
        source_clip->setSourcePackageID(g_Null_UMID);
        mSetsWithDuration.push_back(new StructComponentSet(source_clip));
    }
    
    
    // Preface - ContentStorage - SourcePackage - MultipleDescriptor
    MultipleDescriptor *mult_descriptor = new MultipleDescriptor(mHeaderMetadata);
    file_source_package->setDescriptor(mult_descriptor);
    mult_descriptor->setSampleRate(mVideoSampleRate);
    mult_descriptor->setEssenceContainer(mEssenceContainerUL);
    mSetsWithDuration.push_back(new FileDescriptorSet(mult_descriptor));
    
    
    // Preface - ContentStorage - SourcePackage - MultipleDescriptor - CDCIEssenceDescriptor
    CDCIEssenceDescriptor *cdciDescriptor = new CDCIEssenceDescriptor(mHeaderMetadata);
    mult_descriptor->appendSubDescriptorUIDs(cdciDescriptor);
    cdciDescriptor->setLinkedTrackID(2);
    cdciDescriptor->setSampleRate(mVideoSampleRate);
    cdciDescriptor->setEssenceContainer(mEssenceContainerUL);
    cdciDescriptor->setPictureEssenceCoding(picture_essence_coding_ul);
    cdciDescriptor->setSignalStandard(0x01); // Rec. 601
    cdciDescriptor->setFrameLayout(0x01); // Separate fields
    if (mSampleRate == D10_SAMPLE_RATE_625_50I) {
        cdciDescriptor->setStoredWidth(720);
        cdciDescriptor->setStoredHeight(304);
        cdciDescriptor->setSampledWidth(720);
        cdciDescriptor->setSampledHeight(304);
        cdciDescriptor->setDisplayWidth(720);
        cdciDescriptor->setDisplayHeight(288);
        cdciDescriptor->setDisplayXOffset(0);
        cdciDescriptor->setDisplayYOffset(16);
        cdciDescriptor->appendVideoLineMap(7);
        cdciDescriptor->appendVideoLineMap(320);
    } else {
        cdciDescriptor->setStoredWidth(720);
        cdciDescriptor->setStoredHeight(256);
        cdciDescriptor->setSampledWidth(720);
        cdciDescriptor->setSampledHeight(256);
        cdciDescriptor->setDisplayWidth(720);
        cdciDescriptor->setDisplayHeight(243);
        cdciDescriptor->setDisplayXOffset(0);
        cdciDescriptor->setDisplayYOffset(13);
        cdciDescriptor->appendVideoLineMap(7);
        cdciDescriptor->appendVideoLineMap(270);
    }
    cdciDescriptor->setStoredF2Offset(0);
    cdciDescriptor->setSampledXOffset(0);
    cdciDescriptor->setSampledYOffset(0);
    cdciDescriptor->setDisplayF2Offset(0);
    cdciDescriptor->setAspectRatio(mAspectRatio);
    cdciDescriptor->setAlphaTransparency(false);
    cdciDescriptor->setCaptureGamma(ITUR_BT470_TRANSFER_CH);
    cdciDescriptor->setImageAlignmentOffset(0);
    cdciDescriptor->setFieldDominance(1); // field 1
    cdciDescriptor->setImageStartOffset(0);
    cdciDescriptor->setImageEndOffset(0);
    cdciDescriptor->setComponentDepth(8);
    cdciDescriptor->setHorizontalSubsampling(2);
    cdciDescriptor->setVerticalSubsampling(1);
    cdciDescriptor->setColorSiting(0x04); // Rec. 601
    cdciDescriptor->setReversedByteOrder(false);
    cdciDescriptor->setPaddingBits(0);
    cdciDescriptor->setBlackRefLevel(16);
    cdciDescriptor->setWhiteReflevel(235);
    cdciDescriptor->setColorRange(225);
    mSetsWithDuration.push_back(new FileDescriptorSet(cdciDescriptor));
    
    
    // Preface - ContentStorage - SourcePackage - MultipleDescriptor - GenericSoundEssenceDescriptor
    GenericSoundEssenceDescriptor *soundDescriptor = new GenericSoundEssenceDescriptor(mHeaderMetadata);
    mult_descriptor->appendSubDescriptorUIDs(soundDescriptor);
    soundDescriptor->setLinkedTrackID(3);
    soundDescriptor->setSampleRate(mVideoSampleRate);
    soundDescriptor->setEssenceContainer(mEssenceContainerUL);
    soundDescriptor->setAudioSamplingRate(AUDIO_SAMPLING_RATE);
    soundDescriptor->setLocked(true);
    soundDescriptor->setAudioRefLevel(0);
    soundDescriptor->setChannelCount(mChannelCount);
    soundDescriptor->setQuantizationBits(mAudioQuantizationBits);
    mSetsWithDuration.push_back(new FileDescriptorSet(soundDescriptor));
    
    
    // write the header metadata
    
    mHeaderMetadataStartPos = mMXFFile->tell(); // need this position when we re-write the header metadata
    KAGFillerWriter kag_filler_writer(mHeaderPartition);
    mHeaderMetadata->write(mMXFFile, mHeaderPartition, &kag_filler_writer);
    
    
    
    // write the index table segment
    
    mIndexSegment = new IndexTableSegment();
    mxf_generate_uuid(&uuid);
    mIndexSegment->setInstanceUID(uuid);
    mIndexSegment->setIndexEditRate(mVideoSampleRate);
    mIndexSegment->setIndexDuration(0); // will be updated when writing is completed
    mIndexSegment->setBodySID(1);
    mIndexSegment->setIndexSID(2);
    uint32_t deltaOffset = 0;
    mIndexSegment->appendDeltaEntry(0, 0, deltaOffset);
    deltaOffset += mSystemItemSize;
    mIndexSegment->appendDeltaEntry(0, 0, deltaOffset);
    deltaOffset += mVideoItemSize;
    mIndexSegment->appendDeltaEntry(0, 0, deltaOffset);
    deltaOffset += mAudioItemSize;
    mIndexSegment->setEditUnitByteCount(deltaOffset);

    mIndexSegment->write(mMXFFile, mHeaderPartition, &kag_filler_writer);
}

void D10MXFOP1AWriter::SetTimecode(Timecode ltc)
{
    MXFPP_ASSERT(mMXFFile);
    
    mContentPackage.mLTC = ltc;
}

void D10MXFOP1AWriter::GenerateTimecode()
{
    MXFPP_ASSERT(mMXFFile);
    
    Timecode ltc;
    int64_t tc_count = mStartPosition + mDuration;
    
    if (mDropFrameTimecode && mSampleRate == D10MXFOP1AWriter::D10_SAMPLE_RATE_525_60I) {
        // first 2 frame numbers shall be omitted at the start of each minute,
        //   except minutes 0, 10, 20, 30, 40 and 50
        
        int hour, min;
        int64_t prev_skipped_count = -1;
        int64_t skipped_count = 0;
        while (prev_skipped_count != skipped_count)
        {
            prev_skipped_count = skipped_count;
            
            hour = (tc_count + skipped_count) / (60 * 60 * mRoundedTimecodeBase);
            min = ((tc_count + skipped_count) % (60 * 60 * mRoundedTimecodeBase)) / (60 * mRoundedTimecodeBase);
    
            // add frames skipped
            skipped_count = (60-6) * 2 * hour;      // every whole hour
            skipped_count += (min / 10) * 9 * 2;    // every whole 10 min
            skipped_count += (min % 10) * 2;        // every whole min, except min 0
        }
        
        tc_count += skipped_count;
    }
    
    ltc.hour = tc_count / (60 * 60 * mRoundedTimecodeBase);
    ltc.min = (tc_count % (60 * 60 * mRoundedTimecodeBase)) / (60 * mRoundedTimecodeBase);
    ltc.sec = ((tc_count % (60 * 60 * mRoundedTimecodeBase)) % (60 * mRoundedTimecodeBase)) / mRoundedTimecodeBase;
    ltc.frame = ((tc_count % (60 * 60 * mRoundedTimecodeBase)) % (60 * mRoundedTimecodeBase)) % mRoundedTimecodeBase;
    
    SetTimecode(ltc);
}

void D10MXFOP1AWriter::SetVideo(const unsigned char *data, uint32_t size)
{
    MXFPP_ASSERT(mMXFFile);
    MXFPP_CHECK(size > 0 && size <= mEncodedImageSize);
    
    mContentPackage.mVideoBytes.setBytes(data, size);
    
    if (size < mEncodedImageSize)
        mContentPackage.mVideoBytes.appendZeros(mEncodedImageSize - size);
}

uint32_t D10MXFOP1AWriter::GetAudioSampleCount()
{
    return mAudioSequence[mAudioSequenceIndex];
}

void D10MXFOP1AWriter::SetAudio(uint32_t channel, const unsigned char *data, uint32_t size)
{
    MXFPP_ASSERT(mMXFFile);
    MXFPP_ASSERT(channel < mChannelCount);
    MXFPP_CHECK(size == mAudioSequence[mAudioSequenceIndex] * mAudioBytesPerSample);
    
    mContentPackage.mAudioBytes[channel].setBytes(data, size);
}

void D10MXFOP1AWriter::WriteContentPackage()
{
    MXFPP_ASSERT(mMXFFile);
    MXFPP_CHECK(mContentPackage.IsComplete(mChannelCount));
    
    // write system item
    
    uint32_t element_size = WriteSystemItem();
    mMXFFile->writeFill(mSystemItemSize - element_size);
    
    
    // write video item
    
    mMXFFile->writeFixedKL(&VIDEO_ELEMENT_KEY, LLEN, mContentPackage.mVideoBytes.getSize());
    MXFPP_CHECK(mMXFFile->write(mContentPackage.mVideoBytes.getBytes(), mContentPackage.mVideoBytes.getSize()) ==
                mContentPackage.mVideoBytes.getSize());
    mMXFFile->writeFill(mVideoItemSize - mxfKey_extlen - LLEN - mContentPackage.mVideoBytes.getSize());
    
    
    // write audio item
    
    element_size = WriteAES3AudioElement();
    mMXFFile->writeFill(mAudioItemSize - element_size);
    

    mDuration++;
    
    mContentPackage.Reset();
    mAudioSequenceIndex = (mAudioSequenceIndex + 1) % mAudioSequenceCount;
}

void D10MXFOP1AWriter::CompleteFile()
{
    MXFPP_ASSERT(mMXFFile);

    // write the footer partition pack
    Partition &footer_partition = mMXFFile->createPartition();
    footer_partition.setKey(&MXF_PP_K(ClosedComplete, Footer));
    footer_partition.write(mMXFFile);
    footer_partition.fillToKag(mMXFFile);
    
    
    // update metadata sets and index with duration
    size_t i;
    for (i = 0; i < mSetsWithDuration.size(); i++)
        mSetsWithDuration[i]->UpdateDuration(mDuration);
    mIndexSegment->setIndexDuration(mDuration);

    
    // re-write the header metadata
    mMXFFile->seek(mHeaderMetadataStartPos, SEEK_SET);
    KAGFillerWriter kag_filler_writer(mHeaderPartition);
    mHeaderMetadata->write(mMXFFile, mHeaderPartition, &kag_filler_writer);
    

    // re-write the header index table segment
    mIndexSegment->write(mMXFFile, mHeaderPartition, &kag_filler_writer);


    // update the partition packs
    mMXFFile->updatePartitions();
    
    
    // done with the file
    delete mMXFFile;
    mMXFFile = 0;
}

void D10MXFOP1AWriter::CalculateStartPosition()
{
    int hour, min;

    mStartPosition = mStartTimecode;
    if (mDropFrameTimecode && mSampleRate == D10MXFOP1AWriter::D10_SAMPLE_RATE_525_60I) {
        // first 2 frame numbers shall be omitted at the start of each minute,
        //   except minutes 0, 10, 20, 30, 40 and 50
    
        hour = mStartPosition / (60 * 60 * mRoundedTimecodeBase);
        min = (mStartPosition % (60 * 60 * mRoundedTimecodeBase)) / (60 * mRoundedTimecodeBase);
    
        // remove frames skipped
        mStartPosition -= (60-6) * 2 * hour;   // every whole hour
        mStartPosition -= (min / 10) * 9 * 2;  // every whole 10 min
        mStartPosition -= (min % 10) * 2;      // every whole min, except min 0
    }
}

uint32_t D10MXFOP1AWriter::WriteSystemItem()
{
    // System Metadata Pack
    
    mMXFFile->writeFixedKL(&MXF_EE_K(SDTI_CP_System_Pack), LLEN, SYSTEM_ITEM_METADATA_PACK_SIZE);
    
    // core fields
    mMXFFile->writeUInt8(0x5c); // system bitmap - : 01011100b
    if (mSampleRate == D10_SAMPLE_RATE_625_50I)
        mMXFFile->writeUInt8(0x02 << 1); // 25 fps content package rate
    else
        mMXFFile->writeUInt8(0x03 << 1); // 30 fps content package rate
    mMXFFile->writeUInt8(0x00); // content package type
    mMXFFile->writeUInt16(0x0000); // channel handle
    mMXFFile->writeUInt16((uint32_t)(mDuration % 65536)); // continuity count
    
    // SMPTE Universal Label
    mMXFFile->writeUL(&mEssenceContainerUL);
    
    // TODO: what should go in here?
    // Package creation date / time stamp
    unsigned char bytes[17];
    memset(bytes, 0, sizeof(bytes));
    MXFPP_CHECK(mMXFFile->write(bytes, sizeof(bytes)) == sizeof(bytes));
    
    // User date / time stamp (LTC)
    bytes[0] = 0x81; // SMPTE 12-M timecode
    convert_timecode_to_12m(&mContentPackage.mLTC, mDropFrameTimecode, &bytes[1]);
    MXFPP_CHECK(mMXFFile->write(bytes, sizeof(bytes)) == sizeof(bytes));
    
    
    // Package Metadata Set (empty)
    
    mMXFFile->writeFixedKL(&MXF_EE_K(EmptyPackageMetadataSet), LLEN, 0);
    
    return mxfKey_extlen + LLEN + SYSTEM_ITEM_METADATA_PACK_SIZE + mxfKey_extlen + LLEN;
}

uint32_t D10MXFOP1AWriter::WriteAES3AudioElement()
{
    uint32_t s, c;
    unsigned char bytes[4];

    mMXFFile->writeFixedKL(&AUDIO_ELEMENT_KEY, LLEN, mAudioSequence[mAudioSequenceIndex] * 4 * 8 + 4);
    
    mAES3Block.setSize(0);
    bytes[0] = 0; // element header (FVUCP Valid Flag == 0 (false)
    if (mSampleRate == D10_SAMPLE_RATE_525_60I)
        bytes[0] |= mAudioSequenceIndex & 0x07; // 5-sequence count
    bytes[1] = mAudioSequence[mAudioSequenceIndex] & 0xff; // samples per frame (LSB)
    bytes[2] = (mAudioSequence[mAudioSequenceIndex] >> 8) & 0xff; // samples per frame (MSB)
    bytes[3] = (1 << mChannelCount) - 1; // channel valid flags
    mAES3Block.append(bytes, 4);
    
    for (s = 0; s < mAudioSequence[mAudioSequenceIndex]; s++) {
        for (c = 0; c < mChannelCount; c++) {
            bytes[0] = (unsigned char)c; // channel number
            
            if (mAudioBytesPerSample == 3) { // 24-bit
                bytes[0] |= (mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample] << 4) & 0xf0;
                bytes[1] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample] >> 4) & 0x0f) |
                           ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample + 1] << 4) & 0xf0);
                bytes[2] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample + 1] >> 4) & 0x0f) |
                           ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample + 2] << 4) & 0xf0);
                bytes[3] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample + 2] >> 4) & 0x0f);
            } else if (mAudioBytesPerSample == 2) { // 16-bit
                bytes[1] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample] << 4) & 0xf0);
                bytes[2] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample] >> 4) & 0x0f) |
                           ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample + 1] << 4) & 0xf0);
                bytes[3] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample + 1] >> 4) & 0x0f);
            } else { // 8-bit
                bytes[1] = 0x00;
                bytes[2] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample] << 4) & 0xf0);
                bytes[3] = ((mContentPackage.mAudioBytes[c][s * mAudioBytesPerSample] >> 4) & 0x0f);
            }
            
            mAES3Block.append(bytes, 4);
        }

        memset(bytes, 0, sizeof(bytes));
        for (; c < 8; c++)
            mAES3Block.append(bytes, 4);
    }
    
    MXFPP_CHECK(mMXFFile->write(mAES3Block.getBytes(), mAES3Block.getSize()) == mAES3Block.getSize());
    
    return mxfKey_extlen + LLEN + mAES3Block.getSize();
}

