/*
 * $Id: recorder_client_main.cpp,v 1.3 2009/03/19 17:58:55 john_f Exp $
 *
 * Simple test client for Recorder.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
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

#include <iostream>

#include <ace/Log_Msg.h>

#include <CorbaUtil.h>
#include <tao/ORB_Core.h>

#include "Logfile.h"
#include "RecorderManager.h"
#include "StatusClientImpl.h"

const bool CLIENT = true;

// Globals
RecorderManager * rec_manager;
bool done = false;

// A handler for stdin
class Event_Handler : public ACE_Event_Handler
{
public:
  int handle_input (ACE_HANDLE fd = ACE_INVALID_HANDLE);
};

int Event_Handler::handle_input(ACE_HANDLE h)
{
    char buf[100];
    //std::cout << "Reading..." << std::endl;
    int n = ACE_OS::read(h, buf, sizeof(buf));
    buf[n] = 0;
    //std::cout << "Read: " << buf;

    // remove CR LF etc.
    --n;
    while (buf[n] < 0x20)
    {
        buf[n] = 0;
        --n;
    }
    std::string command = buf;

    if ("q" == command || "quit" == command)
    {
        std::cout << "Quitting" << std::endl;
        done = true;
    }
    else if ("s" == command || "start" == command)
    {
        std::cout << "Starting" << std::endl;
        rec_manager->Start("MyProject");
    }
    else if ("t" == command | "stop" == command)
    {
        std::cout << "Stopping" << std::endl;
        rec_manager->Stop();
    }

    return 0;
}

// Main program
int main(int argc, char * argv[])
{
    Logfile::DebugLevel(2);

// Initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

    bool interactive = true; // default
    bool start = false;
    bool stop = false;
    bool help = false;
    std::string recorder_name;

// Process non-corba command line args
    for (int i = 1; i <argc; ++i)
    {
        if (0 == strcmp(argv[i], "-h"))
        {
            std::cerr << "Usage: RecorderClient <recorder-name>"
                " [-start|-stop] <ORB options>\n";
            exit(0);
        }
        if (0 == strcmp(argv[i], "-start"))
        {
            interactive = false;
            start = true;
        }
        else if (0 == strcmp(argv[i], "-stop"))
        {
            interactive = false;
            stop = true;
        }
        else if (0 == strcmp(argv[i], "-nop"))
        {
            interactive = false;
        }
        else
        {
            recorder_name = argv[i];
        }
    }
    if (help || recorder_name.empty())
    {
        std::cerr << "Usage: RecorderClient <recorder-name>"
            " [-start|-stop] <ORB options>\n";
        exit(0);
    }

// Apply timeout for CORBA operations
    const int timeoutsecs = 5;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// Activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();

// Get the naming service object reference(s) using initial references
// which were passed to Orb from command line arguments.
    CorbaUtil::Instance()->InitNs();

// Get object reference for recorder from naming service
    CosNaming::Name name;
    name.length(3);
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("RecordingDevices");
    if (argc > 1)
    {
        name[2].id = CORBA::string_dup(recorder_name.c_str());
    }
    else
    {
        name[2].id = CORBA::string_dup("Studio A");
    }

    CORBA::Object_var obj = CorbaUtil::Instance()->ResolveObject(name);

// Setup RecorderManager
    // Note that you must have done ORB init before constructing RecorderManager
    rec_manager = new RecorderManager;
    rec_manager->Recorder(obj.in());
    rec_manager->Update();
    if (CLIENT)
    {
        rec_manager->AddStatusClient();
    }

// Now wait on events
    const ACE_Time_Value orb_run_period(0, 500000);  // seconds, microseconds
    if (interactive)
    {

    // Register handler for stdin
        Event_Handler handler;
        ACE_Event_Handler::register_stdin_handler(&handler,
            CorbaUtil::Instance()->OrbCore()->reactor(),
            CorbaUtil::Instance()->OrbCore()->thr_mgr());


        std::cout << "[ s(start) | t(stop) | q(quit) ] <return>" << std::endl;

        while (!done)
        {
            // Check for incoming CORBA requests or other events
            CorbaUtil::Instance()->OrbRun(orb_run_period);
        }
    }
    else if (start)
    {
        rec_manager->Start("MyProject");
    }
    else if (stop)
    {
        rec_manager->Stop();
    }

    if (CLIENT)
    {
        // Check for incoming CORBA requests
        CorbaUtil::Instance()->OrbRun(orb_run_period);
    }

    if (CLIENT)
    {
        rec_manager->RemoveStatusClient();
    }

    // This will clean up status client etc.
    delete rec_manager;

    return 0;
}
