/*
 * $Id: HTTPRecorder.h,v 1.1 2008/07/08 16:25:46 philipn Exp $
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
 
#ifndef __RECORDER_HTTP_RECORDER_H__
#define __RECORDER_HTTP_RECORDER_H__


#include "Recorder.h"
#include "HTTPServer.h"
#include "BarcodeScanner.h"
#include "Threads.h"
#include "Timing.h"


namespace rec
{



class HTTPRecorder : public BarcodeScannerListener, public HTTPConnectionHandler, public HTTPSSIHandler
{
public:
    HTTPRecorder(HTTPServer* server, Recorder* recorder, BarcodeScanner* scanner);
    virtual ~HTTPRecorder();

    // from BarcodeScannerListener    
    virtual void newBarcode(std::string barcode);
    
    // from HTTPConnectionHandler
    virtual void processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection);

    // from HTTPSSIHandler
    virtual void processSSIRequest(std::string name, HTTPConnection* connection);
    
    
    void getRecorderStatus(HTTPConnection* connection);
    
    void getSourceInfo(HTTPConnection* connection);
    void updateSourceInfo(HTTPConnection* connection);

    void checkSelectedDigibeta(HTTPConnection* connection);
    
    void startNewSession(HTTPConnection* connection);
    void getSessionSourceInfo(HTTPConnection* connection);
    
    void startRecording(HTTPConnection* connection);
    void stopRecording(HTTPConnection* connection);
    void chunkFile(HTTPConnection* connection);
    void completeSession(HTTPConnection* connection);
    void abortSession(HTTPConnection* connection);

    void setSessionComments(HTTPConnection* connection);
    void getSessionComments(HTTPConnection* connection);
    
    void getCacheContents(HTTPConnection* connection);
    
    void confReplayControl(HTTPConnection* connection);

    void replayFile(HTTPConnection* connection);

    void playItem(HTTPConnection* connection);
    void playPrevItem(HTTPConnection* connection);
    void playNextItem(HTTPConnection* connection);
    void seekToEOP(HTTPConnection* connection);
    
    void markItemStart(HTTPConnection* connection);
    void clearItem(HTTPConnection* connection);
    
    void moveItemUp(HTTPConnection* connection);
    void moveItemDown(HTTPConnection* connection);
    
    void disableItem(HTTPConnection* connection);
    void enableItem(HTTPConnection* connection);

    void getItemClipInfo(HTTPConnection* connection);
    void getItemSourceInfo(HTTPConnection* connection);
    
    
    Recorder* getRecorder();
    
    static std::string getPSEReportsURL();
    
private:
    void checkBarcodeStatus();

    Recorder* _recorder;
    
    std::string _barcode;
    int _barcodeCount;
    Timer _barcodeExpirationTimer;
    Mutex _barcodeMutex;
    
    Thread* _sourceInfoAgent;
    Mutex _sourceInfoAgentMutex;
    
    Thread* _checkDigibetaAgent;
    Mutex _checkDigibetaAgentMutex;
    
    Thread* _updateSourceInfoAgent;
    Thread* _startSessionAgent;
    Mutex _startSessionOrUpdateSourceInfoAgentMutex;
    
    Thread* _cacheContentsAgent;
    Mutex _cacheContentsAgentMutex;

    Thread* _replayFileAgent;
    Mutex _replayFileAgentMutex;
};





};







#endif

