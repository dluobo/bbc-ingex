/*
 * $Id: Chunking.h,v 1.1 2008/07/08 16:25:40 philipn Exp $
 *
 * Chunks an MXF file into MXF, browse copy and PSE report files for each item
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
 
#ifndef __RECORDER_CHUNKING_H__
#define __RECORDER_CHUNKING_H__


#include "RecordingItems.h"
#include "Threads.h"
#include "D3MXFFile.h"
#include "BrowseEncoder.h"

#include <write_archive_mxf.h>


namespace rec
{

    
typedef enum 
{
    CHUNKING_NOT_STARTED = 0,
    CHUNKING_IN_PROGRESS,
    CHUNKING_FAILED,
    CHUNKING_COMPLETED
} ChunkingState;
    
class ChunkingStatus
{
public:
    ChunkingStatus()
    : state(CHUNKING_NOT_STARTED), itemNumber(-1), frameNumber(-1), duration(-1)
    {}
    
    ChunkingState state;
    int itemNumber;
    int64_t frameNumber;
    int64_t duration;
};


class RecordingSession;

class Chunking : public ThreadWorker
{
public:
    static bool readyForChunking(RecordingItems* recordingItems);
    
public:
    Chunking(RecordingSession* session, RecordingSessionTable* sessionTable, 
        std::string mxfPageFilename, RecordingItems* recordingItems);
    ~Chunking();
    
    virtual void start();
    virtual void stop();
    virtual bool hasStopped() const;
    
    ChunkingStatus getStatus();
    
private:
    void setChunkingState(ChunkingState state);
    ChunkingState getChunkingState();
    
    void writeMXFFrame(ArchiveMXFWriter* writer, D3MXFFrame* frame);
    void writeBrowseFrame(D3MXFFrame* frame, BrowseEncoder* browseEncoder, int64_t frameNumber);    
    void writeTimecodeFrame(D3MXFFrame* frame, int64_t frameNumber, FILE* timecodeFile);
    
    int writePSEReport(int64_t duration, InfaxData* infax, PSEFailure* pseFailures, long numPSEFailures);

    RecordingSession* _session;
    RecordingSessionTable* _sessionTable;
    RecordingItems* _recordingItems;
    bool _stop;
    bool _hasStopped;
    
    D3MXFFile* _inputD3MXFFile;
    
    std::string _mxfFilename;
    std::string _browseFilename;
    std::string _browseTimecodeFilename;
    std::string _browseInfoFilename;
    std::string _pseFilename;
    HardDiskDestination* _hddDest;
    
    int16_t* _stereoAudio;
    unsigned char* _yuv420Video;
    
    Mutex _statusMutex;
    ChunkingStatus _status;
};














};







#endif



