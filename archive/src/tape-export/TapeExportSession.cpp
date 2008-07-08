/*
 * $Id: TapeExportSession.cpp,v 1.1 2008/07/08 16:26:10 philipn Exp $
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
 
/*
    A tape export session is either manual or automatic. A manual session is one
    where the user has selected the files for transfer to LTO. An automatic
    session is one where this class selects the files based on the maximum 
    number of files and maximum LTO size configuration options.
    
    A session can be aborted by the user or system.
    
    Note/TODO: aborting a session will not stop the transfer of a file from 
    disk to tape straight away, or stop a tape rewind. The next session can only 
    be started when the transfer or tape rewind completes. 
*/
 
// to get the PRI64d etc. macros
#define __STDC_FORMAT_MACROS 1

#include <set>

#include "TapeExportSession.h"
#include "TapeExport.h"
#include "RecorderDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"
#include "Timing.h"
#include "FileLock.h"


using namespace std;
using namespace rec;


// filename on LTO is 'LTO barcode' + <count> + '.txt'
static string get_filename_on_lto(string barcode, int count)
{
    char buf[32];
    sprintf(buf, "%02d.mxf", count);
    
    return barcode + buf;
}



// utility class to set and reset busy state

#define GUARD_BUSY_STATE(bs)      BusyGuard __bsguard(bs)

class BusyGuard
{
public:
    BusyGuard(bool& busy)
    : _busy(busy)
    {
        _busy = true;
    }
    ~BusyGuard()
    {
        _busy = false;
    }
    
private:
    bool& _busy;
};


// utility class which resets the status string to empty if it is not explicitly released

#define GUARD_STATUS_STRING(ss, init)      GuardStatusString __ssguard(ss, init)
#define RELEASE_SS_GUARD()                  __ssguard.release()

class GuardStatusString
{
public:
    GuardStatusString(string& statusString, string initialValue)
    : _statusString(statusString), _released(false)
    {
        _statusString = initialValue;
    }
    ~GuardStatusString()
    {
        if (!_released)
        {
            _statusString = "";
        }
    }
    
    void release()
    {
        _released = true;
    }
private:
    string& _statusString;
    bool _released;
};




TapeExportSession::TapeExportSession(TapeExport* tapeExport, string barcode, 
    int64_t maxTotalSize, int64_t minTotalSize, int maxFiles)
: _tapeExport(tapeExport), _barcode(barcode), _maxTotalSize(maxTotalSize), _minTotalSize(minTotalSize),
_maxFiles(maxFiles), _autoTransferMethod(true),
_startedTransfer(false), _completed(false), _aborted(false), _userAborted(false), _abortBusy(false), 
_enableAbort(false), _doneInitialCheck(false), _autoSelectionComplete(false),
_stopThread(false), _threadHasStopped(false),
_abortCalled(false), _ltoStatusChangeCount(0)
{
    // source must have a barcode
    REC_CHECK(barcode.size() > 0);
    
    TapeDeviceDetails tapeDeviceDetails = tapeExport->getTapeDeviceDetails();
    
    _sessionTable = new LTOTransferSessionTable();
    _sessionTable->recorderId = _tapeExport->getRecorderId();
    _sessionTable->status = LTO_TRANSFER_SESSION_STARTED;
    _sessionTable->tapeDeviceVendor = tapeDeviceDetails.vendor;
    _sessionTable->tapeDeviceModel = tapeDeviceDetails.model;
    _sessionTable->tapeDeviceRevision = tapeDeviceDetails.revision;
    
    _sessionTable->lto = new LTOTable();
    _sessionTable->lto->spoolNo = barcode;
    
    Logging::info("A new automatic tape transfer session was created for LTO tape '%s'\n", barcode.c_str());
}

TapeExportSession::TapeExportSession(TapeExport* tapeExport, std::string barcode, std::vector<long> itemIds, int64_t maxTotalSize)
: _tapeExport(tapeExport), _barcode(barcode), _itemIds(itemIds), _maxTotalSize(maxTotalSize), _minTotalSize(0),
_maxFiles(0), _autoTransferMethod(false),
_startedTransfer(false), _completed(false), _aborted(false), _userAborted(false), _abortBusy(false), 
_enableAbort(false), _doneInitialCheck(false), _autoSelectionComplete(false),
_stopThread(false), _threadHasStopped(false),
_abortCalled(false), 
_sessionTable(0), _ltoStatusChangeCount(0)
{
    // source must have a barcode
    REC_CHECK(barcode.size() > 0);
    
    TapeDeviceDetails tapeDeviceDetails = tapeExport->getTapeDeviceDetails();
    
    _sessionTable = new LTOTransferSessionTable();
    _sessionTable->recorderId = _tapeExport->getRecorderId();
    _sessionTable->status = LTO_TRANSFER_SESSION_STARTED;
    _sessionTable->tapeDeviceVendor = tapeDeviceDetails.vendor;
    _sessionTable->tapeDeviceModel = tapeDeviceDetails.model;
    _sessionTable->tapeDeviceRevision = tapeDeviceDetails.revision;
    
    _sessionTable->lto = new LTOTable();
    _sessionTable->lto->spoolNo = barcode;
    
    Logging::info("A new manual tape transfer session was created for LTO tape '%s'\n", barcode.c_str());
}

TapeExportSession::~TapeExportSession()
{
    delete _sessionTable;
}

void TapeExportSession::start()
{
    GUARD_THREAD_START(_threadHasStopped);
    _stopThread = false;
    
    // enable abort now that the thread has started
    _enableAbort = true;
    
    Timer controlTimer;
    bool abortSession = false;
    bool abortSessionFromUser = false;
    string abortComments;
    
    bool exceptionAbort = false;
    int state = _autoTransferMethod ? 0 : 1;
    try
    {
        auto_ptr<FileLock> tapeFileLock(new FileLock(Config::getString("tape_transfer_lock_file"), true));
        
        controlTimer.start(100 * MSEC_IN_USEC);
        
        while (!_stopThread)
        {
            
            bool transferComplete = false;
            switch (state)
            {
                case 0:
                    {
                        GUARD_STATUS_STRING(_sessionStatusString, "Checking cache");
                        
                        // Automatic transfer method
                        if (processCacheItems())
                        {
                            // ready to transfer files
                            state = 2;
                            _autoSelectionComplete = true;
                        }
                        else
                        {
                            RELEASE_SS_GUARD(); // keep session status string
                        }
                    }
                    _doneInitialCheck = true;
                    break;
                    
                case 1:
                    {
                        GUARD_STATUS_STRING(_sessionStatusString, "Checking items");
    
                        // Manual transfer method                    
                        if (processCacheItems(_itemIds))
                        {
                            // ready to transfer files
                            state = 2;
                        }
                        else
                        {
                            abort(false, "Invalid set of items");
                        }
                    }
                    _doneInitialCheck = true;
                    break;
                    
                case 2:
                    // check if transfer can be started
                    {
                        GUARD_STATUS_STRING(_sessionStatusString, "Waiting for tape ready");
    
                        GeneralStats stats;
                        _tapeExport->getTape()->get_general_stats(&stats);
                        
                        if (stats.tape_state == READY)
                        {
                            _sessionStatusString = "Final check of files";
                            
                            if (!checkAndSaveTransferSession())
                            {
                                if (_autoTransferMethod)
                                {
                                    // go back to state 0
                                    Logging::warning("Failed to create transfer session database entries just prior to transfer\n");
                                    state = 0;
                                    {
                                        LOCK_SECTION(_sessionTableMutex);
                                        _sessionTable->lto->ltoFiles.clear();
                                        _ltoStatusChangeCount++;
                                    }
                                    _autoSelectionComplete = false;
                                    break;
                                }
                                else
                                {
                                    abort(false, "Failed to create transfer session database entries just prior to transfer");
                                    break;
                                }
                            }

                            
                            _sessionStatusString = "Starting transfer to tape";
                            
                            // create the file list for the tape transfer system
                            TapeFileInfoList fileList;
                            {
                                LOCK_SECTION(_sessionTableMutex);
                                
                                vector<LTOFileTable*>::const_iterator iter;
                                for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
                                {
                                    TapeFileInfo info;
                                    info.filename = (*iter)->hddName;
                                    info.location = _tapeExport->getCache()->getPath();
                                    getInfaxData(*iter, &info.infax_data);
                                    fileList.push_back(info);
                                }
                            }
                                
                            if (!_tapeExport->getTape()->store_to_tape(fileList, _barcode.c_str()))
                            {
                                Logging::error("Failed to start transferring files to tape\n");
                                abort(false, "Failed to start transferring files to tape");
                                break;
                            }

                            state = 3;
                            _startedTransfer = true;
                        }
                        else
                        {
                            RELEASE_SS_GUARD(); // keep the session status string
                        }
                    }
                    break;
                    
                case 3:
                    {
                        GUARD_STATUS_STRING(_sessionStatusString, "Transferring to tape");
                        
                        updateTransferStatus();
                        
                        if (_tapeExport->getTape()->store_completed())
                        {
                            // completed or failed
                            transferComplete = true;
                            break;
                        }

                        RELEASE_SS_GUARD(); // keep session status string
                    }
                    break;                    
                    
                    
                default:
                    break;
            }

            // make sure we signal that the transfer is busy by locking the tape lock file
            if (state == 3 && !transferComplete)
            {
                tapeFileLock->lock(false);
            }
            else
            {
                tapeFileLock->unlock();
            }

            
            // stop if the transfer is completed with success of failure 
            if (transferComplete)
            {
                updateTransferStatus();
                    
                StoreStats stats;
                _tapeExport->getTape()->get_store_stats(&stats);
                if (stats.store_state == COMPLETED)
                {
                    GUARD_STATUS_STRING(_sessionStatusString, "Complete tape transfer");
                    completeInternal("");
                }
                else // FAILED
                {
                    GUARD_STATUS_STRING(_sessionStatusString, "Abort tape transfer");
                    abortInternal(false, "System aborted session: " + stats.store_filename);
                }
                
                // exit loop
                break;
            }
            
            
            // check if user/system has aborted the session
            {
                LOCK_SECTION(_controlMutex);
                
                abortSession = _abortCalled;
                abortSessionFromUser = _abortFromUser;
                abortComments = _abortComments;
            }
            if (abortSession)
            {
                GUARD_STATUS_STRING(_sessionStatusString, "Abort tape transfer");
                
                if (state == 3)
                {
                    if (!_tapeExport->getTape()->abort_store())
                    {
                        Logging::warning("Failed to abort transfer to tape\n");
                    }
                    updateTransferStatus();
                }
                abortInternal(abortSessionFromUser, abortComments);
                
                // exit loop
                break;
            }

            // don't hog the CPU
            sleep_msec(1);
            
            // check for control input every 100msec
            controlTimer.sleepRemainder();
            controlTimer.start(100 * MSEC_IN_USEC);
        }
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Exception thrown in transfer session control thread: %s\n", ex.getMessage().c_str());
        Logging::errorMore(true, "Aborting session\n");
        exceptionAbort = true;
    }
    catch (...)
    {
        Logging::error("Exception thrown in transfer session control thread - aborting session\n");
        exceptionAbort = true;
    }
    
    _startedTransfer = false;
    
    
    // abort if an exception was thrown
    if (exceptionAbort && _tapeExport->_session != 0)
    {
        try
        {
            GUARD_STATUS_STRING(_sessionStatusString, "Abort tape transfer");
            abortInternal(false, "System abort");
        }
        catch (const RecorderException& ex)
        {
            Logging::error("Failed to abort session after exception was thrown: %s\n", ex.getMessage().c_str());
        }
        catch (...)
        {
            Logging::error("Failed to abort session after exception was thrown\n");
        }
    }
    
    // inform tape export that we are done
    _tapeExport->sessionDone();
}
    
void TapeExportSession::abort(bool fromUser, string comments)
{
    LOCK_SECTION(_controlMutex);
    
    if (!_abortCalled)
    {
        _abortCalled = true;
        _abortFromUser = fromUser;
        _abortComments = comments;
    }
}

SessionStatus TapeExportSession::getStatus()
{
    GeneralStats generalStats;
    _tapeExport->getTape()->get_general_stats(&generalStats);
    
    SessionStatus status;

    status.barcode = _barcode;
    status.autoTransferMethod = _autoTransferMethod;
    status.autoSelectionComplete = _autoSelectionComplete;
    status.startedTransfer = _startedTransfer;
    if (_autoTransferMethod)
    {
        if (_autoSelectionComplete)
        {
            // return lto stats
            getLTOFileStats(&status.totalSize, &status.numFiles);
        }
        else
        {
            // return cache stats
            CacheStatus cacheStats = _tapeExport->getCache()->getStatus();
            status.totalSize = cacheStats.totalSize;
            status.numFiles = cacheStats.numItems;
        }
    }
    else
    {
        if (_doneInitialCheck) // the lto file list is only compiled after the initial check
        {
            // LTO file list is ready
            getLTOFileStats(&status.totalSize, &status.numFiles);
        }
        else
        {
            // LTO file list is not ready
            status.totalSize = 0;
            status.numFiles = 0;
        }
    }
    status.diskSpace = _tapeExport->getRemainingDiskSpace();
    status.enableAbort = !_aborted;
    status.abortBusy = _abortBusy;
    status.enableAbort = _enableAbort;
    status.ltoStatusChangeCount = _ltoStatusChangeCount;
    switch (generalStats.tape_state)
    {
        case READY:
            status.tapeDevStatus = READY_TAPE_DEV_STATE;
            break;
        case BUSY:
            status.tapeDevStatus = BUSY_TAPE_DEV_STATE;
            break;
        case NO_TAPE:
            status.tapeDevStatus = NO_TAPE_TAPE_DEV_STATE;
            break;
        case BAD_TAPE:
            status.tapeDevStatus = BAD_TAPE_TAPE_DEV_STATE;
            break;
        default:
            status.tapeDevStatus = UNKNOWN_TAPE_DEV_STATE;
            break;
    }
    status.tapeDevDetailedStatus = generalStats.tape_detail;
    status.sessionStatusString = _sessionStatusString;
    status.maxTotalSize = _maxTotalSize;
    status.minTotalSize = _minTotalSize;
    status.maxFiles = _maxFiles;
    
    return status;
}

string TapeExportSession::getBarcode()
{
    return _barcode;
}

bool TapeExportSession::wasAborted()
{
    return _aborted;
}

bool TapeExportSession::wasUserAbort()
{
    if (!_aborted)
    {
        return false;
    }
    return _userAborted;
}

LTOTransferSessionTable* TapeExportSession::getSessionTable()
{
    LTOTransferSessionTable* ret = _sessionTable;
    _sessionTable = 0;
    return ret;
}

void TapeExportSession::stop()
{
    _stopThread = true;
}
    
bool TapeExportSession::hasStopped() const
{
    return _threadHasStopped;
}

LTOContents* TapeExportSession::getLTOContents()
{
    LOCK_SECTION(_sessionTableMutex);
    
    auto_ptr<LTOContents> contents(new LTOContents());
    contents->ltoStatusChangeCount = _ltoStatusChangeCount;

    vector<LTOFileTable*>::const_iterator iter;
    for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
    {
        LTOFile ltoFile;
        ltoFile.identifier = (*iter)->databaseId;
        ltoFile.status = (*iter)->status;
        ltoFile.name = (*iter)->name;
        ltoFile.cacheName = (*iter)->hddName;
        ltoFile.size = (*iter)->size;
        ltoFile.duration = (*iter)->realDuration;
        ltoFile.sourceSpoolNo = (*iter)->sourceSpoolNo;
        ltoFile.sourceItemNo = (*iter)->sourceItemNo;
        ltoFile.sourceProgNo = (*iter)->sourceProgNo;
        ltoFile.sourceMagPrefix = (*iter)->sourceMagPrefix;
        ltoFile.sourceProdCode = (*iter)->sourceProdCode;
        ltoFile.transferStarted = (*iter)->transferStarted;
        ltoFile.transferEnded = (*iter)->transferEnded;
        
        contents->ltoFiles.push_back(ltoFile);
    }
    
    return contents.release();
}

bool TapeExportSession::isFileUsedInSession(string name)
{
    LOCK_SECTION(_sessionTableMutex);
    
    vector<LTOFileTable*>::const_iterator iter;
    for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
    {
        if ((*iter)->hddName.compare(name) == 0)
        {
            return true;
        }
    }
    
    return false;
}

set<string> TapeExportSession::getSessionFileNames()
{
    LOCK_SECTION(_sessionTableMutex);
    
    set<string> result;
    
    vector<LTOFileTable*>::const_iterator iter;
    for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
    {
        result.insert((*iter)->hddName);
    }
    
    return result;
}


bool TapeExportSession::completeInternal(string comments)
{
    if (_completed)
    {
        Logging::warning("Attempted to complete a transfer in a session that has already completed\n");
        return false;
    }
    if (_aborted)
    {
        Logging::warning("Attempted to complete a transfer in a session that has already been aborted\n");
        return false;
    }
    

    _completed = true;
    Logging::info("Completed session\n");
    
    // update the session status in the database
    try
    {
        LOCK_SECTION(_sessionTableMutex);
        
        // update session table
        _sessionTable->status = LTO_TRANSFER_SESSION_COMPLETED;
        _sessionTable->comments = comments;
        RecorderDatabase::getInstance()->updateLTOTransferSession(_sessionTable);
    }
    catch (...)
    {
        Logging::error("Failed to update lto transfer session status to completed\n");
    }
    
    // save infax export for each LTO file
    {
        LOCK_SECTION(_sessionTableMutex);
        
        vector<LTOFileTable*>::const_iterator iter;
        for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
        {
            try
            {
                RecorderDatabase::getInstance()->saveInfaxExport(&(*iter)->infaxExport);
            }
            catch (...)
            {
                Logging::error("Failed to save Infax export to database for LTO file '%s'\n", (*iter)->name.c_str());
            }
        }
    }
    
        
    

    // remove the files
    if (Config::getBool("dbg_keep_lto_files"))
    {
        Logging::warning("Deletion of files successfully transferred to lto is disabled\n");
    }
    else
    {
        LOCK_SECTION(_sessionTableMutex);
        
        vector<LTOFileTable*>::const_iterator iter;
        for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
        {
            try
            {
                if (_tapeExport->getCache()->removeItem((*iter)->hddName))
                {
                    Logging::info("Deleted '%s' after successfully transferring to LTO\n", (*iter)->hddName.c_str());
                }
                else
                {
                    Logging::warning("Failed to delete '%s' after successfully transferring to LTO\n", (*iter)->hddName.c_str());
                }
            }
            catch (...)
            {
                Logging::warning("Failed to delete '%s' after successfully transferring to LTO\n", (*iter)->hddName.c_str());
            }
        }
    }
    
    return true;
}

bool TapeExportSession::abortInternal(bool fromUser, string comments)
{
    GUARD_BUSY_STATE(_abortBusy);
    
    if (_completed)
    {
        Logging::warning("Attempted to abort a transfer in a session that has already completed\n");
        return false;
    }
    if (_aborted)
    {
        Logging::warning("Attempted to abort a transfer in a session that has already been aborted\n");
        return false;
    }
    
    _aborted = true;
    _userAborted = fromUser;

    if (fromUser)
    {
        Logging::info("User aborted session\n");
    }
    else
    {
        Logging::info("System aborted session ('%s')\n", comments.c_str());
    }

    // update the transfer session status in the database
    try
    {    
        _sessionTable->status = LTO_TRANSFER_SESSION_ABORTED;
        _sessionTable->abortInitiator = fromUser ? USER_ABORT_SESSION : SYSTEM_ABORT_SESSION;
        _sessionTable->comments = comments;
        RecorderDatabase::getInstance()->updateLTOTransferSession(_sessionTable);
    }
    catch (...)
    {
        Logging::error("Failed to update lto transfer session status to failed\n");
    }


    return true;
}

void TapeExportSession::getLTOFileStats(int64_t* totalSize, int* numFiles)
{
    LOCK_SECTION(_sessionTableMutex);
    
    *numFiles = (int)_sessionTable->lto->ltoFiles.size();
    
    *totalSize = 0;
    vector<LTOFileTable*>::const_iterator iter;
    for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
    {
        *totalSize += (*iter)->size;
    }
}

bool TapeExportSession::processCacheItems(vector<long>& itemIds)
{
    vector<CacheContentItem> itemsForTransfer;
    
    // matche the item ids with items in the cache
    
    set<long> itemIdsMap(itemIds.begin(), itemIds.end());
    auto_ptr<CacheContents> contents(_tapeExport->getCache()->getContents());
    int64_t totalSize = 0;
    vector<CacheContentItem*>::const_iterator iter;
    for (iter = contents->items.begin(); iter != contents->items.end(); iter++)
    {
        CacheContentItem* item = *iter;
        
        if (item->sessionStatus != RECORDING_SESSION_COMPLETED)
        {
            // item is not ready for transfer
            continue;
        }
        
        if (itemIdsMap.find((*iter)->identifier) == itemIdsMap.end())
        {
            // item not selected for transfer
            continue;
        }
        
        itemsForTransfer.push_back(*item);
        totalSize += item->size;
    }
    
    // check all is ok
    if (itemsForTransfer.size() != itemIdsMap.size() || 
        totalSize >= _maxTotalSize)
    {
        return false;
    }
    
    return createLTOFiles(itemsForTransfer);
}


static bool orderCacheItems1(const CacheContentItem* l, const CacheContentItem* r)
{
    return l->sessionCreation < r->sessionCreation ||
        (l->sessionCreation == r->sessionCreation && l->sourceItemNo < r->sourceItemNo);
}

bool TapeExportSession::processCacheItems()
{
    vector<CacheContentItem> itemsForTransfer;
    
    auto_ptr<CacheContents> contents(_tapeExport->getCache()->getContents());

    // sort items in ascending order of session creation date and ascending item no
    sort(contents->items.begin(), contents->items.end(), orderCacheItems1);
    
    // loop through cache and get candidate files for transfer
    int64_t totalSize = 0;
    bool maxSizeReached = false;
    bool maxFilesReached = false;
    vector<CacheContentItem*>::const_iterator iter;
    for (iter = contents->items.begin(); iter != contents->items.end(); iter++)
    {
        CacheContentItem* item = *iter;
        
        if (item->sessionStatus != RECORDING_SESSION_COMPLETED)
        {
            // item is not ready for transfer
            continue;
        }
        
        if (totalSize + item->size >= _maxTotalSize)
        {
            maxSizeReached = true;
            // we continue to see if any of the remaining files would fit
            continue;
        }
        
        itemsForTransfer.push_back(*item);
        totalSize += item->size;
        
        if ((int)itemsForTransfer.size() >= _maxFiles)
        {
            maxFilesReached = true;
            // we have enough files
            break;
        }
    }
    
    // return false if not ready for transfer
    if (itemsForTransfer.size() == 0 || 
        totalSize < _minTotalSize || 
        (!maxSizeReached && !maxFilesReached))
    {
        return false;
    }
    
    return createLTOFiles(itemsForTransfer);
}

bool TapeExportSession::checkAndSaveTransferSession()
{
    LOCK_SECTION(_sessionTableMutex);

    try
    {
        // check files are (still) present in the cache
        vector<LTOFileTable*>::const_iterator iter;
        for (iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++)
        {
            if (!_tapeExport->getCache()->itemExists((*iter)->hddName))
            {
                Logging::warning("Item '%s' is not present in the cache\n", (*iter)->hddName.c_str());
                return false;
            }
        }
    
        // save to the database
        RecorderDatabase::getInstance()->saveLTOTransferSession(_sessionTable);
        
        return true;
    }
    catch (...)
    {
        return false;
    }

}


static bool orderCacheItems2(const CacheContentItem& l, const CacheContentItem& r)
{
    return l.sessionCreation < r.sessionCreation ||
        (l.sessionCreation == r.sessionCreation && l.sourceItemNo < r.sourceItemNo);
}

bool TapeExportSession::createLTOFiles(vector<CacheContentItem>& itemsForTransfer)
{
    LOCK_SECTION(_sessionTableMutex);

    try
    {
        _ltoStatusChangeCount++;
        
        // clear any existing files
        vector<LTOFileTable*>::const_iterator iter1;
        for (iter1 = _sessionTable->lto->ltoFiles.begin(); iter1 != _sessionTable->lto->ltoFiles.end(); iter1++)
        {
            delete *iter1;
        }
        _sessionTable->lto->ltoFiles.clear();

        
        // sort items in ascending order of session creation date and ascending item no
        sort(itemsForTransfer.begin(), itemsForTransfer.end(), orderCacheItems2);
        
        
        Date today = get_todays_date();
        int count = 1; // count 0 is for the index file, count 1 and up is for the mxf files
        LTOFileTable* ltoFileTable;
        vector<CacheContentItem>::const_iterator iter2;
        for (iter2 = itemsForTransfer.begin(); iter2 != itemsForTransfer.end(); iter2++, count++)
        {
            const CacheContentItem& item = (*iter2);
            
            _sessionTable->lto->ltoFiles.push_back(new LTOFileTable());
            ltoFileTable = _sessionTable->lto->ltoFiles.back();
           
            ltoFileTable->status = LTO_FILE_TRANSFER_STATUS_NOT_STARTED; 
            ltoFileTable->name = get_filename_on_lto(_barcode, count);
            ltoFileTable->size = item.size;
            ltoFileTable->realDuration = item.duration;
            
            auto_ptr<Source> source(RecorderDatabase::getInstance()->loadHDDSource(item.identifier));
            D3Source* d3Source = source->getD3Source(item.sourceItemNo);
            // TODO: would it be better to load the D3Source directly from the database?
            // we expect d3 sources to be unique identified by the spool number and item number
            REC_ASSERT(d3Source != 0);
            ltoFileTable->format = "LTO"; // change to LTO
            ltoFileTable->progTitle = d3Source->progTitle;
            ltoFileTable->episodeTitle = d3Source->episodeTitle;
            ltoFileTable->txDate = d3Source->txDate;
            ltoFileTable->magPrefix = d3Source->magPrefix;
            ltoFileTable->progNo = d3Source->progNo;
            ltoFileTable->prodCode = d3Source->prodCode;
            ltoFileTable->spoolStatus = "M"; // 'M' stands for miscelleaneous and in this case indicates it is a dub 
            ltoFileTable->stockDate = today; // LTO's stock date
            if (strncasecmp("programme", d3Source->spoolDescr.c_str(), strlen("programme")) == 0)
            {
                ltoFileTable->spoolDescr = "PROGRAMME (DUB OF " + d3Source->spoolNo + ")";
            }
            else
            {
                ltoFileTable->spoolDescr = "DUB OF " + d3Source->spoolNo;
            }
            ltoFileTable->spoolDescr = ltoFileTable->spoolDescr.substr(0, 29);
            ltoFileTable->memo = item.sessionComments;
            if (!d3Source->memo.empty())
            {
                if (!ltoFileTable->memo.empty())
                {
                    ltoFileTable->memo += "\n" + d3Source->memo;
                }
                else
                {
                    ltoFileTable->memo = d3Source->memo;
                }
            }
            ltoFileTable->memo = ltoFileTable->memo.substr(0, 120);
            ltoFileTable->duration = d3Source->duration;
            ltoFileTable->spoolNo = _barcode; // change to the LTO spool number
            ltoFileTable->accNo = ""; // don't know the accession number at this point
            ltoFileTable->catDetail = d3Source->catDetail;
            ltoFileTable->itemNo = d3Source->itemNo;
    
            ltoFileTable->recordingSessionId = item.sessionId;
            
            ltoFileTable->sourceSpoolNo = d3Source->spoolNo;
            ltoFileTable->sourceItemNo = d3Source->itemNo;
            ltoFileTable->sourceProgNo = d3Source->progNo;
            ltoFileTable->sourceMagPrefix = d3Source->magPrefix;
            ltoFileTable->sourceProdCode = d3Source->prodCode;
            
            auto_ptr<HardDiskDestination> hdd(RecorderDatabase::getInstance()->loadHDDest(item.identifier));
            ltoFileTable->hddHostName = hdd->hostName;
            ltoFileTable->hddPath = hdd->path;
            ltoFileTable->hddName = hdd->name;
            ltoFileTable->materialPackageUID = hdd->materialPackageUID;
            ltoFileTable->filePackageUID = hdd->filePackageUID;
            ltoFileTable->tapePackageUID = hdd->tapePackageUID;

            // get the digibeta destination through the recording session
            Destination* digiDest = 0;
            AutoPointerVector<Destination> dests(RecorderDatabase::getInstance()->loadSessionDestinations(item.sessionId));
            vector<Destination*>::const_iterator destIter;
            for (destIter = dests.get().begin(); destIter != dests.get().end(); destIter++)
            {
                if ((*destIter)->concreteDestination->getTypeId() == DIGIBETA_DEST_TYPE)
                {
                    digiDest = *destIter;
                    break;
                }
            }
            REC_ASSERT(digiDest != 0); // the system currently requires a digibeta destination
            
            ltoFileTable->infaxExport.transferDate = item.sessionCreation;
            ltoFileTable->infaxExport.d3SpoolNo = d3Source->spoolNo;
            ltoFileTable->infaxExport.d3ItemNo = d3Source->itemNo;
            ltoFileTable->infaxExport.progTitle = d3Source->progTitle;
            ltoFileTable->infaxExport.episodeTitle = d3Source->episodeTitle;
            ltoFileTable->infaxExport.magPrefix = d3Source->magPrefix;
            ltoFileTable->infaxExport.progNo = d3Source->progNo;
            ltoFileTable->infaxExport.prodCode = d3Source->prodCode;
            ltoFileTable->infaxExport.mxfName = ltoFileTable->name;
            ltoFileTable->infaxExport.digibetaSpoolNo = digiDest->barcode;
            ltoFileTable->infaxExport.browseName = hdd->browseName;
            ltoFileTable->infaxExport.ltoSpoolNo = _barcode;
        }
        
        return true;
    }
    catch (...)
    {
        // clear up
        try
        {
            vector<LTOFileTable*>::const_iterator iter1;
            for (iter1 = _sessionTable->lto->ltoFiles.begin(); iter1 != _sessionTable->lto->ltoFiles.end(); iter1++)
            {
                delete *iter1;
            }
            _sessionTable->lto->ltoFiles.clear();
        }
        catch (...)
        {}
        return false;
    }
}

void TapeExportSession::updateTransferStatus()
{
    LOCK_SECTION(_sessionTableMutex);
    
    StoreStats storeStats;
    _tapeExport->getTape()->get_store_stats(&storeStats);
    
    if (storeStats.offset <= 0) // -1 if not started writing, 0 if writing the index file
    {
        // no updates to mxf file transfer
        return;
    }

    int offset;    
    int prevStatus; 
    bool statusHasChanged = false;
    vector<LTOFileTable*>::iterator iter;
    for (offset = 1, iter = _sessionTable->lto->ltoFiles.begin(); iter != _sessionTable->lto->ltoFiles.end(); iter++, offset++)
    {
        prevStatus = (*iter)->status;
        
        if (offset == storeStats.offset)
        {
            // check updates to status of this item
            if (storeStats.store_state == STARTED)
            {
                (*iter)->status = LTO_FILE_TRANSFER_STATUS_NOT_STARTED;
            }
            else if (storeStats.store_state == IN_PROGRESS)
            {
                if ((*iter)->status != LTO_FILE_TRANSFER_STATUS_STARTED)
                {
                    (*iter)->status = LTO_FILE_TRANSFER_STATUS_STARTED;
                    (*iter)->transferStarted = get_localtime_now();
                }
            }
            else if (storeStats.store_state == COMPLETED)
            {
                if ((*iter)->status != LTO_FILE_TRANSFER_STATUS_COMPLETED)
                {
                    (*iter)->status = LTO_FILE_TRANSFER_STATUS_COMPLETED;
                    (*iter)->transferEnded = get_localtime_now();
                }
            }
            else // (storeStats.store_state == FAILED)
            {
                if ((*iter)->status != LTO_FILE_TRANSFER_STATUS_FAILED)
                {
                    (*iter)->status = LTO_FILE_TRANSFER_STATUS_FAILED;
                    (*iter)->transferEnded = get_localtime_now();
                }
            }
            
            if ((*iter)->status != prevStatus)
            {
                statusHasChanged = true;
                RecorderDatabase::getInstance()->updateLTOFile((*iter)->databaseId, (*iter)->status);
            }
            
            // done with status updates
            break;
        }
        
        // we assume that the files stored onto tape is in the same order as the files
        // in _ltoFiles. This allows us to assume a file transfer has successfully completed
        // if the current file for transfer is lower down in the list
    
        if ((*iter)->status != LTO_FILE_TRANSFER_STATUS_COMPLETED)
        {
            (*iter)->status = LTO_FILE_TRANSFER_STATUS_COMPLETED;
            (*iter)->transferEnded = get_localtime_now();
        }
        
        if ((*iter)->status != prevStatus)
        {
            statusHasChanged = true;
            RecorderDatabase::getInstance()->updateLTOFile((*iter)->databaseId, (*iter)->status);
        }
    }

    if (statusHasChanged)
    {
        _ltoStatusChangeCount++;
    }
}

void TapeExportSession::getInfaxData(LTOFileTable* ltoFileTable, InfaxData* infaxData)
{
    // initialise and ensure strings are null terminated
    memset((void*)infaxData, 0, sizeof(InfaxData));
    
    strncpy(infaxData->format, ltoFileTable->format.c_str(), FORMAT_SIZE - 1);
    strncpy(infaxData->progTitle, ltoFileTable->progTitle.c_str(), PROGTITLE_SIZE - 1);
    strncpy(infaxData->epTitle, ltoFileTable->episodeTitle.c_str(), EPTITLE_SIZE - 1);
    infaxData->txDate.year = ltoFileTable->txDate.year;
    infaxData->txDate.month = ltoFileTable->txDate.month;
    infaxData->txDate.day = ltoFileTable->txDate.day;
    strncpy(infaxData->magPrefix, ltoFileTable->magPrefix.c_str(), MAGPREFIX_SIZE - 1);
    strncpy(infaxData->progNo, ltoFileTable->progNo.c_str(), PROGNO_SIZE - 1);
    strncpy(infaxData->prodCode, ltoFileTable->prodCode.c_str(), PRODCODE_SIZE - 1);
    strncpy(infaxData->spoolStatus, ltoFileTable->spoolStatus.c_str(), SPOOLSTATUS_SIZE - 1);
    infaxData->stockDate.year = ltoFileTable->stockDate.year;
    infaxData->stockDate.month = ltoFileTable->stockDate.month;
    infaxData->stockDate.day = ltoFileTable->stockDate.day;
    strncpy(infaxData->spoolDesc, ltoFileTable->spoolDescr.c_str(), SPOOLDESC_SIZE - 1);
    strncpy(infaxData->memo, ltoFileTable->memo.c_str(), MEMO_SIZE - 1);
    infaxData->duration = ltoFileTable->duration / 25; // "/ 25" because infax precision in seconds
    strncpy(infaxData->spoolNo, ltoFileTable->spoolNo.c_str(), SPOOLNO_SIZE - 1);
    strncpy(infaxData->accNo, ltoFileTable->accNo.c_str(), ACCNO_SIZE - 1);
    strncpy(infaxData->catDetail, ltoFileTable->catDetail.c_str(), CATDETAIL_SIZE - 1);
    infaxData->itemNo = ltoFileTable->itemNo;
}


