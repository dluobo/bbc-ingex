/*
 * $Id: main.cpp,v 1.1 2008/07/08 16:27:02 philipn Exp $
 *
 * Application for controlling a VTR using a web browser
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "VTRControl.h"
#include "DummyVTRControl.h"
#include "HTTPVTRControl.h"
#include "HTTPServer.h"
#include "RecorderException.h"
#include "Logging.h"
#include "Utilities.h"
#include "Config.h"


using namespace std;
using namespace rec;


static const char* g_defaultDeviceName = "/dev/ttyS0";
static const int g_defaultHTTPPort = 9003;


void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options]\n", cmd);
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help                   Display this usage message\n");
    fprintf(stderr, "* --document-root <dir>        The directory containing the static web pages and resources\n");
    fprintf(stderr, "  --vtr-device <name>          VTR serial device name (default %s)\n", g_defaultDeviceName);
    fprintf(stderr, "  --http-port <num>            The web server listening port (default %d)\n", g_defaultHTTPPort);
    fprintf(stderr, "  --dummy-vtr                  Use the dummy VTR\n");
}

int main(int argc, const char** argv)
{
    int cmdlnIndex = 1;
    string documentRoot;
    string deviceName = g_defaultDeviceName;
    int port = g_defaultHTTPPort;
    bool useDummyVTR = false;
    
    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "--document-root") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            documentRoot = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--vtr-device") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            deviceName = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--http-port") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            port = atoi(argv[cmdlnIndex + 1]);
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dummy-vtr") == 0)
        {
            useDummyVTR = true;
            cmdlnIndex += 1;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }    
    
    // check params
    if (documentRoot.empty())
    {
        usage(argv[0]);
        fprintf(stderr, "Missing --document-root\n");
        return 1;
    }

    
    // initialise logging to console
    try
    {
        Logging::initialise(new Logging(LOG_LEVEL_DEBUG));
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Failed to initialise logging: %s\n", ex.getMessage().c_str());
        return 1;
    }

    Config::setBool("dbg_serial_port_open", true);
    
    // start
    try
    {
        HTTPServer httpServer(port, documentRoot, vector<pair<string, string> > ());
        
        auto_ptr<VTRControl> vtrControl;
        if (useDummyVTR)
        {
            vtrControl = auto_ptr<VTRControl>(new DummyVTRControl(AJ_D350_PAL_DEVICE_TYPE));
        }
        else
        {
            vtrControl = auto_ptr<VTRControl>(new DefaultVTRControl(deviceName));
        }
        vtrControl->pollExtState(true);
        vtrControl->pollVitc(true);
        vtrControl->pollLtc(true);
        
        vector<VTRControl*> vcs;
        vcs.push_back(vtrControl.get());
        HTTPVTRControl httpVTRControl(&httpServer, vcs);
        
        httpServer.start();
           
        while (true)
        {
            sleep(1);
        }
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    
    
    return 0;
}



