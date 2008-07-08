/*
 * $Id: VTRControl.h,v 1.1 2008/07/08 16:27:18 philipn Exp $
 *
 * Used to control a VTR and monitor it's state
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
 
#ifndef __RECORDER_VTR_CONTROL_H__
#define __RECORDER_VTR_CONTROL_H__


#include <vector>
#include <set>

#include "SonyProtocol.h"
#include "Threads.h"
#include "Types.h"


#define NUM_STATE_BYTES         4



namespace rec
{


typedef enum 
{
    NOT_CONNECTED_VTR_STATE = 0,    // no vtr connected
    REMOTE_LOCKOUT_VTR_STATE,       // vtr is not listening to remote calls
    TAPE_UNTHREADED_VTR_STATE,      // tape is unthreaded
    STOPPED_VTR_STATE,              // tape threaded but not in standby
    PAUSED_VTR_STATE,               // tape threaded in standby (paused)
    PLAY_VTR_STATE,                 // playing
    FAST_FORWARD_VTR_STATE,         // fast forward
    FAST_REWIND_VTR_STATE,          // fast rewind  
    EJECTING_VTR_STATE,             // tape is ejecting
    RECORDING_VTR_STATE,            // vtr is recording
    SEEKING_VTR_STATE,              // seeking
    JOG_VTR_STATE,                  // jog
    OTHER_VTR_STATE                 // TODO
} VTRState;

typedef enum
{
    UNKNOWN_DEVICE_TYPE = 0,            // don't know - have a look at the device code
    AJ_D350_NTSC_DEVICE_TYPE,           // TODO: D350 should be D360 for NTSC?
    AJ_D350_PAL_DEVICE_TYPE,
    DVW_A500P_DEVICE_TYPE,          
    DVW_500P_DEVICE_TYPE,          
    SONY_J3_COMPACT_PLAYER_DEVICE_TYPE
} DeviceType;


class VTRControl;

class VTRControlListener
{
public:
    virtual ~VTRControlListener() {}
    
    virtual void vtrDeviceType(VTRControl* vtrControl, int deviceTypeCode, DeviceType deviceType) {}
    virtual void vtrState(VTRControl* vtrControl, VTRState state, const unsigned char* stateBytes) {};
    virtual void vtrPlaybackError(VTRControl* vtrControl, int errorCode, Timecode ltc) {};
    virtual void vtrVITC(VTRControl* vtrControl, Timecode vitc) {};
    virtual void vtrLTC(VTRControl* vtrControl, Timecode ltc) {};
};


class VTRControl
{
public:
    virtual ~VTRControl() {}
    
    // listeners
    virtual void registerListener(VTRControlListener* listener) = 0;
    virtual void unregisterListener(VTRControlListener* listener) = 0;

    virtual void pollExtState(bool poll) = 0;
    virtual void pollVitc(bool poll) = 0;
    virtual void pollLtc(bool poll) = 0;

    // device info
    virtual int getDeviceCode() = 0;
    virtual DeviceType getDeviceType() = 0;
    virtual bool isD3VTR() = 0;
    virtual bool isNonD3VTR() = 0;
    
    // control functions
    virtual bool play() = 0;
    virtual bool stop() = 0;
    virtual bool record() = 0;
    virtual bool fastForward() = 0;
    virtual bool fastRewind() = 0;
    virtual bool standbyOn() = 0;
    virtual bool standbyOff() = 0;
    virtual bool seekToTimecode(Timecode timecode) = 0;
    virtual bool eject() = 0;

    // state functions
    virtual VTRState getState() = 0;
    virtual void getDetailedState(VTRState* state, unsigned char* stateBytes) = 0;


    // utilities
    static std::string getStateString(VTRState state);
    static std::string errorCodeToString(int errorCode);
    static std::string getDeviceTypeString(DeviceType deviceType);
    static int deviceTypeToCode(DeviceType type);
    static std::string stateBytesToString(const unsigned char* stateBytes);
};



class VTRStatePoller;

class DefaultVTRControl : public VTRControl
{
public:
    friend class VTRStatePoller;
    
public:
    DefaultVTRControl(std::string deviceName);
    ~DefaultVTRControl();

    // from VTRControl
    virtual void registerListener(VTRControlListener* listener);
    virtual void unregisterListener(VTRControlListener* listener);
    virtual void pollExtState(bool poll);
    virtual void pollVitc(bool poll);
    virtual void pollLtc(bool poll);
    virtual int getDeviceCode();
    virtual DeviceType getDeviceType();
    virtual bool isD3VTR();
    virtual bool isNonD3VTR();
    virtual bool play();
    virtual bool stop();
    virtual bool record();
    virtual bool fastForward();
    virtual bool fastRewind();
    virtual bool standbyOn();
    virtual bool standbyOff();
    virtual bool seekToTimecode(Timecode timecode);
    virtual bool eject();
    virtual VTRState getState();
    virtual void getDetailedState(VTRState* state, unsigned char* stateBytes);

    
private:
    bool openPort(std::string deviceName);
    bool reopenPort();
    bool portIsOpen();
    std::string getSerialPortName();

    void updateDeviceType(int deviceTypeCode, DeviceType deviceType);
    void updateState(VTRState state, const unsigned char* stateBytes);
    void updatePlaybackError(int errorCode, Timecode ltc);
    void updateVitc(Timecode vitc);
    void updateLtc(Timecode ltc);
    
    bool readDeviceType(int* deviceTypeCode);
    bool readState(VTRState* state, unsigned char* stateBytes);
    bool readExtState(VTRState* state, int* errorCode);
    bool readTimecodes(Timecode* vitc, Timecode* ltc);
    bool readVitc(Timecode* vitc);
    bool readLtc(Timecode* ltc);

    void getStateFromData(const unsigned char* data, VTRState* state);
    void getExtStateFromData(const unsigned char* data, int* errorCode);
    void getTimecodeFromData(const unsigned char* data, Timecode* timecode);


    bool _debugSerialPortOpen;
    
    Thread* _stateThread;

    std::string _serialPortDeviceName;
    std::string _actualSerialPortDeviceName;
    SonyProtocol* _protocol;
    Mutex _serialMutex;
    
    Mutex _listenerMutex;
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
    } ListenerInfo;
    std::vector<ListenerInfo> _listeners;
    
    Mutex _stateMutex;
    VTRState _state;
    unsigned char _stateBytes[NUM_STATE_BYTES];
    
    Mutex _deviceTypeMutex;
    int _deviceTypeCode;
    DeviceType _deviceType;

private:
    static void addSerialPort(std::string deviceName);
    static void removeSerialPort(std::string deviceName);
    static bool serialPortIsAvailable(std::string deviceName);
    
    static std::set<std::string> _serialPortInUse;
    static Mutex _serialPortInUseMutex;
};






};




#endif


