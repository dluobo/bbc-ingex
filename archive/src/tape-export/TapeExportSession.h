/*
 * $Id: TapeExportSession.h,v 1.1 2008/07/08 16:26:15 philipn Exp $
 *
 * Manages a tape export session
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
 
#ifndef __RECORDER_TAPE_EXPORT_SESSION_H__
#define __RECORDER_TAPE_EXPORT_SESSION_H__

#include <set>

#include "Cache.h"
#include "DatabaseThings.h"
#include "Threads.h"

#include <archive_types.h>


namespace rec
{

typedef enum
{
    UNKNOWN_TAPE_DEV_STATE = 0,
    READY_TAPE_DEV_STATE,
    BUSY_TAPE_DEV_STATE,
    NO_TAPE_TAPE_DEV_STATE,
    BAD_TAPE_TAPE_DEV_STATE
} TapeDeviceStatus;
    
    
class LTOFile
{
public:
    LTOFile() : identifier(-1), status(-1), size(-1), duration(-1) {}
    
    long identifier;
    int status;
    std::string name;
    std::string cacheName; // name of file in cache
    int64_t size;
    int64_t duration;
    std::string sourceSpoolNo;
    uint32_t sourceItemNo;
    std::string sourceProgNo;
    std::string sourceMagPrefix;
    std::string sourceProdCode;
    Timestamp transferStarted;
    Timestamp transferEnded;
};
    

class LTOContents
{
public:
    LTOContents() : ltoStatusChangeCount(0) {}
    
    std::vector<LTOFile> ltoFiles;
    int ltoStatusChangeCount;
};
    
class SessionStatus
{
public:
    SessionStatus()
    : autoTransferMethod(false), startedTransfer(false), totalSize(0), numFiles(0), 
        diskSpace(0), enableAbort(false), abortBusy(false), ltoStatusChangeCount(0),
        tapeDevStatus(UNKNOWN_TAPE_DEV_STATE), maxTotalSize(0), minTotalSize(0), maxFiles(0)
    {}
    
    std::string barcode;
    bool autoTransferMethod;
    bool autoSelectionComplete;
    bool startedTransfer;
    int64_t totalSize;
    int numFiles;
    int64_t diskSpace;
    bool enableAbort;
    bool abortBusy;
    int ltoStatusChangeCount;
    TapeDeviceStatus tapeDevStatus;
    std::string tapeDevDetailedStatus;
    std::string sessionStatusString;
    int64_t maxTotalSize;
    int64_t minTotalSize;
    int maxFiles;
};


class TapeExport;

class TapeExportSession : public ThreadWorker
{
public:
    friend class TapeExport;
    
public:
    // automatic transfer session constructor
    TapeExportSession(TapeExport* tapeExport, std::string barcode, 
        int64_t maxTotalSize, int64_t minTotalSize, int maxFiles);
    // manual transfer session contructor
    TapeExportSession(TapeExport* tapeExport, std::string barcode, std::vector<long> itemIds, int64_t maxTotalSize);
    virtual ~TapeExportSession();
    
    virtual void start();
    virtual void stop();
    virtual bool hasStopped() const;
    
    void abort(bool fromUser, std::string comments);
    
    LTOContents* getLTOContents();
    
    bool isFileUsedInSession(std::string name);
    std::set<std::string> getSessionFileNames();
    
    SessionStatus getStatus();
    
private:
    // called by Recorder just before deleting the session
    std::string getBarcode();
    bool wasAborted();
    bool wasUserAbort();
    LTOTransferSessionTable* getSessionTable();

    bool completeInternal(std::string comments);
    bool abortInternal(bool fromUser, std::string comments);
    
    void getLTOFileStats(int64_t* totalSize, int* numFiles);
    bool processCacheItems();
    bool processCacheItems(std::vector<long>& itemIds);
    bool checkAndSaveTransferSession();
    bool createLTOFiles(std::vector<CacheContentItem>& itemsForTransfer);
    void updateTransferStatus();
    void getInfaxData(LTOFileTable* ltoFileTable, InfaxData* infax);    
    
    
    TapeExport* _tapeExport;
    std::string _barcode;
    std::vector<long> _itemIds;
    int64_t _maxTotalSize;
    int64_t _minTotalSize;
    int _maxFiles;
    bool _autoTransferMethod;
    
    
    // session status
    bool _startedTransfer;
    bool _completed;
    bool _aborted;
    bool _userAborted;
    bool _abortBusy;
    bool _enableAbort;
    std::string _sessionStatusString;
    bool _doneInitialCheck;
    bool _autoSelectionComplete;
    
    // session thread
    bool _stopThread;
    bool _threadHasStopped;
    
    // session control
    Mutex _controlMutex;    
    bool _abortCalled;
    bool _abortFromUser;
    std::string _abortComments;
    
    LTOTransferSessionTable* _sessionTable;
    Mutex _sessionTableMutex;
    int _ltoStatusChangeCount;
    
};

};



#endif


