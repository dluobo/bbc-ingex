/*
 * $Id: QCSessionFile.h,v 1.1 2008/07/08 16:25:22 philipn Exp $
 *
 * Provides access to information contained in a qc_player session file
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

#ifndef __QC_SESSION_FILE_H__
#define __QC_SESSION_FILE_H__


#include <string>
#include <vector>


// NOTE: these must match the values defined or passed int through command-line in qc_player.c
// TODO: write configuration to session file and parse here

#define M0_MARK_TYPE                0x00000001
#define M1_MARK_TYPE                0x00000002
#define M2_MARK_TYPE                0x00000004
#define M3_MARK_TYPE                0x00000008
#define M4_MARK_TYPE                0x00000010
#define M5_MARK_TYPE                0x00000020
#define M6_MARK_TYPE                0x00000040
#define M7_MARK_TYPE                0x00000080
#define M8_MARK_TYPE                0x00000100
#define D3_VTR_ERROR_MARK_TYPE      0x00010000
#define D3_PSE_FAILURE_MARK_TYPE    0x00020000

#define CLIP_MARKER_MARK_TYPE       (M0_MARK_TYPE)
#define TRANSFER_MARK_TYPE          (M1_MARK_TYPE)
#define HISTORIC_FAULT_MARK_TYPE    (M2_MARK_TYPE)
#define VIDEO_FAULT_MARK_TYPE       (M3_MARK_TYPE)
#define AUDIO_FAULT_MARK_TYPE       (M4_MARK_TYPE)




class Mark
{
public:
    Mark() : position(-1), type(-1), duration(0) {}
    virtual ~Mark() {}
    
    int64_t position;
    int type;
    int64_t duration;
};


class QCSessionFile
{
public:
    QCSessionFile(std::string filename);
    ~QCSessionFile();
    
    std::string getFilename() { return _filename; }
    std::string getLoadedSessionFilename() { return _loadedSessionFilename; }
    std::string getOperator() { return _operator; }
    std::string getStation() { return _station; }
    std::string getComments() { return _comments; }
    bool requireRetransfer() { return _requireRetransfer; }
    Mark getProgrammeClip() { return _programmeClip; }
    const std::vector<Mark>& getUserMarks() { return _userMarks; }
    
private:
    std::string _filename;
    std::string _loadedSessionFilename;
    std::string _operator;
    std::string _station;
    std::string _comments;
    bool _requireRetransfer;
    Mark _programmeClip;
    std::vector<Mark> _userMarks;
};






#endif


