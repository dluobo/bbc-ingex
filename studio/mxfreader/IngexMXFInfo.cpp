/*
 * $Id: IngexMXFInfo.cpp,v 1.1 2009/09/18 17:29:30 john_f Exp $
 *
 * Extract information from Ingex MXF files.
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

// TODO:
// Missing info:
//   tape source package spoolNumber
// Source code improvements:
//   add avid extensions data model header file to AvidHeaderMetadata
//   add function to get the mxf metadata set key
//   move Avid user comments and attributes to libMXF
//   add TaggedValue to libMXF
//   add subclass function taking metadata set to DataModel
//   missing last character in function get_indirect_string in mxf_avid.c (i < strSize) and not strSize - 1


#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <memory>

#include <libMXF++/MXF.h>
#include <mxf/mxf_avid.h>
#include <Package.h>

#include "IngexMXFInfo.h"
#include "TaggedValue.h"

using namespace mxfpp;
using namespace std;



#define ASSERT_CHECK(cmd) \
    if (!(cmd)) { \
        mxf_log_error("'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
        throw FAILED; \
    }

// these colors match the colors defined in studio/database/src/DatabaseEnums.h and
// AvidRGBColor in libMXF/examples/writeavidmxf/package_definitions.h
static const RGBColor g_rgbColors[] =
{
    {65534, 65535, 65535}, // white
    {41471, 12134, 6564 }, // red
    {58981, 58981, 6553 }, // yellow
    {13107, 52428, 13107}, // green
    {13107, 52428, 52428}, // cyan
    {13107, 13107, 52428}, // blue
    {52428, 13107, 52428}, // magenta
    {0    , 0    , 0    }  // black
};



void convert_umid(mxfUMID mxf_umid, prodauto::UMID *umid)
{
    memcpy(umid, &mxf_umid, sizeof(*umid));
}

void convert_timestamp(mxfTimestamp mxf_timestamp, prodauto::Timestamp *timestamp)
{
    memcpy(timestamp, &mxf_timestamp, sizeof(*timestamp));
}

void convert_rational(mxfRational mxf_rational, prodauto::Rational *rational)
{
    rational->numerator = mxf_rational.numerator;
    rational->denominator = mxf_rational.denominator;
}

void convert_data_def(mxfUL data_def_ul, int *data_def)
{
    if (mxf_is_picture(&data_def_ul))
        *data_def = PICTURE_DATA_DEFINITION;
    else if (mxf_is_sound(&data_def_ul))
        *data_def = SOUND_DATA_DEFINITION;
    else
        *data_def = 0;
}

void convert_color(const RGBColor *color, int *db_color)
{
    int i;
    float diff[8];
    float min_diff = -1;
    int min_diff_index = 0;
    
    assert(sizeof(g_rgbColors) / sizeof(RGBColor) == 8);
    assert(USER_COMMENT_BLACK_COLOUR - USER_COMMENT_WHITE_COLOUR == 7);
    
    // choose the color that has minimum difference to a known color
    
    for (i = 0; i < 8; i++) {
        diff[i] = ((float)(color->red - g_rgbColors[i].red) * (float)(color->red - g_rgbColors[i].red) +
                   (float)(color->green - g_rgbColors[i].green) * (float)(color->green - g_rgbColors[i].green) +
                   (float)(color->blue - g_rgbColors[i].blue) * (float)(color->blue - g_rgbColors[i].blue)) / 3.0;
    }
    
    for (i = 0; i < 8; i++) {
        if (min_diff < 0 || diff[i] < min_diff) {
            min_diff = diff[i];
            min_diff_index = i;
        }
    }
    
    *db_color = min_diff_index + USER_COMMENT_WHITE_COLOUR;
}




IngexMXFInfo::ReadResult IngexMXFInfo::read(string filename, IngexMXFInfo **info)
{
    ReadResult result = SUCCESS;
    File *mxf_file = 0;
    Partition *header_partition = 0;

    try {
        // open the file
        try {
            mxf_file = File::openRead(filename);
        } catch (...) {
            throw FILE_OPEN_ERROR;
        }
        
        // read the header partition pack and check the operational pattern
        header_partition = Partition::findAndReadHeaderPartition(mxf_file);
        if (!header_partition)
            throw NO_HEADER_PARTITION;
    
        if (!is_op_atom(header_partition->getOperationalPattern()))
            throw NOT_OP_ATOM;
        
        // file is now assumed to be ok for extracting info
        *info = new IngexMXFInfo(filename, mxf_file, header_partition);
        
    } catch (ReadResult &error) {
        result = error;
    } catch (...) {
        result = FAILED;
    }

    delete header_partition;
    delete mxf_file;
    
    return result;
}


IngexMXFInfo::IngexMXFInfo(string filename, File *mxf_file, Partition *header_partition)
{
    _filename = filename;
    _data_model = 0;
    _material_package = 0;
    _file_source_package = 0;
    _tape_source_package = 0;
    
    
    AvidHeaderMetadata *header_metadata = 0;
    mxfKey key;
    uint8_t llen;
    uint64_t len;
    vector<mxfUL> container_labels;
    
    try {
         container_labels = header_partition->getEssenceContainers();
         ASSERT_CHECK(container_labels.size() == 1);
        
        // initialise data model with Avid extensions
        _data_model = new DataModel();
        header_metadata = new AvidHeaderMetadata(_data_model);
        
        TaggedValue::registerObjectFactory(header_metadata);

        // move to header metadata and read it
        mxf_file->readNextNonFillerKL(&key, &llen, &len);
        if (!mxf_is_header_metadata(&key))
            throw FAILED;
        
        header_metadata->read(mxf_file, header_partition, &key, llen, len);
        
        
        Preface *preface = header_metadata->getPreface();
        ContentStorage *content = preface->getContentStorage();
        vector<GenericPackage*> packages = content->getPackages();

        // preface info
        if (preface->haveItem(&MXF_ITEM_K(Preface, ProjectName)))
            _project_name = preface->getStringItem(&MXF_ITEM_K(Preface, ProjectName));
        if (preface->haveItem(&MXF_ITEM_K(Preface, ProjectEditRate)))
            convert_rational(preface->getRationalItem(&MXF_ITEM_K(Preface, ProjectEditRate)), &_project_edit_rate);
        
        
        // extract package info
        vector<GenericPackage*>::iterator package_iter;
        for (package_iter = packages.begin(); package_iter != packages.end(); package_iter++)
            extractPackageInfo(*package_iter, &container_labels[0]);
        
        delete header_metadata;
        delete _data_model;
        _data_model = 0;
        
    } catch (...) {
        delete _material_package;
        delete _file_source_package;
        delete _tape_source_package;
        delete header_metadata;
        delete _data_model;
        throw;
    }
}

IngexMXFInfo::~IngexMXFInfo()
{
    delete _material_package;
    delete _file_source_package;
    delete _tape_source_package;
}

void IngexMXFInfo::extractPackageInfo(GenericPackage *mxf_package, mxfUL *essence_container_label)
{
    MaterialPackage *mp = dynamic_cast<MaterialPackage*>(mxf_package);
    SourcePackage *sp = dynamic_cast<SourcePackage*>(mxf_package);
    bool is_mp = false;
    bool is_tape_sp = false;
    bool is_file_sp = false;
    prodauto::Package *db_package;

    // find out what type of package it is
    if (mp) {
        is_mp = true;
    } else if (sp) {
        if (!sp->haveDescriptor())
            return;
        
        if (_data_model->isSubclassOf(&sp->getDescriptor()->getCMetadataSet()->key, &MXF_SET_K(TapeDescriptor)))
            is_tape_sp = true;
        else if (_data_model->isSubclassOf(&sp->getDescriptor()->getCMetadataSet()->key, &MXF_SET_K(FileDescriptor)))
            is_file_sp = true;
        else
            return;
    }
    
    if (is_mp) {
        _material_package = new prodauto::MaterialPackage();
        db_package = _material_package;
    } else if (is_file_sp) {
        _file_source_package = new prodauto::SourcePackage();
        db_package = _file_source_package;
    } else {
        _tape_source_package = new prodauto::SourcePackage();
        db_package = _tape_source_package;
    }
    
    // extract general package info
    
    convert_umid(mxf_package->getPackageUID(), &db_package->uid);
    if (mxf_package->haveName())
        db_package->name = mxf_package->getName();
    convert_timestamp(mxf_package->getPackageCreationDate(), &db_package->creationDate);

    vector<TaggedValue*> attributes = getPackageAttributes(mxf_package);
    db_package->projectName = getStringTaggedValue(attributes, "_PJ");

    vector<TaggedValue*> comments = getPackageComments(mxf_package);
    vector<TaggedValue*>::iterator comment_iter;
    for (comment_iter = comments.begin(); comment_iter != comments.end(); comment_iter++)
        db_package->addUserComment((*comment_iter)->getName(), (*comment_iter)->getStringValue(),
                                   STATIC_COMMENT_POSITION, 0);

    
    vector<GenericTrack*> mxf_tracks = mxf_package->getTracks();
    Track *mxf_track;
    prodauto::Track *track;
    Sequence *mxf_sequence;
    int data_def;
    vector<StructuralComponent*> mxf_components;
    SourceClip *mxf_source_clip;
    vector<GenericTrack*>::iterator track_iter;
    for (track_iter = mxf_tracks.begin(); track_iter != mxf_tracks.end(); track_iter++) {
        
        // a timeline track
        mxf_track = dynamic_cast<Track*>(*track_iter);
        if (!mxf_track)
            continue;

        // containing a sequence referencing a single source clip
        mxf_sequence = dynamic_cast<Sequence*>(mxf_track->getSequence());
        if (!mxf_sequence)
            continue;
        mxf_components = mxf_sequence->getStructuralComponents();
        if (mxf_components.size() != 1)
            continue;
        mxf_source_clip = dynamic_cast<SourceClip*>(mxf_components.front());
        if (!mxf_source_clip)
            continue;

        // which is either sound or picture        
        convert_data_def(mxf_sequence->getDataDefinition(), &data_def);
        if (data_def == 0)
            continue;

        // map to prodauto data structures
        
        mxf_track = dynamic_cast<Track*>(*track_iter);
        db_package->tracks.push_back(new prodauto::Track());
        track = db_package->tracks.back();
        
        ASSERT_CHECK(mxf_track->haveTrackID());
        track->id = mxf_track->getTrackID();
        track->number = mxf_track->getTrackNumber();
        if (mxf_track->haveTrackName())
            track->name = mxf_track->getTrackName();
        track->origin = mxf_track->getOrigin();
        convert_rational(mxf_track->getEditRate(), &track->editRate);
        track->dataDef = data_def;
        
        track->sourceClip = new prodauto::SourceClip();
        convert_umid(mxf_source_clip->getSourcePackageID(), &track->sourceClip->sourcePackageUID);
        track->sourceClip->sourceTrackID = mxf_source_clip->getSourceTrackID();
        track->sourceClip->length = mxf_source_clip->getDuration();
        track->sourceClip->position = mxf_source_clip->getStartPosition();
    }
    
    // extract info specific to package type
    if (is_mp)
        extractMaterialPackageInfo(mp);
    else if (is_file_sp)
        extractFileSourcePackageInfo(sp, essence_container_label);
    else if (is_tape_sp)
        extractTapeSourcePackageInfo(sp);
}

void IngexMXFInfo::extractMaterialPackageInfo(MaterialPackage *mxf_package)
{
    vector<GenericTrack*> mxf_tracks;
    EventTrack *mxf_event_track;
    Sequence *mxf_sequence;
    DMSegment *mxf_dm_segment;
    vector<StructuralComponent*> mxf_components;
    mxfUL mxf_data_def;
    string comment;
    int64_t position;
    RGBColor mxf_color;
    int color;
    
    // extract positioned user comments from the DM event track
    
    mxf_tracks = mxf_package->getTracks();
    vector<GenericTrack*>::iterator track_iter;    
    for (track_iter = mxf_tracks.begin(); track_iter != mxf_tracks.end(); track_iter++) {
        
        // an event track
        mxf_event_track = dynamic_cast<EventTrack*>(*track_iter);
        if (!mxf_event_track)
            continue;

        // containing a descriptive metadata sequence
        mxf_sequence = dynamic_cast<Sequence*>(mxf_event_track->getSequence());
        if (!mxf_sequence)
            continue;
        mxf_data_def = mxf_sequence->getDataDefinition();
        if (!mxf_is_descriptive_metadata(&mxf_data_def))
            continue;

        // extract dm segments and map to database structures
        mxf_components = mxf_sequence->getStructuralComponents();
        vector<StructuralComponent*>::iterator comp_iter;
        for (comp_iter = mxf_components.begin(); comp_iter != mxf_components.end(); comp_iter++) {
            
            // DMSegment
            mxf_dm_segment = dynamic_cast<DMSegment*>(*comp_iter);
            if (!mxf_dm_segment)
                continue;

            // map to database user comment
            
            position = mxf_dm_segment->getEventStartPosition();
            if (mxf_dm_segment->haveEventComment())
                comment = mxf_dm_segment->getEventComment();
            else
                comment = "";
            if (mxf_dm_segment->haveItem(&MXF_ITEM_K(DMSegment, CommentMarkerColor))) {
                MXFPP_CHECK(mxf_avid_get_rgb_color_item(mxf_dm_segment->getCMetadataSet(),
                                                        &MXF_ITEM_K(DMSegment, CommentMarkerColor), &mxf_color));
                convert_color(&mxf_color, &color);
            } else {
                color = USER_COMMENT_RED_COLOUR;
            }

            _material_package->addUserComment(POSITIONED_COMMENT_NAME, comment, position, color);
        }
    }
}

void IngexMXFInfo::extractFileSourcePackageInfo(SourcePackage *mxf_package, mxfUL *essence_container_label)
{
    prodauto::FileEssenceDescriptor *descriptor;
    FileDescriptor *mxf_descriptor;
    GenericPictureEssenceDescriptor *mxf_picture_descriptor;
    GenericSoundEssenceDescriptor *mxf_sound_descriptor;
    int32_t avid_resolution_id = 0;
    mxfUL picture_essence_coding;
    
    memset(&picture_essence_coding, 0, sizeof(picture_essence_coding));

    mxf_descriptor = dynamic_cast<FileDescriptor*>(mxf_package->getDescriptor());
    
    descriptor = new prodauto::FileEssenceDescriptor();
    _file_source_package->descriptor = descriptor;
    
    descriptor->fileLocation = _filename;
    descriptor->fileFormat = MXF_FILE_FORMAT_TYPE;
    
    vector<prodauto::UserComment> source_comments = _material_package->getUserComments(AVID_UC_SOURCE_NAME);
    if (!source_comments.empty())
        _file_source_package->sourceConfigName = source_comments[0].value;
    else
        mxf_log_warn("Missing Avid '%s' (source config name) user comment\n", AVID_UC_SOURCE_NAME);
    
    mxf_picture_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(mxf_descriptor);
    mxf_sound_descriptor = dynamic_cast<GenericSoundEssenceDescriptor*>(mxf_descriptor);

    if (mxf_picture_descriptor) {
        if (mxf_descriptor->haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ResolutionID)))
            avid_resolution_id = mxf_descriptor->getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor,
                                                              ResolutionID));
        if (mxf_picture_descriptor->havePictureEssenceCoding())
            picture_essence_coding = mxf_picture_descriptor->getPictureEssenceCoding();
        
        descriptor->videoResolutionID = getVideoResolutionId(essence_container_label, &picture_essence_coding,
                                                             avid_resolution_id);
        
        if (mxf_picture_descriptor->haveAspectRatio())
            convert_rational(mxf_picture_descriptor->getAspectRatio(), &descriptor->imageAspectRatio);
        
    } else if (mxf_sound_descriptor) {
        if (mxf_sound_descriptor->haveQuantizationBits())
            descriptor->audioQuantizationBits = mxf_sound_descriptor->getQuantizationBits();
    }
}

void IngexMXFInfo::extractTapeSourcePackageInfo(SourcePackage *mxf_package)
{
    prodauto::TapeEssenceDescriptor *descriptor;

    descriptor = new prodauto::TapeEssenceDescriptor();
    _tape_source_package->descriptor = descriptor;
}

vector<TaggedValue*> IngexMXFInfo::getPackageAttributes(GenericPackage *mxf_package)
{
    vector<TaggedValue*> result;

    if (!mxf_package->haveItem(&MXF_ITEM_K(GenericPackage, MobAttributeList)))
        return result;
    
    auto_ptr<ObjectIterator> iter(mxf_package->getStrongRefArrayItem(&MXF_ITEM_K(GenericPackage, MobAttributeList)));
    while (iter->next()) {
        MXFPP_CHECK(dynamic_cast<TaggedValue*>(iter->get()));
        result.push_back(dynamic_cast<TaggedValue*>(iter->get()));
    }
    
    return result;
}

vector<TaggedValue*> IngexMXFInfo::getPackageComments(GenericPackage *mxf_package)
{
    vector<TaggedValue*> result;

    if (!mxf_package->haveItem(&MXF_ITEM_K(GenericPackage, UserComments)))
        return result;
    
    auto_ptr<ObjectIterator> iter(mxf_package->getStrongRefArrayItem(&MXF_ITEM_K(GenericPackage, UserComments)));
    while (iter->next()) {
        MXFPP_CHECK(dynamic_cast<TaggedValue*>(iter->get()));
        result.push_back(dynamic_cast<TaggedValue*>(iter->get()));
    }
    
    return result;
}

string IngexMXFInfo::getStringTaggedValue(vector<TaggedValue*>& tagged_values, string name)
{
    vector<TaggedValue*>::iterator iter;
    for (iter = tagged_values.begin(); iter != tagged_values.end(); iter++) {
        if ((*iter)->getName() == name) {
            if ((*iter)->isStringValue())
                return (*iter)->getStringValue();
            else
                return "";
        }
    }
    
    return "";
}

int IngexMXFInfo::getVideoResolutionId(mxfUL *container_label, mxfUL *picture_essence_coding,
                                       int32_t avid_resolution_id)
{
    // Note: using mxf_equals_ul_mod_regver function below because the Avid labels use a different registry version byte
    if (mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_50_625_50_defined_template)) ||
        mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_50_625_50_extended_template)) ||
        mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_50_625_50_picture_only)) ||
        mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_50_525_60_defined_template)) ||
        mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_50_525_60_extended_template)) ||
        mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_50_525_60_picture_only)) ||
        mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_40_625_50_defined_template)))
    {
        return IMX50_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_40_625_50_extended_template)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_40_625_50_picture_only)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_40_525_60_defined_template)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_40_525_60_extended_template)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_40_525_60_picture_only)))
    {
        return IMX40_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_30_625_50_defined_template)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_30_625_50_extended_template)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_30_625_50_picture_only)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_30_525_60_defined_template)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_30_525_60_extended_template)) ||
             mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(D10_30_525_60_picture_only)))
    {
        return IMX30_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(IECDV_25_525_60_ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(IECDV_25_525_60_FrameWrapped)))
    {
        return DV25_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(IECDV_25_625_50_ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(IECDV_25_625_50_FrameWrapped)))
    {
        return DV25_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(DVBased_25_525_60_ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DVBased_25_525_60_FrameWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DVBased_25_625_50_ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DVBased_25_625_50_FrameWrapped)))
    {
        return DV25_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(DVBased_50_525_60_ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DVBased_50_525_60_FrameWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DVBased_50_625_50_ClipWrapped)) || 
             mxf_equals_ul(container_label, &MXF_EC_L(DVBased_50_625_50_FrameWrapped)))
    {
        return DV50_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(DV720p50ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DV1080i50ClipWrapped)))
    {
        return DVCPROHD_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(AvidMJPEGClipWrapped)))
    {
        // use the Avid resolution id if present, else use the picture essence coding label 
        if (avid_resolution_id != 0x00) {
            switch (avid_resolution_id) {
                case 0x4c:
                    return MJPEG21_MATERIAL_RESOLUTION;
                    break;
                case 0x4d:
                    return MJPEG31_MATERIAL_RESOLUTION;
                    break;
                case 0x6f:
                    return 0; // 4:1 not supported
                    break;
                case 0x4b:
                    return MJPEG101_MATERIAL_RESOLUTION;
                    break;
                case 0x6e:
                    return MJPEG101M_MATERIAL_RESOLUTION;
                    break;
                case 0x4e:
                    return MJPEG151S_MATERIAL_RESOLUTION;
                    break;
                case 0x52:
                    return MJPEG201_MATERIAL_RESOLUTION;
                    break;
                default:
                    return 0;
            }
        } else {
            if (mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG21_PAL)) ||
                mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG21_NTSC)))
            {
                return MJPEG21_MATERIAL_RESOLUTION;
            }
            else if (mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG31_PAL)) ||
                     mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG31_NTSC)))
            {
                return MJPEG31_MATERIAL_RESOLUTION;
            }
            else if (mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG101_PAL)) ||
                     mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG101_NTSC)))
            {
                return MJPEG101_MATERIAL_RESOLUTION;
            }
            else if (mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG101m_PAL)) ||
                     mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG101m_NTSC)))
            {
                return MJPEG101M_MATERIAL_RESOLUTION;
            }
            else if (mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG151s_PAL)) ||
                     mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG151s_NTSC)))
            {
                return MJPEG151S_MATERIAL_RESOLUTION;
            }
            else if (mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG201_PAL)) ||
                     mxf_equals_ul(picture_essence_coding, &MXF_CMDEF_L(AvidMJPEG201_NTSC)))
            {
                return MJPEG201_MATERIAL_RESOLUTION;
            }
            else
            {
                return 0;
            }
        }
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(HD_Unc_1080_50i_422_ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(SD_Unc_625_50i_422_135_ClipWrapped)))
    {
        return UNC_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(DNxHD1080i185ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DNxHD1080p185ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DNxHD720p185ClipWrapped)))
    {
        return DNX185i_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(DNxHD1080i120ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DNxHD1080p120ClipWrapped)) ||
             mxf_equals_ul(container_label, &MXF_EC_L(DNxHD720p120ClipWrapped)))
    {
        return DNX120i_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(DNxHD1080p36ClipWrapped)))
    {
        return DNX36p_MATERIAL_RESOLUTION;
    }
    else if (mxf_equals_ul(container_label, &MXF_EC_L(BWFClipWrapped)))
    {
        return 0;
    }
    else
    {
        return 0;
    }
}


