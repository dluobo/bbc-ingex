/*
 * $Id: HTTPTapeExport.cpp,v 1.1 2008/07/08 16:26:04 philipn Exp $
 *
 * HTTP interface to the tape export
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
    the client browser using a barcode update count in the tape export status 
    info.
    
    PSE reports are displayed in a HTML frame to allow users to easily print 
    and go back to the previous page using a touchscreen monitor. 
*/
 
#include "HTTPTapeExport.h"
#include "RecorderDatabase.h"
#include "JSONObject.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "version.h"


using namespace std;
using namespace rec;


// a barcode expires (ie. will not be sent to a client) after ... seconds
#define BARCODE_EXPIRES_TIME_SEC        30

// all the HTTP service URLs handled by the HTTPTapeExport
static const char* g_tapeExportStatusURL = "/tape/status.json";
static const char* g_checkSelectionURL = "/tape/checkselection.json";
static const char* g_cacheContentsURL = "/tape/cache/contents.json";
static const char* g_deleteCacheItemsURL = "/tape/cache/deleteitems";
static const char* g_newAutoSessionURL = "/tape/newautosession.json";
static const char* g_newManualSessionURL = "/tape/newmanualsession.json";
static const char* g_abortSessionURL = "/tape/session/abort";
static const char* g_ltoContentsURL = "/tape/session/ltocontents.json";

static const char* g_pseReportsURL = "/psereports";
static const char* g_pseReportFramedURL = "/psereport_framed.shtml";

static const char* g_ssiGetPSEReportURL = "get_pse_report_url";




static string get_timestamp_string(Timestamp ts)
{
    char buf[128];

    buf[0] = '\0';
    
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hour, ts.min, ts.sec);
        
    return buf;
}

static string get_time_string(Timestamp ts)
{
    char buf[128];

    buf[0] = '\0';
    
    sprintf(buf, "%02d:%02d:%02d", ts.hour, ts.min, ts.sec);
        
    return buf;
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




class CheckBarcodeSelectionAgent : public ThreadWorker
{
public:
    CheckBarcodeSelectionAgent(TapeExport* tapeExport, string barcode, HTTPConnection* connection)
    : _tapeExport(tapeExport), _barcode(barcode), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~CheckBarcodeSelectionAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        // check that it is a valid barcode
        if (!_tapeExport->isLTOBarcode(_barcode))
        {
            JSONObject json;
            json.setBool("accepted", false);
            json.setString("barcode", _barcode);
            json.setString("message", "Barcode is not a LTO barcode");
            _connection->sendJSON(&json);
            return;
        }
        // check the LTO barcode hasn't been used before in a completed session
        if (RecorderDatabase::getInstance()->ltoUsedInCompletedSession(_barcode))
        {
            JSONObject json;
            json.setBool("accepted", false);
            json.setString("barcode", _barcode);
            json.setString("message", "Barcode has been used in a completed session");
            _connection->sendJSON(&json);
            return;
        }
        
        // generate the response
        
        JSONObject json;
        json.setBool("accepted", true);
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
    TapeExport* _tapeExport;
    string _barcode;
    HTTPConnection* _connection;
    bool _hasStopped;
};


class CacheContentsAgent : public ThreadWorker
{
public:
    CacheContentsAgent(TapeExport* tapeExport, HTTPConnection* connection)
    : _tapeExport(tapeExport), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~CacheContentsAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        
        // load cache contents
        auto_ptr<CacheContents> contents(_tapeExport->getCache()->getContents());
        if (!contents.get())
        {
            Logging::warning("Failed to load cache contents\n");
            _connection->sendServerError("Failed to load cache contents");
            return;
        }
    
        // get the cache status
        CacheStatus status = _tapeExport->getCache()->getStatus();
        
        
        // get the files currently being transferred to LTO
        set<string> sessionFileNames = _tapeExport->getSessionFileNames();
        
        
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
            tv->setString("name", (*iter)->name);
            tv->setNumber("size", (*iter)->size);
            tv->setNumber("duration", (*iter)->duration);
            string pseURL = g_pseReportFramedURL;
            pseURL += "?name=" + (*iter)->pseName;
            tv->setString("pseURL", pseURL);
            tv->setNumber("pseResult", (*iter)->pseResult);
           
            if (sessionFileNames.find((*iter)->name) != sessionFileNames.end())
            {
                tv->setBool("locked", true);
            }
            else
            {
                tv->setBool("locked", false);
            }
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
    TapeExport* _tapeExport;
    HTTPConnection* _connection;
    bool _hasStopped;
};


class DeleteCacheItemsAgent : public ThreadWorker
{
public:
    DeleteCacheItemsAgent(vector<long> items, TapeExport* tapeExport, HTTPConnection* connection)
    : _items(items), _tapeExport(tapeExport), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~DeleteCacheItemsAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        string name;
        vector<long>::const_iterator iter;
        for (iter = _items.begin(); iter != _items.end(); iter++)
        {
            name = _tapeExport->getCache()->getItemName(*iter);
            if (name.size() == 0)
            {
                Logging::warning("Not deleting unknown file %ld from cache requested by client\n", (*iter));
                continue;
            }
            
            if (_tapeExport->isFileUsedInSession(name))
            {
                Logging::warning("Not deleting cache file '%s' requested by client because it is being used in a transfer session\n", name.c_str());
            }
            else if (_tapeExport->getCache()->removeItem(name))
            {
                Logging::info("Deleted cache file '%s' requested by the client\n", name.c_str());
            }
            else
            {
                Logging::warning("Failed to delete cache file '%s' requested by the client\n", name.c_str());
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
    vector<long> _items;
    TapeExport* _tapeExport;
    HTTPConnection* _connection;
    bool _hasStopped;
};



class StartAutoSessionAgent : public ThreadWorker
{
public:
    StartAutoSessionAgent(string barcode, HTTPTapeExport* httpTapeExport, HTTPConnection* connection)
    : _barcode(barcode), _httpTapeExport(httpTapeExport), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~StartAutoSessionAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        // create the session
        TapeExportSession* session;
        int result;
        if ((result = _httpTapeExport->getTapeExport()->startNewAutoSession(_barcode, &session)) != 0)
        {
            // a error has occurred
            JSONObject json;
            json.setBool("error", true);
            switch (result)
            {
                case SESSION_IN_PROGRESS_FAILURE:
                    json.setString("errorMessage", "Session is already in progress");
                    break;
                case INVALID_BARCODE_FAILURE:
                    json.setString("errorMessage", "Barcode is invalid");
                    break;
                case TAPE_NOT_READY_FAILURE:
                    json.setString("errorMessage", "Tape is not ready");
                    break;
                case BARCODE_USED_BEFORE_FAILURE:
                    json.setString("errorMessage", "Barcode has been used in a completed session");
                    break;
                default:
                    json.setString("errorMessage", "Internal server error");
                    break;
            }
            json.setString("barcode", _barcode);
            _connection->sendJSON(&json);
            return;
        }
        
        // reset the barcode
        _httpTapeExport->newBarcode("");
        
        
        JSONObject json;
        json.setBool("error", false);
        json.setString("barcode", _barcode);
        json.setString("redirect", "/transfer.html");
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
    HTTPTapeExport* _httpTapeExport;
    HTTPConnection* _connection;
    bool _hasStopped;
};


class StartManualSessionAgent : public ThreadWorker
{
public:
    StartManualSessionAgent(string barcode, vector<long> itemIds, HTTPTapeExport* httpTapeExport, HTTPConnection* connection)
    : _barcode(barcode), _itemIds(itemIds), _httpTapeExport(httpTapeExport), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~StartManualSessionAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        // create the session
        TapeExportSession* session;
        int result;
        if ((result = _httpTapeExport->getTapeExport()->startNewManualSession(_barcode, _itemIds, &session)) != 0)
        {
            // a error has occurred
            JSONObject json;
            json.setBool("error", true);
            switch (result)
            {
                case SESSION_IN_PROGRESS_FAILURE:
                    json.setString("errorMessage", "Session is already in progress");
                    break;
                case INVALID_BARCODE_FAILURE:
                    json.setString("errorMessage", "Barcode is invalid");
                    break;
                case EMPTY_ITEM_LIST_FAILURE:
                    json.setString("errorMessage", "Item list is empty");
                    break;
                case MAX_SIZE_EXCEEDED_FAILURE:
                    json.setString("errorMessage", "Total size of item list exceeds maximum");
                    break;
                case ZERO_SIZE_FAILURE:
                    json.setString("errorMessage", "Total size of item list is zero");
                    break;
                case UNKNOWN_ITEM_FAILURE:
                    json.setString("errorMessage", "Item list contains an unknown item");
                    break;
                case TAPE_NOT_READY_FAILURE:
                    json.setString("errorMessage", "Tape is not ready");
                    break;
                case BARCODE_USED_BEFORE_FAILURE:
                    json.setString("errorMessage", "Barcode has been used in a completed session");
                    break;
                default:
                    json.setString("errorMessage", "Internal server error");
                    break;
            }
            json.setString("barcode", _barcode);
            _connection->sendJSON(&json);
            return;
        }
        
        // reset the barcode
        _httpTapeExport->newBarcode("");
        
        
        JSONObject json;
        json.setBool("error", false);
        json.setString("barcode", _barcode);
        json.setString("redirect", "/transfer.html");
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
    vector<long> _itemIds;
    HTTPTapeExport* _httpTapeExport;
    HTTPConnection* _connection;
    bool _hasStopped;
};


class LTOContentsAgent : public ThreadWorker
{
public:
    LTOContentsAgent(TapeExport* tapeExport, HTTPConnection* connection)
    : _tapeExport(tapeExport), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~LTOContentsAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        
        // load lto contents
        auto_ptr<LTOContents> contents(_tapeExport->getLTOContents());
        if (!contents.get())
        {
            _connection->sendServerError("No LTO contents available");
            return;
        }
    
        // generate the response
        
        JSONObject json;
        
        json.setNumber("ltoStatusChangeCount", contents->ltoStatusChangeCount);
        
        JSONArray* jitems = json.setArray("items");
        vector<LTOFile>::const_iterator iter;
        for (iter = contents->ltoFiles.begin(); iter != contents->ltoFiles.end(); iter++)
        {
            JSONObject* tv = jitems->appendObject();
            tv->setNumber("status", (*iter).status);
            tv->setString("transferStarted", ((*iter).transferStarted == g_nullTimestamp) ? 
                "" : get_time_string((*iter).transferStarted));
            tv->setString("transferEnded", ((*iter).transferEnded == g_nullTimestamp) ?
                "" : get_time_string((*iter).transferEnded));
            tv->setString("name", (*iter).name);
            tv->setString("cacheName", (*iter).cacheName);
            tv->setNumber("size", (*iter).size);
            tv->setNumber("duration", (*iter).duration);
            tv->setString("srcSpoolNo", (*iter).sourceSpoolNo);
            tv->setNumber("srcItemNo", (*iter).sourceItemNo);
            tv->setString("srcMPProgNo", get_complete_prog_no((*iter).sourceMagPrefix, (*iter).sourceProgNo, (*iter).sourceProdCode));
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
    TapeExport* _tapeExport;
    HTTPConnection* _connection;
    bool _hasStopped;
};





HTTPTapeExport::HTTPTapeExport(HTTPServer* server, TapeExport* tapeExport, BarcodeScanner* scanner)
: _checkBarcodeSelectionAgent(0), _startSessionAgent(0), _cacheContentsAgent(0), _deleteCacheItemsAgent(0),
_ltoContentsAgent(0), _tapeExport(tapeExport), _barcodeCount(0)
{
    HTTPServiceDescription* service;
    
    service = server->registerService(new HTTPServiceDescription(g_tapeExportStatusURL), this);
    service->setDescription("Returns the tape export status");
    service->addArgument("barcode", "boolean", false, "Include barcode scanned in using the barcode scanner");
    service->addArgument("session", "boolean", false, "Include session status information");
    
    service = server->registerService(new HTTPServiceDescription(g_checkSelectionURL), this);
    service->setDescription("Checks the barcode is an LTO tape barcode");
    service->addArgument("barcode", "string", true, "The LTO tape barcode");
    
    service = server->registerService(new HTTPServiceDescription(g_cacheContentsURL), this);
    service->setDescription("Returns the contents of the cache");
    
    service = server->registerService(new HTTPServiceDescription(g_deleteCacheItemsURL), this);
    service->setDescription("Delete items from the cache (POST)");
    service->addArgument("items", "list of long", true, "A comma separated list of item identifiers");
    
    service = server->registerService(new HTTPServiceDescription(g_newAutoSessionURL), this);
    service->setDescription("Starts a new automatic transfer session");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the LTO tape");
    
    service = server->registerService(new HTTPServiceDescription(g_newManualSessionURL), this);
    service->setDescription("Starts a new manual transfer session");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the LTO tape");
    service->addArgument("items", "list of long", true, "A comma separated list of item identifiers");
    
    service = server->registerService(new HTTPServiceDescription(g_abortSessionURL), this);
    service->setDescription("Aborts the current transfer session (POST)");
    service->addArgument("comments", "string", true, "Comments related to the aborted session");

    service = server->registerService(new HTTPServiceDescription(g_ltoContentsURL), this);
    service->setDescription("Returns the contents of the LTO");
    
    server->registerSSIHandler(g_ssiGetPSEReportURL, this);
    
    if (scanner != 0)
    {
        scanner->registerListener(this);
    }
}

HTTPTapeExport::~HTTPTapeExport()
{
    delete _checkBarcodeSelectionAgent;
    delete _startSessionAgent;
    delete _cacheContentsAgent;
    delete _deleteCacheItemsAgent;
    delete _ltoContentsAgent;
}

void HTTPTapeExport::newBarcode(string barcode)
{
    LOCK_SECTION(_barcodeMutex);
    
    if (!_tapeExport->isLTOBarcode(barcode))
    {
        // ignore non-LTO tape barcodes
        return;
    }

    _barcode = barcode;
    
    if (barcode.size() > 0)
    {
        _barcodeExpirationTimer.start(BARCODE_EXPIRES_TIME_SEC * SEC_IN_USEC);
        _barcodeCount++;
    }
}

void HTTPTapeExport::processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection)
{
    // route the connection to the correct function
    
    if (serviceDescription->getURL().compare(g_tapeExportStatusURL) == 0)
    {
        getTapeExportStatus(connection);
    }
    else if (serviceDescription->getURL().compare(g_checkSelectionURL) == 0)
    {
        checkBarcodeSelection(connection);
    }
    else if (serviceDescription->getURL().compare(g_cacheContentsURL) == 0)
    {
        getCacheContents(connection);
    }
    else if (serviceDescription->getURL().compare(g_deleteCacheItemsURL) == 0)
    {
        deleteCacheItems(connection);
    }
    else if (serviceDescription->getURL().compare(g_newAutoSessionURL) == 0)
    {
        startNewAutoSession(connection);
    }
    else if (serviceDescription->getURL().compare(g_newManualSessionURL) == 0)
    {
        startNewManualSession(connection);
    }
    else if (serviceDescription->getURL().compare(g_abortSessionURL) == 0)
    {
        abortSession(connection);
    }
    else if (serviceDescription->getURL().compare(g_ltoContentsURL) == 0)
    {
        getLTOContents(connection);
    }
    else
    {
        connection->sendBadRequest("Unknown URL");
    }
}

void HTTPTapeExport::processSSIRequest(string name, HTTPConnection* connection)
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

void HTTPTapeExport::getTapeExportStatus(HTTPConnection* connection)
{
    bool includeBarcode = false;
    bool includeSession = false;
    bool includeSystem = false;
    bool includeCache = false;
    bool includePrepare = false;
    string barcode;
    int barcodeCount = 0;
    SessionStatus sessionStatus;
    TapeExportSystemStatus systemStatus;
    CacheStatus cacheStatus;

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
    string sessionArg = connection->getQueryValue("session");
    if (sessionArg.compare("true") == 0)
    {
        includeSession = true;
        sessionStatus = _tapeExport->getSessionStatus();
    }
    string cacheArg = connection->getQueryValue("cache");
    if (cacheArg.compare("true") == 0)
    {
        includeCache = true;
        cacheStatus = _tapeExport->getCache()->getStatus();
    }
    string systemArg = connection->getQueryValue("system");
    if (systemArg.compare("true") == 0)
    {
        includeSystem = true;
        systemStatus = _tapeExport->getSystemStatus();
    }
    string prepareArg = connection->getQueryValue("prepare");
    if (prepareArg.compare("true") == 0)
    {
        includePrepare = true;
    }

    
    // get the tape export status
    TapeExportStatus status = _tapeExport->getStatus();
    
    
    // generate JSON response
    
    JSONObject json;
    json.setString("recorderName", status.recorderName);
    json.setBool("database", status.databaseOk);
    json.setBool("tapeDevice", status.tapeDeviceOk);
    json.setBool("sessionInProgress", status.sessionInProgress);
    json.setNumber("tapeDevStatus", status.tapeDevStatus);
    json.setString("tapeDevDetailedStatus", status.tapeDevDetailedStatus);
    json.setBool("readyToExport", status.readyToExport);
    if (includeBarcode && barcode.size() > 0)
    {
        json.setString("barcode", barcode);
        json.setNumber("barcodeCount", barcodeCount);
    }
    if (includeSession)
    {
        if (status.sessionInProgress)
        {
            JSONObject* jsonSessionStatus = json.setObject("sessionStatus");
            jsonSessionStatus->setString("spoolNum", sessionStatus.barcode);
            jsonSessionStatus->setBool("autoTransferMethod", sessionStatus.autoTransferMethod);
            jsonSessionStatus->setBool("autoSelectionComplete", sessionStatus.autoSelectionComplete);
            jsonSessionStatus->setBool("startedTransfer", sessionStatus.startedTransfer);
            jsonSessionStatus->setNumber("totalSize", sessionStatus.totalSize);
            jsonSessionStatus->setNumber("numFiles", sessionStatus.numFiles);
            jsonSessionStatus->setNumber("diskSpace", sessionStatus.diskSpace);
            jsonSessionStatus->setBool("enableAbort", sessionStatus.enableAbort);
            jsonSessionStatus->setBool("abortBusy", sessionStatus.abortBusy);
            jsonSessionStatus->setBool("enableAbort", sessionStatus.enableAbort);
            jsonSessionStatus->setNumber("ltoStatusChangeCount", sessionStatus.ltoStatusChangeCount);
            jsonSessionStatus->setNumber("tapeDevStatus", sessionStatus.tapeDevStatus);
            jsonSessionStatus->setString("tapeDevDetailedStatus", sessionStatus.tapeDevDetailedStatus);
            jsonSessionStatus->setString("sessionStatus", sessionStatus.sessionStatusString);
            jsonSessionStatus->setNumber("maxTotalSize", sessionStatus.maxTotalSize);
            jsonSessionStatus->setNumber("minTotalSize", sessionStatus.minTotalSize);
            jsonSessionStatus->setNumber("maxFiles", sessionStatus.maxFiles);
        }
        else
        {
            string barcode;
            bool completed;
            bool userAborted;
            if (_tapeExport->getLastSessionStatus(&barcode, &completed, &userAborted))
            {
                JSONObject* jsonLastSession = json.setObject("lastSession");
                jsonLastSession->setString("barcode", barcode);
                jsonLastSession->setBool("completed", completed);
                jsonLastSession->setBool("userAborted", userAborted);
            }
        }
    }
    if (includeCache)
    {
        json.setNumber("numCacheItems", cacheStatus.numItems);
        json.setNumber("statusChangeCount", cacheStatus.statusChangeCount);
    }
    if (includeSystem)
    {
        json.setString("version", get_version());
        json.setString("buildDate", get_build_date());
        json.setNumber("diskSpace", systemStatus.remDiskSpace);
        json.setNumber("recordingTime", systemStatus.remDuration);
    }
    if (includePrepare)
    {
        json.setNumber("maxTotalSize", status.maxTotalSize);
        json.setNumber("minTotalSize", status.minTotalSize);
        json.setNumber("maxFiles", status.maxFiles);
    }

    connection->sendJSON(&json);
}

void HTTPTapeExport::checkBarcodeSelection(HTTPConnection* connection)
{
    LOCK_SECTION(_checkBarcodeSelectionAgentMutex);
    
    if (_checkBarcodeSelectionAgent != 0 &&
        _checkBarcodeSelectionAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_checkBarcodeSelectionAgent != 0)
    {
        SAFE_DELETE(_checkBarcodeSelectionAgent);
    }
    

    // check it is an LTO tape barcode
    string barcode = connection->getQueryValue("barcode");
    if (!_tapeExport->isLTOBarcode(barcode))
    {
        JSONObject json;
        json.setBool("accepted", false);
        json.setString("barcode", barcode);
        json.setString("message", "Barcode is not an LTO tape barcode");
        
        connection->sendJSON(&json);
        return;
    }
    
    
    // start the agent
    _checkBarcodeSelectionAgent = new Thread(new CheckBarcodeSelectionAgent(_tapeExport, barcode, connection), true);
    _checkBarcodeSelectionAgent->start();
}

void HTTPTapeExport::getCacheContents(HTTPConnection* connection)
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
    _cacheContentsAgent = new Thread(new CacheContentsAgent(_tapeExport, connection), true);
    _cacheContentsAgent->start();
}

void HTTPTapeExport::deleteCacheItems(HTTPConnection* connection)
{
    LOCK_SECTION(_deleteCacheItemsAgentMutex);
    
    if (_deleteCacheItemsAgent != 0 &&
        _deleteCacheItemsAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_deleteCacheItemsAgent != 0)
    {
        SAFE_DELETE(_deleteCacheItemsAgent);
    }

    // get the "items" post data
    string itemsString = connection->getPostValue("items");
    if (itemsString.size() == 0)
    {
        connection->sendBadRequest("No items value in post data");
        Logging::debug("Client request to delete cache items contained no 'items' post data parameter\n");
        return;
    }
    vector<long> items;
    long itemId;
    size_t pos = -1;
    do
    {
        if (sscanf(&itemsString.c_str()[pos + 1], "%ld", &itemId) != 1)
        {
            connection->sendBadRequest("Invalid items identifier list");
            Logging::debug("Client request to delete cache items has invalid 'items' post data parameter: %s\n", itemsString.c_str());
            return;
        }
        items.push_back(itemId);
    }
    while ((pos = itemsString.find(",", pos + 1)) != string::npos);
    
    // start the agent
    _deleteCacheItemsAgent = new Thread(new DeleteCacheItemsAgent(items, _tapeExport, connection), true);
    _deleteCacheItemsAgent->start();
}

void HTTPTapeExport::startNewAutoSession(HTTPConnection* connection)
{
    LOCK_SECTION(_startSessionAgentMutex);
    
    if (_startSessionAgent != 0 &&
        _startSessionAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_startSessionAgent != 0)
    {
        SAFE_DELETE(_startSessionAgent);
    }

    // process the URL and get and check the barcode argument
    string barcode = connection->getQueryValue("barcode");
    if (!_tapeExport->isLTOBarcode(barcode))
    {
        // no barcode query argument or zero length barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Barcode is not a LTO tape barcode");
        json.setString("barcode", barcode);
        connection->sendJSON(&json);
        return;
    }

    
    // start the agent
    _startSessionAgent = new Thread(new StartAutoSessionAgent(barcode, 
        this, connection), true);
    _startSessionAgent->start();
}

void HTTPTapeExport::startNewManualSession(HTTPConnection* connection)
{
    LOCK_SECTION(_startSessionAgentMutex);
    
    if (_startSessionAgent != 0 &&
        _startSessionAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_startSessionAgent != 0)
    {
        SAFE_DELETE(_startSessionAgent);
    }

    // process the URL and get and check the barcode argument
    string barcode = connection->getQueryValue("barcode");
    if (!_tapeExport->isLTOBarcode(barcode))
    {
        // no barcode query argument or zero length barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Barcode is not a LTO tape barcode");
        json.setString("barcode", barcode);
        connection->sendJSON(&json);
        return;
    }
    // get the items
    string itemsString = connection->getQueryValue("items");
    if (itemsString.size() == 0)
    {
        connection->sendBadRequest("No items value in post data");
        Logging::debug("Client request to start manual transfer session contained no 'items' in query parameter\n");
        return;
    }
    vector<long> items;
    long itemId;
    size_t pos = -1;
    do
    {
        if (sscanf(&itemsString.c_str()[pos + 1], "%ld", &itemId) != 1)
        {
            connection->sendBadRequest("Invalid items identifier list");
            Logging::debug("Client request to start manual transfer session contained invalid 'items' in query parameter\n");
            return;
        }
        items.push_back(itemId);
    }
    while ((pos = itemsString.find(",", pos + 1)) != string::npos);
    
    
    // start the agent
    _startSessionAgent = new Thread(new StartManualSessionAgent(barcode, items,
        this, connection), true);
    _startSessionAgent->start();
}

void HTTPTapeExport::abortSession(HTTPConnection* connection)
{
    if (!_tapeExport->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    string comments = connection->getPostValue("comments");

    _tapeExport->abortSession(true, comments);
    connection->sendOk();
}

void HTTPTapeExport::getLTOContents(HTTPConnection* connection)
{
    LOCK_SECTION(_ltoContentsAgentMutex);
    
    if (_ltoContentsAgent != 0 &&
        _ltoContentsAgent->isRunning())
    {
        connection->sendServerBusy("Server is busy with the previous request");
        return;
    }
    
    // clean-up
    if (_ltoContentsAgent != 0)
    {
        SAFE_DELETE(_ltoContentsAgent);
    }

    if (!_tapeExport->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    
    // start the agent
    _ltoContentsAgent = new Thread(new LTOContentsAgent(_tapeExport, connection), true);
    _ltoContentsAgent->start();
}


TapeExport* HTTPTapeExport::getTapeExport()
{
    return _tapeExport;
}

string HTTPTapeExport::getPSEReportsURL()
{
    return g_pseReportsURL;
}

void HTTPTapeExport::checkBarcodeStatus()
{
    LOCK_SECTION(_barcodeMutex);
    
    if (_barcode.size() > 0 && _barcodeExpirationTimer.timeLeft() == 0)
    {
        _barcode = "";
    }
}


