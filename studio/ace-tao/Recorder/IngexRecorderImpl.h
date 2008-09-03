/*
 * $Id: IngexRecorderImpl.h,v 1.3 2008/09/03 14:09:05 john_f Exp $
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
#include "IngexRecorder.h"
#include "CopyManager.h"
#include "recorder_types.h" // for framecount_t


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


  // Initialisation
  bool Init(std::string name, std::string db_user, std::string db_pw);

  // Identity
  const char * Name() const { return mName.c_str(); }

// IDL operations
  virtual
  char * RecordingFormat (
      
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
  
	void NotifyCompletion(IngexRecorder * rec);

private:
// methods
	void DoStop(framecount_t timecode, framecount_t post_roll);
    void UpdateShmSourceNames();
    void StartCopying(unsigned int index);
    int TranslateLocatorColour(ProdAuto::LocatorColour::EnumType e);
// data
	bool mRecording;
    unsigned int mRecordingIndex;
	IngexRecorder * mpIngexRecorder;
    CopyManager mCopyManager;
    std::string mHostname;

// static instance pointer
	static IngexRecorderImpl * mInstance;
};


#endif //#ifndef IngexRecorderImpl_h
