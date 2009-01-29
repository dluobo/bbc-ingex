/*
 * $Id: test_JogShuttle.cpp,v 1.2 2009/01/29 07:10:27 stuart_hc Exp $
 *
 * Test the JogShuttle class
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "JogShuttle.h"


using namespace std;
using namespace ingex;


/* avoid compiler warning "warning: unused parameter ..." */
#define JSL_UNUSED_PARAM(param) \
    (void) param;



class TestJogShuttleListener : public JogShuttleListener
{
public:
    TestJogShuttleListener()
    {
    }
    virtual ~TestJogShuttleListener()
    {
    }

    virtual void connected(JogShuttle* jogShuttle, JogShuttleDevice device)
    {
        printf("Connected to '%s'\n", jogShuttle->getDeviceName(device).c_str());
    }

    virtual void disconnected(JogShuttle* jogShuttle, JogShuttleDevice device)
    {
        printf("Disconnected from '%s'\n", jogShuttle->getDeviceName(device).c_str());
    }

    virtual void ping(JogShuttle* jogShuttle)
    {
        JSL_UNUSED_PARAM(jogShuttle);
        // do nothing
    }

    virtual void buttonPressed(JogShuttle* jogShuttle, int number)
    {
        JSL_UNUSED_PARAM(jogShuttle);
        printf("Button pressed: %d\n", number);
    }

    virtual void buttonReleased(JogShuttle* jogShuttle, int number)
    {
        JSL_UNUSED_PARAM(jogShuttle);
        printf("Button released: %d\n", number);
    }

    virtual void jog(JogShuttle* jogShuttle, bool clockwise, int position)
    {
        JSL_UNUSED_PARAM(jogShuttle);
        printf("Jog: %s, %d\n", (clockwise) ? "clockwise" : "anti-clockwise", position);
    }

    virtual void shuttle(JogShuttle* jogShuttle, bool clockwise, int position)
    {
        JSL_UNUSED_PARAM(jogShuttle);
        printf("Shuttle: %s, %d\n", (clockwise) ? "clockwise" : "anti-clockwise", position);
    }
};



static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options]\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help               display this usage message\n");
    fprintf(stderr, "\n");
}

int main (int argc, const char** argv)
{
    int cmdlnIndex = 1;

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
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }

    JogShuttle* jogShuttle = new JogShuttle();
    JogShuttleListener* listener = new TestJogShuttleListener();

    jogShuttle->addListener(listener);

    jogShuttle->start();


    // loop forever
    while (true)
    {
        sleep(5);
    }


    delete jogShuttle;
    delete listener;

    return 0;
}

