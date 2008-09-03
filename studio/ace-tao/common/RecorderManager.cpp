/*
 * $Id: RecorderManager.cpp,v 1.3 2008/09/03 13:43:34 john_f Exp $
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

#include "RecorderManager.h"

#include <sstream>

const ProdAuto::Rational EDIT_RATE = { 25, 1 }; // We assume 25 frames per second
const unsigned int MAX_STATUS_MESSAGES = 100;

/** Constructor for the TrackInfo struct */
TrackInfo::TrackInfo()
: type(ProdAuto::AUDIO), rec(false)
{
}

/** Receive status message */
void RecorderManager::Observe(const std::string & name, const std::string & value)
{
    // Just in case this gets called by another thread, we use
    // a guard for mStatusMessage
    ACE_Guard<ACE_Thread_Mutex> guard(mMessagesMutex);
    std::ostringstream ss;
    ss << name << ": " << value;
    mStatusMessages.push(ss.str());

    while (mStatusMessages.size() > MAX_STATUS_MESSAGES)
    {
        mStatusMessages.pop();
    }
}

bool RecorderManager::GetStatusMessage(std::string & message)
{
    ACE_Guard<ACE_Thread_Mutex> guard(mMessagesMutex);
    bool result = false;
    if (! mStatusMessages.empty())
    {
        message = mStatusMessages.front();
        mStatusMessages.pop();
        result = true;
    }
    return result;
}

// Recorder management

RecorderManager::RecorderManager()
: mStatusClient(0)
{
    // Make an instance of the servant class on the heap.
    mStatusClient = new StatusClientImpl(this);
    if (mStatusClient == 0)
    {
        ACE_DEBUG(( LM_ERROR, "Failed to create status client.n" ));
    }


    // Create a CORBA object and register the servant as the implementation.
    // NB. You must have done your ORB_init before this happens
    try
    {
        mClientRef = mStatusClient->_this();
    }
    catch (const CORBA::Exception & e)
    {
        ACE_DEBUG(( LM_ERROR, "Exception creating workstation object %C\n", e._name() ));
    }
    catch (...)
    {
        ACE_DEBUG(( LM_ERROR, "Exception creating workstation CORBA object\n" ));
    }
}

RecorderManager::~RecorderManager()
{
// StatusClient servant and CORBA object
    // Deactivate the CORBA object and
    // relinquish our reference to the servant.
    // The POA will delete it at an appropriate time.
    mStatusClient->Destroy();
}

void RecorderManager::Recorder(CORBA::Object_ptr obj)
{
    if (!CORBA::is_nil(obj))
    {
        try
        {
            mRecorder = ProdAuto::Recorder::_narrow(obj);
        }
        catch (const CORBA::Exception &)
        {
            mRecorder = ProdAuto::Recorder::_nil();
        }
    }
    else
    {
        mRecorder = ProdAuto::Recorder::_nil();
    }
}

void RecorderManager::Init()
{
    this->Update();
}

void RecorderManager::Update()
{
    // Get tracks and status
    if (!CORBA::is_nil(mRecorder))
    {
        try
        {
            ProdAuto::TrackList_var track_list = mRecorder->Tracks();
            ProdAuto::TrackStatusList_var track_status_list = mRecorder->TracksStatus();
            mTrackCount = track_list->length();
            mTrackInfos.clear();
            for (CORBA::ULong i = 0; i < track_list->length(); ++i)
            {
                TrackInfo track_info;
                track_info.type = track_list->operator[](i).type;
                track_info.rec = track_status_list->operator[](i).rec;
                mTrackInfos.push_back(track_info);
            }
        }
        catch (const CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Tracks() gave exception: %C\n"), e._name()));
            //mRecorder = ProdAuto::Recorder::_nil();
            mTrackCount = 0;
        }
    }
}

void RecorderManager::AddStatusClient()
{
    if (!CORBA::is_nil(mRecorder))
    {
        try
        {
            mRecorder->AddStatusClient(mClientRef.in());
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("StatusClient attached.\n")));
        }
        catch (const CORBA::Exception &)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to attach StatusClient.\n")));
        }
    }
}

void RecorderManager::RemoveStatusClient()
{
    if (!CORBA::is_nil(mRecorder))
    {
        try
        {
            mRecorder->RemoveStatusClient(mClientRef.in());
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Successfully detached.\n")));
        }
        catch (const CORBA::Exception &e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to detach: %C\n"), e._name()));
        }
    }
}

void RecorderManager::Start(const std::string & project)
{
    if (!CORBA::is_nil(mRecorder.in()))
    {
        ProdAuto::MxfTimecode start_tc;
        start_tc.edit_rate = EDIT_RATE;
        start_tc.samples = 0;
        start_tc.undefined = true;  // Meaning "now"

        ProdAuto::MxfDuration pre_roll;
        pre_roll.edit_rate = EDIT_RATE;
        pre_roll.samples = 5;
        pre_roll.undefined = false;

        //::ProdAuto::BooleanList rec_enable;
        ::CORBA::BooleanSeq rec_enable;
        rec_enable.length(mTrackCount);
        for (unsigned int i = 0; i < mTrackCount; ++i)
        {
            rec_enable[i] = 1;
        }

        CORBA::ULong card_count = mTrackCount / 5; // dodgy
        CORBA::StringSeq tapes;
        tapes.length(card_count);
        for (CORBA::ULong i = 0; i < card_count; ++i)
        {
            std::ostringstream ss;
            ss << "Tape0" << i;
            //std::cerr << ss.str() << std::endl;
            //const char * s = ss.str().c_str();
            tapes[i] = CORBA::string_dup(ss.str().c_str());
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Tape: %C\n"), (const char *)tapes[i]));
        }

        try
        {
            mRecorder->Start(start_tc, pre_roll, rec_enable, project.c_str(), false);
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Start command sent.\n")));
        }
        catch (const CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Start() gave exception: %C\n"), e._name()));
            //mRecorder = ProdAuto::Recorder::_nil();
        }
    }
}

void RecorderManager::Stop()
{
    if (! CORBA::is_nil(mRecorder))
    {
        ProdAuto::MxfTimecode stop_tc;
        stop_tc.edit_rate = EDIT_RATE;
        stop_tc.samples = 0;
        stop_tc.undefined = true;  // Meaning "now"

        ProdAuto::MxfDuration post_roll;
        post_roll.edit_rate = EDIT_RATE;
        post_roll.samples = 5;
        post_roll.undefined = false;

        CORBA::StringSeq_var files;
        ::ProdAuto::LocatorSeq locators;

        try
        {
            mRecorder->Stop(stop_tc, post_roll, "", locators, files.out());
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Stop command sent.\n")));
        }
        catch(const CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Stop() gave exception: %C\n"), e._name()));
            //mRecorder = ProdAuto::Recorder::_nil();
        }
    }
}

