/*
 * $Id: DummyVTRControl.h,v 1.1 2008/07/08 16:27:12 philipn Exp $
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
 
#ifndef __RECORDER_DUMMY_VTR_CONTROL_H__
#define __RECORDER_DUMMY_VTR_CONTROL_H__


#include <string>
#include <vector>

#include "VTRControl.h"


namespace rec
{

    
class DummyVTRStatePoller;


class DummyVTRControl : public VTRControl
{
public:
    friend class DummyVTRStatePoller;
    
public:
    DummyVTRControl(DeviceType type);
    virtual ~DummyVTRControl();
    
    // listeners
    virtual void registerListener(VTRControlListener* listener);
    virtual void unregisterListener(VTRControlListener* listener);

    virtual void pollExtState(bool poll);
    virtual void pollVitc(bool poll);
    virtual void pollLtc(bool poll);

    // device info
    virtual int getDeviceCode();
    virtual DeviceType getDeviceType();
    virtual bool isD3VTR();
    virtual bool isNonD3VTR();
    
    // control functions
    virtual bool play();
    virtual bool stop();
    virtual bool record();
    virtual bool fastForward();
    virtual bool fastRewind();
    virtual bool standbyOn();
    virtual bool standbyOff();
    virtual bool seekToTimecode(Timecode timecode);
    virtual bool eject();

    // state functions
    virtual VTRState getState();
    virtual void getDetailedState(VTRState* state, unsigned char* stateBytes);
    
    
private:
    void updateDeviceType(int deviceTypeCode, DeviceType deviceType);
    void updateState(VTRState state, unsigned char* stateBytes);
    void updatePlaybackError(int errorCode, Timecode ltc);
    void updateVitc(Timecode vitc);
    void updateLtc(Timecode ltc);

    Thread* _stateThread;

    typedef struct
    {
        VTRControlListener* listener;
        int deviceTypeCode;
        DeviceType deviceType;
        VTRState state;
        int errorCode;
        Timecode ltcAtError;
        Timecode vitc;
        Timecode ltc;
    } VTRControlListenerInfo;
    
    std::vector<VTRControlListenerInfo> _listeners;
    Mutex _listenerMutex;
    
    VTRState _state;
    unsigned char _stateBytes[NUM_STATE_BYTES];
    Mutex _stateMutex;
    int _nextState;
    
    int _deviceTypeCode;
    DeviceType _deviceType;
    Mutex _deviceTypeMutex;
};
    
}


#endif

