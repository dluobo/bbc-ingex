/*
 * $Id: TapeExport.h,v 1.1 2008/07/08 16:26:13 philipn Exp $
 *
 * Provides main access to the tape export application
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
 
#ifndef __RECORDER_TAPE_EXPORT_H__
#define __RECORDER_TAPE_EXPORT_H__


#include <string>
#include <set>

#include "TapeExportSession.h"
#include "Cache.h"
#include "tape.h"


// return failure codes for startNewSession
#define SESSION_IN_PROGRESS_FAILURE     1
#define INVALID_BARCODE_FAILURE         2
#define EMPTY_ITEM_LIST_FAILURE         3
#define MAX_SIZE_EXCEEDED_FAILURE       4
#define ZERO_SIZE_FAILURE               5
#define UNKNOWN_ITEM_FAILURE            6
#define TAPE_NOT_READY_FAILURE          7
#define BARCODE_USED_BEFORE_FAILURE     8
#define INTERNAL_FAILURE                10

// size of page files used to record multi-item tapes; equivalent to approx. 1 min recording
#define MXF_PAGE_FILE_SIZE                  (1313622000LL)

// more than 2 * MXF_PAGE_FILE_SIZE + approx 10 seconds space required to start or continue an ingest session
#define DISK_SPACE_MARGIN                   (2 * MXF_PAGE_FILE_SIZE + 218937000LL)


namespace rec
{

class TapeExportStatus
{
public:
    TapeExportStatus() : databaseOk(false), tapeDeviceOk(false), sessionInProgress(false),
        maxTotalSize(0), minTotalSize(0), maxFiles(0)
    {}
    
    std::string recorderName;
    bool databaseOk;
    bool tapeDeviceOk;
    bool sessionInProgress;
    TapeDeviceStatus tapeDevStatus;
    std::string tapeDevDetailedStatus;
    bool readyToExport;
    int64_t maxTotalSize;
    int64_t minTotalSize;
    int maxFiles;
};
    
class TapeExportSystemStatus
{
public:
    TapeExportSystemStatus() : remDiskSpace(0), remDuration(0) {}
    
    int64_t remDiskSpace;
    int64_t remDuration;
    TapeDeviceStatus tapeDevStatus;
};

class TapeDeviceDetails
{
public:
    std::string vendor;
    std::string model;
    std::string revision;
};



class TapeExport
{
public:
    friend class TapeExportSession;
    
public:
    TapeExport(std::string tapeDevice, std::string recorderName, std::string cacheDirectory);
    virtual ~TapeExport();
    
    // returns 0 when successful; else see failure codes above
    int startNewAutoSession(std::string barcode, TapeExportSession** session);
    int startNewManualSession(std::string barcode, std::vector<long> itemIds, TapeExportSession** session);
    bool haveSession();

    void abortSession(bool fromUser, std::string comments);

    LTOContents* getLTOContents();
    
    bool isFileUsedInSession(std::string name);
    std::set<std::string> getSessionFileNames();
    
    
    SessionStatus getSessionStatus();

    bool getLastSessionStatus(std::string* barcode, bool* completed, bool* userAbort);    
    
    TapeExportStatus getStatus();
    TapeExportSystemStatus getSystemStatus();
    
    int64_t getRemainingDiskSpace();
    
    long getRecorderId();
    
    Cache* getCache();
    ::Tape* getTape();
    
    TapeDeviceDetails getTapeDeviceDetails();
    
    bool isLTOBarcode(string barcode);
    
private:
    // called by TapeExportSession 
    void sessionDone();

    void checkSessionStatus();
    void readTapeDeviceDetails(std::string devicePath);
    
    std::string _recorderName;
    long _recorderId;
    int64_t _maxTotalSize;
    int64_t _minTotalSize;
    int _maxFiles;
    std::vector<std::string> _ltoBarcodePrefixes;
    int64_t _remDurationFactor;
    
    
    Cache* _cache;
    
    ReadWriteLock _sessionRWLock;
    bool _sessionDone;
    TapeExportSession* _session;
    Thread* _sessionThread;
    
    std::string _lastSessionBarcode;
    bool _lastSessionCompleted;
    bool _lastSessionUserAbort;
    LTOTransferSessionTable* _lastSessionTable;
    
    ::Tape* _tape;
  
    TapeDeviceDetails _tapeDeviceDetails;
};


};





#endif

