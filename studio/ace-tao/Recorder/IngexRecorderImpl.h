/*
 * $Id: IngexRecorderImpl.h,v 1.7 2009/09/18 16:11:55 john_f Exp $
 *
 * Servant class for Recorder.
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

#ifndef IngexRecorderImpl_h
#define IngexRecorderImpl_h

#include <vector>
#include <string>


#include "RecorderImpl.h"
#include "CopyManager.h"
#include "recorder_types.h" // for framecount_t

class IngexRecorder;


class  IngexRecorderImpl
  : public RecorderImpl
{
public:
  // Singelton accessor
	static IngexRecorderImpl * Instance() { return mInstance; }
  // Constructor 
  IngexRecorderImpl (void);
  
  // Destructor 
  virtual ~IngexRecorderImpl (void);

  // Set/get parameters
  void Name(const std::string & name) { mName = name; mCopyManager.RecorderName(mName); }
  std::string Name() const { return mName; }
  void FfmpegThreads(int n) { mFfmpegThreads = n; }

  // Initialisation
  bool Init();


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
  ::ProdAuto::Rational EditRate (
      
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
  ::ProdAuto::TrackList * Tracks (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::TrackStatusList * TracksStatus (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::Recorder::ReturnCode Start (
      ::ProdAuto::MxfTimecode & start_timecode,
      const ::ProdAuto::MxfDuration & pre_roll,
      const ::CORBA::BooleanSeq & rec_enable,
      const char * project,
      ::CORBA::Boolean test_only
   )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::Recorder::ReturnCode Stop (
      ::ProdAuto::MxfTimecode & stop_timecode,
      const ::ProdAuto::MxfDuration & post_roll,
      const char * description,
      const ::ProdAuto::LocatorSeq & locators,
      ::CORBA::StringSeq_out files
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

	void NotifyCompletion(IngexRecorder * rec);

public:
// methods
    void GetGlobals(int & ffmpeg_threads);

    int Fps() { return mFps; }
    bool Df() { return mDf; }
  
private:
// methods
	void DoStop(framecount_t timecode, framecount_t post_roll);
    void UpdateShmSourceNames();
    void StartCopying(unsigned int index);
    int TranslateLocatorColour(ProdAuto::LocatorColour::EnumType e);

// data
    ACE_Thread_Mutex mGlobalsMutex;
    int mFfmpegThreads;

	bool mRecording;
    unsigned int mRecordingIndex;
	IngexRecorder * mpIngexRecorder;
    CopyManager mCopyManager;
    std::string mHostname;
    prodauto::ProjectName mProjectName;

    //ProdAuto::Rational mEditRate;
    int mFps;
    bool mDf;

// static instance pointer
	static IngexRecorderImpl * mInstance;
};


#endif //#ifndef IngexRecorderImpl_h
