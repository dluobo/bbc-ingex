/*
 * $Id: RecorderImpl.cpp,v 1.4 2008/09/03 13:43:34 john_f Exp $
 *
 * Base class for Recorder servant.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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

#include <ace/OS_NS_time.h>
#include <ace/Min_Max.h>

#include <sstream>
#include <map>
#include <string>

#include <Database.h>
#include <DatabaseEnums.h>
#include <DBException.h>

#include "RecorderSettings.h"
#include "RecorderImpl.h"


// Implementation skeleton constructor
RecorderImpl::RecorderImpl (void)
: mMaxInputs(0), mMaxTracksPerInput(0), mVideoTrackCount(0), mFormat("Ingex recorder")
{
    mTracks = new ProdAuto::TrackList;
    mTracksStatus = new ProdAuto::TrackStatusList;
}

// Initialise the recorder
bool RecorderImpl::Init(std::string name, std::string db_user, std::string db_pw,
                        unsigned int max_inputs, unsigned int max_tracks_per_input)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("RecorderImpl::Init(%C, %C, %C)\n"),
        name.c_str(), db_user.c_str(), db_pw.c_str()));

    // Save name
    mName = name;

    // Set maximum number of tracks for this particular implementation,
    // e.g. as determined from capture cards installed in the machine.
    mMaxInputs = max_inputs;
    mMaxTracksPerInput = max_tracks_per_input;

    // Initialise database connection
    bool db_init_ok = false;
    try
    {
        prodauto::Database::initialise("prodautodb", db_user, db_pw, 4, 12);
        db_init_ok = true;
    }
    catch (...)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed!\n")));
    }

    // Read enumeration names from database.
    // Do it now to avoid any delay when first used.
    DatabaseEnums::Instance();

    // Update tracks and settings from database
    UpdateFromDatabase();


    return db_init_ok;
}

// Implementation skeleton destructor
RecorderImpl::~RecorderImpl (void)
{
    prodauto::Database::close();
}

::ProdAuto::MxfDuration RecorderImpl::MaxPreRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPreRoll;
}

::ProdAuto::MxfDuration RecorderImpl::MaxPostRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPostRoll;
}

char * RecorderImpl::RecordingFormat (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return CORBA::string_dup(mFormat.c_str());
}

::ProdAuto::Rational RecorderImpl::EditRate (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return EDIT_RATE;
}

::ProdAuto::TrackList * RecorderImpl::Tracks (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // (The IngexGUI controller invokes the Tracks operation
    // when it connects to a recorder.)

    // Update from database
    UpdateFromDatabase();

    // Make a copy of tracks to return
    ProdAuto::TrackList_var tracks = mTracks;
    return tracks._retn();
}

::CORBA::StringSeq * RecorderImpl::Configs (
    void
  )
  ACE_THROW_SPEC ((
    ::CORBA::SystemException
  ))
{
    CORBA::StringSeq_var config_list = new CORBA::StringSeq;

    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    prodauto::Recorder * rec = 0;
    if (db)
    {
        try
        {
            rec = db->loadRecorder(mName);
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }
    std::auto_ptr<prodauto::Recorder> recorder(rec); // so it gets deleted

    if (rec)
    {
        try
        {
            std::vector<prodauto::RecorderConfig *> & configs = rec->getAllConfigs();
            unsigned int n_configs = configs.size();
            config_list->length(n_configs);
            for (unsigned int i = 0; i < n_configs; ++i)
            {
                config_list->operator[](i) = CORBA::string_dup(configs[i]->name.c_str());
            }
              
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }

    return config_list._retn();
}

char * RecorderImpl::CurrentConfig (
    void
  )
  ACE_THROW_SPEC ((
    ::CORBA::SystemException
  ))
{
    std::string config_name;
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    prodauto::Recorder * rec = 0;
    if (db)
    {
        try
        {
            rec = db->loadRecorder(mName);
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }
    std::auto_ptr<prodauto::Recorder> recorder(rec); // so it gets deleted

    if (rec)
    {
        try
        {
            prodauto::RecorderConfig * rc = rec->getConfig();
            if (rc)
            {
                config_name = rc->name;
            }
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }

    return CORBA::string_dup(config_name.c_str());
}

::CORBA::Boolean RecorderImpl::SelectConfig (
    const char * config
  )
  ACE_THROW_SPEC ((
    ::CORBA::SystemException
  ))
{
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    prodauto::Recorder * rec = 0;
    if (db)
    {
        try
        {
            rec = db->loadRecorder(mName);
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }
    std::auto_ptr<prodauto::Recorder> recorder(rec); // so it gets deleted

    bool found = false;
    if (rec)
    {
        try
        {
            std::vector<prodauto::RecorderConfig *> & configs = rec->getAllConfigs();
            for (std::vector<prodauto::RecorderConfig *>::const_iterator it = configs.begin();
                !found && it != configs.end(); ++it)
            {
                if (ACE_OS::strcmp((*it)->name.c_str(), config) == 0)
                {
                    rec->setConfig(*it);
                    found = true;
                }
            }
              
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }

    return (found ? 1 : 0);
}

::CORBA::StringSeq * RecorderImpl::ProjectNames (
    void
  )
  throw (
    ::CORBA::SystemException
  )
{
    CORBA::StringSeq_var names = new CORBA::StringSeq;

    std::vector<prodauto::ProjectName> project_names;
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
        if (db)
        {
            project_names = db->loadProjectNames();
        }
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("RecorderImpl::ProjectNames()\n")));
    CORBA::ULong i = 0;
    for (std::vector<prodauto::ProjectName>::const_iterator it = project_names.begin();
        it != project_names.end(); ++it)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("  %C\n"), it->name.c_str()));
        names->length(i + 1);
        names->operator[](i) = CORBA::string_dup(it->name.c_str());
        ++i;
    }

    return names._retn();
}

void RecorderImpl::AddProjectNames (
    const ::CORBA::StringSeq & project_names
  )
  throw (
    ::CORBA::SystemException
  )
{
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
        for (CORBA::ULong i = 0; i < project_names.length(); ++i)
        {
            db->loadOrCreateProjectName((const char *)project_names[i]);
        }
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }
}

void RecorderImpl::SetTapeNames (
    const ::CORBA::StringSeq & source_names,
    const ::CORBA::StringSeq & tape_names
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("RecorderImpl::SetTapeNames()\n")));

    // Update map of tape names with source package name as key
    std::map<std::string, std::string> tape_map;
    for (CORBA::ULong i = 0; i < source_names.length() && i < tape_names.length(); ++i)
    {
        mTapeMap[(const char *)source_names[i]] = (const char *)tape_names[i];
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Source \"%C\"  Tape \"%C\"\n"),
            (const char *)source_names[i],  (const char *)tape_names[i]));
    }

    // Set source package names
    SetSourcePackages();
}

/**
Private method to set source package names.
If we have a tape name, we use it.
Otherwise we use source name and date.
*/
void RecorderImpl::SetSourcePackages()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("RecorderImpl::SetSourcePackages()\n")));

    try
    {
        prodauto::Recorder * rec = mRecorder.get();

        prodauto::RecorderConfig * rc = 0;
        if (rec && rec->hasConfig())
        {
            rc = rec->getConfig();
        }

        if (rc)
        {
            // Go through inputs
            for (unsigned int i = 0; i < rc->recorderInputConfigs.size(); ++i)
            {
                prodauto::RecorderInputConfig * ric = rc->getInputConfig(i + 1);

                // Go through tracks
                for (unsigned int j = 0; ric && j < ric->trackConfigs.size(); ++j)
                {
                    prodauto::RecorderInputTrackConfig * ritc = 0;
                    if (ric)
                    {
                        ritc = ric->trackConfigs[j];
                    }

                    // SourceConfig for the track
                    prodauto::SourceConfig * sc = 0;
                    if (ritc)
                    {
                        sc = ritc->sourceConfig;
                    }

                    if (sc)
                    {
                        // Source package name is sc->name
                        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Input %d, track %d, source package name \"%C\"\n"),
                        i, j, sc->name.c_str()));

                        if (mTapeMap.find(sc->name) != mTapeMap.end())
                        {
                            // We have a corresponding tape name.
                            sc->setSourcePackage(mTapeMap[sc->name]);
                        }
                        else
                        {
                            // No corresponding tape name, use source name and date
                            sc->setSessionSourcePackage();
                        }
                    }
                } // tracks
            } // inputs
       }
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }
}



/**
Means of externally forcing a re-reading of config from database.
*/
void RecorderImpl::UpdateConfig (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    UpdateFromDatabase();
}

/**
Update tracks and settings from database.
*/
void RecorderImpl::UpdateFromDatabase()
{
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    prodauto::Recorder * rec = 0;
    if (db)
    {
        try
        {
            rec = db->loadRecorder(mName);
        }
        catch(const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }

    // Store the Recorder object
    mRecorder.reset(rec);

    // Clear the set of SourceConfigs
    // and the track map
    mSourceConfigs.clear();
    mTrackMap.clear();

    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
    }

    // Now we have RecorderConfig, we can set the source track names
    if (rc)
    {
        ACE_DEBUG((LM_INFO, ACE_TEXT("UpdateSources() loaded config \"%C\" of recorder \"%C\".\n"),
            rc->name.c_str(), mRecorder->name.c_str()));

        const unsigned int n_inputs = ACE_MIN((unsigned int)rc->recorderInputConfigs.size(), mMaxInputs);
        CORBA::ULong track_i = 0;
        unsigned int n_video_tracks = 0;
        for (unsigned int i = 0; i < n_inputs; ++i)
        {
            prodauto::RecorderInputConfig * ric = rc->getInputConfig(i + 1);
            // We force number of tracks per input to be the hardware max because
            // this makes it easier to map to hardware parameters when filling out
            // tracks status with "signal present" etc.
            //const unsigned int n_tracks = ACE_MIN(ric->trackConfigs.size(), mMaxTracksPerInput);
            const unsigned int n_tracks = mMaxTracksPerInput;
            for (unsigned int j = 0; j < n_tracks; ++j)
            {
                prodauto::RecorderInputTrackConfig * ritc = 0;
                if (ric && j < ric->trackConfigs.size())
                {
                    ritc = ric->trackConfigs[j];
                }
                prodauto::SourceConfig * sc = 0;
                if (ritc)
                {
                    sc = ritc->sourceConfig;
                }
                prodauto::SourceTrackConfig * stc = 0;
                if (sc)
                {
                    stc = sc->getTrackConfig(ritc->sourceTrackID);
                }
            
                // Update our set of SourceConfig.  Actually using
                // a map with database ID as the key, simply because
                // having a pointer as a key is not good practice.
                if (sc)
                {
                    long id = sc->getDatabaseID();
                    mSourceConfigs[id] = sc;
                }

                // Update map from source to hardware tracks
                if (stc)
                {
                    long id = stc->getDatabaseID();
                    HardwareTrack trk = {i, j};
                    mTrackMap[id] = trk;
                }

                mTracks->length(track_i + 1);
                ProdAuto::Track & track = mTracks[track_i];

                // Assuming first track of an input is video, others audio.
                std::ostringstream s;
                if (j == 0)
                {
                    ++n_video_tracks;
                    track.type = ProdAuto::VIDEO;
                    s << "V";
                }
                else
                {
                    track.type = ProdAuto::AUDIO;
                    s << "A" << j;
                }
                s << "  (input " << i << ")";
                track.name = CORBA::string_dup(s.str().c_str());
                track.id = j; // Is this ever used?

                if (sc && stc)
                {
                    track.has_source = 1;
                    track.src.package_name = CORBA::string_dup(sc->name.c_str());
                    track.src.track_name = CORBA::string_dup(stc->name.c_str());
                }
                else
                {
                    // No connection to this input
                    track.has_source = 0;
                    track.src.package_name = CORBA::string_dup("zz No Connection");
                    track.src.track_name = CORBA::string_dup("");
                }
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Input %d, track %d, src.track_name %C\n"),
                    i, j, (const char *) track.src.track_name));

                ++track_i;
            } // tracks
        } // inputs
        mVideoTrackCount = n_video_tracks;
    }
    else
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("UpdateSources() failed to load recorder config!\n")));
    }

    // Re-initialise tracks status, if necessary.
    if (mTracksStatus->length() != mTracks->length())
    {
        mTracksStatus->length(mTracks->length());

        for (unsigned int i = 0; i < mTracksStatus->length(); ++i)
        {
            ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
            ts.rec = 0;
            ts.signal_present = 0;
            ts.timecode.undefined = true;
            ts.timecode.edit_rate = EDIT_RATE;
        }
    }

    // Set source package names (using tape names if available)
    SetSourcePackages();
}


void RecorderImpl::Destroy()
{
  // Get the POA used when activating the object.
  PortableServer::POA_var poa =
    this->_default_POA ();

  // Get the object ID associated with this servant.
  PortableServer::ObjectId_var oid =
    poa->servant_to_id (this);

  // Now deactivate the object.
  poa->deactivate_object (oid.in ());

  // Decrease the reference count on ourselves.
  this->_remove_ref ();
}

