/*
 * $Id: BarcodeScanner.h,v 1.1 2008/07/08 16:19:56 philipn Exp $
 *
 * Used to reads barcodes from a scanner device
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
 
#ifndef __RECORDER_BARCODE_SCANNER_H__
#define __RECORDER_BARCODE_SCANNER_H__


#include <string>
#include <vector>

#include "Threads.h"


namespace rec
{


class BarcodeScannerListener
{
public:
    virtual ~BarcodeScannerListener() {}
    
    virtual void newBarcode(std::string barcode) = 0;
};



class BarcodeScanner
{
public:
    BarcodeScanner();
    virtual ~BarcodeScanner();
    
    void registerListener(BarcodeScannerListener* listener);
    void unregisterListener(BarcodeScannerListener* listener);

    std::string getBarcode();

protected:    
    void  setBarcode(std::string barcode);   

    std::vector<BarcodeScannerListener*> _listeners;
    Mutex _listenerMutex;
    
    std::string _barcode;
    Mutex _barcodeMutex;    
};



class BarcodeScannerDevice;

class BarcodeScannerDeviceWorker : public ThreadWorker
{
public:
    BarcodeScannerDeviceWorker(BarcodeScannerDevice* scanner);
    virtual ~BarcodeScannerDeviceWorker();
    
    virtual void start();
    virtual void stop();
    virtual bool hasStopped() const;
    
private:
    BarcodeScannerDevice* _scanner;
    bool _hasStopped;
    bool _stop;
};


class BarcodeScannerDevice : public BarcodeScanner
{
public:
    friend class BarcodeScannerDeviceWorker;
    
public:
    BarcodeScannerDevice();
    virtual ~BarcodeScannerDevice();
    
private:
    bool openDevice();

    Thread* _scannerThread;
    int _devfd;
};



};



#endif


