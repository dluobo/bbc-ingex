/*
 * $Id: ArchiveMXFWriter.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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
 
#ifndef __MXFPP_ARCHIVE_MXF_WRITER_H__
#define __MXFPP_ARCHIVE_MXF_WRITER_H__


#include <string>

#include <libMXF++/MXF.h>

#include <CommonTypes.h>
#include "ArchiveMXFContentPackage.h"


namespace mxfpp
{


class SetForDurationUpdate;

class ArchiveMXFWriter
{
public:
    ArchiveMXFWriter(std::string filename, int numAudioTracks, int64_t startPosition);
    virtual ~ArchiveMXFWriter();
    
    // write each element of the content package in order: timecode, video, audio 1..x
    void writeTimecode(Timecode vitc, Timecode ltc);
    void writeVideoFrame(unsigned char* data, uint32_t size);
    void writeAudioFrame(unsigned char* data, uint32_t size);
    
    // write the content package in one go
    void writeContentPackage(ArchiveMXFContentPackage* cp);
    
    void complete();
    
private:
    int _numAudioTracks;
    
    mxfpp::File* _mxfFile;
    mxfpp::DataModel* _dataModel;
    mxfpp::HeaderMetadata* _headerMetadata;
    mxfpp::IndexTableSegment* _indexSegment;
    
    std::vector<SetForDurationUpdate*> _setsForDurationUpdate;
    
    int64_t _duration;
    int64_t _headerMetadataStartPos;
    bool _isComplete;
    int _writeState;
};


};



#endif

