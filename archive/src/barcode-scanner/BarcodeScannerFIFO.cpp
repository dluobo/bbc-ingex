/*
 * $Id: BarcodeScannerFIFO.cpp,v 1.1 2008/07/08 16:19:53 philipn Exp $
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

/* 
    The BarcodeScannerFIFO reads barcodes from a FIFO file and sends the barcode 
    to the listeners. 
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <linux/input.h>

#include "BarcodeScannerFIFO.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Timing.h"


using namespace std;
using namespace rec;



BarcodeScannerFIFOWorker::BarcodeScannerFIFOWorker(BarcodeScannerFIFO* scanner)
: _scanner(scanner), _hasStopped(false), _stop(false)
{
}

BarcodeScannerFIFOWorker::~BarcodeScannerFIFOWorker()
{
}

void BarcodeScannerFIFOWorker::start()
{
    GUARD_THREAD_START(_hasStopped);

    
    char inData[256]; // more than enough to hold all barcode scanned within 1/20th second 
    fd_set rfds;
    struct timeval tv;
    int retval;
    ssize_t numRead;
    Timer lastReopenAttemptTimer;
    string barcode;
    int state; // 0 means poll, 1 means try reopen the fifo

    state = (_scanner->_fifo >= 0) ? 0 : 1;

    while (!_stop)
    {
        if (state == 0) // poll the fifo 
        {
            FD_ZERO(&rfds);
            FD_SET(_scanner->_fifo, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 50000; // check at least every 1/20 second
            
            retval = select(_scanner->_fifo + 1, &rfds, NULL, NULL, &tv);
    
            // don't read if we have been stopped
            if (_stop)
            {
                break;
            }
            
            
            if (retval == -1)
            {
                if (errno == EINTR)
                {
                    // select was interupted by a signal - retry
                    continue;
                }
                
                 // select error and we assume the fifo is lost
                Logging::warning("Select error in barcode scanner: %s\n", strerror(errno));
                Logging::info("Attempting to reopen barcode scanner fifo\n");
                
                // goto state 1 to attempt to reopen the fifo
                state = 1; 
                continue;
            }
            else if (retval)
            {
                numRead = read(_scanner->_fifo, inData, sizeof(inData));
                if (numRead == sizeof(inData))
                {
                    Logging::error("Barcode buffer is too small for reading from fifo - discarding data\n");
                }
                else if (numRead > 0)
                {
                    // make sure the data is null terminated
                    inData[sizeof(inData) - 1] = '\0';
                    
                    // each barcode is separated by a new line character
                    // note: barcode writes to the fifo are atomic
                    char* barcodeStr = inData;
                    char* newLine;
                    while ((newLine = strchr(barcodeStr, '\n')) != 0)
                    {
                        *newLine = '\0';
                        _scanner->setBarcode(barcodeStr);
                        
                        barcodeStr = newLine + 1;
                    }
                }
                else // numRead <= 0 -> assume fifo has been deleted (numRead == 0) or else there was an error
                {
                    if (numRead == 0)
                    {
                        Logging::warning("Barcode scanner fifo was deleted\n");
                    }
                    else
                    {
                        Logging::warning("Failed to read from scanner: %s\n", strerror(errno));
                    }
                    Logging::info("Attempting to reopen barcode scanner fifo\n");
                    
                    // goto state 1 to attempt to reopen the fifo
                    state = 1; 
                    continue;
                }
            }
        }
        
        else // state == 1 -> attempt to reopen the device every 2 seconds
        {
            if (!lastReopenAttemptTimer.isRunning())
            {
                lastReopenAttemptTimer.start();
            }
            else if (lastReopenAttemptTimer.elapsedTime() > 2 * SEC_IN_USEC)
            {
                if (_scanner->openFIFO())
                {
                    // back online
                    state = 0;
                    lastReopenAttemptTimer.reset();
                }
                else
                {
                    // restart the timer
                    lastReopenAttemptTimer.start();
                }
            }
            else
            {
                // don't hog the CPU
                sleep_msec(100);
            }
        }
    }   
}

void BarcodeScannerFIFOWorker::stop()
{
    _stop = true;
}

bool BarcodeScannerFIFOWorker::hasStopped() const
{
    return _hasStopped;
}



BarcodeScannerFIFO::BarcodeScannerFIFO(string fifoFilename)
: BarcodeScanner(), _fifoFilename(fifoFilename), _fifoReaderThread(0), _fifo(-1) 
{
    if (!openFIFO())
    {
        Logging::warning("Failed to open a barcode scanner fifo '%s'. Will try again later.\n", _fifoFilename.c_str());
    }
    
    _fifoReaderThread = new Thread(new BarcodeScannerFIFOWorker(this), true);
    _fifoReaderThread->start();
}

BarcodeScannerFIFO::~BarcodeScannerFIFO()
{
    delete _fifoReaderThread;
    
    if (_fifo >= 0)
    {
        close(_fifo);
    }
}

bool BarcodeScannerFIFO::openFIFO()
{
    int fd;
    
    // close the fifo if it is already open
    if (_fifo >= 0)
    {
        close(_fifo);
        _fifo = -1;
    }
    
    // O_NONBLOCK so that we don't wait for a writer
    if ((fd = open(_fifoFilename.c_str(), O_RDONLY | O_NONBLOCK)) >= 0)
    {
        _fifo = fd;
        Logging::info("Opened barcode scanner fifo '%s' for reading \n", _fifoFilename.c_str()); 
        return true;
    }
    
    return false;
}


