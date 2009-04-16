/*
 * $Id: copy_manager_main.cpp,v 1.2 2009/04/16 18:32:04 john_f Exp $
 *
 * Client to manage copying of recordings.
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
#include <ace/Signal.h>

#include "CorbaUtil.h"
#include "RecorderManager.h"

#include "CpMgrStatusClientImpl.h"


// Globals
bool done = false;

// Handler to terminate the event loop
extern "C" void handler (int)
{
    done = true;
}


// Main program
int main(int argc, char * argv[])
{
// Setup handler for ctrl-c
    ACE_Sig_Action sa ((ACE_SignalHandler) handler, SIGINT);

// Initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

// Apply timeout for CORBA operations
    const int timeoutsecs = 5;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// Get the naming service object reference(s) using initial references
// which were passed to Orb from command line arguments.
    CorbaUtil::Instance()->InitNs();

// Get object reference for recorder from naming service
    CosNaming::Name name;
    name.length(3);
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("RecordingDevices");
    if(argc > 1)
    {
        name[2].id = CORBA::string_dup(ACE_TEXT_ALWAYS_CHAR(argv[1]));
    }
    else
    {
        name[2].id = CORBA::string_dup("Studio A");
    }

    CORBA::Object_var obj = CorbaUtil::Instance()->ResolveObject(name);

// Setup RecorderManager
    RecorderManager rec_manager;
    rec_manager.Recorder(obj.in());
    rec_manager.Init();


// Now set up the StatusClient servant

// Create the servant object
    CpMgrStatusClientImpl * p_servant = new CpMgrStatusClientImpl();

// Activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();

// Incarnate servant object
    ProdAuto::StatusClient_var client_ref = p_servant->_this();

// Attach client to recorder
    rec_manager.AddStatusClient(client_ref.in());

// Now wait on events
    while ( !done )
    {
        // Check for incoming CORBA requests
        //std::cout << "C" << std::flush;
        ACE_Time_Value corba_timeout(0, 100000);  // seconds, microseconds
        CorbaUtil::Instance()->OrbRun(corba_timeout);
    }

    // Detach client from recorder
    rec_manager.RemoveStatusClient(client_ref.in());

    // Destroy client servant object
    p_servant->Destroy();

    return 0;
}

