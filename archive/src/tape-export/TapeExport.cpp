/*
 * $Id: TapeExport.cpp,v 1.1 2008/07/08 16:26:09 philipn Exp $
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
 
/*
    The tape export is used to start and stop a session. Access to the session 
    is provided through this class.
*/

#include "TapeExport.h"
#include "RecorderDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"

#include "write_archive_mxf.h"


using namespace std;
using namespace rec;


#define SESSION_ACCESS_SECTION() \
    READ_LOCK_SECTION(_sessionRWLock); \
    checkSessionStatus();


static string read_tape_device_info(string path, string name)
{
    FILE* infoFile;
    string filename;
    char buffer[256];
    size_t numRead;
    int i;
    string result;
    
    filename = join_path(path, name);
    if ((infoFile = fopen(filename.c_str(), "rb")) != NULL)
    {
        if ((numRead = fread(buffer, 1, 256, infoFile)) > 0)
        {
            // terminate string and remove trailing whitespace
            buffer[numRead - 1] = '\0';
            i = numRead - 2;
            while (i >= 0 && buffer[i] == ' ')
            {
                buffer[i] = '\0';
                i--;
            }
            
            result = buffer;
        }
        fclose(infoFile);
    }
    
    return result;
}





TapeExport::TapeExport(string tapeDevice, string recorderName, string cacheDirectory)
: _recorderName(recorderName), _recorderId(-1), _maxTotalSize(0), _minTotalSize(0),
_maxFiles(0), _cache(0), _sessionDone(false), _session(0), _sessionThread(0),
_lastSessionTable(0), _tape(0)
{
    _ltoBarcodePrefixes = Config::getStringArray("lto_barcode_prefixes");
    _remDurationFactor = get_archive_mxf_content_package_size(Config::getInt("num_audio_tracks"));
    _maxTotalSize = Config::getInt("max_lto_size_gb") * 1000LL * 1000LL * 1000LL;
    _minTotalSize = 0;
    _maxFiles = Config::getInt("max_auto_files_on_lto");

    
    _tape = new ::Tape(tapeDevice);
    _tape->set_log_level(Config::getInt("tape_log_level"));
    if (!_tape->init_tape_monitor())
    {
        REC_LOGTHROW(("Failed to access tape device"));
    }
    readTapeDeviceDetails(tapeDevice);
    
    
    _cache = new Cache(recorderName, cacheDirectory);
    
    // get the recorder identifier
    auto_ptr<RecorderTable> tmpRecorder(RecorderDatabase::getInstance()->loadRecorder(recorderName));
    _recorderId = tmpRecorder->databaseId;
    
    // cleanup old transfer sessions stuck in start state
    AutoPointerVector<LTOTransferSessionTable> startedSessions(
        RecorderDatabase::getInstance()->loadLTOTransferSessions(_recorderId, LTO_TRANSFER_SESSION_STARTED));
    vector<LTOTransferSessionTable*>::iterator iter;
    for (iter = startedSessions.get().begin(); iter != startedSessions.get().end(); iter++)
    {
        // update session status to abort
        (*iter)->status = LTO_TRANSFER_SESSION_ABORTED;
        (*iter)->abortInitiator = SYSTEM_ABORT_SESSION;
        (*iter)->comments = "Existing session was aborted at tape transfer system startup";
        RecorderDatabase::getInstance()->updateLTOTransferSession(*iter);
        Logging::warning("Aborted existing session (%s) when restarting tape transfer system\n", 
            get_user_timestamp_string((*iter)->creation).c_str());
    }
    
    // TODO: remove any soft-links in the working directory
}

TapeExport::~TapeExport()
{
    delete _tape;
    delete _lastSessionTable;
    // _session is owned by _sessionThread
    delete _sessionThread;
}

int TapeExport::startNewAutoSession(string barcode, TapeExportSession** session)
{
    SESSION_ACCESS_SECTION();
    
    // no session must be active
    if (_session)
    {
        return SESSION_IN_PROGRESS_FAILURE;
    }
    // check barcode
    if (!isLTOBarcode(barcode))
    {
        return INVALID_BARCODE_FAILURE;
    }
    // check barcode hasn't been used before in a completed session
    if (RecorderDatabase::getInstance()->ltoUsedInCompletedSession(barcode))
    {
        return BARCODE_USED_BEFORE_FAILURE;
    }
    // check the tape is ready
    GeneralStats generalStats;
    _tape->get_general_stats(&generalStats);
    if (generalStats.tape_state != READY)
    {
        return TAPE_NOT_READY_FAILURE;
    }

    _session = new TapeExportSession(this, barcode, _maxTotalSize, _minTotalSize, _maxFiles);
    _sessionThread = new Thread(_session, true);
    _sessionThread->start();    
    
    *session = _session;
    return 0;
}

int TapeExport::startNewManualSession(std::string barcode, std::vector<long> itemIds, TapeExportSession** session)
{
    SESSION_ACCESS_SECTION();
    
    // no session must be active
    if (_session)
    {
        return SESSION_IN_PROGRESS_FAILURE;
    }
    // check barcode
    if (!isLTOBarcode(barcode))
    {
        return INVALID_BARCODE_FAILURE;
    }
    // check barcode hasn't been used before in a completed session
    if (RecorderDatabase::getInstance()->ltoUsedInCompletedSession(barcode))
    {
        return BARCODE_USED_BEFORE_FAILURE;
    }
    // check item list is not empty
    if (itemIds.size() == 0)
    {
        return EMPTY_ITEM_LIST_FAILURE;
    }
    // check all the items are known
    if (!_cache->itemsAreKnown(itemIds))
    {
        return UNKNOWN_ITEM_FAILURE;
    }
    // check the total size
    int64_t totalSize = _cache->getTotalSize(itemIds);
    if (totalSize == 0)
    {
        return ZERO_SIZE_FAILURE;
    }
    else if (totalSize >= _maxTotalSize)
    {
        return MAX_SIZE_EXCEEDED_FAILURE;
    }
    // check the tape is ready
    GeneralStats generalStats;
    _tape->get_general_stats(&generalStats);
    if (generalStats.tape_state != READY)
    {
        return TAPE_NOT_READY_FAILURE;
    }
    

    _session = new TapeExportSession(this, barcode, itemIds, _maxTotalSize);
    _sessionThread = new Thread(_session, true);
    _sessionThread->start();    
    
    *session = _session;
    return 0;
}

bool TapeExport::haveSession()
{
    SESSION_ACCESS_SECTION();
    
    return _session != 0;
}    

SessionStatus TapeExport::getSessionStatus()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->getStatus();
    }
    return SessionStatus();
}

void TapeExport::abortSession(bool fromUser, string comments)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->abort(fromUser, comments);
    }
}

LTOContents* TapeExport::getLTOContents()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->getLTOContents();
    }
    return 0;
}

bool TapeExport::isFileUsedInSession(string name)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->isFileUsedInSession(name);
    }
    return false;
}

set<string> TapeExport::getSessionFileNames()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->getSessionFileNames();
    }
    return set<string>();
}


bool TapeExport::getLastSessionStatus(string* barcode, bool* completed, bool* userAbort)
{
    SESSION_ACCESS_SECTION();
    
    if (_lastSessionBarcode.size() == 0)
    {
        return false;
    }
    
    *barcode = _lastSessionBarcode;
    *completed = _lastSessionCompleted;
    *userAbort = _lastSessionUserAbort;
    return true;
}

TapeExportStatus TapeExport::getStatus()
{
    SESSION_ACCESS_SECTION();

    TapeExportStatus status;
    status.recorderName = _recorderName;
    status.databaseOk = RecorderDatabase::getInstance()->haveConnection();
    // TODO: check if tape device can be accessed when constructing TapeExport 
    status.tapeDeviceOk = true;
    status.sessionInProgress = _session != 0;

    GeneralStats generalStats;
    _tape->get_general_stats(&generalStats);
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

    status.readyToExport = status.databaseOk && !status.sessionInProgress && 
        status.tapeDevStatus == READY_TAPE_DEV_STATE;
        
    status.maxTotalSize = _maxTotalSize;
    status.minTotalSize = _minTotalSize;
    status.maxFiles = _maxFiles;
    
    return status;
}

TapeExportSystemStatus TapeExport::getSystemStatus()
{
    TapeExportSystemStatus status;
    
    GeneralStats generalStats;
    _tape->get_general_stats(&generalStats);
    
    status.remDiskSpace = getRemainingDiskSpace();
    status.remDuration = status.remDiskSpace / _remDurationFactor;
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
    
    return status;
}

int64_t TapeExport::getRemainingDiskSpace()
{
    int64_t remDiskSpace = _cache->getDiskSpace() - DISK_SPACE_MARGIN;
    remDiskSpace = (remDiskSpace < 0) ? 0 : remDiskSpace;
    
    return remDiskSpace;
}

long TapeExport::getRecorderId()
{
    return _recorderId;
}

Cache* TapeExport::getCache()
{
    return _cache;
}

::Tape* TapeExport::getTape()
{
    return _tape;
}

TapeDeviceDetails TapeExport::getTapeDeviceDetails()
{
    return _tapeDeviceDetails;
}

bool TapeExport::isLTOBarcode(string barcode)
{
    if (!is_valid_barcode(barcode))
    {
        return false;
    }
    
    vector<string>::const_iterator iter;
    for (iter = _ltoBarcodePrefixes.begin(); iter != _ltoBarcodePrefixes.end(); iter++)
    {
        if (barcode.compare(0, (*iter).size(), *iter) == 0)
        {
            return true;
        }
    }
    
    return false;
}

void TapeExport::sessionDone()
{
    _sessionDone = true;
    
    // signal an pseudo update because files involved in the tranfer are now no longer 'locked'
    _cache->signalCacheUpdate();
}

void TapeExport::checkSessionStatus()
{
    if (_sessionDone)
    {
        if (_session)
        {
            SAFE_DELETE(_lastSessionTable);
            
            // cleanup the session
            _lastSessionBarcode = _session->getBarcode();
            _lastSessionCompleted = !_session->wasAborted();
            _lastSessionUserAbort = _session->wasUserAbort();
            _lastSessionTable = _session->getSessionTable();
            
            SAFE_DELETE(_sessionThread);
            // _session is owned by _sessionThread
            _session = 0;
        }
        
        _sessionDone = false;
    }
}

void TapeExport::readTapeDeviceDetails(string devicePath)
{
    string name;
    if (devicePath.rfind('/') != string::npos)
    {
        name = devicePath.substr(devicePath.rfind('/') + 1);
    }
    else
    {
        name = devicePath;
    }
    string deviceInfoDir = join_path("/sys/class/scsi_tape", name, "device");
    
    // vendor
    _tapeDeviceDetails.vendor = read_tape_device_info(deviceInfoDir, "vendor");
    if (_tapeDeviceDetails.vendor.empty())
    {
        Logging::warning("Failed to read 'vendor' info for tape device '%s'\n", name.c_str());
    }
    
    // model
    _tapeDeviceDetails.model = read_tape_device_info(deviceInfoDir, "model");
    if (_tapeDeviceDetails.model.empty())
    {
        Logging::warning("Failed to read 'model' info for tape device '%s'\n", name.c_str());
    }
    
    // revision 
    _tapeDeviceDetails.revision = read_tape_device_info(deviceInfoDir, "rev");
    if (_tapeDeviceDetails.revision.empty())
    {
        Logging::warning("Failed to read 'rev' info for tape device '%s'\n", name.c_str());
    }
    
    
    Logging::info("Tape device: vendor='%s', model='%s', revision='%s'\n", 
        _tapeDeviceDetails.vendor.c_str(), _tapeDeviceDetails.model.c_str(), _tapeDeviceDetails.revision.c_str());
}


