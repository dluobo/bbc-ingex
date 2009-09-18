/*
 * $Id: SimplerouterloggerImpl.h,v 1.11 2009/09/18 16:22:17 john_f Exp $
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

#ifndef SimplerouterloggerImpl_h
#define SimplerouterloggerImpl_h

#include <vector>
#include <string>
#include <orbsvcs/CosNamingC.h>

#include "RecorderS.h"

#include "RecorderImpl.h"
#include "TimecodeReader.h"
#include "RouterObserver.h"
#include "RouterDestination.h"
#include "Timecode.h"
#include "CopyManager.h"

const ProdAuto::Rational EDIT_RATE = { 25 , 1 }; // Assume 25 fps

class Vt
{
public:
    Vt(unsigned int rd, const std::string & n);
    unsigned int router_dest;
    unsigned int router_src; // what is routed to that VT
    std::string name;
    uint32_t selector_index; // MCSelector index
};

class CutsDatabase;

class  SimplerouterloggerImpl
  : public RouterObserver, public RecorderImpl
{
public:
  // Singelton accessor
    //static SimplerouterloggerImpl * Instance() { return mInstance; }
  // Constructor 
  SimplerouterloggerImpl (void);
  
  // Destructor 
  virtual ~SimplerouterloggerImpl (void);


  // Initialisation
  bool Init(const std::string & name, const std::string & mc_clip_name, const std::string & db_file,
            unsigned int mix_dest, const std::vector<RouterDestination*> & dests);

  void Destination(unsigned int dest) { mMixDestination = dest; }


  void SetTimecodePort(std::string tcp);

  void SetRouterPort(std::string rp);

  
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
  void UpdateConfig (
      
    )
    throw (
      ::CORBA::SystemException
    );


    // From Observer base class
    void Observe(unsigned int src, unsigned int dest);


private:
    // Methods
    void StartSaving(const Timecode & tc, const std::string & filename);
    void StopSaving();
    void StartCopying(unsigned int index);
    void StopCopying(unsigned int index);

    // Data
    ProdAuto::MxfDuration mMaxPreRoll;
    ProdAuto::MxfDuration mMaxPostRoll;

    FILE * mpFile;
    std::string mRouterPort;
    std::string mTimecodePort;

    CutsDatabase * mpCutsDatabase;
    CopyManager mCopyManager;

    unsigned int mRecIndex;

    unsigned int mMixDestination;

    std::auto_ptr<prodauto::MCClipDef> mMcClipDef; // multi-cam clip we are following
    long mMcTrackId;         // multi-cam track id we are following

    std::vector<Vt> mVts;   // eventually from database

    // Details of last relevant cut
    std::string mLastSrc;
    int32_t mLastSelectorIndex;
    std::string mLastTc;
    int mLastYear;
    int mLastMonth;
    int mLastDay;

    bool mSaving; // whether recording in progress
};


#endif //#ifndef SimplerouterloggerImpl_h
