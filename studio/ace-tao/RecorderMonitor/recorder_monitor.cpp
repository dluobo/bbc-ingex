/*
 * $Id: recorder_monitor.cpp,v 1.1 2008/09/16 11:40:39 stuart_hc Exp $
 *
 * Recorder monitor demo.
 *
 * Copyright (C) 2008  British Broadcasting Corporation.
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

#include <ace/Signal.h>
#include <ace/Log_Msg.h>
#include <tao/ORB_Core.h>

#include "CorbaUtil.h"
#include "Logfile.h"
#include "RecorderManager.h"
#include "StatusClientImpl.h"


const std::string USAGE = "Usage: RecorderMonitor <recorder-name> <ORB options>";

// Globals
RecorderManager * rec_manager = 0;
bool done = false;

// Handler for ctrl-c
extern "C" void sigint_handler (int)
{
    done = true;
}

// Handler for HTTP requests
void http_handler ()
{
    rec_manager->Update();
    const std::vector<TrackInfo> & track_infos = rec_manager->TrackInfos();

    // Use track_infos to construct HTTP response
}


// Main program
int main (int argc, char * argv[])
{
    Logfile::DebugLevel(2);

// Initialise ORB
// CORBA options are removed from argc/argv
    CorbaUtil::Instance()->InitOrb(argc, argv);


// Process non-CORBA command line args
    bool help = false;
    std::string recorder_name;
    for (int i = 1; i < argc; ++i)
    {
        if (0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "--help"))
        {
            help = true;
        }
        else
        {
            recorder_name = argv[i];
        }
    }
    if (help || recorder_name.empty())
    {
        std::cerr << USAGE << std::endl;
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
    name[2].id = CORBA::string_dup(recorder_name.c_str());

    CORBA::Object_var obj = CorbaUtil::Instance()->ResolveObject(name);

// Setup RecorderManager
    rec_manager = new RecorderManager;
    rec_manager->Recorder(obj.in());
    rec_manager->Update();
    rec_manager->AddStatusClient();


// Setup handler for ctrl-c
    ACE_Sig_Action sa ((ACE_SignalHandler) sigint_handler, SIGINT);

// Now wait on events
    const ACE_Time_Value orb_run_period(0, 100000);  // seconds, microseconds
    while (!done)
    {
        // Check for incoming CORBA requests
        CorbaUtil::Instance()->OrbRun(orb_run_period);

        // Check for incoming HTTP requests
        //shttpd_poll();
    }

    rec_manager->RemoveStatusClient();
    delete rec_manager;

    return 0;
}
