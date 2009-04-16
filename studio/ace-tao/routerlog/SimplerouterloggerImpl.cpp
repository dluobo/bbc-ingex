/*
 * $Id: SimplerouterloggerImpl.cpp,v 1.12 2009/04/16 18:32:47 john_f Exp $
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

#include "Database.h"
#include "DBException.h"

//#ifdef WIN32
//const std::string RECORD_DIR = "C:\\TEMP\\RouterLogs\\";
//#else
//const std::string RECORD_DIR = "/var/tmp/RouterLogs/";
//#endif

// Constructor for Vt struct
Vt::Vt(unsigned int rd, const std::string & n)
: router_dest(rd), router_src(0), name(n), selector_id(0)
{
}

//SimplerouterloggerImpl * SimplerouterloggerImpl::mInstance = 0;


// Implementation skeleton constructor
SimplerouterloggerImpl::SimplerouterloggerImpl (void)
: mpFile(0), mpCutsDatabase(0), mMixDestination(0), mMcTrackId(0)
{
}

// Implementation skeleton destructor
SimplerouterloggerImpl::~SimplerouterloggerImpl (void)
{
    delete mpCutsDatabase;
}


// Initialise the routerlogger
bool SimplerouterloggerImpl::Init(const std::string & name, const std::string & mc_clip_name, const std::string & db_file,
                                  unsigned int mix_dest, const std::vector<RouterDestination*> & dests)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SimplerouterloggerImpl::Init() name \"%C\", mix_dest \"%C\" on %d\n"),
        name.c_str(), mc_clip_name.c_str(), mix_dest));
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

    // Load multi-cam clip def we are recording cuts for.
    prodauto::MCClipDef * mc_clip_def = 0;
    if (!mc_clip_name.empty())
    {
        try
        {
            mc_clip_def = prodauto::Database::getInstance()->loadMultiCameraClipDef(mc_clip_name);
        }
        catch (const prodauto::DBException & ex)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Databse exception:\n  %C\n"), ex.getMessage().c_str()));
        }
    }

    // Now find multi-cam track def we are recording cuts for.
    // We want the video track and assume it's the one with index == 1.
    prodauto::MCTrackDef * mc_track_def = 0;
    if (mc_clip_def)
    {
        mc_track_def = mc_clip_def->trackDefs[1];
    }
    if (mc_track_def)
    {
        // Store Id of track we are recording selections for.
        mMcTrackId = mc_track_def->getDatabaseID();

        // Look at the possible selectors and their source config names
        for (std::map<uint32_t, prodauto::MCSelectorDef *>::const_iterator
            it = mc_track_def->selectorDefs.begin(); it != mc_track_def->selectorDefs.end(); ++it)
        {
            prodauto::SourceConfig * sc = it->second->sourceConfig;
            if (sc)
            {
                // Search for source config name in our list of VTs
                bool found = false;
                for (std::vector<Vt>::iterator vt = mVts.begin(); vt != mVts.end() && !found; ++vt)
                {
                    if (sc->name == vt->name)
                    {
                        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Selector with SourceConfig \"%C\" found\n"), sc->name.c_str()));
                        found = true;
                        vt->selector_id = it->second->getDatabaseID();
                    }
                }
            }
        }
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
    ACE_UNUSED_ARG (pre_roll);
    ACE_UNUSED_ARG (rec_enable);
    ACE_UNUSED_ARG (project);
    ACE_UNUSED_ARG (test_only);

    ACE_DEBUG((LM_INFO, ACE_TEXT("%C SimplerouterloggerImpl::Start()\n"), mName.c_str()));

    //FileUtils::CreatePath(RECORD_DIR);

    //std::string date = DateTime::DateNoSeparators();
    //Timecode tc = routerloggerApp::Instance()->Timecode().c_str();
    //std::ostringstream ss;
    //ss << RECORD_DIR << date << "_" << tc.TextNoSeparators() << "_" << mName << ".txt";


    // Start saving to cuts database
    StartSaving();

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.rec = 1;

    // Return start timecode
    // (otherwise with start now it would be left as zero)
    Timecode tc = routerloggerApp::Instance()->Timecode().c_str();
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
    ACE_UNUSED_ARG (mxf_stop_timecode);
    ACE_UNUSED_ARG (mxf_post_roll);
    ACE_UNUSED_ARG (description);
    ACE_UNUSED_ARG (locators);

    ACE_DEBUG((LM_INFO, ACE_TEXT("%C SimplerouterloggerImpl::Stop()\n"), mName.c_str()));

    StopSaving();

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


void SimplerouterloggerImpl::StartSaving()
{
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

#if 0
    // No longer using this format
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
        std::string tc = routerloggerApp::Instance()->Timecode();
        ACE_OS::fprintf (mpFile, "Destination index = %d, name = %s\n", mMixDestination, deststring.c_str() );
        ACE_OS::fprintf (mpFile, "%s start   source name = %s\n", tc.c_str(), mLastSrc.c_str() );
    }
#endif

    // Open database file
    if (mpCutsDatabase)
    {
        mpCutsDatabase->OpenAppend();
        if (! mLastSrc.empty())
        {
            mpCutsDatabase->AppendEntry(mLastSrc, mLastTc, mLastYear, mLastMonth, mLastDay);
        }
    }
}

void SimplerouterloggerImpl::StopSaving()
{
#if 0
    // No longer using this format
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
#endif

    if (mpCutsDatabase)
    {
        mpCutsDatabase->Close();
    }
}

void SimplerouterloggerImpl::Observe(unsigned int src, unsigned int dest)
{
    // Get timecode
    std::string tc = routerloggerApp::Instance()->Timecode();

    // Get date
    int year, month, day;
    DateTime::GetDate(year, month, day);

    // Date as prodauto::Date
    prodauto::Date date;
    date.year = year;
    date.month = month;
    date.day = day;

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
        long mc_selector = 0;
        for (std::vector<Vt>::const_iterator it = mVts.begin(); it != mVts.end(); ++it)
        {
            if (it->router_src == src)
            {
                src_name = it->name;
                mc_selector = it->selector_id;
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
            mLastYear = year;
            mLastMonth = month;
            mLastDay = day;

            //save if file is open
            if (mpFile != 0)
            {
                ACE_OS::fprintf (mpFile, "%s update  source index = %3d, name = %s\n", tc.c_str(), src, src_name.c_str() );
            }

            // update database file
            if (mpCutsDatabase)
            {
                mpCutsDatabase->AppendEntry(src_name, tc, year, month, day);
            }

            // experimentally try writing to main database
            if (mMcTrackId)
            {
                try
                {
                    std::auto_ptr<prodauto::MCCut> mc_cut(new prodauto::MCCut);

                    mc_cut->mcTrackId = mMcTrackId;
                    mc_cut->mcSelectorId = mc_selector;
                    mc_cut->cutDate = date;
                    mc_cut->position = Timecode(tc.c_str()).FramesSinceMidnight();
                    mc_cut->editRate = prodauto::g_palEditRate;

                    prodauto::Database::getInstance()->saveMultiCameraCut(mc_cut.get());
                }
                catch (const prodauto::DBException & ex)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("Databse exception:\n  %C\n"), ex.getMessage().c_str()));
                }
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



