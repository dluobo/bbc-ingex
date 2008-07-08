/*
 * $Id: HTTPTapeExport.h,v 1.1 2008/07/08 16:26:12 philipn Exp $
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
 
#ifndef __RECORDER_HTTP_TAPE_EXPORT_H__
#define __RECORDER_HTTP_TAPE_EXPORT_H__


#include "TapeExport.h"
#include "HTTPServer.h"
#include "BarcodeScanner.h"
#include "Threads.h"
#include "Timing.h"


namespace rec
{


class HTTPTapeExport : public BarcodeScannerListener, public HTTPConnectionHandler, public HTTPSSIHandler
{
public:
    HTTPTapeExport(HTTPServer* server, TapeExport* tapeExport, BarcodeScanner* scanner);
    virtual ~HTTPTapeExport();

    // from BarcodeScannerListener    
    virtual void newBarcode(std::string barcode);
    
    // from HTTPConnectionHandler
    virtual void processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection);

    // from HTTPSSIHandler
    virtual void processSSIRequest(std::string name, HTTPConnection* connection);
    
    
    void getTapeExportStatus(HTTPConnection* connection);
    
    void checkBarcodeSelection(HTTPConnection* connection);

    void getCacheContents(HTTPConnection* connection);
    void deleteCacheItems(HTTPConnection* connection);
    
    void startNewAutoSession(HTTPConnection* connection);
    void startNewManualSession(HTTPConnection* connection);
    
    void abortSession(HTTPConnection* connection);
    
    void getLTOContents(HTTPConnection* connection);
    
    
    TapeExport* getTapeExport();

    
    static std::string getPSEReportsURL();
    
private:
    void checkBarcodeStatus();
    bool isLTOBarcode(std::string barcode);

    Thread* _checkBarcodeSelectionAgent;
    Mutex _checkBarcodeSelectionAgentMutex;
    
    Thread* _startSessionAgent;
    Mutex _startSessionAgentMutex;
    
    Thread* _cacheContentsAgent;
    Mutex _cacheContentsAgentMutex;

    Thread* _deleteCacheItemsAgent;
    Mutex _deleteCacheItemsAgentMutex;
    
    Thread* _ltoContentsAgent;
    Mutex _ltoContentsAgentMutex;
    
    TapeExport* _tapeExport;
    
    std::string _barcode;
    int _barcodeCount;
    Timer _barcodeExpirationTimer;
    Mutex _barcodeMutex;
};    



};





#endif

