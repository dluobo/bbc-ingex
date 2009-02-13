/*
 * $Id: RecorderImpl.h,v 1.11 2009/02/13 10:20:23 john_f Exp $
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

#ifndef RecorderImpl_h
#define RecorderImpl_h

#include <vector>
#include <string>

#include "Database.h"  // For Recorder
#include "RecorderS.h"
#include "DataSourceImpl.h"


struct HardwareTrack
{
    unsigned int channel;
    unsigned int track;
};

class  RecorderImpl
  : public DataSourceImpl, public virtual POA_ProdAuto::Recorder
{
public:
  // Constructor 
  RecorderImpl (void);
  
  // Destructor 
  virtual ~RecorderImpl (void);

  // Servant destroy
  void Destroy();

  // Initialisation
  bool Init(const std::string & name,
      unsigned int max_inputs, unsigned int max_tracks_per_input);

  // Identity
  const char * Name() const { return mName.c_str(); }

// IDL operations
  virtual
  ::ProdAuto::MxfDuration MaxPreRoll (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::MxfDuration MaxPostRoll (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  char * RecordingFormat (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::Rational EditRate (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::TrackList * Tracks (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::CORBA::StringSeq * Configs (
      void
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  char * CurrentConfig (
      void
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::CORBA::Boolean SelectConfig (
      const char * config
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::CORBA::StringSeq * ProjectNames (
      void
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  void AddProjectNames (
      const ::CORBA::StringSeq & project_names
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  void SetTapeNames (
      const ::CORBA::StringSeq & source_names,
      const ::CORBA::StringSeq & tape_names
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  void UpdateConfig (
      
    )
    throw (
      ::CORBA::SystemException
    );

  prodauto::Recorder * Recorder() { return mRecorder.get(); }

    int Fps() { return mFps; }
    bool Df() { return mDf; }
  
public:
// thread safe accessors for maps
    HardwareTrack TrackHwMap(long id);
    unsigned int TrackIndexMap(long id);
    std::string RecordingLocationMap(long id);

private:
// mutexes to protect maps
    ACE_Thread_Mutex mTrackMapMutex;
    ACE_Thread_Mutex mTrackIndexMapMutex;
    ACE_Thread_Mutex mRecordingLocationMapMutex;

public:
// data which needs to be visible to clases such as IngexRecorder
    ProdAuto::TrackList_var mTracks;

    // maps with SourceTrackConfig database id as key
    std::map<long, HardwareTrack> mTrackMap;
    std::map<long, unsigned int> mTrackIndexMap;

    // map with RecordingLocation database id as key
    std::map<long, std::string> mRecordingLocationMap;

protected:
// data
    ProdAuto::MxfDuration mMaxPreRoll;
    ProdAuto::MxfDuration mMaxPostRoll;
    unsigned int mMaxInputs;
    unsigned int mMaxTracksPerInput;
    unsigned int mVideoTrackCount;
    ProdAuto::TrackStatusList_var mTracksStatus;
    std::string mName;
    std::string mFormat;
    ProdAuto::Rational mEditRate;
    int mFps;
    bool mDf;
    std::auto_ptr<prodauto::Recorder> mRecorder;

    // map with SourceConfig database id as key
    std::map<long, prodauto::SourceConfig *> mSourceConfigs;

    std::map<std::string, std::string> mTapeMap;
// methods
    bool UpdateFromDatabase();

private:
// methods
    bool SetSourcePackages();

// friend function
    friend ACE_THR_FUNC_RETURN start_record_thread(void *p_arg);
};


#endif //#ifndef RecorderImpl_h
