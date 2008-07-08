/*
 * $Id: Recorder.cpp,v 1.1 2008/07/08 16:25:37 philipn Exp $
 *
 * Provides main access to the recorder application
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
    The Recorder is used to start and stop a recording session. Access to the
    Recording Session is provided through the Recorder.
    
    The Recorder listens to the state of the DVS capture, preventing a session
    from starting if the signal is bad or initiating a session abort if the 
    signal is bad or buffer overflows whilst recording.
    
    The Recorder can be used to playback a file and control the player.
*/


#include "Recorder.h"
#include "DummyVTRControl.h"
#include "RecorderDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"


using namespace std;
using namespace rec;



#define SESSION_ACCESS_SECTION() \
    checkSessionStatus(); \
    READ_LOCK_SECTION(_sessionRWLock);

#define REPLAY_ACCESS_SECTION() \
   LOCK_SECTION(_replayMutex); \
   if (_replay != 0 && !_replay->isRunning()) \
   { \
       std::string replayFilename = _replay->getFilename(); \
       try \
       { \
           SAFE_DELETE(_replay); \
           Logging::info("Closed replay of cache file '%s'\n", replayFilename.c_str()); \
       } \
       catch (...) \
       { \
           Logging::error("Failed to delete replay for cache file '%s'\n", replayFilename.c_str()); \
           _replay = 0; \
       } \
   }  


static int check_vtr_ready(VTRControl* control)
{
    if (control == 0)
    {
        return 1;
    }
    VTRState vtrState = control->getState();
    if (vtrState == NOT_CONNECTED_VTR_STATE)
    {
        return 2;
    }
    else if (vtrState == REMOTE_LOCKOUT_VTR_STATE)
    {
        return 3;
    }
    else if (vtrState == TAPE_UNTHREADED_VTR_STATE || vtrState == EJECTING_VTR_STATE)
    {
        return 4;
    }
    
    return 0;
}


   

Recorder::Recorder(string name, std::string cacheDirectory, std::string browseDirectory, std::string pseDirectory, 
    int replayPort, string vtrSerialDeviceName1, string vtrSerialDeviceName2)
: _name(name), _numAudioTracks(4), _replayPort(replayPort),
_capture(0), _sessionDone(false), _session(0), _sessionThread(0), _lastSessionRecordingItems(0), 
_cache(0), _recorderTable(0), _replay(0)
{
    _digibetaBarcodePrefixes = Config::getStringArray("digibeta_barcode_prefixes");
    // TODO: _numAudioTracks should only be stored in the config and not in the Recorder
    _numAudioTracks = Config::getInt("num_audio_tracks");
    _remDurationFactor = get_archive_mxf_content_package_size(_numAudioTracks);

    // create and initialise SDI capture system
    
    vector<int> vitcLines = Config::getIntArray("vitc_lines");
    vector<int> ltcLines = Config::getIntArray("ltc_lines");
    
    _capture = new ::Capture(this);
    
    _capture->set_loglev(Config::getInt("capture_log_level"));
    _capture->set_browse_enable(Config::getBool("browse_enable"));
    _capture->set_browse_video_bitrate(Config::getInt("browse_video_bit_rate"));
    _capture->set_browse_thread_count(Config::getInt("browse_thread_count"));
    _capture->set_browse_overflow_frames(Config::getInt("browse_overflow_frames"));
    _capture->set_pse_enable(Config::getBool("pse_enable"));
    _capture->set_vitc_lines(&vitcLines[0], (int)vitcLines.size());
    _capture->set_ltc_lines(&ltcLines[0], (int)ltcLines.size());
    _capture->set_debug_avsync(Config::getBool("dbg_av_sync"));
    _capture->set_debug_vitc_failures(Config::getBool("dbg_vitc_failures"));
    _capture->set_debug_store_thread_buffer(Config::getBool("dbg_store_thread_buffer"));
    _capture->set_num_audio_tracks(_numAudioTracks);
    _capture->set_mxf_page_size(MXF_PAGE_FILE_SIZE);
    
    if (!_capture->init_capture(Config::getInt("ring_buffer_size")))
    {
        REC_LOGTHROW(("Failed to initialise the DVS SDI Ingex capture system"));
    }
    Logging::info("Initialised the capture system\n");
    
    
    // connect to VTRs
    if (vtrSerialDeviceName1.compare("DUMMY_D3") == 0)
    {
        _vtrControl1 = new DummyVTRControl(AJ_D350_PAL_DEVICE_TYPE);
        Logging::warning("Opened dummy D3 vtr control 1\n");
    }
    else if (vtrSerialDeviceName1.compare("DUMMY_DIGIBETA") == 0)
    {
        _vtrControl1 = new DummyVTRControl(DVW_A500P_DEVICE_TYPE);
        Logging::warning("Opened dummy Digibeta vtr control 1\n");
    }
    else
    {
        _vtrControl1 = new DefaultVTRControl(vtrSerialDeviceName1);
        Logging::info("Using vtr control serial device 1 '%s'\n", vtrSerialDeviceName1.c_str());
    }
    
    if (vtrSerialDeviceName2.compare("DUMMY_D3") == 0)
    {
        _vtrControl2 = new DummyVTRControl(AJ_D350_PAL_DEVICE_TYPE);
        Logging::warning("Opened dummy D3 vtr control 2\n");
    }
    else if (vtrSerialDeviceName2.compare("DUMMY_DIGIBETA") == 0)
    {
        _vtrControl2 = new DummyVTRControl(DVW_A500P_DEVICE_TYPE);
        Logging::warning("Opened dummy Digibeta vtr control 2\n");
    }
    else
    {
        _vtrControl2 = new DefaultVTRControl(vtrSerialDeviceName2);
        Logging::info("Using vtr control serial device 2 '%s'\n", vtrSerialDeviceName2.c_str());
    }
    
    _vtrControl1->registerListener(this);
    _vtrControl2->registerListener(this);
    
    
    // load or create recorder data from the database
    _recorderTable = RecorderDatabase::getInstance()->loadRecorder(name);
    if (_recorderTable)
    {
        Logging::info("Loaded recorder '%s' data from database\n", name.c_str());
    }
    else
    {
        auto_ptr<RecorderTable> newRecorderTable(new RecorderTable());
        newRecorderTable->name = name;
        RecorderDatabase::getInstance()->saveRecorder(newRecorderTable.get());
        _recorderTable = newRecorderTable.release();

        Logging::info("Created and saved recorder '%s' data to database\n", name.c_str());
    }
    
    
    // create the cache
    _cache = new Cache(_recorderTable->databaseId, cacheDirectory, browseDirectory, pseDirectory);
    
    
    // abort sessions that were left in the start state

    AutoPointerVector<RecordingSessionTable> startedSessions(
        RecorderDatabase::getInstance()->loadMinimalSessions(_recorderTable->databaseId, 
            RECORDING_SESSION_STARTED));
            
    vector<RecordingSessionTable*>::const_iterator sessIter;
    for (sessIter = startedSessions.get().begin(); sessIter != startedSessions.get().end(); sessIter++)
    {
        // update session status to abort
        (*sessIter)->status = RECORDING_SESSION_ABORTED;
        (*sessIter)->abortInitiator = SYSTEM_ABORT_SESSION;
        (*sessIter)->comments = "Existing session was aborted at recorder system startup";
        RecorderDatabase::getInstance()->updateSession(*sessIter);
        Logging::warning("Aborted existing session (%s) when restarting recorder system\n", 
            get_user_timestamp_string((*sessIter)->creation).c_str());
        
        // remove items created by the session from the cache
        // note: any files on disk have probably already been deleted from the cache creating
        // directory when creating the _cache above
        AutoPointerVector<Destination> sessionDestinations(
            RecorderDatabase::getInstance()->loadSessionDestinations((*sessIter)->databaseId));
        HardDiskDestination* hdd;
        vector<Destination*>::iterator destIter;
        for (destIter = sessionDestinations.get().begin(); destIter != sessionDestinations.get().end(); destIter++)
        {
            if ((*destIter)->concreteDestination != 0 && 
                (*destIter)->concreteDestination->getTypeId() == HARD_DISK_DEST_TYPE)
            {
                hdd = dynamic_cast<HardDiskDestination*>((*destIter)->concreteDestination);
                if (_cache->removeItem(hdd->databaseId))
                {
                    Logging::warning("Removed file '%s' from aborted session from cache\n", hdd->name.c_str());
                }
            }
        }
    }
}

Recorder::~Recorder()
{
    delete _capture;
    // _session is owned by _sessionThread
    delete _sessionThread;
    delete _lastSessionRecordingItems;
    delete _recorderTable;
    delete _vtrControl1;
    delete _vtrControl2;
    delete _replay;
}

void Recorder::sdiStatusChanged(bool videoOK, bool audioOK, bool recordingOK)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        if (!videoOK || !audioOK)
        {
            // only abort if the recording was started or was not stopped
            if (_session->abortIfRecording(false, "SDI signal bad"))
            {
                Logging::warning("Sending system 'abort if recording' to recording session because SDI signal is bad\n"); 
            }
        }
        else if (!recordingOK)
        {
            // only abort if the recording was started or was not stopped
            if (_session->abortIfRecording(false, "Capture system stopped"))
            {
                Logging::warning("Sending system 'abort if recording' to recording session because capture system stopped\n"); 
            }
        }
    }
}

void Recorder::mxfBufferOverflow()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::warning("Sending system 'abort' because the MXF buffer has overflowed - problem with disk I/O?\n"); 
        _session->abort(false, "MXF buffer overflow");
    }
}

void Recorder::browseBufferOverflow()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->browseBufferOverflow();
    }
}

void Recorder::vtrDeviceType(VTRControl* vtrControl, int deviceTypeCode, DeviceType deviceType)
{
    // only poll the extended state when the VTR is the D3 VTR so that the D3 VTR 
    // playback error is available to the recording session for writing into the MXF file
    if (vtrControl->isD3VTR())
    {
        vtrControl->pollExtState(true);
    }
    else
    {
        vtrControl->pollExtState(false);
    }
}

int Recorder::startNewSession(Source* source, string digibetaBarcode, RecordingSession** session)
{
    SESSION_ACCESS_SECTION();
    
    // we are now responsible for deleting the Source if something fails
    auto_ptr<RecordingItems> recordingItems(new RecordingItems(source));
    
    
    // no session must be active
    if (_session)
    {
        return SESSION_IN_PROGRESS_FAILURE;
    }

    // must be ready to record
    ::GeneralStats captureStats;
    _capture->get_general_stats(&captureStats);

    if (!captureStats.video_ok)
    {
        return VIDEO_SIGNAL_BAD_FAILURE;
    }
    if (!captureStats.audio_ok)
    {
        return AUDIO_SIGNAL_BAD_FAILURE;
    }
    if (captureStats.recording)
    {
        Logging::warning("Capture system running without a controlling recording session\n");
        if (_capture->abort_record())
        {
            Logging::info("Capture was aborted\n");
        }
        else
        {
            // TODO: need to halt the recorder completely
            Logging::warning("Failed to abort capture\n");
        }
        return INTERNAL_FAILURE;
    }

    
    // check D3 and Digibeta VTRs can be controlled and that tape are present
    
    VTRControl* d3VTRControl = getD3VTRControl();
    int result = check_vtr_ready(d3VTRControl);
    if (result != 0)
    {
        switch (result)
        {
            case 3:
                return D3_VTR_REMOTE_LOCKOUT_FAILURE;
            case 4:
                return NO_D3_TAPE;
            case 1:
            case 2:
            default:
                return D3_VTR_CONNECT_FAILURE;
        }
    }

    VTRControl* digibetaVTRControl = getDigibetaVTRControl();
    result = check_vtr_ready(digibetaVTRControl);
    if (result != 0)
    {
        switch (result)
        {
            case 3:
                return DIGIBETA_VTR_REMOTE_LOCKOUT_FAILURE;
            case 4:
                return NO_DIGIBETA_TAPE;
            case 1:
            case 2:
            default:
                return DIGIBETA_VTR_CONNECT_FAILURE;
        }
    }
    
    
    // check that multi-item support is enabled if source has multiple items
    if (!checkMultiItemSupport(recordingItems->getSource()))
    {
        return MULTI_ITEM_NOT_ENABLED_FAILURE;
    }
    
    // check the multi-item MXF file are not present in the cache
    if (recordingItems->getItemCount() > 1 &&
        !_cache->checkMultiItemMXF(recordingItems->getD3SpoolNo(), recordingItems->getItemCount()))
    {
        return MULTI_ITEM_MXF_EXISTS_FAILURE;
    }
    
    
    // check minimum disk space is available
    if (getRemainingDiskSpace() == 0)
    {
        return DISK_SPACE_FAILURE;
    }
    
    
    // stop replay
    try
    {
        REPLAY_ACCESS_SECTION();

        if (_replay)
        {
            SAFE_DELETE(_replay);
            Logging::info("Closed replay of cache file\n");
        }
    }
    catch (...)
    {
        Logging::error("Failed to stop file replay\n");
        _replay = 0;
        return INTERNAL_FAILURE;
    }
    
    // start the session
    try
    {
        _session = new RecordingSession(this, recordingItems.get(), digibetaBarcode, _replayPort,
            d3VTRControl, digibetaVTRControl);
        recordingItems.release(); // _session now owns it
        _sessionThread = new Thread(_session, true);
        _sessionThread->start();
    }
    catch (...)
    {
        Logging::error("Failed to start a new session\n");
        return INTERNAL_FAILURE;
    }
    
    *session = _session;
    return 0;
}

bool Recorder::haveSession()
{
    SESSION_ACCESS_SECTION();
    
    return _session != 0;
}    

SessionState Recorder::getSessionState()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->getState();
    }
    
    return SessionState();
}

SessionStatus Recorder::getSessionStatus()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->getStatus();
    }
    
    return SessionStatus();
}

void Recorder::startRecording()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'start record' to recording session\n");
        _session->startRecording();
    }
}

void Recorder::stopRecording()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'stop record' to recording session\n");
        _session->stopRecording();
    }
}

void Recorder::chunkFile()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'chunk file' to recording session\n");
        _session->chunkFile();
    }
}

void Recorder::completeSession(string comments)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'complete session' to recording session\n");
        _session->complete(comments);
    }
}

void Recorder::abortSession(bool fromUser, string comments)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'abort session' to recording session\n");
        _session->abort(fromUser, comments);
    }
}

void Recorder::setSessionComments(string comments)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        string trimmedComments = trim_string(comments);
        if (!trimmedComments.empty())
        {
            Logging::info("Setting session comments\n");
            _session->setSessionComments(trimmedComments);
        }
    }
}

bool Recorder::confidenceReplayActive()
{
    // try local replay
    {
        REPLAY_ACCESS_SECTION();

        if (_replay)
        {
            return true;
        }
    }
    
    // try the session replay
    {
        SESSION_ACCESS_SECTION();
        
        if (_session)
        {
            return _session->confidenceReplayActive();
        }
    }
    
    return false;
}

void Recorder::forwardConfidenceReplayControl(string command)
{
    // try local replay
    {
        REPLAY_ACCESS_SECTION();
        
        if (_replay)
        {
            _replay->forwardControl(command);
            return;
        }
    }
    
    // try the session replay
    {
        SESSION_ACCESS_SECTION();
        
        if (_session)
        {
            _session->forwardConfidenceReplayControl(command);
        }
    }
}

int Recorder::replayFile(string name)
{
    SESSION_ACCESS_SECTION();
    REPLAY_ACCESS_SECTION(); // place after session access section 
    
    if (_session)
    {
        return SESSION_IN_PROGRESS_REPLAY_FAILURE;
    }
    if (!_cache->itemExists(name))
    {
        return UNKNOWN_FILE_REPLAY_FAILURE;
    }
    
    if (_replay)
    {
        // play file or seek to start if the file is already playing
        if (_replay->getFilename().compare(_cache->getCompleteFilename(name)) == 0)
        {
            _replay->seekPositionOrPlay(0);
            return 0;
        }
        
        // close existing replay
        try
        {
            SAFE_DELETE(_replay);
            Logging::info("Closed replay of cache file\n");
        }
        catch (...)
        {
            Logging::error("Failed to close replay in recorder\n");
            _replay = 0;
            return INTERNAL_REPLAY_FAILURE;
        }
    }
    
    try
    {
        _replay = new ConfidenceReplay(_cache->getCompleteFilename(name), _replayPort, 0, false);
        _replay->start();
        Logging::info("Started replay of file '%s' in cache\n", name.c_str());
    }
    catch (...)
    {
        try { delete _replay; } catch (...) {}
        Logging::error("Failed to open replay in recorder\n");
        _replay = 0;
        return INTERNAL_REPLAY_FAILURE;
    }
    
    return 0;
}

void Recorder::playItem(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->playItem(id, index);
    }
}

void Recorder::playPrevItem()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->playPrevItem();
    }
}

void Recorder::playNextItem()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->playNextItem();
    }
}

void Recorder::seekToEOP()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->seekToEOP();
    }
}

int Recorder::markItemStart(int* id, bool* isJunk, int64_t* filePosition, int64_t* fileDuration)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->markItemStart(id, isJunk, filePosition, fileDuration);
    }
    
    return -1;
}

int Recorder::clearItem(int id, int index, int64_t* filePosition)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->clearItem(id, index, filePosition);
    }
    
    return -1;
}

int Recorder::moveItemUp(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->moveItemUp(id, index);
    }
    
    return -1;
}

int Recorder::moveItemDown(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->moveItemDown(id, index);
    }
    
    return -1;
}

int Recorder::disableItem(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->disableItem(id, index);
    }
    
    return -1;
}

int Recorder::enableItem(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->enableItem(id, index);
    }
    
    return -1;
}

RecorderStatus Recorder::getStatus()
{
    ::GeneralStats captureStats;
    _capture->get_general_stats(&captureStats);
    
    VTRControl* d3VTRControl = getD3VTRControl();
    VTRState d3VTRState = (d3VTRControl != 0) ? d3VTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    VTRControl* digibetaVTRControl = getDigibetaVTRControl();
    VTRState digibetaVTRState = (digibetaVTRControl != 0) ? digibetaVTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    
    RecorderStatus status;
    status.recorderName = _name;
    status.databaseOk = RecorderDatabase::getInstance()->haveConnection();
    status.sdiCardOk = captureStats.video_ok && captureStats.audio_ok;
    status.videoOk = captureStats.video_ok;
    status.audioOk = captureStats.audio_ok;
    status.vtrOk = d3VTRState != NOT_CONNECTED_VTR_STATE && d3VTRState != REMOTE_LOCKOUT_VTR_STATE &&
        digibetaVTRState != NOT_CONNECTED_VTR_STATE && digibetaVTRState != REMOTE_LOCKOUT_VTR_STATE;

    bool sessionActive;
    {
        SESSION_ACCESS_SECTION();
        sessionActive = _session != 0;
    }

    status.d3VTRState = d3VTRState;
    status.digibetaVTRState = digibetaVTRState;
    
    status.readyToRecord = !sessionActive && 
        status.sdiCardOk && status.videoOk && status.audioOk && 
        status.databaseOk && 
        status.vtrOk && 
        d3VTRState != TAPE_UNTHREADED_VTR_STATE && d3VTRState != EJECTING_VTR_STATE &&
        digibetaVTRState != TAPE_UNTHREADED_VTR_STATE && digibetaVTRState != EJECTING_VTR_STATE;
    
    {
        REPLAY_ACCESS_SECTION();
    
        status.replayActive = _replay != 0;
        status.replayFilename = (_replay != 0) ? _replay->getFilename() : "" ;
    }
    
    return status;
}

RecorderSystemStatus Recorder::getSystemStatus()
{
    VTRControl* d3VTRControl = getD3VTRControl();
    VTRControl* digibetaVTRControl = getDigibetaVTRControl();
    
    RecorderSystemStatus status;
    
    status.numAudioTracks = _numAudioTracks;
    status.remDiskSpace = getRemainingDiskSpace();
    status.remDuration = status.remDiskSpace / _remDurationFactor;
    status.d3VTRState = (d3VTRControl != 0) ? d3VTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    status.digibetaVTRState = (digibetaVTRControl != 0) ? digibetaVTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    
    return status;
}

void Recorder::getLastSessionResult(SessionResult* result, string* sourceSpoolNo, string* failureReason)
{
    SESSION_ACCESS_SECTION();

    *result = _lastSessionResult;    
    *sourceSpoolNo = _lastSessionSourceSpoolNo;    
    *failureReason = _lastSessionFailureReason;    
}

int64_t Recorder::getRemainingDiskSpace()
{
    int64_t remDiskSpace = _cache->getDiskSpace() - DISK_SPACE_MARGIN;
    remDiskSpace = (remDiskSpace < 0) ? 0 : remDiskSpace;
    
    return remDiskSpace;
}

RecordingItems* Recorder::getSessionRecordingItems()
{
    SESSION_ACCESS_SECTION();

    if (_session)
    {
        return _session->getRecordingItems();
    }

    return 0;
}

bool Recorder::getSessionComments(string* comments, int* count)
{
    SESSION_ACCESS_SECTION();

    if (_session)
    {
        return _session->getSessionComments(comments, count);
    }

    return false;
}

Cache* Recorder::getCache()
{
    return _cache;
}

::Capture* Recorder::getCapture()
{
    return _capture;
}

VTRControl* Recorder::getD3VTRControl()
{
    if (_vtrControl1->isD3VTR())
    {
        return _vtrControl1;
    }
    else if (_vtrControl2->isD3VTR())
    {
        return _vtrControl2;
    }
    
    // D3 VTR control is not connected
    return 0;
}

VTRControl* Recorder::getDigibetaVTRControl()
{
    // any VTR that that is not a D3 VTR is assumed to be the Digibeta VTR
    
    if (_vtrControl1->isNonD3VTR())
    {
        return _vtrControl1;
    }
    else if (_vtrControl2->isNonD3VTR())
    {
        return _vtrControl2;
    }
    
    // Digibeta VTR control is not connected
    return 0;
}

vector<VTRControl*> Recorder::getVTRControls()
{
    vector<VTRControl*> vcs;
    vcs.push_back(_vtrControl1);
    vcs.push_back(_vtrControl2);
    
    return vcs;
}

RecorderTable* Recorder::getRecorderTable()
{
    return _recorderTable;
}

string Recorder::getName()
{
    return _name;
}

bool Recorder::isDigibetaBarcode(string barcode)
{
    if (!is_valid_barcode(barcode))
    {
        return false;
    }
    
    vector<string>::const_iterator iter;
    for (iter = _digibetaBarcodePrefixes.begin(); iter != _digibetaBarcodePrefixes.end(); iter++)
    {
        if (barcode.compare(0, (*iter).size(), *iter) == 0)
        {
            return true;
        }
    }
    
    return false;
}

bool Recorder::checkMultiItemSupport(Source* source)
{
    return Config::getBool("enable_multi_item") || source->concreteSources.size() <= 1;
}

void Recorder::sessionDone()
{
    _sessionDone = true;
}

void Recorder::checkSessionStatus()
{
    if (_sessionDone)
    {
        WRITE_LOCK_SECTION(_sessionRWLock);
        
        if (_session)
        {
            delete _lastSessionRecordingItems;
            _lastSessionRecordingItems = _session->getRecordingItems();
            
            _session->getFinalResult(&_lastSessionResult, &_lastSessionSourceSpoolNo, &_lastSessionFailureReason);
            
            SAFE_DELETE(_sessionThread);
            // don't delete _session which is owned by _sessionThread
            _session = 0;
        }
        
        _sessionDone = false;
    }
}

