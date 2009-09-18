/*
 * $Id: main.cpp,v 1.2 2009/09/18 16:13:51 philipn Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Modifications: Matthew Marks
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

#include <LocalIngexPlayer.h>
#include "HTTPIngexPlayer.h"
#include "HTTPServer.h"
#include "IngexException.h"
#include "Logging.h"
#include "Utilities.h"


using namespace std;
using namespace ingex;
using namespace prodauto;


static const int g_defaultHTTPPort = 9008;
static const char* g_defaultDocumentRoot = "/dev/null";
static const int g_defaultDVSCard = -1;
static const int g_defaultDVSChannel = -1;


void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options]\n", cmd);
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help                   Display this usage message\n");
    fprintf(stderr, "  --http-port <num>            The web server listening port (default %d)\n", g_defaultHTTPPort);
    fprintf(stderr, "  --doc-root <dir>             Root directory for serving web pages (default '%s')\n", g_defaultDocumentRoot);
    fprintf(stderr, "  --dvs-card <num>             Set the starting DVS card number (default is %d)\n", g_defaultDVSCard);
    fprintf(stderr, "  --dvs-channel <num>          Set the starting DVS channel number (default is %d)\n", g_defaultDVSChannel);
}

int main(int argc, const char** argv)
{
    int cmdlnIndex = 1;
    int port = g_defaultHTTPPort;
    string documentRoot = g_defaultDocumentRoot;
    int dvsCard = g_defaultDVSCard;
    int dvsChannel = g_defaultDVSChannel;

    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
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
        else if (strcmp(argv[cmdlnIndex], "--doc-root") == 0)
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
        else if (strcmp(argv[cmdlnIndex], "--dvs-card") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &dvsCard) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dvs-channel") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &dvsChannel) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }

    // initialise logging to console
    try
    {
        Logging::initialise(new Logging(LOG_LEVEL_DEBUG));
    }
    catch (const IngexException& ex)
    {
        fprintf(stderr, "Failed to initialise logging: %s\n", ex.getMessage().c_str());
        return 1;
    }

    // start
    try
    {
        HTTPServer httpServer(port, documentRoot, vector<pair<string, string> > ());

        IngexPlayerListenerRegistry* listener_registry = new IngexPlayerListenerRegistry;
        // TODO: change X11 to DVS
        LocalIngexPlayer* localPlayer = new LocalIngexPlayer(listener_registry);

        localPlayer->setOutputType(X11_AUTO_OUTPUT);
        localPlayer->setVideoSplit(NO_SPLIT_VIDEO_SWITCH);
        localPlayer->setDVSTarget(dvsCard, dvsChannel);

        HTTPIngexPlayer httpPlayer(&httpServer, localPlayer, listener_registry);

        httpServer.start();

        while (true)
        {
            sleep(1);
        }
    }
    catch (const IngexException& ex)
    {
        fprintf(stderr, "Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }


    return 0;
}
