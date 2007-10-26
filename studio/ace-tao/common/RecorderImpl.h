/*
 * $Id: RecorderImpl.h,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
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

#include "RecorderS.h"

#include "DataSourceImpl.h"

const ProdAuto::Rational EDIT_RATE = { 25, 1 };

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
  void UpdateConfig (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
protected:
	std::string mName;
	ProdAuto::MxfDuration mMaxPreRoll;
	ProdAuto::MxfDuration mMaxPostRoll;
    unsigned int mMaxInputs;
	unsigned int mMaxTracksPerInput;
    unsigned int mVideoTrackCount;
	ProdAuto::TrackList_var mTracks;
	ProdAuto::TrackStatusList_var mTracksStatus;

private:
// methods
	void UpdateSources();
};


#endif //#ifndef RecorderImpl_h