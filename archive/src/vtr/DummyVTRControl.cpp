/*
 * $Id: DummyVTRControl.cpp,v 1.1 2008/07/08 16:26:56 philipn Exp $
 *
 * Implements a dummy VTR
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#include <unistd.h>

#include "DummyVTRControl.h"
#include "Logging.h"
#include "Utilities.h"
#include "RecorderException.h"
#include "Timing.h"


using namespace std;
using namespace rec;



typedef struct
{
    VTRState state;
    unsigned char stateBytes[4];
} DummyStateInfo;

static const DummyStateInfo g_dummyStateInfo[] = 
{
    // state                         b0    b1    b2    b3
    {NOT_CONNECTED_VTR_STATE,       {0x00, 0x00, 0x00, 0x00}},
    {REMOTE_LOCKOUT_VTR_STATE,      {0x01, 0x00, 0x00, 0x00}},
    {TAPE_UNTHREADED_VTR_STATE,     {0x20, 0x00, 0x00, 0x00}},
    {STOPPED_VTR_STATE,             {0x00, 0x20, 0x00, 0x00}},
    {PAUSED_VTR_STATE,              {0x00, 0xA0, 0x00, 0x00}},
    {PLAY_VTR_STATE,                {0x00, 0x01, 0x00, 0x00}},
    {FAST_FORWARD_VTR_STATE,        {0x00, 0x04, 0x00, 0x00}},
    {FAST_REWIND_VTR_STATE,         {0x00, 0x08, 0x00, 0x00}},
    {EJECTING_VTR_STATE,            {0x00, 0x10, 0x00, 0x00}},
    {RECORDING_VTR_STATE,           {0x00, 0x02, 0x00, 0x00}},
    {SEEKING_VTR_STATE,             {0x00, 0x00, 0x20, 0x00}},
    {JOG_VTR_STATE,                 {0x00, 0x00, 0x10, 0x00}},
    {OTHER_VTR_STATE,               {0x00, 0x00, 0x00, 0x00}}
};


namespace rec
{

class DummyVTRStatePoller : public ThreadWorker
{
public:
    DummyVTRStatePoller(DummyVTRControl* vtrControl)
    : _vtrControl(vtrControl), _hasStopped(true), _stop(false)
    {}
    virtual ~DummyVTRStatePoller()
    {}

    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped); 

        VTRState state;
        unsigned char stateBytes[NUM_STATE_BYTES];
        
        sleep_sec(1);
        
        state = STOPPED_VTR_STATE;
        memcpy(stateBytes, g_dummyStateInfo[state].stateBytes, NUM_STATE_BYTES);
        _vtrControl->updateState(state, stateBytes);
        
        
        while (!_stop)
        {
            if (_vtrControl->_nextState != -1)
            {
                state = (VTRState)_vtrControl->_nextState;
                memcpy(stateBytes, g_dummyStateInfo[state].stateBytes, NUM_STATE_BYTES);
                
                _vtrControl->_nextState = -1;
                
                _vtrControl->updateState(state, stateBytes);
            }

            // sleep to give other threads a chance to operate the vtr
            sleep_msec(20);
        }

        _hasStopped = true;
    }
    
    virtual void stop()
    {
        _stop = true;
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }

    
    
private:
    DummyVTRControl* _vtrControl;
    bool _hasStopped;
    bool _stop;
};

}; // namespace rec









DummyVTRControl::DummyVTRControl(DeviceType type)
: _state(NOT_CONNECTED_VTR_STATE), _nextState(-1), _deviceTypeCode(0), _deviceType(type)
{
    memset(_stateBytes, 0, NUM_STATE_BYTES);
    
    _deviceTypeCode = VTRControl::deviceTypeToCode(type);
    
    _stateThread = new Thread(new DummyVTRStatePoller(this), true);
    _stateThread->start();
}

DummyVTRControl::~DummyVTRControl()
{
    delete _stateThread;
}

void DummyVTRControl::registerListener(VTRControlListener* listener)
{
    LOCK_SECTION(_listenerMutex);

    VTRControlListenerInfo info;
    info.listener = listener;
    info.state = NOT_CONNECTED_VTR_STATE;
    info.errorCode = 0x00;
    info.ltcAtError.hour = 99;
    info.vitc.hour = 99;
    info.ltc.hour = 99;
    
    // send device type straight away
    {
        LOCK_SECTION(_deviceTypeMutex);
        info.deviceTypeCode = _deviceTypeCode;
        info.deviceType = _deviceType;
    }
    listener->vtrDeviceType(this, info.deviceTypeCode, info.deviceType);

    _listeners.push_back(info);
}

void DummyVTRControl::unregisterListener(VTRControlListener* listener)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<VTRControlListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).listener == listener)
        {
            _listeners.erase(iter);
            return;
        }
    }
    
    Logging::debug("Attempt to unregister an unknown VTR control listener\n");
}

void DummyVTRControl::pollExtState(bool poll)
{
}

void DummyVTRControl::pollVitc(bool poll)
{
}

void DummyVTRControl::pollLtc(bool poll)
{
}

int DummyVTRControl::getDeviceCode()
{
    return _deviceTypeCode;
}

DeviceType DummyVTRControl::getDeviceType()
{
    return _deviceType;
}

bool DummyVTRControl::isD3VTR()
{
    return _deviceType == AJ_D350_NTSC_DEVICE_TYPE || _deviceType == AJ_D350_PAL_DEVICE_TYPE; 
}

bool DummyVTRControl::isNonD3VTR()
{
    return !isD3VTR() && _deviceType != 0x00;
}

bool DummyVTRControl::play()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    if (_state != NOT_CONNECTED_VTR_STATE &&
        _state != REMOTE_LOCKOUT_VTR_STATE &&
        _state != TAPE_UNTHREADED_VTR_STATE)
    {
        _nextState = PLAY_VTR_STATE;
    }
    
    return true;
}

bool DummyVTRControl::stop()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    if (_state != NOT_CONNECTED_VTR_STATE &&
        _state != REMOTE_LOCKOUT_VTR_STATE &&
        _state != TAPE_UNTHREADED_VTR_STATE &&
        _state != STOPPED_VTR_STATE)
    {
        _nextState = PAUSED_VTR_STATE;
    }
    
    return true;
}

bool DummyVTRControl::record()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    _nextState = RECORDING_VTR_STATE;
    
    return true;
}

bool DummyVTRControl::fastForward()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    _nextState = FAST_FORWARD_VTR_STATE;
    
    return true;
}

bool DummyVTRControl::fastRewind()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    _nextState = FAST_REWIND_VTR_STATE;
    
    return true;
}

bool DummyVTRControl::standbyOn()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    if (_state == STOPPED_VTR_STATE)
    {
        _nextState = PAUSED_VTR_STATE;
    }
    
    return true;
}

bool DummyVTRControl::standbyOff()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    if (_state == PAUSED_VTR_STATE)
    {
        _nextState = STOPPED_VTR_STATE;
    }
    
    return true;
}

bool DummyVTRControl::seekToTimecode(Timecode timecode)
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    _nextState = SEEKING_VTR_STATE;
    
    return true;
}

bool DummyVTRControl::eject()
{
    LOCK_SECTION(_stateMutex);
    
    // TODO: complete logic for state change
    _nextState = TAPE_UNTHREADED_VTR_STATE;
    
    return true;
}

VTRState DummyVTRControl::getState()
{
    LOCK_SECTION(_stateMutex);
    
    return _state;
}

void DummyVTRControl::getDetailedState(VTRState* state, unsigned char* stateBytes)
{
    LOCK_SECTION(_stateMutex);
    
    *state = _state;
    memcpy(stateBytes, _stateBytes, NUM_STATE_BYTES);
}

void DummyVTRControl::updateState(VTRState state, unsigned char* stateBytes)
{
    // update the state returned by getState()
    {
        LOCK_SECTION(_stateMutex);
        _state = state;
        memcpy(_stateBytes, stateBytes, NUM_STATE_BYTES);
    }
    
    {
        LOCK_SECTION(_listenerMutex);
        
        vector<VTRControlListenerInfo>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if ((*iter).state != state)
            {
                (*iter).listener->vtrState(this, state, stateBytes);
                (*iter).state = state;
            }
        }
    }    
}

void DummyVTRControl::updatePlaybackError(int errorCode, Timecode ltc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<VTRControlListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).errorCode != errorCode ||
            (*iter).ltcAtError != ltc)
        {
            (*iter).listener->vtrPlaybackError(this, errorCode, ltc);
            (*iter).errorCode = errorCode;
            (*iter).ltcAtError = ltc;
        }
    }
}

void DummyVTRControl::updateVitc(Timecode vitc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<VTRControlListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).vitc != vitc)
        {
            (*iter).listener->vtrVITC(this, vitc);
            (*iter).vitc = vitc;
        }
    }
}

void DummyVTRControl::updateLtc(Timecode ltc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<VTRControlListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).ltc != ltc)
        {
            (*iter).listener->vtrLTC(this, ltc);
            (*iter).ltc = ltc;
        }
    }
}


