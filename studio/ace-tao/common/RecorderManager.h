/*
 * $Id: RecorderManager.h,v 1.2 2008/09/03 13:43:34 john_f Exp $
 *
 * Wrapper for ProdAuto::Recorder.
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

#ifndef RecorderManager_h
#define RecorderManager_h

#include "RecorderC.h"
#include "StatusClientImpl.h"

#include <string>
#include <vector>

class TrackInfo
{
public:
    TrackInfo();
    ProdAuto::TrackType type;
    bool rec;
};

class RecorderManager
  : public virtual StatusObserver
{
public:
// Constructor & Destructor
    RecorderManager();
    ~RecorderManager();

// Status observer
    void Observe(const std::string & name, const std::string & value);

// Recorder management
    void Recorder(CORBA::Object_ptr obj);
    void Init();
    void Update();
    void AddStatusClient();
    void RemoveStatusClient();
    void Start(const std::string & project = "");
    void Stop();
    const std::vector<TrackInfo> & TrackInfos() { return mTrackInfos; }
    bool GetStatusMessage(std::string & message);
private:
    ProdAuto::Recorder_var mRecorder;
    unsigned int mTrackCount;
    std::vector<TrackInfo> mTrackInfos;

// StatusClient servant
    StatusClientImpl * mStatusClient;
    ProdAuto::StatusClient_var mClientRef;

    std::queue<std::string> mStatusMessages;
    ACE_Thread_Mutex mMessagesMutex;
};

#endif //#ifndef RecorderManager_h
