/*
 * $Id: RecorderApp.cpp,v 1.7 2009/09/18 16:15:14 john_f Exp $
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

#include <ace/Get_Opt.h>
#include <ace/Log_Msg.h>
#include <ace/Time_Value.h>
#include <ace/OS_NS_unistd.h>
#include <iostream>

#include "CorbaUtil.h"
#include "Logfile.h"
#include "DateTime.h"
#include "DatabaseManager.h"
#include "DBException.h"
#include "ffmpeg_encoder.h" // for THREADS_USE_BUILTIN_TUNING

#include "RecorderApp.h"

const char * const USAGE =
    "Usage: Recorder [-n | --name <name>] \\\n"
    "  [--dbhost <db_host>] [--dbname <db_name>] [--dbuser <db_user>] [--dbpass <db_password>] \\\n"
    "  [-v | --verbose] [-t | --ffmpeg-threads <num ffmpeg threads>] \\\n"
    "  <CORBA options>\n"
    "Example CORBA options: -ORBDefaultInitRef corbaloc:iiop:192.168.1.1:8888\n";

namespace
{
void exit_usage()
{
    fprintf(stderr, USAGE);
    exit(0);
}
}

// Static member
RecorderApp * RecorderApp::mInstance = 0;

bool RecorderApp::Init(int argc, char * argv[])
{
// Enable app to run
    mTerminated = false;

// Initialise return value
    bool ok = true;

// Initialise ORB
    int initial_argc = argc;
    CorbaUtil::Instance()->InitOrb(argc, argv);

// and check whether ORB options were supplied
    if (argc == initial_argc)
    {
        // No CORBA options supplied
        exit_usage();
    }

// Defaults for command line parameters
    std::string recorder_name = "Ingex";
    std::string db_host = "localhost";
    std::string db_name = "prodautodb";
    std::string db_username;
    std::string db_password;
    unsigned int debug_level = 2; // need level 3 for LM_DEBUG messages
    int ffmpeg_threads = THREADS_USE_BUILTIN_TUNING;

// Now that InitOrb has consumed its arguments, check for
// other command line arguments.
    for (int argindex = 1; argindex < argc; ++argindex)
    {
        if (strcmp(argv[argindex], "-h") == 0 ||
            strcmp(argv[argindex], "--help") == 0)
        {
            exit_usage();
        }
        else if (strcmp(argv[argindex], "-v") == 0 ||
            strcmp(argv[argindex], "--verbose") == 0)
        {
            ++debug_level;
        }
        else if (strcmp(argv[argindex], "-t") == 0 ||
            strcmp(argv[argindex], "--ffmpeg-threads") == 0)
        {
            if (++argindex < argc)
            {
                ffmpeg_threads = ACE_OS::atoi(argv[argindex]);
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-n") == 0 ||
            strcmp(argv[argindex], "--name") == 0)
        {
            if (++argindex < argc)
            {
                recorder_name = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbhost") == 0)
        {
            if (++argindex < argc)
            {
                db_host = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbname") == 0)
        {
            if (++argindex < argc)
            {
                db_name = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbuser") == 0)
        {
            if (++argindex < argc)
            {
                db_username = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbpass") == 0)
        {
            if (++argindex < argc)
            {
                db_password = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
    }

// Set logging to file
    Logfile::Init();
    Logfile::AddPathComponent("var");
    Logfile::AddPathComponent("tmp");
    Logfile::AddPathComponent("IngexLogs");
    std::string dir = recorder_name;
    dir += '_';
    dir += DateTime::DateTimeNoSeparators();

    Logfile::AddPathComponent(dir.c_str());
    Logfile::Open("Main", false);
    ACE_DEBUG(( LM_NOTICE, ACE_TEXT("Main thread\n\n") ));

// Set verbosity of debug messages
    Logfile::DebugLevel(debug_level);

	ACE_DEBUG(( LM_NOTICE, ACE_TEXT("Recorder name \"%C\"\n\n"), recorder_name.c_str() ));

    // Initialise database connection
    try
    {
        DatabaseManager::Instance()->Initialise(db_host, db_name, db_username, db_password, 4, 12);
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed: %C\n"), dbe.getMessage().c_str()));
    }
    catch (...)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed!\n")));
    }

// Create the servant object.
    mpServant = new IngexRecorderImpl();

// Set name and other parameters
    mpServant->Name(recorder_name);
    mpServant->FfmpegThreads(ffmpeg_threads);

// Initialise
    mpServant->Init();

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
    const ACE_Time_Value attempt_interval(1);
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
    while (!mTerminated)
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
    if (mpServant)
    {
        mpServant->Destroy();
    }
}


