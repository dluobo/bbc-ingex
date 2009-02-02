/*
 * $Id: ArchiveMXFReader.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#ifndef __MXFPP_ARCHIVE_MXF_READER_H__
#define __MXFPP_ARCHIVE_MXF_READER_H__


#include <string>

#include <libMXF++/MXF.h>

#include "CommonTypes.h"
#include "ArchiveMXFContentPackage.h"


namespace mxfpp
{

class ArchiveMXFReader
{
public:
    ArchiveMXFReader(std::string filename);
    virtual ~ArchiveMXFReader();

    int64_t getDuration();
    int64_t getPosition();
    
    void seekToPosition(int64_t position);
    bool seekToTimecode(Timecode vitc, Timecode ltc);
    
    bool isEOF();
    
    const ArchiveMXFContentPackage* read();
    
private:
    uint32_t readElementData(DynamicByteArray* bytes, const mxfKey* elementKey);

    mxfpp::File* _mxfFile;
    mxfpp::DataModel* _dataModel;
    mxfpp::HeaderMetadata* _headerMetadata;

    int _numAudioTracks;
    int64_t _position;
    int64_t _duration;
    int64_t _actualPosition;
    ArchiveMXFContentPackage _cp;

    int64_t _startOfEssenceFilePos;    
};



};


#endif



