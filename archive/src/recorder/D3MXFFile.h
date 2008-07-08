/*
 * $Id: D3MXFFile.h,v 1.1 2008/07/08 16:25:44 philipn Exp $
 *
 * Provides access to an MXF page file and can forward truncate it
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
 
#ifndef __RECORDER_D3_MXF_FILE_H__
#define __RECORDER_D3_MXF_FILE_H__



#include <archive_types.h>
#include <mxf_reader.h>
#include <mxf/mxf_page_file.h>

#include <string>


namespace rec
{

class D3MXFFrame
{
public:
    D3MXFFrame();
    ~D3MXFFrame();
    
    ArchiveTimecode vitc;
    ArchiveTimecode ltc;
    
    unsigned char* video;
    int videoSize;
    
    unsigned char* audio[8];
    int audioSize;
    int numAudioTracks;
};


class D3MXFFile
{
public:
    D3MXFFile(std::string filename, int64_t pageSize);
    ~D3MXFFile();
 
    int64_t getDuration();
    
    InfaxData* getD3InfaxData();
    InfaxData* getLTOInfaxData();
    
    long getPSEFailures(PSEFailure** failures);
    long getVTRErrors(VTRErrorAtPos** errors);
    
    
    bool nextFrame(D3MXFFrame*& frame);
    
    bool skipFrames(int64_t count);
    
    
    void forwardTruncate();
    
private:
    static int accept_frame(MXFReaderListener* mxfListener, int trackIndex);
    static int allocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer, uint32_t bufferSize);
    static void deallocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer);
    static int receive_frame(MXFReaderListener* mxfListener, int trackIndex, uint8_t* buffer, uint32_t bufferSize);


    MXFPageFile* _mxfPageFile;
    MXFReader* _mxfReader;
    MXFDataModel* _dataModel;
    
    int64_t _mxfDuration;
    InfaxData _d3InfaxData;
    InfaxData _ltoInfaxData;
    PSEFailure* _pseFailures;
    long _numPSEFailures;
    VTRErrorAtPos* _vtrErrors;
    long _numVTRErrors;

    D3MXFFrame _frame;
    int _videoTrack;
    int _audioTrack[8];
};


};





#endif


