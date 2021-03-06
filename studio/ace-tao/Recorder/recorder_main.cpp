/*
 * $Id: recorder_main.cpp,v 1.1 2007/09/11 14:08:31 stuart_hc Exp $
 *
 * Main program for CORBA-controlled recorder.
 *
 * Copyright (C) 2006  John Fletcher <john_f@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <ace/Signal.h>
#include <iostream>

#include "RecorderApp.h"

extern "C" void handler (int)
{
    RecorderApp::Instance()->Stop();
}

int main(int argc, char * argv[])
{
    // Setup handler for ctrl-c
    ACE_Sig_Action sa ((ACE_SignalHandler) handler, SIGINT);

    // Get instance of app
    App * app = RecorderApp::Instance();

    // Initialise app
    bool init_ok = app->Init(argc, argv);

    // Run the app
    if (init_ok)
    {
        std::cerr << "Recorder running...\n";
        app->Run();
        std::cerr << "Recorder stopped\n";
    }
    else
    {
        std::cerr << "Recorder initialisation failed\n";
    }

    // Clean up
    app->Clean();

    return 0;
}
