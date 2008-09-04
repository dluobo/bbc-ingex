/*
 * $Id: RecorderImpl.h,v 1.5 2008/09/04 15:33:19 john_f Exp $
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
  bool Init(std::string name, std::string db_user, std::string db_pw,
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
  
protected:
// data
    ProdAuto::MxfDuration mMaxPreRoll;
    ProdAuto::MxfDuration mMaxPostRoll;
    unsigned int mMaxInputs;
    unsigned int mMaxTracksPerInput;
    unsigned int mVideoTrackCount;
    ProdAuto::TrackList_var mTracks;
    ProdAuto::TrackStatusList_var mTracksStatus;
    std::string mName;
    std::string mFormat;
    ProdAuto::Rational mEditRate;
    int mFps;
    bool mDf;
    std::auto_ptr<prodauto::Recorder> mRecorder;
    std::map<long, prodauto::SourceConfig *> mSourceConfigs;
    std::map<long, HardwareTrack> mTrackMap;
    std::map<std::string, std::string> mTapeMap;
// methods
    void UpdateFromDatabase();

private:
// methods
    void SetSourcePackages();
};


#endif //#ifndef RecorderImpl_h
