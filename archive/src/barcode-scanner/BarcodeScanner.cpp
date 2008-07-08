/*
 * $Id: BarcodeScanner.cpp,v 1.1 2008/07/08 16:19:49 philipn Exp $
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

/* 
    The BarcodeScanner reads from the barcode scanner device file and sends the
    barcode to the listeners. 
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <linux/input.h>

#include "BarcodeScanner.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Timing.h"


using namespace std;
using namespace rec;


// input event buffer > maximum barcode length
#define EVENT_BUFFER_SIZE       128 

// number of event device files to check
#define EVENT_DEV_INDEX_MAX     32


// info about the supported barcode scanners
typedef struct
{
    short vendor;
    short product;
    const char* name;
} BarcodeScannerData;

// note: mane sure that we have read permission for these devices 
// eg. check the MODE in the KERNEL=="event*" line in /etc/udev/rules.d/50-udev-default.rules
static const char* g_eventDeviceTemplate = "/dev/input/event%d";

// map the keys defined in /usr/include/linux/input.h (KEY_...) to characters
static const char g_keyToCharMap[256] = 
{
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0, 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '#', 0, '\\', 'Z', 'X', 'C', 'V', 
    'B', 'N', 'M', ',', '.', '/', 0, 0, 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const BarcodeScannerData g_supportedScanners[] = 
{
    {0x05e0, 0x1200, "Symbol Bar Code Scanner"},
    {0x04b4, 0x0101, "Marson Wasp Barcode USBi"}
};
static const size_t g_supportedScannersSize = sizeof(g_supportedScanners) / sizeof(BarcodeScannerData);




BarcodeScanner::BarcodeScanner() 
{
}

BarcodeScanner::~BarcodeScanner()
{
}

void BarcodeScanner::registerListener(BarcodeScannerListener* listener)
{
    LOCK_SECTION(_listenerMutex);
    
    // check if already present
    vector<BarcodeScannerListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if (*iter == listener)
        {
            // only add to list if it is not already present
            Logging::debug("Registering barcode scanner listener which is already in the list of listeners\n");
            return;
        }
    }
    
    _listeners.push_back(listener);
}

void BarcodeScanner::unregisterListener(BarcodeScannerListener* listener)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<BarcodeScannerListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if (*iter == listener)
        {
            // remove listener from list
            _listeners.erase(iter);
            return;
        }
    }
    
    Logging::debug("Attempt made to un-register a unknown barcode scanner listener\n");
}

string BarcodeScanner::getBarcode()
{
    string result;
    
    {
        LOCK_SECTION(_barcodeMutex);
        result = _barcode;
    }
    
    return result;
}

void BarcodeScanner::setBarcode(string barcode)
{
    LOCK_SECTION(_barcodeMutex);
    LOCK_SECTION_2(_listenerMutex);

    if (barcode.size() > 0 || _barcode.size() > 0)
    {
        _barcode = barcode;
        
        vector<BarcodeScannerListener*>::const_iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            (*iter)->newBarcode(barcode);
        }
            
        if (barcode.size() > 0)
        {
            Logging::info("New barcode scanned: '%s'\n", barcode.c_str());
        }
        else
        {
            Logging::info("Barcode has been reset\n");
        }
    }
}






BarcodeScannerDeviceWorker::BarcodeScannerDeviceWorker(BarcodeScannerDevice* scanner)
: _scanner(scanner), _hasStopped(false), _stop(false)
{
}

BarcodeScannerDeviceWorker::~BarcodeScannerDeviceWorker()
{
}

void BarcodeScannerDeviceWorker::start()
{
    GUARD_THREAD_START(_hasStopped);
    
    int j;
    struct input_event inEvent[EVENT_BUFFER_SIZE]; 
    fd_set rfds;
    struct timeval tv;
    int retval;
    ssize_t numRead;
    int numEvents;
    Timer lastEventTimer;
    Timer lastReopenAttemptTimer;
    string barcode;
    int state; // 0 means poll, 1 means try reopen the device

    state = (_scanner->_devfd >= 0) ? 0 : 1;

    while (!_stop)
    {
        if (state == 0) // poll the barcode scanner event device 
        {
            FD_ZERO(&rfds);
            FD_SET(_scanner->_devfd, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 50 * MSEC_IN_USEC; // check at least every 1/20 second
            
            retval = select(_scanner->_devfd + 1, &rfds, NULL, NULL, &tv);
    
            // don't read if we have been stopped
            if (_stop)
            {
                break;
            }
            
            // if it has been > 25msec since the last event then assume the last barcode is complete
            if (lastEventTimer.isRunning() && lastEventTimer.elapsedTime() > 25 * MSEC_IN_USEC)
            {
                _scanner->setBarcode(barcode);
                lastEventTimer.reset();                    
            }
            
            
            if (retval == -1)
            {
                if (errno == EINTR)
                {
                    // select was interupted by a signal - retry
                    continue;
                }
                
                 // select error and we assume the device has been disconnected
                 
                Logging::warning("Select error in barcode scanner: %s\n", strerror(errno));
                Logging::warning("Barcode scanner device disconnected\n");
                Logging::info("Attempting to reopen barcode scanner device\n");
                
                // goto state 1 to attempt to reopen the device
                state = 1; 
                continue;
            }
            else if (retval)
            {
                numRead = read(_scanner->_devfd, inEvent, EVENT_BUFFER_SIZE * sizeof(struct input_event));
                if (numRead > 0)
                {
                    numEvents = numRead / sizeof(struct input_event);
                    
                    // reset the barcode if reading new chars
                    if (!lastEventTimer.isRunning())
                    {
                        barcode = "";
                    }

                    // add chars to the barcode
                    for (j = 0; j < numEvents; j++)
                    {
                        if (inEvent[j].type == EV_KEY && /* key event */
                            inEvent[j].code < 256 && g_keyToCharMap[inEvent[j].code] != 0 && /* supported key code */ 
                            inEvent[j].value == 1 /* key pressed */)
                        {
                            barcode += g_keyToCharMap[inEvent[j].code];
                        }
                    }
                    
                    // start the timer so we can finalise the barcode
                    lastEventTimer.start();
                }
                else // numRead <= 0 -> assume the device has been disconnected
                {
                    Logging::warning("Barcode scanner device disconnected\n");
                    Logging::info("Attempting to reopen barcode scanner device\n");
                    
                    // goto state 1 to attempt to reopen the device
                    state = 1; 
                }
            }
        }
        
        else // state == 1 -> attempt to reopen the device every 2 seconds
        {
            if (!lastReopenAttemptTimer.isRunning())
            {
                // start the timer running
                lastReopenAttemptTimer.start();
            }
            else if (lastReopenAttemptTimer.elapsedTime() > 2 * SEC_IN_USEC)
            {
                if (_scanner->openDevice())
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

void BarcodeScannerDeviceWorker::stop()
{
    _stop = true;
}

bool BarcodeScannerDeviceWorker::hasStopped() const
{
    return _hasStopped;
}



BarcodeScannerDevice::BarcodeScannerDevice()
: BarcodeScanner(), _scannerThread(0), _devfd(-1) 
{
    if (!openDevice())
    {
        Logging::warning("Failed to open a barcode scanner event device. Will try again later.\n");
    }
    
    _scannerThread = new Thread(new BarcodeScannerDeviceWorker(this), true);
    _scannerThread->start();
}

BarcodeScannerDevice::~BarcodeScannerDevice()
{
    delete _scannerThread;
    
    if (_devfd >= 0)
    {
        close(_devfd);
    }
}

bool BarcodeScannerDevice::openDevice()
{
    int i;
    int fd;
    struct input_id deviceInfo = {0,0,0,0};
    size_t devIndex;
    char eventDevName[256];
    int grab = 0;
    
    // close the device if it is already open
    if (_devfd >= 0)
    {
        close(_devfd);
        _devfd = -1;
    }
    
    // try find a the scanner event device
    for (i = 0; i < EVENT_DEV_INDEX_MAX; i++)
    {
        sprintf(eventDevName, g_eventDeviceTemplate, i);
        
        if ((fd = open(eventDevName, O_RDONLY)) != -1)
        {
            // get the device information
            if (ioctl(fd, EVIOCGID, &deviceInfo) >= 0)
            {
                for (devIndex = 0; devIndex < g_supportedScannersSize; devIndex++)
                {
                    // check the vendor and product id
                    if (g_supportedScanners[devIndex].vendor == deviceInfo.vendor &&
                        g_supportedScanners[devIndex].product == deviceInfo.product)
                    {
                        Logging::info("Opened barcode scanner device '%s' at '%s'\n", 
                            g_supportedScanners[devIndex].name, eventDevName);
                            
                        // try grab the device for exclusive access
                        if (ioctl(fd, EVIOCGRAB, &grab) != 0)
                        {
                            Logging::warning("Failed to grab the barcode scanner device for exclusive access\n");
                        }
                        
                        _devfd = fd;
                        return true;
                    }
                }
            }
            
            close(fd);
        }
    }

    return false;    
}





