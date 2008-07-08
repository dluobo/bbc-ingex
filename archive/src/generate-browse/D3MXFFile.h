/*
 * $Id: D3MXFFile.h,v 1.1 2008/07/08 16:24:08 philipn Exp $
 *
 * Provides access to the metadata and essence contained in a D3 MXF file.
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#ifndef __BROWSE_D3_MXF_FILE_H__
#define __BROWSE_D3_MXF_FILE_H__



#include <archive_types.h>
#include <mxf_reader.h>

#include <string>


class D3MXFFrame
{
public:
    D3MXFFrame();
    ~D3MXFFrame();
    
    ArchiveTimecode vitc;
    ArchiveTimecode ltc;
    unsigned char* video;
    unsigned char* audio[8];
    int numAudioTracks;
};


class D3MXFFile
{
public:
    D3MXFFile(std::string filename);
    ~D3MXFFile();
 
    std::string getD3SpoolNo();
    InfaxData* getD3InfaxData();
    int64_t getDuration();
    
    bool nextFrame(D3MXFFrame*& frame);
    
private:
    static int accept_frame(MXFReaderListener* mxfListener, int trackIndex);
    static int allocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer, uint32_t bufferSize);
    static void deallocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer);
    static int receive_frame(MXFReaderListener* mxfListener, int trackIndex, uint8_t* buffer, uint32_t bufferSize);


    MXFReader* _mxfReader;
    MXFDataModel* _dataModel;
    
    int64_t _mxfDuration;
    InfaxData _d3InfaxData;
    InfaxData _ltoInfaxData;

    D3MXFFrame _frame;
    int _videoTrack;
    int _audioTrack[8];
};








#endif


