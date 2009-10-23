/*
 * $Id: OPAtomTrackReader.h,v 1.2 2009/10/23 09:05:21 philipn Exp $
 *
 * Read a single OP-Atom file representing a clip track
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

#ifndef __OPATOM_TRACK_READER_H__
#define __OPATOM_TRACK_READER_H__

#include <string>

#include "OPAtomContentPackage.h"
#include "RawEssenceParser.h"
#include "FrameOffsetIndexTable.h"

namespace mxfpp
{
class HeaderMetadata;
class File;
};


typedef enum
{
    OP_ATOM_SUCCESS = 0,
    OP_ATOM_FAIL = -1,
    OP_ATOM_FILE_OPEN_READ_ERROR = -2,
    OP_ATOM_NO_HEADER_PARTITION = -3,
    OP_ATOM_NOT_OP_ATOM = -4,
    OP_ATOM_HEADER_ERROR = -5,
    OP_ATOM_NO_FILE_PACKAGE = -6,
    OP_ATOM_ESSENCE_DATA_NOT_FOUND = -7
} OPAtomOpenResult;


class OPAtomTrackReader
{
public:
    static OPAtomOpenResult Open(std::string filename, OPAtomTrackReader **track_reader);
    static std::string ErrorToString(OPAtomOpenResult result);
    
public:
    ~OPAtomTrackReader();
    
    std::string GetFilename() const { return mFilename; }
    mxfpp::HeaderMetadata* GetHeaderMetadata() const { return mHeaderMetadata; }
    mxfpp::DataModel* GetDataModel() const { return mDataModel; }
    uint32_t GetMaterialTrackId() const { return mTrackId; }
    
    mxfUL GetEssenceContainerLabel();
    int64_t GetDuration();
    int64_t DetermineDuration();
    int64_t GetPosition();
    
    bool Read(OPAtomContentPackage *content);
    
    bool Seek(int64_t position);
    
    bool IsEOF();

private:
    OPAtomTrackReader(std::string filename, mxfpp::File *mxf_file, mxfpp::Partition *header_partition);
    
private:
    std::string mFilename;
    
    uint32_t mTrackId;
    int64_t mDurationInMetadata;
    bool mIsPicture;
    
    mxfpp::HeaderMetadata *mHeaderMetadata;
    mxfpp::DataModel *mDataModel;
    
    FrameOffsetIndexTableSegment *mIndexTable;
    
    RawEssenceParser *mEssenceParser;
};



#endif

