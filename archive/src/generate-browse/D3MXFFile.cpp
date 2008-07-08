/*
 * $Id: D3MXFFile.cpp,v 1.1 2008/07/08 16:24:04 philipn Exp $
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
 
#include "D3MXFFile.h"
#include "Logging.h"
#include "RecorderException.h"

#include <d3_mxf_info_lib.h>


using namespace std;
using namespace rec;


struct _MXFReaderListenerData
{
    D3MXFFile* mxfFile;
};


D3MXFFrame::D3MXFFrame()
: video(0)
{
    memset(audio, 0, sizeof(audio));
    memset(&vitc, 0, sizeof(vitc));
    memset(&ltc, 0, sizeof(ltc));
}

D3MXFFrame::~D3MXFFrame()
{
    delete [] video;
    int i;
    for (i = 0; i < 8; i++)
    {
        delete [] audio[i];
    }
}


D3MXFFile::D3MXFFile(string filename)
: _mxfReader(0)
{
    // extract properties
    
    MXFFile* mxfFile = 0;
    D3MXFInfo d3MXFInfo;
    try
    {
        // open the mxf file
        if (!mxf_disk_file_open_read(filename.c_str(), &mxfFile))
        {
            throw RecorderException("Failed to open MXF disk file '%s'\n", filename.c_str());
        }
        
        // load d3 extensions and init reader
        REC_CHECK(mxf_load_data_model(&_dataModel));
        REC_CHECK(d3_mxf_load_extensions(_dataModel));
        REC_CHECK(mxf_finalise_data_model(_dataModel));
        REC_CHECK(init_mxf_reader_2(&mxfFile, _dataModel, &_mxfReader));

        // get metadata
        memset(&d3MXFInfo, 0, sizeof(D3MXFInfo));
        REC_CHECK(d3_mxf_get_info(get_header_metadata(_mxfReader), &d3MXFInfo));
        _mxfDuration = get_duration(_mxfReader);
        _d3InfaxData = d3MXFInfo.d3InfaxData;
        _ltoInfaxData = d3MXFInfo.ltoInfaxData;
        
        // allocate buffers
        _frame.video = new unsigned char[720*576*2];
        _frame.numAudioTracks = 0;
        int numTracks = get_num_tracks(_mxfReader);
        int i;
        for (i = 0; i < numTracks; i++)
        {
            MXFTrack* track = get_mxf_track(_mxfReader, i);
            if (track->isVideo)
            {
                _videoTrack = i;
            }
            else // is audio
            {
                _audioTrack[_frame.numAudioTracks] = i;
                _frame.audio[_frame.numAudioTracks] = new unsigned char[1920 * 3];
                _frame.numAudioTracks++;
            }
        }
    }
    catch (...)
    {
        if (_mxfReader != 0)
        {
            close_mxf_reader(&_mxfReader);
        }
        else if (mxfFile != 0)
        {
            mxf_file_close(&mxfFile);
        }
        
        if (_dataModel != 0)
        {
            mxf_free_data_model(&_dataModel);
        }
        
        throw;
    }
    
}

D3MXFFile::~D3MXFFile()
{
    close_mxf_reader(&_mxfReader);
    mxf_free_data_model(&_dataModel);
}

string D3MXFFile::getD3SpoolNo()
{
    return _d3InfaxData.spoolNo;
}

InfaxData* D3MXFFile::getD3InfaxData()
{
    return &_d3InfaxData;
}

int64_t D3MXFFile::getDuration()
{
    return _mxfDuration;
}

bool D3MXFFile::nextFrame(D3MXFFrame*& frame)
{
    MXFReaderListener mxfListener;
    MXFReaderListenerData mxfListenerData;
    
    mxfListenerData.mxfFile = this;
    
    mxfListener.data = &mxfListenerData;
    mxfListener.accept_frame = accept_frame;
    mxfListener.allocate_buffer = allocate_buffer;
    mxfListener.deallocate_buffer = deallocate_buffer;
    mxfListener.receive_frame = receive_frame;
    
    int result = read_next_frame(_mxfReader, &mxfListener);
    if (result != 1)
    {
        return false;
    }
    
    // get the VITC and LTC contained in the System Item
    MXFTimecode mxfTimecode;
    int mxfTimecodeType;
    int count;
    int numSourceTimecodes = get_num_source_timecodes(_mxfReader);
    int i;
    for (i = 0; i < numSourceTimecodes; i++)
    {
        if (get_source_timecode(_mxfReader, i, &mxfTimecode, &mxfTimecodeType, &count) == 1)
        {
            if (mxfTimecodeType == SYSTEM_ITEM_TC_ARRAY_TIMECODE)
            {
                // VITC is first, followed by LTC
                if (count == 0)
                {
                    _frame.vitc.dropFrame = 0;
                    _frame.vitc.hour = mxfTimecode.hour;
                    _frame.vitc.min = mxfTimecode.min;
                    _frame.vitc.sec = mxfTimecode.sec;
                    _frame.vitc.frame = mxfTimecode.frame;
                }
                else if (count == 1)
                {
                    _frame.ltc.dropFrame = 0;
                    _frame.ltc.hour = mxfTimecode.hour;
                    _frame.ltc.min = mxfTimecode.min;
                    _frame.ltc.sec = mxfTimecode.sec;
                    _frame.ltc.frame = mxfTimecode.frame;
                }
            }
        }
    }
    
    frame = &_frame;
    return true;    
}

int D3MXFFile::accept_frame(MXFReaderListener* mxfListener, int trackIndex)
{
    D3MXFFile* mxfFile = mxfListener->data->mxfFile;
    
    if (trackIndex == mxfFile->_videoTrack)
    {
        return 1;
    }
    else
    {
        int i;
        for (i = 0; i < mxfFile->_frame.numAudioTracks; i++)
        {
            if (trackIndex == mxfFile->_audioTrack[i])
            {
                return 1;
            }
        }
    }
    return 0;
}

int D3MXFFile::allocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer, uint32_t bufferSize)
{
    D3MXFFile* mxfFile = mxfListener->data->mxfFile;
    
    if (trackIndex == mxfFile->_videoTrack)
    {
        *buffer = mxfFile->_frame.video;
        return 1;
    }
    else
    {
        int i;
        for (i = 0; i < mxfFile->_frame.numAudioTracks; i++)
        {
            if (trackIndex == mxfFile->_audioTrack[i])
            {
                *buffer = mxfFile->_frame.audio[i];
                return 1;
            }
        }
    }

    return 0;
}

void D3MXFFile::deallocate_buffer(MXFReaderListener* mxfListener, int trackIndex, uint8_t** buffer)
{
    // do nothing
}

int D3MXFFile::receive_frame(MXFReaderListener* mxfListener, int trackIndex, uint8_t* buffer, uint32_t bufferSize)
{
    return 1;
}


