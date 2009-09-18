/*
 * $Id: SimplerouterloggerImpl.cpp,v 1.14 2009/09/18 16:22:17 john_f Exp $
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

#include "integer_types.h"

#include <ace/Log_Msg.h>

#include <sstream>
#include <map>

#include "SimplerouterloggerImpl.h"
#include "routerloggerApp.h"
#include "FileUtils.h"
#include "ClockReader.h"
#include "EasyReader.h"
#include "DateTime.h"
#include "Timecode.h"
#include "quartzRouter.h"
#include "CutsDatabase.h"
#include "RecorderSettings.h"

#include "Database.h"
#include "DBException.h"

// We only record one "track"
const unsigned int max_inputs = 1;
const unsigned int max_tracks_per_input = 1;

const bool USE_PROJECT_SUBDIR = true;

// Path separator.  It would be different for Windows.
const char PATH_SEPARATOR = '/';

// Format in cut recording file
const char * const mccut_fmt = "<McCut ClipName=\"%s\" SelectorIndex=\"%d\" "
                "Date=\"%d-%02d-%02d\" Position=\"%"PRId64"\" EditRate=\"%d/%d\"/>\n";


namespace
{
void clean_filename(std::string & filename)
{
    const std::string allowed_chars = ".0123456789abcdefghijklmnopqrstuvwxyz_-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char replacement_char = '_';

    size_t pos;
    while (std::string::npos != (pos = filename.find_first_not_of(allowed_chars)))
    {
        filename.replace(pos, 1, 1, replacement_char);
    }
}
};


// Constructor for Vt struct
Vt::Vt(unsigned int rd, const std::string & n)
: router_dest(rd), router_src(0), name(n), selector_index(0)
{
}

//SimplerouterloggerImpl * SimplerouterloggerImpl::mInstance = 0;


// Implementation skeleton constructor
SimplerouterloggerImpl::SimplerouterloggerImpl (void)
: mpFile(0), mpCutsDatabase(0), mCopyManager(CopyMode::NEW), mRecIndex(0), mMixDestination(0), mMcTrackId(0), mLastSelectorIndex(0), mSaving(false)
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

    // Setup CopyManager
    mCopyManager.RecorderName(name);

    // Setup pre-roll
    mMaxPreRoll.undefined = false;
    mMaxPreRoll.edit_rate = EDIT_RATE;
    // Pre-roll is really zero but gui will use lowest when controlling
    // more than one recorder so we make it 10.
    mMaxPreRoll.samples = 10;

    // Setup post-roll
    mMaxPostRoll.undefined = true; // no limit to post-roll
    mMaxPostRoll.edit_rate = EDIT_RATE;
    mMaxPostRoll.samples = 0;

    // Base class initialisation
    RecorderImpl::Init(name);
    ok = ok && UpdateFromDatabase(max_inputs, max_tracks_per_input);

    // Setup format reply
    mFormat = "*ROUTER*";

    // Init cuts database
    if (! db_file.empty())
    {
        mpCutsDatabase = new CutsDatabase();
        if (mpCutsDatabase)
        {
            mpCutsDatabase->Filename(db_file);
        }
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

    // Store whole multi-cam clip structure
    mMcClipDef.reset(mc_clip_def);

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
                        vt->selector_index = it->second->index;
                    }
                }
            }
        }
    }

    // Start copy process
    this->StartCopying(0);

    return ok;
}

::ProdAuto::MxfDuration SimplerouterloggerImpl::MaxPreRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPreRoll;
}

::ProdAuto::MxfDuration SimplerouterloggerImpl::MaxPostRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPostRoll;
}

::ProdAuto::Rational SimplerouterloggerImpl::EditRate (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return EDIT_RATE;
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
    ts.timecode.edit_rate.numerator = tc.EditRateNumerator();
    ts.timecode.edit_rate.denominator = tc.EditRateDenominator();

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("TracksStatus - timecode %C\n"), tc.Text()));

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

/**
  Start
*/
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
    ACE_UNUSED_ARG (rec_enable);
    ACE_UNUSED_ARG (project);
    ACE_UNUSED_ARG (test_only);

    ACE_DEBUG((LM_INFO, ACE_TEXT("%C SimplerouterloggerImpl::Start()\n"), mName.c_str()));

    // Get current recorder settings
    RecorderSettings * settings = RecorderSettings::Instance();
    settings->Update(this->Recorder());

    // Create any needed paths.
    // NB. Paths need to be same as those used in recorder_fucntions.cpp
    for (std::vector<EncodeParams>::iterator it = settings->encodings.begin();
        it != settings->encodings.end(); ++it)
    {
        if (USE_PROJECT_SUBDIR)
        {
            std::string project_subdir = project;
            clean_filename(project_subdir);
            it->dir += PATH_SEPARATOR;
            it->dir += project_subdir;
            it->copy_dest += PATH_SEPARATOR;
            it->copy_dest += project_subdir;
        }

        FileUtils::CreatePath(it->dir);
    }

    // Calculate start timecode
    Timecode tc;
    if (start_timecode.undefined)
    {
        // Start now
        tc = routerloggerApp::Instance()->Timecode().c_str();
    }
    else
    {
        // Start at start - pre-roll
        tc = start_timecode.samples - pre_roll.samples;
    }

    // Generate filename for the recording
    std::ostringstream ss;
    
    std::string date = DateTime::DateNoSeparators();
    std::string tcode = tc.TextNoSeparators();

    ss << date << "_" << tcode
        << "_" << project
        << "_" << this->Name()
        << ".xml";

    std::string filename = ss.str();
    clean_filename(filename);

    std::string pathname;
    if (settings->encodings.size() > 0)
    {
        pathname = settings->encodings.begin()->dir;
        pathname += PATH_SEPARATOR;
    }
    pathname += filename;

    // Enable traffic control on copy process.
    this->StopCopying(++mRecIndex);

    // Start saving to cuts database
    StartSaving(tc, pathname);

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.rec = 1;

    // Return start timecode
    start_timecode.undefined = false; 
    start_timecode.edit_rate.numerator = tc.EditRateNumerator();
    start_timecode.edit_rate.denominator = tc.EditRateDenominator();
    start_timecode.samples = tc.FramesSinceMidnight();

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

/**
  Stop
*/
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
    ACE_UNUSED_ARG (mxf_post_roll);
    ACE_UNUSED_ARG (description);
    ACE_UNUSED_ARG (locators);

    ACE_DEBUG((LM_INFO, ACE_TEXT("%C SimplerouterloggerImpl::Stop()\n"), mName.c_str()));

    StopSaving();

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.rec = 0;

    // Return stop timecode
    // (otherwise with stop now it would be left as zero)
    Timecode tc = routerloggerApp::Instance()->Timecode().c_str();
    mxf_stop_timecode.undefined = false; 
    mxf_stop_timecode.edit_rate.numerator = tc.EditRateNumerator();
    mxf_stop_timecode.edit_rate.denominator = tc.EditRateDenominator();
    mxf_stop_timecode.samples = tc.FramesSinceMidnight();

    // Create out parameter to return (dummy) filename
    files = new ::CORBA::StringSeq;
    files->length(1);
    // TODO: Put the actual filename in here
    files->operator[](0) = CORBA::string_dup("filename.txt");

    // Release traffic control on copy process.
    this->StartCopying(mRecIndex);

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

/**
Means of externally forcing a re-reading of config from database.
*/
void SimplerouterloggerImpl::UpdateConfig (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    UpdateFromDatabase(max_inputs, max_tracks_per_input);
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

// Non-IDL stuff follows

void SimplerouterloggerImpl::SetTimecodePort(std::string tcp)
{
    mTimecodePort = tcp;
}

void SimplerouterloggerImpl::SetRouterPort(std::string rp)
{
    mRouterPort = rp;
}


void SimplerouterloggerImpl::StartSaving(const Timecode & tc, const std::string & filename)
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

    // Open file
    mpFile = ACE_OS::fopen (filename.c_str(), ACE_TEXT ("w+"));

    if (mpFile == 0)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT ("problem with filename %p\n"), ACE_TEXT (filename.c_str()) ));
    }
    else
    {
        // Add header
        ACE_OS::fprintf (mpFile, "<?xml version=\"1.0\" encoding =\"UTF-8\" standalone=\"no\"?>\n<root>\n");

        // First of all, find the SourceConfigs and Locations relevant to the multi-cam clip
        std::map<std::string, prodauto::SourceConfig *> sc_map;
        std::map<long, std::string> loc_map;
        for (std::map<uint32_t, prodauto::MCTrackDef *>::const_iterator
            trk_i = mMcClipDef->trackDefs.begin(); trk_i != mMcClipDef->trackDefs.end(); ++trk_i)
        {
            // multi-cam track
            const prodauto::MCTrackDef * const & trk = trk_i->second;

            for (std::map<uint32_t, prodauto::MCSelectorDef *>::const_iterator
                sel_i = trk->selectorDefs.begin(); sel_i != trk->selectorDefs.end(); ++sel_i)
            {
                // multi-cam selector
                const prodauto::MCSelectorDef * const & sel = sel_i->second;

                // source config
                prodauto::SourceConfig * sc = sel->sourceConfig;

                // store in map
                sc_map[sc->name] = sc;

                // location
                long loc_id = sc->recordingLocation;
                try
                {
                    std::string loc_name = prodauto::Database::getInstance()->loadLocationName(loc_id);
                    loc_map[loc_id] = loc_name;
                }
                catch (const prodauto::DBException & ex)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("Databse exception:\n  %C\n"), ex.getMessage().c_str()));
                }
            }
        }

        // Write locations to XML
        for (std::map<long, std::string>::const_iterator
            it = loc_map.begin(); it != loc_map.end(); ++it)
        {
            ACE_OS::fprintf (mpFile, "<Location Name=\"%s\"/>\n", it->second.c_str());
        }

        // Write SourceConfigs to XML
        for (std::map<std::string, prodauto::SourceConfig *>::const_iterator
            it = sc_map.begin(); it != sc_map.end(); ++it)
        {
            const prodauto::SourceConfig * const & sc = it->second;
            if (sc)
            {
                // source config
                ACE_OS::fprintf (mpFile, "<SourceConfig Name=\"%s\" LocationName=\"%s\">\n",
                    sc->name.c_str(), loc_map[sc->recordingLocation].c_str());

                for (std::vector<prodauto::SourceTrackConfig *>::const_iterator
                    stc_i = sc->trackConfigs.begin(); stc_i != sc->trackConfigs.end(); ++stc_i)
                {
                    // source config track
                    const prodauto::SourceTrackConfig * const & stc = *stc_i;
                    if (stc)
                    {
                        ACE_OS::fprintf (mpFile, "  <SourceTrackConfig Name=\"%s\" Id=\"%d\" Number=\"%d\" DataDef=\"%s\"/>\n",
                            stc->name.c_str(), stc->id, stc->number,
                            DatabaseEnums::Instance()->DataDefName(stc->dataDef).c_str());
                    }
                }

                ACE_OS::fprintf (mpFile, "</SourceConfig>\n");
            }
        }

        // Add multi-cam clip info
        ACE_OS::fprintf (mpFile, "<McClip Name=\"%s\">\n", mMcClipDef->name.c_str());

        for (std::map<uint32_t, prodauto::MCTrackDef *>::const_iterator
            trk_i = mMcClipDef->trackDefs.begin(); trk_i != mMcClipDef->trackDefs.end(); ++trk_i)
        {
            // multi-cam track
            const prodauto::MCTrackDef * const & trk = trk_i->second;
            ACE_OS::fprintf (mpFile, "  <McTrack Index=\"%d\" Number=\"%d\">\n", trk->index, trk->number);

            for (std::map<uint32_t, prodauto::MCSelectorDef *>::const_iterator
                sel_i = trk->selectorDefs.begin(); sel_i != trk->selectorDefs.end(); ++sel_i)
            {
                // multi-cam selector
                const prodauto::MCSelectorDef * const & sel = sel_i->second;

                // source config
                prodauto::SourceConfig * sc = sel->sourceConfig;

                ACE_OS::fprintf (mpFile, "    <McSelector Index=\"%d\" SourceConfigName=\"%s\" SourceTrackConfigId=\"%d\"/>\n",
                    sel->index,
                    (sc ? sc->name.c_str() : ""),
                    sel->sourceTrackID);
            }

            ACE_OS::fprintf (mpFile, "  </McTrack>\n");
        }

        ACE_OS::fprintf (mpFile, "</McClip>\n");
    }

    // Open database file
    if (mpCutsDatabase)
    {
        mpCutsDatabase->OpenAppend();
        if (! mLastSrc.empty())
        {
            mpCutsDatabase->AppendEntry(mLastSrc, mLastTc, mLastYear, mLastMonth, mLastDay);
        }
    }

    // Add an initial entry at the start record timecode
    if (mMcTrackId && mLastSelectorIndex)
    {
        // Get date
        int year, month, day;
        DateTime::GetDate(year, month, day);

        // Date as prodauto::Date
        prodauto::Date date;
        date.year = year;
        date.month = month;
        date.day = day;

        std::auto_ptr<prodauto::MCCut> mc_cut(new prodauto::MCCut);

        mc_cut->mcTrackId = mMcTrackId;
        mc_cut->mcSelectorIndex = mLastSelectorIndex;
        mc_cut->cutDate = date;
        mc_cut->position = tc.FramesSinceMidnight();
        mc_cut->editRate = prodauto::g_palEditRate;

        // Save to database
        try
        {
            prodauto::Database::getInstance()->saveMultiCameraCut(mc_cut.get());
        }
        catch (const prodauto::DBException & ex)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Databse exception:\n  %C\n"), ex.getMessage().c_str()));
        }

        // Save to file
        if (mpFile != 0)
        {
            ACE_OS::fprintf(mpFile, mccut_fmt,
                mMcClipDef->name.c_str(), mc_cut->mcSelectorIndex,
                mc_cut->cutDate.year, mc_cut->cutDate.month, mc_cut->cutDate.day,
                mc_cut->position, mc_cut->editRate.numerator, mc_cut->editRate.denominator);
        }
    }
    
    mSaving = true;
}

void SimplerouterloggerImpl::StopSaving()
{
    if (mpFile)
    {
        // Add footer
        ACE_OS::fprintf (mpFile, "</root>\n");

        // Close file
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

    mSaving = false;
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
        uint32_t mc_selector_index = 0;
        for (std::vector<Vt>::const_iterator it = mVts.begin(); it != mVts.end(); ++it)
        {
            if (it->router_src == src)
            {
                src_name = it->name;
                mc_selector_index = it->selector_index;
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
            mLastSelectorIndex = mc_selector_index;
            mLastTc = tc;
            mLastYear = year;
            mLastMonth = month;
            mLastDay = day;

            if (mSaving)
            {
                // Update database file
                if (mpCutsDatabase)
                {
                    mpCutsDatabase->AppendEntry(src_name, tc, year, month, day);
                }

                // Write to main database and to file
                if (mMcTrackId)
                {
                    std::auto_ptr<prodauto::MCCut> mc_cut(new prodauto::MCCut);

                    mc_cut->mcTrackId = mMcTrackId;
                    mc_cut->mcSelectorIndex = mc_selector_index;
                    mc_cut->cutDate = date;
                    mc_cut->position = Timecode(tc.c_str()).FramesSinceMidnight();
                    mc_cut->editRate = prodauto::g_palEditRate;

                    // Write to database
                    try
                    {
                        prodauto::Database::getInstance()->saveMultiCameraCut(mc_cut.get());
                    }
                    catch (const prodauto::DBException & ex)
                    {
                        ACE_DEBUG((LM_ERROR, ACE_TEXT("Databse exception:\n  %C\n"), ex.getMessage().c_str()));
                    }

                    // Write to file
                    if (mpFile)
                    {
                        ACE_OS::fprintf(mpFile, mccut_fmt,
                            mMcClipDef->name.c_str(), mc_cut->mcSelectorIndex,
                            mc_cut->cutDate.year, mc_cut->cutDate.month, mc_cut->cutDate.day,
                            mc_cut->position, mc_cut->editRate.numerator, mc_cut->editRate.denominator);
                    }
                }

            }
        }
    }
}

void SimplerouterloggerImpl::StartCopying(unsigned int index)
{
    mCopyManager.Command(RecorderSettings::Instance()->copy_command);
    mCopyManager.ClearSrcDest();
    std::vector<EncodeParams> & encodings = RecorderSettings::Instance()->encodings;

    for (std::vector<EncodeParams>::const_iterator it = encodings.begin(); it != encodings.end(); ++it)
    {
        mCopyManager.AddSrcDest(it->dir, it->copy_dest, it->copy_priority);
    }

    mCopyManager.StartCopying(index);
}

void SimplerouterloggerImpl::StopCopying(unsigned int index)
{
    mCopyManager.StopCopying(index);
}




