/*
 * $Id: D3MXFFile.h,v 1.1 2008/07/08 16:25:20 philipn Exp $
 *
 * Provides access to the header metadata and timecode contained in a D3 MXF file
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

#ifndef __D3_MXF_FILE_H__
#define __D3_MXF_FILE_H__


#include <string>
#include <map>
#include <vector>

#include <d3_mxf_info_lib.h>


class Timecode
{
public:
    Timecode() 
        : hour(0), min(0), sec(0), frame(0) {}
    Timecode(const Timecode& tc) 
        : hour(tc.hour), min(tc.min), sec(tc.sec), frame(tc.frame) {}
    
    int hour;
    int min;
    int sec;
    int frame;
};

class TimecodeGroup
{
public:
    Timecode ctc;
    Timecode vitc;
    Timecode ltc;
};


class D3MXFFile
{
public:    
    D3MXFFile(std::string filepath);
    ~D3MXFFile();

    void completeTimecodeInfo(TimecodeGroup& timecodes);

    bool isComplete() { return _isComplete; }
    std::string getFilename() { return _filename; }
    int64_t getMXFDuration() { return _mxfDuration; }
    const InfaxData& getD3InfaxData() { return _d3InfaxData; } 
    const InfaxData& getLTOInfaxData() { return _ltoInfaxData; }
    const std::vector<PSEFailure>& getPSEFailures() { return _pseFailures; }
    
private:
    void completePSETimecodeInfo(PSEFailure* pseFailure);
    
    bool _isComplete;
    
    FILE* _mxfFile;
    int64_t _startFilePosition;
    int64_t _contentPackageSize;
    
    std::string _filename;
    
    int64_t _mxfDuration;
    InfaxData _d3InfaxData;
    InfaxData _ltoInfaxData;
    
    std::vector<PSEFailure> _pseFailures;
};






#endif


