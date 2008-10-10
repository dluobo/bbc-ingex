/*
 * $Id: routerloggerApp.cpp,v 1.5 2008/10/10 16:50:49 john_f Exp $
 *
 * Router recorder application class.
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

#include "CorbaUtil.h"
#include "Logfile.h"
#include "routerloggerApp.h"
#include "SimplerouterloggerImpl.h"
#include "quartzRouter.h"
#include "EasyReader.h"
#include "ClockReader.h"

#include <string>
#include <sstream>

const char * const USAGE =
    "Usage: Routerlogger.exe [-v] [-r <router port>] [-s] [-t <timecode port>] [-u]"
    " [-n <name>] [-f <db file>]"
    " [-d <name> -p <number>] [-m <MixerOut dest>]"
    " <CORBA options>\n"
    "    -s  router on TCP socket, router port in format host:port\n"
    "    -u  timecode reader on TCP socket, timecode port in format host:port\n"
    "    example CORBA options: -ORBDefaultInitRef corbaloc:iiop:192.168.1.1:8888\n";

const char * const OPTS = "vr:st:un:f:d:p:m:";

#ifdef WIN32
    const char * const DB_PATH = "C:\\TEMP\\RouterLogs\\";
#else
    const char * const DB_PATH = "/var/tmp/RouterLogs/";
#endif


// Static member
routerloggerApp * routerloggerApp::mInstance = 0;

// Constructor
routerloggerApp::routerloggerApp()
: mpRouter(0)
{
}

routerloggerApp::~routerloggerApp()
{
    if (mpRouter)
    {
        mpRouter->Stop();
        mpRouter->wait();
        delete mpRouter;
    }
}

bool routerloggerApp::Init(int argc, char * argv[])
{
    bool ok = true;

    // Initialise ORB
    int initial_argc = argc;
    CorbaUtil::Instance()->InitOrb(argc, argv);

    // and check whether ORB options were supplied
    if (argc == initial_argc)
    {
        // No CORBA options supplied
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT (USAGE)), 0);
    }

    // get command line args
    ACE_Get_Opt cmd_opts (argc, argv, OPTS);


    std::string routerlogger_name = "RouterLog"; // default name
    std::string db_file; // We'll add a default name later if none supplied

    std::string router_port;
    Transport::EnumType  router_transport = Transport::SERIAL;
    std::string tc_port;
    Transport::EnumType  tc_transport = Transport::SERIAL;

    unsigned int mix_dest = 1; // default destination to record
    unsigned int vt1_dest = 1;
    unsigned int vt2_dest = 2;
    unsigned int vt3_dest = 3;
    unsigned int vt4_dest = 4;

    unsigned int debug_level = 2; // need level 3 for LM_DEBUG messages

    int option;
    while ((option = cmd_opts ()) != EOF)
    {
        //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("option %d\n"), option ));
        switch (option)
        {
        case 'v':
            // verbose
            ++debug_level;
            break;

        case 'r':
            // router port
            router_port = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
            break;
                
        case 's':
            router_transport = Transport::TCP;
            break;
        
        case 't':
            // timecode reader port
            tc_port = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
            break;

        case 'u':
            tc_transport = Transport::TCP;
            break;
        
        case 'n':
            {
                ServantInfo * servant_info = new ServantInfo();
                mServantInfo.push_back(servant_info);
            }
            mServantInfo.back()->name = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
            break;

        case 'f':
            // cuts databse filename
            // If not supplied, a name will be generated.
            if (!mServantInfo.empty())
            {
                mServantInfo.back()->db_file = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
            }
            break;
    
        case 'd':
            // Router destination name
            if (!mServantInfo.empty())
            {
                RouterDestination * rd = new RouterDestination(ACE_TEXT_ALWAYS_CHAR(cmd_opts.opt_arg()), 0);
                mServantInfo.back()->destinations.push_back(rd);
            }
            break;

        case 'p':
            // Router destination number
            if (!mServantInfo.empty() && !mServantInfo.back()->destinations.empty())
            {
                mServantInfo.back()->destinations.back()->output_number = ACE_OS::atoi(cmd_opts.opt_arg());
            }
            break;

        case 'm':
            // Router destination to record
            if (!mServantInfo.empty())
            {
                mServantInfo.back()->mix_dest = ACE_OS::atoi( cmd_opts.opt_arg() );
            }
            break;

        case 'h':
            ACE_ERROR_RETURN
                ((LM_ERROR, ACE_TEXT (USAGE)), 0);  // help 
            break;

        case ':':
            ACE_ERROR_RETURN
                ((LM_ERROR, ACE_TEXT ("-%c requires an argument\n"), cmd_opts.opt_opt()), 0);
            break;

        default:
            ACE_ERROR_RETURN
                ((LM_ERROR, ACE_TEXT ("Parse Error.\n")), 0);
            //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("other args\n") ));
            break;

        }
    }

// Set verbosity of debug messages
    Logfile::DebugLevel(debug_level);

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("routerlogger name is \"%C\"\n"), routerlogger_name.c_str() ));
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("router on   \"%C\"\n"), router_port.c_str() ));
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("timecode on \"%C\"\n"), tc_port.c_str() ));

// Note the router destinations we're interested in.
// Eventually this will come from database but 
// hard-coded for now.
#if 0
    mDestinations.push_back(RouterDestination("VT1", vt1_dest));
    mDestinations.push_back(RouterDestination("VT2", vt2_dest));
    mDestinations.push_back(RouterDestination("VT3", vt3_dest));
    mDestinations.push_back(RouterDestination("VT4", vt4_dest));
    mMixDestination = mix_dest;
#endif

// apply timeout for CORBA operations
    const int timeoutsecs = 5;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();

// Create router monitor
    mpRouter = new Router;
    mpRouter->Init(router_port, router_transport);

// Create timecode reader
    if (tc_port.empty())
    {
        // Use PC clock
        mpTcReader = new ClockReader;
    }
    else
    {
        // Use external timecode reader
        mpTcReader = new EasyReader;
    }

    EasyReader * er = dynamic_cast<EasyReader *>(mpTcReader);
    if (er)
    {
        if (er->Init(tc_port))
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("EasyReader initialised on port \"%C\"\n"),
                tc_port.c_str()));
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("EasyReader initialisation failed on port \"%C\"\n"),
                tc_port.c_str()));
            ok = false;
        }
    }
    else
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Timecode will be derived from PC clock\n")));
    }

    // Initialise database connection
    try
    {
        prodauto::Database::initialise("prodautodb", "bamzooki", "bamzooki", 4, 12);
    }
    catch (...)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed!\n")));
    }

    // Get the NameService object reference(s)
    // which were passed to Orb from command line arguments
    CorbaUtil::Instance()->InitNs();

#if 0
    ServantInfo * servant_info1 = new ServantInfo();
    mServantInfo.push_back(servant_info1);
    servant_info1->mName = "RouterLog1";
    servant_info1->mDest = 1;

    ServantInfo * servant_info2 = new ServantInfo();
    servant_info2->mName = "RouterLog2";
    servant_info2->mDest = 2;
    mServantInfo.push_back(servant_info2);
#endif

    // Set up the servant(s)
    for (std::vector<ServantInfo *>::iterator
        it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
    {
        ServantInfo * servant_info = *it;

        if (servant_info->db_file.empty())
        {
            std::ostringstream dbf;
            dbf << DB_PATH << servant_info->name << ".txt";
            servant_info->db_file = dbf.str();
        }

        // Create object
        servant_info->servant = new SimplerouterloggerImpl();

        // Initialise
        ok = ok && servant_info->servant->Init(servant_info->name, servant_info->db_file,
                                                    servant_info->mix_dest, servant_info->destinations);

        // Incarnate servant object
        servant_info->ref = servant_info->servant->_this();

        // Set the name to be used in Naming Service
        servant_info->cosname.length(3);
        servant_info->cosname[0].id = CORBA::string_dup("ProductionAutomation");
        servant_info->cosname[1].id = CORBA::string_dup("RecordingDevices");
        servant_info->cosname[2].id = CORBA::string_dup(servant_info->name.c_str());

        // Try to advertise in naming services
        ok = ok && CorbaUtil::Instance()->Advertise(servant_info->ref, servant_info->cosname);
    }

// other initialisation
    mTerminated = false;

    return ok; 
}

void routerloggerApp::Run()
{
    if (mpRouter->Connected())
    {
        // Start monitoring messages from router
        for (std::vector<ServantInfo *>::iterator
            it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
        {
            ServantInfo * servant_info = *it;
            mpRouter->AddObserver(servant_info->servant);
        }
        mpRouter->activate();

        for (std::vector<ServantInfo *>::iterator
            it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
        {
            ServantInfo * svi = *it;
            // Query routing to destinations we're interested in
            for (std::vector<RouterDestination *>::const_iterator
                it = svi->destinations.begin(); it != svi->destinations.end(); ++it)
            {
                mpRouter->QuerySrc((*it)->output_number);
            }
            mpRouter->QuerySrc(svi->mix_dest);
        }
    }

    // Accept CORBA requests until told to stop
    ACE_Time_Value timeout(1,0);  // seconds, microseconds
    while (!mTerminated)
    {
        CorbaUtil::Instance()->OrbRun(timeout);
    }
}

void routerloggerApp::Stop()
{
    // Cause Run loop to exit
    mTerminated = true;
}

void routerloggerApp::Clean()
{
    for (std::vector<ServantInfo *>::iterator
        it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
    {
        ServantInfo * servant_info = *it;

        // remove from naming services
        CorbaUtil::Instance()->Unadvertise(servant_info->cosname);

        // Deactivate the CORBA object and
        // relinquish our reference to the servant.
        // The POA will delete it at an appropriate time.
        if (servant_info->servant)
        {
            servant_info->servant->Destroy();
        }

        delete servant_info;
    }
}

std::string routerloggerApp::Timecode()
{
    return mpTcReader->Timecode();
}


