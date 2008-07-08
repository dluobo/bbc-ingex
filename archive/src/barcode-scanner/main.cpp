/*
 * $Id: main.cpp,v 1.1 2008/07/08 16:19:54 philipn Exp $
 *
 * Copies scanned barcodes to one or more FIFO files.
 *
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
    The output of a barcode scanner device is written to one or more FIFO files. 
    A separate process reads the barcodes from the FIFO. This allows multiple 
    processes, each with it's own FIFO, to share a barcode scanner.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
       
#include <string>
#include <vector>

#include "BarcodeScanner.h"
#include "RecorderException.h"
#include "Logging.h"
#include "Utilities.h"


using namespace std;
using namespace rec;



class FIFOBarcodeScannerListener : public BarcodeScannerListener
{
public:
    FIFOBarcodeScannerListener(vector<int>& fifos, vector<string>& deferredFIFOs)
    : _fifos(fifos), _deferredFIFOs(deferredFIFOs)
    {}
    
    virtual ~FIFOBarcodeScannerListener() 
    {}
    
    virtual void newBarcode(string barcode)
    {
        LOCK_SECTION(_fifosMutex);
        
        int numSent = 0;
        
        // the barcode has no newline; use a newline to separate the barcodes
        string sendBarcode = barcode + "\n";
        
        vector<int>::const_iterator iter;
        for (iter = _fifos.begin(); iter != _fifos.end(); iter++)
        {
            if (write(*iter, sendBarcode.c_str(), sendBarcode.size()) != (ssize_t)sendBarcode.size())
            {
                // we expect a EPIPE if there are no readers
                if (errno != EPIPE)
                {
                    Logging::error("Failed to write to fifo: %s\n", strerror(errno));
                }
            }
            else
            {
                numSent++;
            }
        }
        
        // try open the deferred FIFOs and write the barcode if successfull
        int fifo;
        vector<string>::iterator fiter = _deferredFIFOs.begin();
        while (fiter != _deferredFIFOs.end())
        {
            // O_NONBLOCK means we don't wait if there are no readers
            if ((fifo = open((*fiter).c_str(), O_WRONLY | O_NONBLOCK)) >= 0)
            {
                Logging::info("Opened fifo '%s'\n", (*fiter).c_str());
                
                _fifos.push_back(fifo);
                fiter = _deferredFIFOs.erase(fiter);

                // write the barcode
                if (write(fifo, sendBarcode.c_str(), sendBarcode.size()) != (ssize_t)sendBarcode.size())
                {
                    // we expect a EPIPE if there are no readers
                    if (errno != EPIPE)
                    {
                        Logging::error("Failed to write to fifo: %s\n", strerror(errno));
                    }
                }
                else
                {
                    numSent++;
                }
            }
            else
            {
                fiter++;
            }
        }

        Logging::info("Sent barcode to %d listeners\n", numSent);
    }
    
private:
    vector<int>& _fifos;
    Mutex _fifosMutex;
    
    vector<string> _deferredFIFOs;
};



void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [<fifo filename>]+\n", cmd);
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help                   Display this usage message\n");
}

int main(int argc, const char** argv)
{
    int cmdlnIndex = 1;
    vector<string> fifoFilenames;
    
    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else
        {
            fifoFilenames.push_back(argv[cmdlnIndex]);
            cmdlnIndex++;
        }
    }    
    
    if (fifoFilenames.size() == 0)
    {
        usage(argv[0]);
        fprintf(stderr, "No fifo filenames\n");
        return 1;
    }
    
    // initialise logging to console only
    try
    {
        Logging::initialise(LOG_LEVEL_DEBUG);
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Failed to initialise logging: %s\n", ex.getMessage().c_str());
        return 1;
    }
    
    // disable the SIGPIPE signal, which is sent when writing to the fifo when
    // no readers are attached
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        fprintf(stderr, "Failed to disable the SIGPIPE signal: %s\n", strerror(errno)); 
        return 1;
    }
    
    // create the FIFO files if they don't exist
    struct stat statBuf;
    vector<string>::const_iterator iter;
    for (iter = fifoFilenames.begin(); iter != fifoFilenames.end(); iter++)
    {
        if (stat((*iter).c_str(), &statBuf) == 0)
        {
            Logging::info("Named fifo '%s' already exists\n", (*iter).c_str());
        }
        else
        {
            if (mkfifo((*iter).c_str(), 0666) != 0)
            {
                fprintf(stderr, "Failed to create named fifo '%s': '%s'\n", (*iter).c_str(), strerror(errno));
                return 1;
            }
            else
            {
                Logging::info("Created named fifo '%s'\n", (*iter).c_str());
            }
        }
    }
    
    // open the fifo files
    vector<int> fifos;
    vector<string> deferredFIFOs;
    int fifo;
    for (iter = fifoFilenames.begin(); iter != fifoFilenames.end(); iter++)
    {
        // O_NONBLOCK means we don't wait for readers
        if ((fifo = open((*iter).c_str(), O_WRONLY | O_NONBLOCK)) < 0)
        {
            // ENXIO occurs when no reader is attached to the FIFO
            if (errno == ENXIO)
            {
                deferredFIFOs.push_back(*iter);
                Logging::info("Deferred opening '%s' fifo because there are no readers\n", (*iter).c_str());
            }
            else
            {
                fprintf(stderr, "Failed to open fifo '%s': %s\n", (*iter).c_str(), strerror(errno));
                return 1;
            }
        }
        else
        {
            fifos.push_back(fifo);
        }
    }

    // start    
    try
    {
        BarcodeScannerDevice scanner;
        FIFOBarcodeScannerListener listener(fifos, deferredFIFOs);
        scanner.registerListener(&listener);
        
        while (true)
        {
            sleep(5);
        }
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }

    vector<int>::const_iterator fiter;
    for (fiter = fifos.begin(); fiter != fifos.end(); fiter++)
    {
        close(*fiter);
    }
    
    return 0;
}


