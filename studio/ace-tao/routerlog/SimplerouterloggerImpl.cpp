/*
 * $Id: SimplerouterloggerImpl.cpp,v 1.7 2008/11/06 11:08:37 john_f Exp $
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
#include "quartzRouter.h"
#include "CutsDatabase.h"
#include "routerloggerApp.h"

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

//SimplerouterloggerImpl * SimplerouterloggerImpl::mInstance = 0;


// Implementation skeleton constructor
SimplerouterloggerImpl::SimplerouterloggerImpl (void)
: mpFile(0), mpCutsDatabase(0), mMixDestination(0)
{
}

// Implementation skeleton destructor
SimplerouterloggerImpl::~SimplerouterloggerImpl (void)
{
    delete mpCutsDatabase;
}


// Initialise the routerlogger
bool SimplerouterloggerImpl::Init(const std::string & name, const std::string & db_file,
                                  unsigned int mix_dest, const std::vector<RouterDestination*> & dests)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SimplerouterloggerImpl::Init() name \"%C\" mix_dest %d\n"),
        name.c_str(), mix_dest));
    bool ok = true;

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

    // Base class initialisation
    // Each channel has 1 video and 4 or 8 audio tracks
    const unsigned int max_inputs = 1;
    const unsigned int max_tracks_per_input = 1;
    ok = ok && RecorderImpl::Init(name, max_inputs, max_tracks_per_input);

    // Setup format reply
    mFormat = "*ROUTER*";

    // Init cuts database
    mpCutsDatabase = new CutsDatabase();
    if (mpCutsDatabase)
    {
        mpCutsDatabase->Filename(db_file);
    }


    // Set destination
    mMixDestination = mix_dest;

    // VT name and router info
    mVts.clear();
    for (std::vector<RouterDestination *>::const_iterator
        it = dests.begin(); it != dests.end(); ++it)
    {
        RouterDestination * rd = *it;
        mVts.push_back( Vt(rd->output_number, rd->name) );
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C: dest %d \"%C\"\n"),
            mName.c_str(), rd->output_number, rd->name.c_str()));
    }

    return ok;
}

::ProdAuto::TrackStatusList * SimplerouterloggerImpl::TracksStatus (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    std::string tc_string = routerloggerApp::Instance()->Timecode();
    Timecode tc(tc_string.c_str());

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.timecode.samples = tc.FramesSinceMidnight();
    ts.timecode.undefined = 0;

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("TracksStatus - timecode %C\n"), tc.Text()));

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

::ProdAuto::Recorder::ReturnCode SimplerouterloggerImpl::Start (
    ::ProdAuto::MxfTimecode & start_timecode,
    const ::ProdAuto::MxfDuration & pre_roll,
    const ::CORBA::BooleanSeq & rec_enable,
    const char * project,
    ::CORBA::Boolean test_only
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_INFO, ACE_TEXT("%C SimplerouterloggerImpl::Start()\n"), mName.c_str()));

    FileUtils::CreatePath(RECORD_DIR);

    std::string date = DateTime::DateNoSeparators();
    Timecode tc = routerloggerApp::Instance()->Timecode().c_str();
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
    const char * description,
    const ::ProdAuto::LocatorSeq & locators,
    ::CORBA::StringSeq_out files
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_INFO, ACE_TEXT("%C SimplerouterloggerImpl::Stop()\n"), mName.c_str()));

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
    std::string tc = routerloggerApp::Instance()->Timecode();

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
    // No longer using this format
    //mpFile = ACE_OS::fopen (filename.c_str(), ACE_TEXT ("w+"));

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
        std::string tc = routerloggerApp::Instance()->Timecode();
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

void SimplerouterloggerImpl::Observe(unsigned int src, unsigned int dest)
{
    // Get timestamp
    std::string tc = routerloggerApp::Instance()->Timecode();

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Observe: tc=%C\n"), tc.c_str()));

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
        if (src_name.empty())
        {
            ACE_DEBUG((LM_INFO, ACE_TEXT ("%C: mix_dest src %d not being recorded\n"),
                mName.c_str(), src));
        }
        else
        {
            ACE_DEBUG((LM_INFO, ACE_TEXT ("%C: mix_dest src %d being recorded on %C\n"),
                mName.c_str(), src, src_name.c_str()));

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



