/*
 * $Id: Chunking.cpp,v 1.1 2008/07/08 16:25:33 philipn Exp $
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

/*
    Chunking does the same as capture: create an MXF file, browse copy file, 
    timecode text file, information text file and PSE report file for each item.
    The difference is that the source essence data is from a previously created 
    MXF file rather than from captured SDI data.
    
    The original MXF file was written as a "page file" and this reduces the
    disk space requirement from 2 x file size to 2 x page size. Page files are 
    deleted when transferring the data if the remaining disk space is nearing 
    the disk space margin.
    
    The chunking is throttled if a tape transfer is in progress to ensure a
    near maximum tape transfer speed is maintained.
*/

#include <errno.h>

#include "Chunking.h"
#include "Recorder.h"
#include "RecordingSession.h"
#include "RecorderDatabase.h"
#include "Cache.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"
#include "Timing.h"
#include "FileLock.h"

#include "video_conversion.h"
#include "PSEReport.h"

#include <mxf/mxf_page_file.h>


using namespace std;
using namespace rec;


// check whether throttling is required every x frames
#define THROTTLE_CHECK_INTERVAL         25


static void convert_umid(mxfUMID* from, UMID* to)
{
    memcpy(to->bytes, &from->octet0, 32);
}


bool Chunking::readyForChunking(RecordingItems* recordingItems)
{
    vector<RecordingItem> items = recordingItems->getItems();
    
    if (items.empty())
    {
        return false;
    }
    
    // seek to start of items not set
    // and then check items not set are disabled
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        if ((*iter).duration < 0)
        {
            break;
        }
    }
    for (; iter != items.end(); iter++)
    {
        if (!(*iter).isDisabled)
        {
            return false;
        }
    }
    
    return true;
}


Chunking::Chunking(RecordingSession* session, RecordingSessionTable* sessionTable, string mxfPageFilename, 
    RecordingItems* recordingItems)
: _session(session), _sessionTable(sessionTable), _recordingItems(recordingItems), 
_stop(false), _hasStopped(false), _inputD3MXFFile(0), _hddDest(0), _stereoAudio(0), _yuv420Video(0)
{
    try
    {
        _inputD3MXFFile = new D3MXFFile(mxfPageFilename, MXF_PAGE_FILE_SIZE);

        
        REC_ASSERT(recordingItems->getItems().size() > 0);
    
        _status.duration = recordingItems->getTotalDuration();
        REC_ASSERT(_status.duration == _inputD3MXFFile->getDuration());

        
        _stereoAudio = new int16_t[1920 * 2];
        memset(_stereoAudio, 0, 1920 * 2 * sizeof(int16_t));
    
        _yuv420Video = new unsigned char[720 * 576 * 3 / 2];
        memset(_yuv420Video, 0, 720 * 576 * 3 / 2);
        
        _status.itemNumber = 1;
        _status.frameNumber = 0;
    }
    catch (...)
    {
        delete _inputD3MXFFile;
        delete [] _stereoAudio;
        delete [] _yuv420Video;

        throw;
    }
}

Chunking::~Chunking()
{
    delete _inputD3MXFFile;
    
    delete [] _stereoAudio;
    delete [] _yuv420Video;
}

void Chunking::start()
{
    GUARD_THREAD_START(_hasStopped);

    vector<RecordingItem> items = _recordingItems->getItems();
    REC_ASSERT(items.size() > 0);
    
    // handle case where all items have been disabled
    if (items.front().isDisabled)
    {
        setChunkingState(CHUNKING_COMPLETED);
        return;
    }
    
    
    int numAudioTracks = Config::getInt("num_audio_tracks");
    int videoKBps = Config::getInt("browse_video_bit_rate");
    int threadCount = Config::getInt("browse_thread_count");
    int chunkingThrottleFPS = Config::getInt("chunking_throttle_fps");
    
    long chunkingThrottleUSec = (long)(40 * THROTTLE_CHECK_INTERVAL * 25.0 / chunkingThrottleFPS) * MSEC_IN_USEC; 
    
    PSEFailure* pseFailures = 0;
    long numPSEFailures = _inputD3MXFFile->getPSEFailures(&pseFailures);
    VTRErrorAtPos* vtrPosErrors = 0;
    long numVTRErrors = _inputD3MXFFile->getVTRErrors(&vtrPosErrors);
    // note: vtrPosErrors does not include the timecode information and therefore
    // we create a second array of VTRErrors which will have the timecodes
    // filled in
    VTRError* vtrErrors = 0;
    if (numVTRErrors > 0)
    {
        vtrErrors = new VTRError[numVTRErrors];
    }
    
    bool completed = false;
    Cache* cache = _session->_recorder->getCache();
    vector<RecordingItem>::iterator itemIter = items.begin();
    RecordingItem* item = &(*itemIter);
    int itemNumber = 1;
    D3MXFFrame* d3MXFFrame = 0;
    int64_t inputFrameNumber = 0;
    int64_t itemFrameNumber = 0;
    ArchiveMXFWriter* outputMXFFile = 0;
    BrowseEncoder* browseEncoder = 0;
    FILE* timecodeFile = 0;
    long firstPSEFailure = 0;
    long firstVTRError = 0;
    long numItemPSEFailures = 0;
    long numItemVTRErrors = 0;
    int64_t diskSpaceCheckInterval = 0;
    Timer throttleTimer;
    bool noMore;
    try
    {
        setChunkingState(CHUNKING_IN_PROGRESS);
        
        while (!_stop)
        {
            // prepare stuff if at start of item
            if (itemFrameNumber == 0)
            {
                if (item->isJunk)
                {
                    Logging::info("Skipping junk item %d\n", itemNumber);

                    // update status
                    {
                        LOCK_SECTION(_statusMutex);
                        _status.frameNumber = inputFrameNumber;
                        _status.itemNumber = itemNumber;
                    }
                
                    // skip frames
                    if (!_inputD3MXFFile->skipFrames(item->duration))
                    {
                        REC_LOGTHROW(("D3 MXF file is missing frames"));
                    }

                    inputFrameNumber += item->duration;
                    itemFrameNumber += item->duration;

                    // update status
                    {
                        LOCK_SECTION(_statusMutex);
                        _status.frameNumber = inputFrameNumber;
                        _status.itemNumber = itemNumber;
                    }

                    
                    // update PSE failure and VTR error counts
                    noMore = false;
                    while (!noMore)
                    {
                        noMore = true;
                        if (pseFailures != 0 && firstPSEFailure + numItemPSEFailures < numPSEFailures)
                        {
                            PSEFailure* pseFailure = &pseFailures[firstPSEFailure + numItemPSEFailures];
                            
                            if (pseFailure->position < inputFrameNumber)
                            {
                                numItemPSEFailures++;
                                noMore = false;
                            }
                        }
                        if (vtrErrors != 0 && firstVTRError + numItemVTRErrors < numVTRErrors)
                        {
                            VTRErrorAtPos* vtrPosError = &vtrPosErrors[firstVTRError + numItemVTRErrors];
                            
                            if (vtrPosError->position < inputFrameNumber)
                            {
                                numItemVTRErrors++;
                                noMore = false;
                            }
                        }
                    }
                    
                    // junk it 
                    _recordingItems->setJunked(item->id);
                    
                    
                    // move on to the next item
    
                    firstPSEFailure += numItemPSEFailures;
                    numItemPSEFailures = 0;
                    firstVTRError += numItemVTRErrors;
                    numItemVTRErrors = 0;
    
                    itemIter++;
                    if (itemIter == items.end() || (*itemIter).isDisabled)
                    {
                        completed = true;
                        break;
                    }
                    
                    itemFrameNumber = 0;
                    itemNumber++;
                    item = &*itemIter;
                    
                    continue;
                }

                
                Logging::info("Starting chunking of item %d\n", itemNumber);

                if (!_session->_recorder->getCache()->getMultiItemFilenames(_recordingItems->getSource()->barcode, 
                    item->d3Source->itemNo, &_mxfFilename, &_browseFilename, &_browseTimecodeFilename, &_browseInfoFilename, &_pseFilename))
                {
                    REC_LOGTHROW(("Failed to get unique filenames from the cache"));
                }

                _hddDest = _session->addHardDiskDestination(_mxfFilename, _browseFilename, _pseFilename, item->d3Source, itemNumber);

                
                REC_CHECK(prepare_archive_mxf_file(cache->getCompleteCreatingFilename(_mxfFilename).c_str(), numAudioTracks, 0, 1, &outputMXFFile));
                
                diskSpaceCheckInterval = MXF_PAGE_FILE_SIZE / get_archive_mxf_content_package_size(numAudioTracks);
                REC_ASSERT(diskSpaceCheckInterval > 0);
                
                
                browseEncoder = BrowseEncoder::create(cache->getCompleteBrowseFilename(_browseFilename).c_str(), videoKBps, threadCount);
                if (browseEncoder == 0)
                {
                    REC_LOGTHROW(("Failed to open the browse encoder"));
                }
                
                timecodeFile = fopen(cache->getCompleteBrowseFilename(_browseTimecodeFilename).c_str(), "wb");
                if (timecodeFile == 0)
                {
                    REC_LOGTHROW(("Failed to open the timecode file '%s': %s", _browseTimecodeFilename.c_str(), strerror(errno)));
                }

                _session->writeBrowseInfo(cache->getCompleteBrowseFilename(_browseInfoFilename), item, false);
            }

            // update status
            {
                LOCK_SECTION(_statusMutex);
                _status.frameNumber = inputFrameNumber;
                _status.itemNumber = itemNumber;
            }
            
            
            // read next input frame
            if (!_inputD3MXFFile->nextFrame(d3MXFFrame))
            {
                REC_LOGTHROW(("D3 MXF file is missing frames"));
            }
            
            // check every second if a tape transfer is in progress and if so
            // then throttle the chunking
            if (inputFrameNumber % 25 == 0)
            {
                // if this file is locked then a tape transfer is in progress 
                if (FileLock::isLocked(Config::getString("tape_transfer_lock_file")))
                {
                    throttleTimer.sleepRemainder();
                    throttleTimer.start(chunkingThrottleUSec);
                }
            }
            
            // check disk space and truncate the input MXF file is neccessary
            
            if (inputFrameNumber % diskSpaceCheckInterval == 0)
            {
                int64_t diskSpace = _session->_recorder->getRemainingDiskSpace();
                if (diskSpace < DISK_SPACE_MARGIN)
                {
                    _inputD3MXFFile->forwardTruncate();
                }
            }
            
            
            // write frame
            
            REC_ASSERT(numAudioTracks == d3MXFFrame->numAudioTracks);
            writeMXFFrame(outputMXFFile, d3MXFFrame);
            writeBrowseFrame(d3MXFFrame, browseEncoder, itemFrameNumber);
            writeTimecodeFrame(d3MXFFrame, itemFrameNumber, timecodeFile);
            
            
            // complete PSE failure and VTR error for current frame
            
            if (pseFailures != 0 && firstPSEFailure + numItemPSEFailures < numPSEFailures)
            {
                PSEFailure* pseFailure = &pseFailures[firstPSEFailure + numItemPSEFailures];
                
                if (pseFailure->position == inputFrameNumber)
                {
                    pseFailure->vitcTimecode = d3MXFFrame->vitc;
                    pseFailure->ltcTimecode = d3MXFFrame->ltc;
                    pseFailure->position = itemFrameNumber; // position is now relative to start of item
                    
                    numItemPSEFailures++;
                }
            }
            if (vtrErrors != 0 && firstVTRError + numItemVTRErrors < numVTRErrors)
            {
                VTRErrorAtPos* vtrPosError = &vtrPosErrors[firstVTRError + numItemVTRErrors];
                VTRError* vtrError = &vtrErrors[firstVTRError + numItemVTRErrors];
                
                if (vtrPosError->position == inputFrameNumber)
                {
                    vtrError->vitcTimecode = d3MXFFrame->vitc;
                    vtrError->ltcTimecode = d3MXFFrame->ltc;
                    vtrError->errorCode = vtrPosError->errorCode;
                    
                    numItemVTRErrors++;
                }
            }
            
            inputFrameNumber++;
            itemFrameNumber++;
            

            // update status
            {
                LOCK_SECTION(_statusMutex);
                _status.frameNumber = inputFrameNumber;
                _status.itemNumber = itemNumber;
            }
            
            
            // complete the item if we are at the end
            if (itemFrameNumber == item->duration)
            {
                // complete the item
                
                mxfUMID mxfMaterialPackageUID = get_material_package_uid(outputMXFFile);
                mxfUMID mxfFilePackageUID = get_file_package_uid(outputMXFFile);
                mxfUMID mxfTapePackageUID = get_tape_package_uid(outputMXFFile);
                
                InfaxData infaxData;
                _session->getInfaxData(item, &infaxData);
                
                REC_CHECK(complete_archive_mxf_file(&outputMXFFile, &infaxData,
                    (numItemPSEFailures > 0) ? &pseFailures[firstPSEFailure] : 0, numItemPSEFailures, 
                    (numItemVTRErrors > 0) ? &vtrErrors[firstVTRError] : 0, numItemVTRErrors));
                
                SAFE_DELETE(browseEncoder);
                
                fclose(timecodeFile);
                timecodeFile = 0;

                _session->writeBrowseInfo(cache->getCompleteBrowseFilename(_browseInfoFilename), item, false);
                
                int pseResult = writePSEReport(itemFrameNumber, &infaxData, 
                    (numItemPSEFailures > 0) ? &pseFailures[firstPSEFailure] : 0, numItemPSEFailures);


                Logging::info("Completed chunking of item %d\n", itemNumber);


                // update hard disk destination in database
                
                _hddDest->duration = itemFrameNumber;
                _hddDest->pseResult = pseResult;
                _hddDest->size = _session->_recorder->getCache()->getCreatingFileSize(_hddDest->name);
                _hddDest->browseSize = _session->_recorder->getCache()->getBrowseFileSize(_hddDest->browseName);
                convert_umid(&mxfMaterialPackageUID, &_hddDest->materialPackageUID);
                convert_umid(&mxfFilePackageUID, &_hddDest->filePackageUID);
                convert_umid(&mxfTapePackageUID, &_hddDest->tapePackageUID);
                _session->updateHardDiskDestination(_hddDest);
                
                
                // update items
                
                _recordingItems->setChunked(item->id, cache->getCompleteCreatingFilename(_mxfFilename));
                
                
                // move on to the next item

                firstPSEFailure += numItemPSEFailures;
                numItemPSEFailures = 0;
                firstVTRError += numItemVTRErrors;
                numItemVTRErrors = 0;

                itemIter++;
                if (itemIter == items.end() || (*itemIter).isDisabled)
                {
                    completed = true;
                    break;
                }
                
                itemFrameNumber = 0;
                itemNumber++;
                item = &*itemIter;
            }
        }
        
        
        // check all frames from the input mxf file have been used 
        if (completed && _inputD3MXFFile->nextFrame(d3MXFFrame))
        {
            REC_LOGTHROW(("D3 MXF file has extra frames"));
        }

        
        // clean up
        
        if (pseFailures != 0)
        {
            free(pseFailures);
            pseFailures = 0;
        }
        if (vtrPosErrors != 0)
        {
            free(vtrPosErrors);
            vtrPosErrors = 0;
        }
        delete [] vtrErrors;
        vtrErrors = 0;
    }
    catch (...)
    {
        if (outputMXFFile != 0)
        {
            abort_archive_mxf_file(&outputMXFFile);
        }
        SAFE_DELETE(browseEncoder);
        if (timecodeFile != 0)
        {
            fclose(timecodeFile);
        }

        if (pseFailures != 0)
        {
            free(pseFailures);
            pseFailures = 0;
        }
        if (vtrPosErrors != 0)
        {
            free(vtrPosErrors);
            vtrPosErrors = 0;
        }
        delete [] vtrErrors;
        
        
        completed = false;
    }
    
    
    if (completed)
    {
        setChunkingState(CHUNKING_COMPLETED);
    }
    else
    {
        setChunkingState(CHUNKING_FAILED);
    }
}

void Chunking::stop()
{
    _stop = true;
}

bool Chunking::hasStopped() const
{
    return _hasStopped;
}

ChunkingStatus Chunking::getStatus()
{
    LOCK_SECTION(_statusMutex);
    return _status;
}

void Chunking::setChunkingState(ChunkingState state)
{
    LOCK_SECTION(_statusMutex);

    _status.state = state;
}

ChunkingState Chunking::getChunkingState()
{
    LOCK_SECTION(_statusMutex);

    return _status.state;
}

void Chunking::writeMXFFrame(ArchiveMXFWriter* writer, D3MXFFrame* frame)
{
    int i;
    REC_CHECK(write_timecode(writer, frame->vitc, frame->ltc));
    REC_CHECK(write_video_frame(writer, frame->video, frame->videoSize));
    for (i = 0; i < frame->numAudioTracks; i++)
    {
        REC_CHECK(write_audio_frame(writer, frame->audio[i], frame->audioSize));
    }
}

void Chunking::writeBrowseFrame(D3MXFFrame* frame, BrowseEncoder* browseEncoder, int64_t frameNumber)
{
    // convert audio to 16 bit stereo
    if (frame->numAudioTracks > 0)
    {
        uint16_t* outputA12 = (uint16_t*)_stereoAudio;
        unsigned char* inputA1 = frame->audio[0];
        unsigned char* inputA2 = frame->audio[1];
        int i;
        if (frame->numAudioTracks > 1)
        {
            for (i = 0; i < 1920 * 3; i += 3) 
            {
                *outputA12++ = (((uint16_t)inputA1[i + 2]) << 8) | inputA1[i + 1];
                *outputA12++ = (((uint16_t)inputA2[i + 2]) << 8) | inputA2[i + 1];
            }
        }
        else
        {
            for (i = 0; i < 1920 * 3; i += 3) 
            {
                *outputA12++ = (((uint16_t)inputA1[i + 2]) << 8) | inputA1[i + 1];
                *outputA12++ = 0;
            }
        }
    }
    
    // convert video to yuv420    
    uyvy_to_yuv420(720, 576, 0, frame->video, _yuv420Video);

    // encode
    REC_CHECK(browseEncoder->encode(_yuv420Video, _stereoAudio, frame->ltc, frame->vitc, frameNumber));
}
            
void Chunking::writeTimecodeFrame(D3MXFFrame* frame, int64_t frameNumber, FILE* timecodeFile)
{
    ArchiveTimecode ctc;
    ctc.hour = frameNumber / (60 * 60 * 25);
    ctc.min = (frameNumber % (60 * 60 * 25)) / (60 * 25);
    ctc.sec = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) / 25;
    ctc.frame = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) % 25;
    
    fprintf(timecodeFile, "C%02d:%02d:%02d:%02d V%02d:%02d:%02d:%02d L%02d:%02d:%02d:%02d\n", 
    ctc.hour, ctc.min, ctc.sec, ctc.frame,
    frame->vitc.hour, frame->vitc.min, frame->vitc.sec, frame->vitc.frame,
    frame->ltc.hour, frame->ltc.min, frame->ltc.sec, frame->ltc.frame);
}
            
int Chunking::writePSEReport(int64_t duration, InfaxData* infaxData, PSEFailure* pseFailures, long numPSEFailures)
{
    string filename = _session->_recorder->getCache()->getCompletePSEFilename(_pseFilename);
    
    PSEReport* pseReport = PSEReport::open(filename);
    if (pseReport == 0) 
    {
        REC_LOGTHROW(("Failed to open PSE report file '%s' for writing: %s", filename.c_str()));
    }
    
    try
    {
        bool passed;
        REC_CHECK(pseReport->write(0, duration - 1, "", infaxData, pseFailures, numPSEFailures, &passed));

        delete pseReport;

        return (passed) ? PSE_RESULT_PASSED : PSE_RESULT_FAILED;
    }
    catch (...)
    {
        delete pseReport;
        throw;
    }
}

