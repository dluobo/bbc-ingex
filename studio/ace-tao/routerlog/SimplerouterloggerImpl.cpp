/*
 * $Id: SimplerouterloggerImpl.cpp,v 1.4 2008/09/04 15:43:53 john_f Exp $
 *
 * Servant class for RouterRecorder.
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

#include <ace/Log_Msg.h>

#include <sstream>

#include "SimplerouterloggerImpl.h"
#include "FileUtils.h"
#include "ClockReader.h"
#include "EasyReader.h"
#include "DateTime.h"
#include "Timecode.h"
#include "SourceReader.h"
#include "quartzRouter.h"
#include "CutsDatabase.h"

#ifdef WIN32
const std::string RECORD_DIR = "C:\\TEMP\\RouterLogs\\";
#else
const std::string RECORD_DIR = "/var/tmp/RouterLogs/";
#endif

// Constructor for Vt struct
Vt::Vt(int rd, const std::string & n)
: router_dest(rd), router_src(0), name(n)
{
}

SimplerouterloggerImpl * SimplerouterloggerImpl::mInstance = 0;


// Implementation skeleton constructor
SimplerouterloggerImpl::SimplerouterloggerImpl (void)
: mpRouter(0), mpTcReader(0), mpSrcReader(0), mpCutsDatabase(0), mpFile(0), mMixDestination(0)
{
    mInstance = this;

    mpRouter = new Router;
    mpSrcReader = new SourceReader;
    mpCutsDatabase = new CutsDatabase();
}

// Implementation skeleton destructor
SimplerouterloggerImpl::~SimplerouterloggerImpl (void)
{
    if (mpRouter)
    {
        mpRouter->Stop();
        mpRouter->wait();
        delete mpRouter;
    }

    delete mpTcReader;
    delete mpSrcReader;
    delete mpCutsDatabase;
}


// Initialise the routerlogger
bool SimplerouterloggerImpl::Init(const std::string & rport, bool router_tcp,
    const std::string & tcport, bool tc_tcp,
    const std::string & db_file,
    unsigned int mix_dest, unsigned int vt1_dest, unsigned int vt2_dest, unsigned int vt3_dest, unsigned int vt4_dest)
{
    bool result = true;

    // Assume 25 fps
    mEditRate.numerator   = 25;
    mEditRate.denominator = 1;

    // Setup pre-roll
    mMaxPreRoll.undefined = false;
    mMaxPreRoll.edit_rate = mEditRate;
    mMaxPreRoll.samples = 0;

    // Setup post-roll
    mMaxPostRoll.undefined = true; // no limit to post-roll
    mMaxPostRoll.edit_rate = mEditRate;
    mMaxPostRoll.samples = 0;

    // Setup format reply
    mFormat = "*ROUTER*";

    // Create tracks
    mTracks->length(1);
    ProdAuto::Track & track = mTracks->operator[](0);
    track.type = ProdAuto::VIDEO;
    track.name = "V";
    track.id = 1;
    // Setting source package name to appear in controller
    track.has_source = 1;
    track.src.package_name = CORBA::string_dup("Mixer out destination");

    mTracksStatus->length(1);
    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.signal_present = true;
    ts.timecode.edit_rate = mEditRate;
    ts.timecode.undefined = 0;
    ts.timecode.samples = 0;

    // Init database connection
    mpSrcReader->Init("KW-Router", "prodautodb", "bamzooki", "bamzooki");

    if (mpCutsDatabase)
    {
        mpCutsDatabase->Filename(db_file);
    }

    // Init timecode reader, if present
    delete mpTcReader;
    if (tcport.empty())
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
        if (er->Init(tcport))
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("EasyReader initialised on port \"%C\"\n"),
                tcport.c_str()));
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("EasyReader initialisation failed on port \"%C\"\n"),
                tcport.c_str()));
            result = false;
        }
    }
    else
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Timecode will be derived from PC clock\n")));
    }

    // Set destination
    mMixDestination = mix_dest;

    // VT name and router info
    // Eventually this will come from database but 
    // hard-coded for now.
    mVts.clear();
    mVts.push_back( Vt(vt1_dest, "VT1") );
    mVts.push_back( Vt(vt2_dest, "VT2") );
    mVts.push_back( Vt(vt3_dest, "VT3") );
    mVts.push_back( Vt(vt4_dest, "VT4") );


    // Init router
    result &= mpRouter->Init(rport, router_tcp);

#if 0
    // Get info about current routing to write at start of file
    // and/or in database
    std::string tc = mpTcReader->Timecode();

    unsigned int src_index = mpRouter->CurrentSrc(mMixDestination);

    if (src_index > 0)
    {
        std::string srcstring;
        uint32_t track_i;
        mpSrcReader->GetSource(src_index, srcstring, track_i);

        mLastSrc = srcstring;
        mLastTc = tc;
    }
#endif

    if (mpRouter->Connected())
    {
        // Start monitoring messages from router
        mpRouter->SetObserver(this);
        mpRouter->activate();

        // Query routing to destinations we're interested in
        mpRouter->QuerySrc(mMixDestination);
        for (std::vector<Vt>::iterator it = mVts.begin(); it != mVts.end(); ++it)
        {
            mpRouter->QuerySrc(it->router_dest);
        }
    }

    return result;
}

::ProdAuto::TrackStatusList * SimplerouterloggerImpl::TracksStatus (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    std::string tc_string = mpTcReader->Timecode();
    Timecode tc(tc_string.c_str());

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.timecode.samples = tc.FramesSinceMidnight();

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("TracksStatus - timecode %C\n"), tc.Text()));

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

::ProdAuto::Recorder::ReturnCode SimplerouterloggerImpl::Start (
    ::ProdAuto::MxfTimecode & start_timecode,
    const ::ProdAuto::MxfDuration & pre_roll,
    const ::CORBA::BooleanSeq & rec_enable,
    ::CORBA::Boolean test_only
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SimplerouterloggerImpl::Start()\n")));

    FileUtils::CreatePath(RECORD_DIR);

    std::string date = DateTime::DateNoSeparators();
    Timecode tc = mpTcReader->Timecode().c_str();
    std::ostringstream ss;
    ss << RECORD_DIR << date << "_" << tc.TextNoSeparators() << "_" << mName << ".txt";


    //start saving to file
    StartSavingFile(ss.str());

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.rec = 1;

    // so a sensible number comes back, even for start now
    start_timecode.undefined = false; 
    start_timecode.samples = tc.FramesSinceMidnight();

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

::ProdAuto::Recorder::ReturnCode SimplerouterloggerImpl::Stop (
    ::ProdAuto::MxfTimecode & mxf_stop_timecode,
    const ::ProdAuto::MxfDuration & mxf_post_roll,
    const char * project,
    const char * description,
    const ::ProdAuto::LocatorSeq & locators,
    ::CORBA::StringSeq_out files
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SimplerouterloggerImpl::Stop()\n")));

    StopSavingFile();

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.rec = 0;

    // Create out parameter
    files = new ::CORBA::StringSeq;
    files->length(1);
    // TODO: Put the actual filename in here
    files->operator[](0) = CORBA::string_dup("filename.txt");

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

void SimplerouterloggerImpl::SetTimecodePort(std::string tcp)
{
    mTimecodePort = tcp;
}

void SimplerouterloggerImpl::SetRouterPort(std::string rp)
{
    mRouterPort = rp;
}


void SimplerouterloggerImpl::StartSavingFile(const std::string & filename)
{
    // Get info about current routing to write at start of file
    // and/or in database
    std::string tc = mpTcReader->Timecode();
#if 0
    unsigned int src_index = mpRouter->CurrentSrc(mMixDestination);

    std::string srcstring;
    uint32_t track;
    mpSrcReader->GetSource(src_index, srcstring, track);
    std::string deststring;
    mpSrcReader->GetDestination(mMixDestination, deststring);
#else
    std::string deststring = "Mixer Out";
#endif

    // Open file
    mpFile = ACE_OS::fopen (filename.c_str(), ACE_TEXT ("w+"));

    if (mpFile == 0)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT ("problem with filename %p\n"), ACE_TEXT (filename.c_str()) ));
    }
    else
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT ("filename ok %C\n"), filename.c_str()));

        // Write info at head of file
	    ACE_OS::fprintf (mpFile, "Destination index = %d, name = %s\n", mMixDestination, deststring.c_str() );
	    ACE_OS::fprintf (mpFile, "%s start   source name = %s\n", tc.c_str(), mLastSrc.c_str() );
    }

    // Open database file
    if (mpCutsDatabase)
    {
        mpCutsDatabase->OpenAppend();
        if (! mLastSrc.empty())
        {
            mpCutsDatabase->AppendEntry(mLastSrc, mLastTc);
        }
    }
}

void SimplerouterloggerImpl::StopSavingFile()
{
	// Put finish time at end of file and close
    if (mpFile)
    {
        std::string tc = mpTcReader->Timecode();
	    ACE_OS::fprintf (mpFile, "%s end\n", tc.c_str() );

        if (ACE_OS::fclose (mpFile) == -1)
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT ("problem closing file\n")));
        }
        mpFile = 0;
    }

    if (mpCutsDatabase)
    {
        mpCutsDatabase->Close();
    }
}

void SimplerouterloggerImpl::Observe(std::string msg)
{
    // Get timestamp
    std::string tc = mpTcReader->Timecode();

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C %C\n"), tc.c_str(), msg.c_str()));
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C %C\n"), tc.c_str(), srcstring.c_str()));

    unsigned int dest = ACE_OS::atoi(msg.substr(2, 3).c_str());
    unsigned int src  = ACE_OS::atoi(msg.substr(6).c_str());

    // Update our routing records for the recorders
    for (std::vector<Vt>::iterator it = mVts.begin(); it != mVts.end(); ++it)
    {
        if (dest == it->router_dest)
        {
            it->router_src = src;
        }
    }


    // Process changes to mixer-out destination
    if (dest == mMixDestination)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT ("mixer-out destination\n")));

        // Are we recording the source just selected?
        // Avoid VT1 if possible as that may be recording mixer out
        // so we take highest number match.
        std::string src_name;
        for (std::vector<Vt>::const_iterator it = mVts.begin(); it != mVts.end(); ++it)
        {
            if (it->router_src == src)
            {
                src_name = it->name;
            }
        }

        // Remember last cut for when we start a new recording
        mLastSrc = src_name;
        mLastTc = tc;

        //save if file is open
        if (mpFile != 0)
        {
    	    ACE_OS::fprintf (mpFile, "%s update  source index = %3d, name = %s\n", tc.c_str(), src, src_name.c_str() );
        }

        // update database file
        if (mpCutsDatabase)
        {
            mpCutsDatabase->AppendEntry(src_name, tc);
        }
    }
}

::ProdAuto::TrackList * SimplerouterloggerImpl::Tracks (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    ProdAuto::TrackList_var tracks = mTracks;
    return tracks._retn();
}



