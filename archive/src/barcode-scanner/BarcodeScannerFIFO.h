/*
 * $Id: BarcodeScannerFIFO.h,v 1.1 2008/07/08 16:19:55 philipn Exp $
 *
 * Used to reads barcodes from a FIFO file
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
 
#ifndef __RECORDER_BARCODE_SCANNER_FIFO_H__
#define __RECORDER_BARCODE_SCANNER_FIFO_H__


#include "BarcodeScanner.h"


namespace rec
{


class BarcodeScannerFIFO;

class BarcodeScannerFIFOWorker : public ThreadWorker
{
public:
    BarcodeScannerFIFOWorker(BarcodeScannerFIFO* scanner);
    virtual ~BarcodeScannerFIFOWorker();
    
    virtual void start();
    virtual void stop();
    virtual bool hasStopped() const;
    
private:
    BarcodeScannerFIFO* _scanner;
    bool _hasStopped;
    bool _stop;
};


class BarcodeScannerFIFO : public BarcodeScanner
{
public:
    friend class BarcodeScannerFIFOWorker;
    
public:
    BarcodeScannerFIFO(std::string fifoFilename);
    virtual ~BarcodeScannerFIFO();
    
private:
    bool openFIFO();

    std::string _fifoFilename;
    Thread* _fifoReaderThread;
    int _fifo;
};



};



#endif


