/*
 * $Id: RecorderApp.cpp,v 1.4 2009/01/29 07:36:58 stuart_hc Exp $
 *
 * Encapsulation of the recorder application.
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

#include <ace/Time_Value.h>
#include <ace/OS_NS_unistd.h>
#include <iostream>

#include "CorbaUtil.h"
#include "Logfile.h"
#include "DateTime.h"
#include "DatabaseManager.h"
#include "DBException.h"

#include "RecorderApp.h"

// Static member
RecorderApp * RecorderApp::mInstance = 0;

bool RecorderApp::Init(int argc, char * argv[])
{
#if 1
// Set logging to file
	Logfile::Init();
	Logfile::AddPathComponent("var");
	Logfile::AddPathComponent("tmp");
	Logfile::AddPathComponent("IngexLogs");
	std::string dir = "Recorder_";
	dir += DateTime::DateTimeNoSeparators();

    Logfile::AddPathComponent(dir.c_str());
	Logfile::Open("Main", false);
	ACE_DEBUG(( LM_NOTICE, ACE_TEXT("Main thread\n\n") ));
#endif

// Create the servant object.
    mpServant = new IngexRecorderImpl();

// other initialisation
    mTerminated = false;

// initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

// Now that InitOrb has consumed its arguments, check for
// command line arguments.
    std::string recorder_name;
    std::string db_username;
    std::string db_password;
    if (argc > 1)
    {
        recorder_name = argv[1];
    }
    if (argc > 2)
    {
        db_username = argv[2];
    }
    if (argc > 3)
    {
        db_password = argv[3];
    }

    int verbosity = 2;
    for (int i = 4; i < argc; ++i)
    {
        if (0 == strcmp(argv[i], "-v"))
        {
            ++verbosity;
        }
    }

// Set verbosity of debug messages
    Logfile::DebugLevel(verbosity);  // need level 3 for LM_DEBUG messages

	ACE_DEBUG(( LM_NOTICE, ACE_TEXT("Recorder name \"%C\"\n\n"), recorder_name.c_str() ));

    // Initialise database connection
    try
    {
        DatabaseManager::Instance()->Initialise(db_username, db_password, 4, 12);
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed: %C\n"), dbe.getMessage().c_str()));
    }
    catch (...)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed!\n")));
    }

// Initialise recorder with name
    bool ok = mpServant->Init(recorder_name);

// apply timeout for CORBA operations
    const int timeoutsecs = 8;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();

// incarnate servant object
    mRef = mpServant->_this();

// get the NameService object reference(s)
// which were passed to Orb from command line arguments
    CorbaUtil::Instance()->InitNs();

// Set the name to be used in Naming Service
    mName.length(3);
    mName[0].id = CORBA::string_dup("ProductionAutomation");
    mName[1].id = CORBA::string_dup("RecordingDevices");
    mName[2].id = CORBA::string_dup(recorder_name.c_str());

// Try to advertise in naming services
    const int max_attempts = 5;
    const ACE_Time_Value attempt_interval(10);
    mIsAdvertised = false;
    for (int i = 0; ok && !mIsAdvertised && i < max_attempts; ++i)
    {
        if (i > 0)
        {
            std::cerr << "Pausing before re-attempting\n";
            ACE_OS::sleep(attempt_interval);
        }
        mIsAdvertised = CorbaUtil::Instance()->Advertise(mRef, mName);
    }

    ok = ok && mIsAdvertised;

    return ok;
}

void RecorderApp::Run()
{
    // Accept CORBA requests until told to stop
    ACE_Time_Value timeout(1,0);  // seconds, microseconds
    while(!mTerminated)
    {
        CorbaUtil::Instance()->OrbRun(timeout);
    }
}

void RecorderApp::Stop()
{
    // Cause Run loop to exit
    mTerminated = true;
}

void RecorderApp::Clean()
{
// remove from naming services
    if (mIsAdvertised)
    {
        CorbaUtil::Instance()->Unadvertise(mName);
    }

// Deactivate the CORBA object and
// relinquish our reference to the servant.
// The POA will delete it at an appropriate time.
    mpServant->Destroy();
}


