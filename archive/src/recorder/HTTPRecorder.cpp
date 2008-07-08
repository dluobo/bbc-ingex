/*
 * $Id: HTTPRecorder.cpp,v 1.1 2008/07/08 16:25:35 philipn Exp $
 *
 * HTTP interface to the recorder
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
    The set of URLs handled by this class are listed near the top of this file.
    The requests are sometimes handled by "agents" running is a new thread to 
    avoid holding up other HTTP requests.
    
    New barcodes read using a server side barcode scanner are signalled back to 
    the client browser using a barcode update count in the recorder status info.
    
    PSE reports are displayed in a HTML frame to allow users to easily print 
    and go back to the previous page using a touchscreen monitor. 
*/
 
#include "HTTPRecorder.h"
#include "JSONObject.h"
#include "RecorderDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "version.h"


using namespace std;
using namespace rec;


// a barcode expires (ie. will not be sent to a client) after ... seconds
#define BARCODE_EXPIRES_TIME_SEC        30

// all the HTTP service URLs handled by the HTTPRecorder
static const char* g_recorderStatusURL = "/recorder/status.json";
static const char* g_sourceInfoURL = "/recorder/sourceinfo.json";
static const char* g_updateSourceInfoURL = "/recorder/updatesourceinfo.json";
static const char* g_checkSelectedDigibetaURL = "/recorder/checkselecteddigibeta.json";
static const char* g_newSessionURL = "/recorder/newsession.json";
static const char* g_sessionSourceInfoURL = "/recorder/session/sourceinfo.json";
static const char* g_startRecordingURL = "/recorder/session/start";
static const char* g_stopRecordingURL = "/recorder/session/stop";
static const char* g_chunkFileURL = "/recorder/session/chunkfile";
static const char* g_completeSessionURL = "/recorder/session/complete";
static const char* g_abortSessionURL = "/recorder/session/abort";
static const char* g_setSessionCommentsURL = "/recorder/session/setsessioncomments";
static const char* g_getSessionCommentsURL = "/recorder/session/getsessioncomments.json";
static const char* g_cacheContentsURL = "/recorder/cache/contents.json";
static const char* g_confReplayURL = "/confreplay/*";
static const char* g_replayFileURL = "/recorder/replay";
static const char* g_playItemURL = "/recorder/session/playitem";
static const char* g_playPrevItemURL = "/recorder/session/playprevitem";
static const char* g_playNextItemURL = "/recorder/session/playnextitem";
static const char* g_seekToEOPURL = "/recorder/session/seektoeop";
static const char* g_markItemStartURL = "/recorder/session/markitemstart.json";
static const char* g_clearItemURL = "/recorder/session/clearitem.json";
static const char* g_moveItemUpURL = "/recorder/session/moveitemup.json";
static const char* g_moveItemDownURL = "/recorder/session/moveitemdown.json";
static const char* g_disableItemURL = "/recorder/session/disableitem.json";
static const char* g_enableItemURL = "/recorder/session/enableitem.json";
static const char* g_getItemClipInfoURL = "/recorder/session/getitemclipinfo.json";
static const char* g_getItemSourceInfoURL = "/recorder/session/getitemsourceinfo.json";

static const char* g_pseReportsURL = "/psereports";
static const char* g_pseReportFramedURL = "/psereport_framed.shtml";

static const char* g_ssiGetPSEReportURL = "get_pse_report_url";



static bool parse_int(string intStr, int* value)
{
    return sscanf(intStr.c_str(), "%d", value) == 1; 
}

static string get_size_string(int64_t size)
{
    char buf[32];
    buf[0] = '\0';
    
    if (size < 0)
    {
        sprintf(buf, "0B");
    }
    else if (size < 1000LL)
    {
        sprintf(buf, "%dB", (int)size);
    }
    else if (size < 1000LL * 1000LL)
    {
        sprintf(buf, "%.2fKB", size / 1000.0);
    }
    else if (size < 1000LL * 1000LL * 1000LL)
    {
        sprintf(buf, "%.2fMB", size / (1000.0 * 1000.0));
    }
    else if (size < 1000LL * 1000LL * 1000LL * 1000LL)
    {
        sprintf(buf, "%.2fGB", size / (1000.0 * 1000.0 * 1000.0));
    }
    else
    {
        sprintf(buf, "%.2fTB", size / (1000.0 * 1000.0 * 1000.0 * 1000.0));
    }
    
    return buf;
}

static string get_timestamp_string(Timestamp ts)
{
    char buf[128];

    buf[0] = '\0';
    
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hour, ts.min, ts.sec);
        
    return buf;
}

static string get_session_status_string(long status)
{
    switch (status)
    {
        case RECORDING_SESSION_STARTED:
            return "Started";
        case RECORDING_SESSION_COMPLETED:
            return "Completed";
        case RECORDING_SESSION_ABORTED:
            return "Aborted";
        default:
            return "Unknown";
    }
}

static string get_complete_prog_no(string magPrefix, string progNo, string prodCode)
{
    string trimProdCode = trim_string(prodCode);
    string trimMagPrefix = trim_string(magPrefix);
    
    string result;
    if (!trimMagPrefix.empty())
    {
        result = trimMagPrefix + ":";
    }
    result += trim_string(progNo);
    if (!trimProdCode.empty())
    {
        result += "/";
        if (trimProdCode.size() == 1 && 
            trimProdCode[0] >= '0' && trimProdCode[0] <= '9')
        {
            // replace single digit with 2 digit starting with '0'
            result += "0";
        }
        result += trimProdCode;
    }
    
    return result;
}

static string get_vtr_state_string(VTRState state)
{
    switch (state)
    {
        case NOT_CONNECTED_VTR_STATE:
            return "Not connected";
        case REMOTE_LOCKOUT_VTR_STATE:
            return "Remote lockout";
        case TAPE_UNTHREADED_VTR_STATE:
            return "Unthreaded";
        case STOPPED_VTR_STATE:
            return "Stopped";
        case PAUSED_VTR_STATE:
            return "Paused";
        case PLAY_VTR_STATE:
            return "Playing";
        case FAST_FORWARD_VTR_STATE:
            return "Fast forward";
        case FAST_REWIND_VTR_STATE:
            return "Fast rewind";
        case EJECTING_VTR_STATE:
            return "Ejecting";
        case RECORDING_VTR_STATE:
            return "Recording";
        case SEEKING_VTR_STATE:
            return "Seeking";
        case JOG_VTR_STATE:
            return "Jog";
        case OTHER_VTR_STATE:
            return "Other";
        default:
            return "Unknown";
    }
}



class SourceInfoAgent : public ThreadWorker
{
public:
    SourceInfoAgent(Recorder* recorder, string barcode, HTTPConnection* connection)
    : _recorder(recorder), _barcode(barcode), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~SourceInfoAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        
        // load source info
        
        auto_ptr<Source> source(RecorderDatabase::getInstance()->loadSource(_barcode));
        if (source.get() == 0)
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 1);
            json.setString("errorMessage", "Unknown source");
            _connection->sendJSON(&json);
            return;
        }
        Logging::info("Loaded source info '%s'\n", _barcode.c_str());
        
        
        // check multi-item support
        
        if (!_recorder->checkMultiItemSupport(source.get()))
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 2);
            json.setString("errorMessage", "Multi-item ingest not enabled");
            _connection->sendJSON(&json);
            return;
        }
        

        
        // calculate the total Infax duration
        
        int64_t totalInfaxDuration = -1;
        vector<ConcreteSource*>::const_iterator iter;
        for (iter = source->concreteSources.begin(); iter != source->concreteSources.end(); iter++)
        {
            D3Source* d3Source = dynamic_cast<D3Source*>(*iter);
            
            if (d3Source->duration > 0)
            {
                if (totalInfaxDuration < 0)
                {
                    totalInfaxDuration = d3Source->duration;
                }
                else
                {
                    totalInfaxDuration += d3Source->duration;
                }
            }
        }        
        
        
        // generate the response

        JSONObject json;
        json.setBool("error", false);
        json.setString("barcode", _barcode);
            
        JSONArray* itemArray = 0;
        for (iter = source->concreteSources.begin(); iter != source->concreteSources.end(); iter++)
        {
            D3Source* d3Source = dynamic_cast<D3Source*>(*iter);
            
            if (iter == source->concreteSources.begin())
            {
                JSONObject* tapeInfo = json.setObject("tapeInfo");
                tapeInfo->setString("format", d3Source->format);
                tapeInfo->setString("spoolNo", d3Source->spoolNo);
                tapeInfo->setString("accNo", d3Source->accNo);
                tapeInfo->setString("spoolStatus", d3Source->spoolStatus);
                tapeInfo->setString("stockDate", d3Source->stockDate.toString());
                tapeInfo->setNumber("totalInfaxDuration", totalInfaxDuration);

                itemArray = json.setArray("items");
            }
            
            JSONObject* items = itemArray->appendObject();
            items->setString("progTitle", d3Source->progTitle);
            items->setString("spoolDescr", d3Source->spoolDescr);
            items->setString("episodeTitle", d3Source->episodeTitle);
            items->setString("txDate", d3Source->txDate.toString());
            items->setString("progNo", get_complete_prog_no(d3Source->magPrefix, d3Source->progNo, d3Source->prodCode));
            items->setNumber("infaxDuration", d3Source->duration);
            items->setString("catDetail", d3Source->catDetail);
            items->setString("memo", d3Source->memo);
            items->setNumber("itemNo", d3Source->itemNo);
        }

        // set message to warn if the D3 has been ingested before
        if (RecorderDatabase::getInstance()->d3UsedInCompletedSession(_barcode))
        {
            json.setNumber("warningCode", 0);
            json.setString("warningMessage", "Warning: D3 has been ingested before");
        }
        
        
        _connection->sendJSON(&json);
    }
    
    virtual void stop()
    {
        // nothing to stop
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    Recorder* _recorder;
    string _barcode;
    HTTPConnection* _connection;
    bool _hasStopped;
};


// class to feed source properties from the HTTP POST data
class HTTPPropertySource : public AssetPropertySource
{
public:
    HTTPPropertySource(HTTPConnection* connection)
    : _connection(connection)
    {
    }
    virtual ~HTTPPropertySource() 
    {}
    
    virtual bool haveValue(int assetId, string assetName)
    {
        string fullName = assetName + "-" + int_to_string(assetId);
        return _connection->havePostValue(fullName);
    }
    
    virtual string getValue(int assetId, string assetName)
    {
        string fullName = assetName + "-" + int_to_string(assetId);
        return _connection->getPostValue(fullName);
    }
    
private:
    HTTPConnection* _connection;
};


class UpdateSourceInfoAgent : public ThreadWorker
{
public:
    UpdateSourceInfoAgent(string barcode, HTTPConnection* connection)
    : _barcode(barcode), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~UpdateSourceInfoAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        // check that the source is not being used in a recording session
        if (RecorderDatabase::getInstance()->sessionInProgress(_barcode))
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 1);
            json.setString("errorMessage", "A session is using the source");
            _connection->sendJSON(&json);
            return;
        }
        
        // load source info
        auto_ptr<Source> source(RecorderDatabase::getInstance()->loadSource(_barcode));
        if (source.get() == 0)
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 2);
            json.setString("errorMessage", "Unknown source");
            _connection->sendJSON(&json);
            return;
        }
        else if (source->concreteSources.size() > 1)
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 3);
            json.setString("errorMessage", "Tape with multiple items is not currently supported");
            _connection->sendJSON(&json);
            return;
        }
        Logging::info("Loaded source info '%s' for update\n", _barcode.c_str());
        
        
        // update and validate

        HTTPPropertySource propSource(_connection);
        string errMessage;
        if (!source->concreteSources.front()->parseSourceProperties(&propSource, &errMessage))
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 4);
            json.setString("errorMessage", errMessage);
            _connection->sendJSON(&json);
            return;
        }
        try
        {
            RecorderDatabase::getInstance()->updateConcreteSource(source->concreteSources.front());
        }
        catch (...)
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 5);
            json.setString("errorMessage", "Failed to update source in database");
            _connection->sendJSON(&json);
            return;
        }
        
        Logging::info("Updated source info for '%s'\n", _barcode.c_str());
        
        
        // generate the response
        
        JSONObject json;
        json.setBool("error", false);
        json.setString("barcode", _barcode);
        
        JSONArray* values = json.setArray("values");
        vector<AssetProperty> props = source->concreteSources.front()->getSourceProperties();
        vector<AssetProperty>::const_iterator iter;
        for (iter = props.begin(); iter != props.end(); iter++)
        {
            JSONObject* tv = values->appendObject();
            tv->setString("name", (*iter).name + "-" + int_to_string((*iter).id));
            tv->setString("title", (*iter).title);
            tv->setString("value", (*iter).value);
            tv->setString("type", (*iter).type);
            tv->setNumber("maxSize", (*iter).maxSize);
            tv->setBool("editable", (*iter).editable);
        }
        _connection->sendJSON(&json);
    }
    
    virtual void stop()
    {
        // nothing to stop
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    string _barcode;
    HTTPConnection* _connection;
    bool _hasStopped;
};


class CheckDigibetaAgent : public ThreadWorker
{
public:
    CheckDigibetaAgent(Recorder* recorder, string barcode, HTTPConnection* connection)
    : _recorder(recorder), _barcode(barcode), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~CheckDigibetaAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        // check that it is a valid barcode
        if (!_recorder->isDigibetaBarcode(_barcode))
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("barcode", _barcode);
            json.setNumber("errorCode", 0);
            json.setString("errorMessage", "Barcode is not a Digibeta barcode");
            _connection->sendJSON(&json);
            return;
        }
        
        // generate the response
        
        JSONObject json;
        json.setBool("error", false);
        json.setString("barcode", _barcode);
        
        // set message to warn if the digibeta has been used before in a completed session
        if (RecorderDatabase::getInstance()->digibetaUsedInCompletedSession(_barcode))
        {
            json.setNumber("warningCode", 0);
            json.setString("warningMessage", "Warning: digibeta has been used before");
        }
        
        _connection->sendJSON(&json);
    }
    
    virtual void stop()
    {
        // nothing to stop
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    Recorder* _recorder;
    string _barcode;
    HTTPConnection* _connection;
    bool _hasStopped;
};


class StartSessionAgent : public ThreadWorker
{
public:
    StartSessionAgent(string barcode, string digibetaBarcode, HTTPRecorder* httpRecorder, HTTPConnection* connection)
    : _barcode(barcode), _digibetaBarcode(digibetaBarcode), _httpRecorder(httpRecorder), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~StartSessionAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        
        // get the source information
        auto_ptr<Source> source(RecorderDatabase::getInstance()->loadSource(_barcode));
        if (source.get() == 0)
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("errorMessage", "Unknown source");
            json.setString("barcode", _barcode);
            _connection->sendJSON(&json);
            return;
        }
        Logging::info("Loaded source info '%s' for new session\n", _barcode.c_str());

        
        // create the session
        RecordingSession* session;
        int result = _httpRecorder->getRecorder()->startNewSession(source.release(), _digibetaBarcode, &session);
        if (result != 0)
        {
            // an error has occurred
            JSONObject json;
            json.setBool("error", true);
            json.setNumber("errorCode", result);
            switch (result)
            {
                case SESSION_IN_PROGRESS_FAILURE:
                    json.setString("errorMessage", "Session is already in progress");
                    break;
                case VIDEO_SIGNAL_BAD_FAILURE:
                    json.setString("errorMessage", "Video signal is bad");
                    break;
                case AUDIO_SIGNAL_BAD_FAILURE:
                    json.setString("errorMessage", "Audio signal is bad");
                    break;
                case D3_VTR_CONNECT_FAILURE:
                    json.setString("errorMessage", "No D3 VTR connection");
                    break;
                case D3_VTR_REMOTE_LOCKOUT_FAILURE:
                    json.setString("errorMessage", "D3 VTR remote lockout");
                    break;
                case NO_D3_TAPE:
                    json.setString("errorMessage", "No D3 tape");
                    break;
                case DIGIBETA_VTR_CONNECT_FAILURE:
                    json.setString("errorMessage", "No Digibeta VTR connection");
                    break;
                case DIGIBETA_VTR_REMOTE_LOCKOUT_FAILURE:
                    json.setString("errorMessage", "Digibeta VTR remote lockout");
                    break;
                case NO_DIGIBETA_TAPE:
                    json.setString("errorMessage", "No Digibeta tape");
                    break;
                case DISK_SPACE_FAILURE:
                    json.setString("errorMessage", "Not enough disk space, <" + get_size_string(DISK_SPACE_MARGIN));
                    break;
                case MULTI_ITEM_MXF_EXISTS_FAILURE:
                    json.setString("errorMessage", "Multi-item MXF files already in cache");
                    break;
                case MULTI_ITEM_NOT_ENABLED_FAILURE:
                    json.setString("errorMessage", "Multi-item ingest not enabled");
                    break;
                case INTERNAL_FAILURE:
                default:
                    json.setString("errorMessage", "Internal server error");
                    break;
            }
            json.setString("barcode", _barcode);
            _connection->sendJSON(&json);
            return;
        }
        
        // reset the barcode
        _httpRecorder->newBarcode("");
        
        
        JSONObject json;
        json.setBool("error", false);
        json.setString("barcode", _barcode);
        _connection->sendJSON(&json);
    }
    
    virtual void stop()
    {
        // nothing to stop
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    string _barcode;
    string _digibetaBarcode;
    HTTPRecorder* _httpRecorder;
    HTTPConnection* _connection;
    bool _hasStopped;
};


class CacheContentsAgent : public ThreadWorker
{
public:
    CacheContentsAgent(Recorder* recorder, HTTPConnection* connection)
    : _recorder(recorder), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~CacheContentsAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        
        // load cache contents
        auto_ptr<CacheContents> contents(_recorder->getCache()->getContents());
        if (!contents.get())
        {
            Logging::warning("Failed to load cache contents\n");
            _connection->sendServerError("Failed to load cache contents");
            return;
        }
    
        // get the cache status
        CacheStatus status = _recorder->getCache()->getStatus();

        
        // generate the response
        
        JSONObject json;
        
        json.setString("path", contents->path);
        json.setNumber("statusChangeCount", status.statusChangeCount);
        
        JSONArray* jitems = json.setArray("items");
        vector<CacheContentItem*>::const_iterator iter;
        for (iter = contents->items.begin(); iter != contents->items.end(); iter++)
        {
            JSONObject* tv = jitems->appendObject();
            tv->setNumber("identifier", (*iter)->identifier);
            tv->setString("srcSpoolNo", (*iter)->sourceSpoolNo);
            tv->setNumber("srcItemNo", (*iter)->sourceItemNo);
            tv->setString("srcMPProgNo", get_complete_prog_no((*iter)->sourceMagPrefix, (*iter)->sourceProgNo, (*iter)->sourceProdCode));
            tv->setString("sessionCreation", get_timestamp_string((*iter)->sessionCreation));
            tv->setNumber("sessionStatus", (*iter)->sessionStatus);
            tv->setString("sessionStatusString", get_session_status_string((*iter)->sessionStatus));
            tv->setString("name", (*iter)->name);
            tv->setNumber("size", (*iter)->size);
            tv->setNumber("duration", (*iter)->duration);
            string pseURL = g_pseReportFramedURL;
            pseURL += "?name=" + (*iter)->pseName;
            tv->setString("pseURL", pseURL);
            tv->setNumber("pseResult", (*iter)->pseResult);
        }
        _connection->sendJSON(&json);
    }
    
    virtual void stop()
    {
        // nothing to stop
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    Recorder* _recorder;
    HTTPConnection* _connection;
    bool _hasStopped;
};



class ReplayFileAgent : public ThreadWorker
{
public:
    ReplayFileAgent(Recorder* recorder, HTTPConnection* connection, string filename)
    : _recorder(recorder), _connection(connection), _filename(filename), _hasStopped(false) 
    {}
    
    virtual ~ReplayFileAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        
        int result = _recorder->replayFile(_filename);
        if (result != 0)
        {
            switch (result)
            {
                case SESSION_IN_PROGRESS_REPLAY_FAILURE:
                    _connection->sendBadRequest("Recording session in progress");
                    return;
                case UNKNOWN_FILE_REPLAY_FAILURE:
                    _connection->sendBadRequest("Unknown file");
                    return;
                case INTERNAL_REPLAY_FAILURE:
                default:
                    _connection->sendServerError("Replay failed");
                    return;
            }
        }

        _connection->sendOk();
    }
    
    virtual void stop()
    {
        // nothing to stop
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    Recorder* _recorder;
    HTTPConnection* _connection;
    string _filename;
    bool _hasStopped;
};




HTTPRecorder::HTTPRecorder(HTTPServer* server, Recorder* recorder, BarcodeScanner* scanner)
: _recorder(recorder), _barcodeCount(0), _sourceInfoAgent(0), _checkDigibetaAgent(0), _updateSourceInfoAgent(0),
_startSessionAgent(0), _cacheContentsAgent(0), _replayFileAgent(0)
{
    HTTPServiceDescription* service;
    
    service = server->registerService(new HTTPServiceDescription(g_recorderStatusURL), this);
    service->setDescription("Returns the recorder status");
    service->addArgument("barcode", "boolean", false, "Include barcode scanned in using the barcode scanner");
    service->addArgument("session", "boolean", false, "Include recording session status information");
    service->addArgument("cache", "boolean", false, "Include cache status information");
    service->addArgument("system", "boolean", false, "Include system information");
    
    service = server->registerService(new HTTPServiceDescription(g_sourceInfoURL), this);
    service->setDescription("Returns the source information");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the source item");
    
    service = server->registerService(new HTTPServiceDescription(g_updateSourceInfoURL), this);
    service->setDescription("Updates the source information and returns the validation result and the source information if ok (POST)");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the source item");
    service->addArgument("(field name)*", "string", true, "The list of updated field values");
    
    service = server->registerService(new HTTPServiceDescription(g_checkSelectedDigibetaURL), this);
    service->setDescription("Checks the status of the selected digibeta tape");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the digibeta tape");
    
    service = server->registerService(new HTTPServiceDescription(g_newSessionURL), this);
    service->setDescription("Starts a new recording session");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the source item");
    service->addArgument("digibetabarcode", "string", true, "The Digibeta barcode that uniquely identifies the Digibeta tape");
    
    service = server->registerService(new HTTPServiceDescription(g_sessionSourceInfoURL), this);
    service->setDescription("Returns the source information associated with the current session");
    
    service = server->registerService(new HTTPServiceDescription(g_startRecordingURL), this);
    service->setDescription("Starts recording in the current session");
    
    service = server->registerService(new HTTPServiceDescription(g_stopRecordingURL), this);
    service->setDescription("Stops recording in the current session");
    
    service = server->registerService(new HTTPServiceDescription(g_chunkFileURL), this);
    service->setDescription("Initiates chunking of the file");
    
    service = server->registerService(new HTTPServiceDescription(g_completeSessionURL), this);
    service->setDescription("Complete the current recording session (POST)");
    service->addArgument("comments", "string", true, "Comments related to the completed session");
    
    service = server->registerService(new HTTPServiceDescription(g_abortSessionURL), this);
    service->setDescription("Aborts the current recording session (POST)");
    service->addArgument("comments", "string", true, "Comments related to the aborted session");

    service = server->registerService(new HTTPServiceDescription(g_cacheContentsURL), this);
    service->setDescription("Returns the contents of the cache");
    
    service = server->registerService(new HTTPServiceDescription(g_confReplayURL), this);
    service->setDescription("Forwards control commands to the confidence replay player");
    
    service = server->registerService(new HTTPServiceDescription(g_replayFileURL), this);
    service->setDescription("Plays the file if not busy recording");
    service->addArgument("name", "string", true, "The name of the file in the cache");
    
    service = server->registerService(new HTTPServiceDescription(g_setSessionCommentsURL), this);
    service->setDescription("Sets the session comments (POST)");
    service->addArgument("comments", "string", true, "Comments related to the session");
    
    service = server->registerService(new HTTPServiceDescription(g_getSessionCommentsURL), this);
    service->setDescription("Returns the session comments");
    
    service = server->registerService(new HTTPServiceDescription(g_playItemURL), this);
    service->setDescription("Play the item");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_playPrevItemURL), this);
    service->setDescription("Plays the previous item");
    
    service = server->registerService(new HTTPServiceDescription(g_playNextItemURL), this);
    service->setDescription("Plays the next item");
    
    service = server->registerService(new HTTPServiceDescription(g_seekToEOPURL), this);
    service->setDescription("Seek to the end of the programme in the current item");
    
    service = server->registerService(new HTTPServiceDescription(g_markItemStartURL), this);
    service->setDescription("Mark the start of a item at the current playing position");
    
    service = server->registerService(new HTTPServiceDescription(g_clearItemURL), this);
    service->setDescription("Clear the item at given index");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_moveItemUpURL), this);
    service->setDescription("Move the item up the list of items");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_moveItemDownURL), this);
    service->setDescription("Move the item down the list of items");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_disableItemURL), this);
    service->setDescription("Disable the item and move to the end of the list");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_enableItemURL), this);
    service->setDescription("Enable the item and move to after the last item with clip set");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_getItemClipInfoURL), this);
    service->setDescription("Returns the clip information associated with each item in the session");
    
    service = server->registerService(new HTTPServiceDescription(g_getItemSourceInfoURL), this);
    service->setDescription("Returns the source information associated with each item in the session");
    
    
    server->registerSSIHandler(g_ssiGetPSEReportURL, this);
    
    if (scanner != 0)
    {
        scanner->registerListener(this);
    }
}

HTTPRecorder::~HTTPRecorder()
{
    delete _sourceInfoAgent;
    delete _checkDigibetaAgent;
    delete _updateSourceInfoAgent;
    delete _startSessionAgent;
    delete _cacheContentsAgent;
    delete _replayFileAgent;
}

void HTTPRecorder::newBarcode(string barcode)
{
    LOCK_SECTION(_barcodeMutex);
    
    if (barcode.compare(0, 3, "LTA") == 0)
    {
        // ignore LTO tape barcodes
        return;
    }
    
    _barcode = barcode;
    
    if (barcode.size() > 0)
    {
        _barcodeExpirationTimer.start(BARCODE_EXPIRES_TIME_SEC * SEC_IN_USEC);
        _barcodeCount++;
    }
}

void HTTPRecorder::processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection)
{
    // route the connection to the correct function
    
    if (serviceDescription->getURL().compare(g_recorderStatusURL) == 0)
    {
        getRecorderStatus(connection);
    }
    else if (serviceDescription->getURL().compare(g_sourceInfoURL) == 0)
    {
        getSourceInfo(connection);
    }
    else if (serviceDescription->getURL().compare(g_updateSourceInfoURL) == 0)
    {
        updateSourceInfo(connection);
    }
    else if (serviceDescription->getURL().compare(g_checkSelectedDigibetaURL) == 0)
    {
        checkSelectedDigibeta(connection);
    }
    else if (serviceDescription->getURL().compare(g_newSessionURL) == 0)
    {
        startNewSession(connection);
    }
    else if (serviceDescription->getURL().compare(g_sessionSourceInfoURL) == 0)
    {
        getSessionSourceInfo(connection);
    }
    else if (serviceDescription->getURL().compare(g_startRecordingURL) == 0)
    {
        startRecording(connection);
    }
    else if (serviceDescription->getURL().compare(g_stopRecordingURL) == 0)
    {
        stopRecording(connection);
    }
    else if (serviceDescription->getURL().compare(g_chunkFileURL) == 0)
    {
        chunkFile(connection);
    }
    else if (serviceDescription->getURL().compare(g_completeSessionURL) == 0)
    {
        completeSession(connection);
    }
    else if (serviceDescription->getURL().compare(g_abortSessionURL) == 0)
    {
        abortSession(connection);
    }
    else if (serviceDescription->getURL().compare(g_setSessionCommentsURL) == 0)
    {
        setSessionComments(connection);
    }
    else if (serviceDescription->getURL().compare(g_getSessionCommentsURL) == 0)
    {
        getSessionComments(connection);
    }
    else if (serviceDescription->getURL().compare(g_cacheContentsURL) == 0)
    {
        getCacheContents(connection);
    }
    else if (serviceDescription->getURL().compare(g_confReplayURL) == 0)
    {
        confReplayControl(connection);
    }
    else if (serviceDescription->getURL().compare(g_replayFileURL) == 0)
    {
        replayFile(connection);
    }
    else if (serviceDescription->getURL().compare(g_playItemURL) == 0)
    {
        playItem(connection);
    }
    else if (serviceDescription->getURL().compare(g_playPrevItemURL) == 0)
    {
        playPrevItem(connection);
    }
    else if (serviceDescription->getURL().compare(g_playNextItemURL) == 0)
    {
        playNextItem(connection);
    }
    else if (serviceDescription->getURL().compare(g_seekToEOPURL) == 0)
    {
        seekToEOP(connection);
    }
    else if (serviceDescription->getURL().compare(g_markItemStartURL) == 0)
    {
        markItemStart(connection);
    }
    else if (serviceDescription->getURL().compare(g_clearItemURL) == 0)
    {
        clearItem(connection);
    }
    else if (serviceDescription->getURL().compare(g_moveItemUpURL) == 0)
    {
        moveItemUp(connection);
    }
    else if (serviceDescription->getURL().compare(g_moveItemDownURL) == 0)
    {
        moveItemDown(connection);
    }
    else if (serviceDescription->getURL().compare(g_disableItemURL) == 0)
    {
        disableItem(connection);
    }
    else if (serviceDescription->getURL().compare(g_enableItemURL) == 0)
    {
        enableItem(connection);
    }
    else if (serviceDescription->getURL().compare(g_getItemClipInfoURL) == 0)
    {
        getItemClipInfo(connection);
    }
    else if (serviceDescription->getURL().compare(g_getItemSourceInfoURL) == 0)
    {
        getItemSourceInfo(connection);
    }
    else
    {
        connection->sendBadRequest("Unknown URL");
    }
}

void HTTPRecorder::processSSIRequest(string name, HTTPConnection* connection)
{
    if (name.compare(g_ssiGetPSEReportURL) == 0)
    {
        if (connection->haveQueryValue("name"))
        {
            connection->responseStream() << join_path(g_pseReportsURL, connection->getQueryValue("name"));
        }
        connection->setResponseDataIsReady();
    }
    else
    {
        connection->sendBadRequest("Unknown URL");
    }
}

void HTTPRecorder::getRecorderStatus(HTTPConnection* connection)
{
    bool includeBarcode = false;
    bool includeSessionRecord = false;
    bool includeSessionReview = false;
    bool includeCache = false;
    bool includeSystem = false;
    string barcode;
    int barcodeCount = 0;
    SessionStatus sessionStatus;
    CacheStatus cacheStatus;
    RecorderSystemStatus systemStatus;
    SessionState sessionState;
    SessionResult lastSessionResult = UNKNOWN_SESSION_RESULT;
    string lastSessionSourceSpoolNo;
    string lastSessionFailureReason;

    // process the URL query string
    string barcodeArg = connection->getQueryValue("barcode");
    if (barcodeArg.compare("true") == 0)
    {
        includeBarcode = true;
        
        checkBarcodeStatus();
        
        {
            LOCK_SECTION(_barcodeMutex);
            barcode = _barcode;
            barcodeCount = _barcodeCount;
        }
    }
    string sessionArg = connection->getQueryValue("sessionrecord");
    if (sessionArg.compare("true") == 0)
    {
        includeSessionRecord = true;
    }
    else
    {
        sessionArg = connection->getQueryValue("sessionreview");
        if (sessionArg.compare("true") == 0)
        {
            includeSessionReview = true;
        }
    }
    if (includeSessionRecord || includeSessionReview)
    {
        if (_recorder->haveSession())
        {
            sessionStatus = _recorder->getSessionStatus();
        }
        _recorder->getLastSessionResult(&lastSessionResult, &lastSessionSourceSpoolNo, &lastSessionFailureReason);
    }
    string cacheArg = connection->getQueryValue("cache");
    if (cacheArg.compare("true") == 0)
    {
        includeCache = true;
        cacheStatus = _recorder->getCache()->getStatus();
    }
    string systemArg = connection->getQueryValue("system");
    if (systemArg.compare("true") == 0)
    {
        includeSystem = true;
        systemStatus = _recorder->getSystemStatus();
    }

    
    // get the recorder status
    RecorderStatus status = _recorder->getStatus();
    if (includeSessionRecord || includeSessionReview)
    {
        sessionState = sessionStatus.state;
    }
    else
    {
        sessionState = _recorder->getSessionState();
    }
    
    
    // generate JSON response
    
    JSONObject json;
    json.setString("recorderName", status.recorderName);
    json.setBool("database", status.databaseOk);
    json.setBool("sdiCard", status.sdiCardOk);
    json.setBool("video", status.videoOk);
    json.setBool("audio", status.audioOk);
    json.setBool("vtr", status.vtrOk);
    json.setString("d3VTRState", get_vtr_state_string(status.d3VTRState));
    json.setString("digibetaVTRState", get_vtr_state_string(status.digibetaVTRState));
    json.setBool("readyToRecord", status.readyToRecord);
    if (includeBarcode && barcode.size() > 0)
    {
        json.setString("barcode", barcode);
        json.setNumber("barcodeCount", barcodeCount);
    }
    json.setNumber("sessionState", sessionState);
    if (includeSessionRecord)
    {
        JSONObject* jsonSessionStatus = json.setObject("sessionStatus");
        jsonSessionStatus->setNumber("state", sessionStatus.state);
        jsonSessionStatus->setNumber("itemCount", sessionStatus.itemCount);
        jsonSessionStatus->setNumber("sessionCommentsCount", sessionStatus.sessionCommentsCount);
        jsonSessionStatus->setString("d3SpoolNo", sessionStatus.d3SpoolNo);
        jsonSessionStatus->setString("digibetaSpoolNo", sessionStatus.digibetaSpoolNo);
        jsonSessionStatus->setString("statusMessage", sessionStatus.statusMessage);
        jsonSessionStatus->setString("vtrState", get_vtr_state_string(sessionStatus.vtrState));
        jsonSessionStatus->setNumber("vtrErrorCount", sessionStatus.vtrErrorCount);
        jsonSessionStatus->setString("startVITC", sessionStatus.startVITC.toString());
        jsonSessionStatus->setString("startLTC", sessionStatus.startLTC.toString());
        jsonSessionStatus->setString("currentVITC", sessionStatus.currentVITC.toString());
        jsonSessionStatus->setString("currentLTC", sessionStatus.currentLTC.toString());
        jsonSessionStatus->setNumber("duration", sessionStatus.duration);
        jsonSessionStatus->setNumber("infaxDuration", sessionStatus.infaxDuration);
        jsonSessionStatus->setString("filename", sessionStatus.filename);
        jsonSessionStatus->setNumber("fileSize", sessionStatus.fileSize);
        jsonSessionStatus->setNumber("diskSpace", sessionStatus.diskSpace);
        jsonSessionStatus->setString("browseFilename", sessionStatus.browseFilename);
        jsonSessionStatus->setNumber("browseFileSize", sessionStatus.browseFileSize);
        jsonSessionStatus->setBool("browseBufferOverflow", sessionStatus.browseBufferOverflow);
        jsonSessionStatus->setBool("startBusy", sessionStatus.startBusy);
        jsonSessionStatus->setBool("stopBusy", sessionStatus.stopBusy);
        jsonSessionStatus->setBool("abortBusy", sessionStatus.abortBusy);
    }
    else if (includeSessionReview)
    {
        JSONObject* jsonSessionStatus = json.setObject("sessionStatus");
        jsonSessionStatus->setNumber("state", sessionStatus.state);
        jsonSessionStatus->setNumber("itemCount", sessionStatus.itemCount);
        jsonSessionStatus->setBool("readyToChunk", sessionStatus.readyToChunk);
        jsonSessionStatus->setNumber("sessionCommentsCount", sessionStatus.sessionCommentsCount);
        jsonSessionStatus->setString("d3SpoolNo", sessionStatus.d3SpoolNo);
        jsonSessionStatus->setString("digibetaSpoolNo", sessionStatus.digibetaSpoolNo);
        jsonSessionStatus->setString("statusMessage", sessionStatus.statusMessage);
        jsonSessionStatus->setNumber("vtrErrorCount", sessionStatus.vtrErrorCount);
        jsonSessionStatus->setNumber("pseResult", sessionStatus.pseResult);
        jsonSessionStatus->setNumber("duration", sessionStatus.duration);
        jsonSessionStatus->setNumber("infaxDuration", sessionStatus.infaxDuration);
        jsonSessionStatus->setBool("chunkBusy", sessionStatus.chunkBusy);
        jsonSessionStatus->setBool("completeBusy", sessionStatus.completeBusy);
        jsonSessionStatus->setBool("abortBusy", sessionStatus.abortBusy);
        jsonSessionStatus->setNumber("playingItemId", sessionStatus.playingItemId);
        jsonSessionStatus->setNumber("playingItemIndex", sessionStatus.playingItemIndex);
        jsonSessionStatus->setNumber("playingItemPosition", sessionStatus.playingItemPosition);
        jsonSessionStatus->setNumber("playingFilePosition", sessionStatus.playingFilePosition);
        jsonSessionStatus->setNumber("playingFileDuration", sessionStatus.playingFileDuration);
        jsonSessionStatus->setNumber("itemClipChangeCount", sessionStatus.itemClipChangeCount);
        jsonSessionStatus->setNumber("itemSourceChangeCount", sessionStatus.itemSourceChangeCount);
        jsonSessionStatus->setNumber("chunkingItemNumber", sessionStatus.chunkingItemNumber);
        jsonSessionStatus->setNumber("chunkingPosition", sessionStatus.chunkingPosition);
        jsonSessionStatus->setNumber("chunkingDuration", sessionStatus.chunkingDuration);
        jsonSessionStatus->setNumber("lastSessionResult", lastSessionResult);
        jsonSessionStatus->setString("lastSessionSourceSpoolNo", lastSessionSourceSpoolNo);
        jsonSessionStatus->setString("lastSessionFailureReason", lastSessionFailureReason);
    }
    if (includeCache)
    {
        json.setNumber("numCacheItems", cacheStatus.numItems);
        json.setNumber("statusChangeCount", cacheStatus.statusChangeCount);
        json.setBool("replayActive", status.replayActive);
        json.setString("replayFilename", status.replayFilename);
    }
    if (includeSystem)
    {
        json.setString("version", get_version());
        json.setString("buildDate", get_build_date());
        json.setNumber("numAudioTracks", systemStatus.numAudioTracks);
        json.setNumber("diskSpace", systemStatus.remDiskSpace);
        json.setNumber("recordingTime", systemStatus.remDuration);
    }

    connection->sendJSON(&json);
}

void HTTPRecorder::getSourceInfo(HTTPConnection* connection)
{
    LOCK_SECTION(_sourceInfoAgentMutex);
    
    if (_sourceInfoAgent != 0 &&
        _sourceInfoAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_sourceInfoAgent != 0)
    {
        SAFE_DELETE(_sourceInfoAgent);
    }

    // process the URL and get the barcode argument
    if (!connection->haveQueryValue("barcode"))
    {
        // no barcode query argument or zero length barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", "");
        json.setNumber("errorCode", 0);
        json.setString("errorMessage", "Barcode is empty string");
        connection->sendJSON(&json);
        return;
    }

    
    // start the agent
    _sourceInfoAgent = new Thread(new SourceInfoAgent(_recorder, connection->getQueryValue("barcode"), 
        connection), true);
    _sourceInfoAgent->start();
}

void HTTPRecorder::updateSourceInfo(HTTPConnection* connection)
{
    LOCK_SECTION(_startSessionOrUpdateSourceInfoAgentMutex);
    
    // cannot update source info if starting a new session or already busy
    if (_updateSourceInfoAgent != 0 &&
        _updateSourceInfoAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    if (_startSessionAgent != 0 &&
        _startSessionAgent->isRunning())
    {
        connection->sendServerBusy("Server is about to start a new session");
        return;
    }
    
    // clean-up
    if (_updateSourceInfoAgent != 0)
    {
        SAFE_DELETE(_updateSourceInfoAgent);
    }
    if (_startSessionAgent != 0)
    {
        SAFE_DELETE(_startSessionAgent);
    }
    
    // check that the barcode argument is present
    if (!connection->havePostValue("barcode"))
    {
        // no barcode query argument or zero length barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", "");
        json.setNumber("errorCode", 0);
        json.setString("errorMessage", "Barcode is empty string");
        connection->sendJSON(&json);
        return;
    }

    
    // start the agent
    _updateSourceInfoAgent = new Thread(new UpdateSourceInfoAgent(connection->getPostValue("barcode"), 
        connection), true);
    _updateSourceInfoAgent->start();
}

void HTTPRecorder::checkSelectedDigibeta(HTTPConnection* connection)
{
    LOCK_SECTION(_checkDigibetaAgentMutex);
    
    if (_checkDigibetaAgent != 0 &&
        _checkDigibetaAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_checkDigibetaAgent != 0)
    {
        SAFE_DELETE(_checkDigibetaAgent);
    }

    // process the URL and get the barcode argument
    string barcode = connection->getQueryValue("barcode");
    if (!_recorder->isDigibetaBarcode(barcode))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", barcode);
        json.setNumber("errorCode", 0);
        json.setString("errorMessage", "Barcode is not a Digibeta barcode");
        connection->sendJSON(&json);
        return;
    }
    
    
    // start the agent
    _checkDigibetaAgent = new Thread(new CheckDigibetaAgent(_recorder, barcode, connection), true);
    _checkDigibetaAgent->start();
}

void HTTPRecorder::startNewSession(HTTPConnection* connection)
{
    LOCK_SECTION(_startSessionOrUpdateSourceInfoAgentMutex);
    
    // cannot start a new session if updating source info or already busy
    if (_updateSourceInfoAgent != 0 &&
        _updateSourceInfoAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the source info update");
        return;
    }
    if (_startSessionAgent != 0 &&
        _startSessionAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_updateSourceInfoAgent != 0)
    {
        SAFE_DELETE(_updateSourceInfoAgent);
    }
    if (_startSessionAgent != 0)
    {
        SAFE_DELETE(_startSessionAgent);
    }

    // process the URL and get the barcode argument
    if (!connection->haveQueryValue("barcode"))
    {
        // no barcode query argument or zero length barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Missing 'barcode' argument");
        json.setString("barcode", "");
        connection->sendJSON(&json);
        return;
    }
    if (!connection->haveQueryValue("digibetabarcode"))
    {
        // no digibeta barcode query argument or zero length digibeta barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Missing 'digibetabarcode' argument");
        json.setString("barcode", "");
        connection->sendJSON(&json);
        return;
    }

    
    // start the agent
    _startSessionAgent = new Thread(new StartSessionAgent(connection->getQueryValue("barcode"),
        connection->getQueryValue("digibetabarcode"), this, connection), true);
    _startSessionAgent->start();
}

void HTTPRecorder::getSessionSourceInfo(HTTPConnection* connection)
{
    RecordingItems* recordingItems = _recorder->getSessionRecordingItems();
    if (recordingItems == 0)
    {
        // no session in progress
        connection->sendBadRequest("No session in progress");
        return;
    }
    

    // get the items and change counts
    
    vector<RecordingItem> items;
    int itemSourceChangeCount;
    int itemClipChangeCount;
    recordingItems->getItems(&items, &itemClipChangeCount, &itemSourceChangeCount);
    
    
    // generate the response
        
    JSONObject json;
    json.setNumber("itemClipChangeCount", itemClipChangeCount);
    json.setNumber("itemSourceChangeCount", itemSourceChangeCount);
    
    JSONArray* itemArray = 0;
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        const RecordingItem& item = *iter;
        
        if (iter == items.begin())
        {
            JSONObject* tapeInfo = json.setObject("tapeInfo");
            tapeInfo->setString("format", item.d3Source->format);
            tapeInfo->setString("spoolNo", item.d3Source->spoolNo);
            tapeInfo->setString("accNo", item.d3Source->accNo);
            tapeInfo->setString("spoolStatus", item.d3Source->spoolStatus);
            tapeInfo->setString("stockDate", item.d3Source->stockDate.toString());
            tapeInfo->setNumber("totalInfaxDuration", recordingItems->getTotalInfaxDuration());

            itemArray = json.setArray("items");
        }
        
        JSONObject* items = itemArray->appendObject();
        items->setNumber("id", item.id);
        items->setNumber("index", item.index);
        items->setBool("isDisabled", item.isDisabled);
        items->setBool("isJunk", item.isJunk);
        items->setNumber("itemStartPosition", item.startPosition);
        items->setNumber("itemDuration", item.duration);
        if (!item.isJunk)
        {
            items->setString("progTitle", item.d3Source->progTitle);
            items->setString("spoolDescr", item.d3Source->spoolDescr);
            items->setString("episodeTitle", item.d3Source->episodeTitle);
            items->setString("txDate", item.d3Source->txDate.toString());
            items->setString("progNo", get_complete_prog_no(item.d3Source->magPrefix, item.d3Source->progNo, item.d3Source->prodCode));
            items->setNumber("infaxDuration", item.d3Source->duration);
            items->setString("catDetail", item.d3Source->catDetail);
            items->setString("memo", item.d3Source->memo);
            items->setNumber("itemNo", item.d3Source->itemNo);
        }
    }
    
    connection->sendJSON(&json);
}

void HTTPRecorder::startRecording(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->startRecording();
    connection->sendOk();
}

void HTTPRecorder::stopRecording(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->stopRecording();
    connection->sendOk();
}

void HTTPRecorder::chunkFile(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    _recorder->chunkFile();
    connection->sendOk();
}

void HTTPRecorder::completeSession(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    string comments = connection->getPostValue("comments");
    
    _recorder->completeSession(comments);
    connection->sendOk();
}

void HTTPRecorder::abortSession(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    string comments = connection->getPostValue("comments");

    _recorder->abortSession(true, comments);
    connection->sendOk();
}

void HTTPRecorder::setSessionComments(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    string comments = connection->getPostValue("comments");

    _recorder->setSessionComments(comments);
    connection->sendOk();
}

void HTTPRecorder::getSessionComments(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    string comments;
    int count;
    if (!_recorder->getSessionComments(&comments, &count))
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    JSONObject json;
    json.setString("comments", comments);    
    json.setNumber("count", count);    

    connection->sendJSON(&json);
}

void HTTPRecorder::getCacheContents(HTTPConnection* connection)
{
    LOCK_SECTION(_cacheContentsAgentMutex);
    
    if (_cacheContentsAgent != 0 &&
        _cacheContentsAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_cacheContentsAgent != 0)
    {
        SAFE_DELETE(_cacheContentsAgent);
    }

    
    // start the agent
    _cacheContentsAgent = new Thread(new CacheContentsAgent(_recorder, connection), true);
    _cacheContentsAgent->start();
}

void HTTPRecorder::confReplayControl(HTTPConnection* connection)
{
    if (!_recorder->confidenceReplayActive())
    {
        connection->sendBadRequest("Confidence replay is not active");
        return;
    }
    
    // get the command
    string requestURI = connection->getRequestURI();
    size_t prefixIndex;
    if ((prefixIndex = requestURI.find("confreplay/")) == string::npos)
    {
        connection->sendServerError("Unexpected URL format");
        return;
    }
    string command = requestURI.substr(prefixIndex + strlen("confreplay/"));
    
    // add the query string
    string queryString = connection->getQueryString();
    if (queryString.size() > 0)
    {
        command += "?" + queryString;
    }

    // forward to replay player    
    _recorder->forwardConfidenceReplayControl(command);
    connection->sendOk();
}

void HTTPRecorder::replayFile(HTTPConnection* connection)
{
    LOCK_SECTION(_replayFileAgentMutex);
    
    if (_replayFileAgent != 0 &&
        _replayFileAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_replayFileAgent != 0)
    {
        SAFE_DELETE(_replayFileAgent);
    }

    // check for the 'name' argument
    if (!connection->haveQueryValue("name"))
    {
        connection->sendBadRequest("Missing 'name' argument");
        return;
    }
    
    // start the agent
    _replayFileAgent = new Thread(new ReplayFileAgent(_recorder, connection, connection->getQueryValue("name")), true);
    _replayFileAgent->start();
}

void HTTPRecorder::playItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    _recorder->playItem(id, index);
    connection->sendOk();
}

void HTTPRecorder::playPrevItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->playPrevItem();
    connection->sendOk();
}

void HTTPRecorder::playNextItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->playNextItem();
    connection->sendOk();
}

void HTTPRecorder::seekToEOP(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->seekToEOP();
    connection->sendOk();
}

void HTTPRecorder::markItemStart(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int itemId;
    bool isJunk;
    int64_t filePosition;
    int64_t fileDuration;
    int changeCount = _recorder->markItemStart(&itemId, &isJunk, &filePosition, &fileDuration);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to mark item start");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemId", itemId);
    json.setBool("isJunk", isJunk);
    json.setNumber("filePosition", filePosition);
    json.setNumber("fileDuration", fileDuration);
    json.setNumber("itemClipChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::clearItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int64_t filePosition;
    int changeCount = _recorder->clearItem(id, index, &filePosition);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to clear item");
        return;
    }

    JSONObject json;
    json.setNumber("itemId", id);
    json.setNumber("filePosition", filePosition);
    json.setNumber("itemClipChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::moveItemUp(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->moveItemUp(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to move item up");
        return;
    }
        
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::moveItemDown(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->moveItemDown(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to move item down");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::disableItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->disableItem(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to disable item");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::enableItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->enableItem(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to enable item");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::getItemClipInfo(HTTPConnection* connection)
{
    RecordingItems* recordingItems = _recorder->getSessionRecordingItems();
    if (recordingItems == 0)
    {
        // no session in progress
        connection->sendBadRequest("No session in progress");
        return;
    }
    

    // get the items and change counts
    
    vector<RecordingItem> items;
    int itemSourceChangeCount;
    int itemClipChangeCount;
    recordingItems->getItems(&items, &itemClipChangeCount, &itemSourceChangeCount);
    
    
    // generate the response
        
    JSONObject json;
    json.setNumber("itemClipChangeCount", itemClipChangeCount);
    json.setNumber("itemSourceChangeCount", itemSourceChangeCount);

    JSONArray* itemArray = json.setArray("items");
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        const RecordingItem& item = *iter;

        JSONObject* items = itemArray->appendObject();
        items->setNumber("id", item.id);
        items->setNumber("index", item.index);
        items->setBool("isJunk", item.isJunk);
        items->setNumber("itemStartPosition", item.startPosition);
        items->setNumber("itemDuration", item.duration);
    }
    
    connection->sendJSON(&json);
}

void HTTPRecorder::getItemSourceInfo(HTTPConnection* connection)
{
    RecordingItems* recordingItems = _recorder->getSessionRecordingItems();
    if (recordingItems == 0)
    {
        // no session in progress
        connection->sendBadRequest("No session in progress");
        return;
    }
    

    // get the items and change counts
    
    vector<RecordingItem> items;
    int itemSourceChangeCount;
    int itemClipChangeCount;
    recordingItems->getItems(&items, &itemClipChangeCount, &itemSourceChangeCount);
    
    
    // generate the response
        
    JSONObject json;
    json.setNumber("itemClipChangeCount", itemClipChangeCount);
    json.setNumber("itemSourceChangeCount", itemSourceChangeCount);

    JSONArray* itemArray = json.setArray("items");
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        const RecordingItem& item = *iter;

        JSONObject* items = itemArray->appendObject();
        items->setNumber("id", item.id);
        items->setNumber("index", item.index);
    }
    
    connection->sendJSON(&json);
}

Recorder* HTTPRecorder::getRecorder()
{
    return _recorder;
}

string HTTPRecorder::getPSEReportsURL()
{
    return g_pseReportsURL;
}

void HTTPRecorder::checkBarcodeStatus()
{
    LOCK_SECTION(_barcodeMutex);
    
    if (_barcode.size() > 0 && _barcodeExpirationTimer.timeLeft() == 0)
    {
        _barcode = "";
    }
}


