/*
 * $Id: VTRControl.cpp,v 1.1 2008/07/08 16:27:11 philipn Exp $
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
 
#include "VTRControl.h"
#include "RecorderException.h"
#include "Logging.h"
#include "Timing.h"
#include "Config.h"
#include "Utilities.h"


using namespace std;
using namespace rec;


// time between retrying to open the serial port
#define REOPEN_PORT_RETRY_SEC           2

// number of usecs before a failure to poll the VTR state is determined to be a connection failure
#define CONNECTION_FAIL_PERIOD          (1 * 1000 * 1000)

// number of usecs before a failure to poll the VTR state results in the port being reopened
// NOTE: REOPEN_PORT_FAIL_PERIOD must be > CONNECTION_FAIL_PERIOD
#define REOPEN_PORT_FAIL_PERIOD         (5 * 1000 * 1000)

// range of ports checked when the deviceName has the %d suffix; ttyUSB0..ttyUSB3, or ttyS0..ttyS3
#define MAX_PORT_DEVICE_NUMBER          4



typedef struct
{
    DeviceType type;
    int code;
    const char* name;
} VTRDeviceInfo;

// NOTE: this table is indexed using DeviceType and needs to be kept in sync
static const VTRDeviceInfo g_vtrDeviceInfo[] = 
{
    {UNKNOWN_DEVICE_TYPE,                   0x0000, "Unknown"},
    {AJ_D350_NTSC_DEVICE_TYPE,              0xF019, "AJ-D350 NTSC"},
    {AJ_D350_PAL_DEVICE_TYPE,               0xF119, "AJ-D350 PAL"},
    {DVW_A500P_DEVICE_TYPE,                 0xB100, "DVW A500 PAL"},
    {DVW_500P_DEVICE_TYPE,                  0xB110, "DVW 500 PAL"},
    {SONY_J3_COMPACT_PLAYER_DEVICE_TYPE,    0xB170, "Sony J-3 Compact Player"}
};


// Note: Tape Trouble and Hard Error could be specific to the D3 VTR
static const char* g_vtrStateBitStrings[NUM_STATE_BYTES][8] = 
{
    {"X", "X", "Tape Unthread", "Servo Ref Missing", "?Tape Trouble", "?Hard Error", "X", "Local"},
    {"Standby", "X", "Stop", "Eject", "Rewind", "Fast Fwd", "Record", "Play"},
    {"Servo Lock", "TSO Mode", "Shuttle", "Jog", "Var", "Tape Dir", "Still", "Cue Up"},
    {"Auto Mode", "Freeze On", "X", "CF Mode", "A Out", "A In", "Out", "In"}
};



string VTRControl::getStateString(VTRState state)
{
    switch (state)
    {
        case NOT_CONNECTED_VTR_STATE:
            return "Not connected state";
        case REMOTE_LOCKOUT_VTR_STATE:
            return "Remote lockout state";
        case TAPE_UNTHREADED_VTR_STATE:
            return "Tape unthreaded state";
        case STOPPED_VTR_STATE:
            return "Stopped state";
        case PAUSED_VTR_STATE:
            return "Paused state";
        case PLAY_VTR_STATE:
            return "play state";
        case FAST_FORWARD_VTR_STATE:
            return "Fast forward state";
        case FAST_REWIND_VTR_STATE:
            return "Fast rewind state";
        case EJECTING_VTR_STATE:
            return "Ejecting state";
        case RECORDING_VTR_STATE:
            return "Recording state";
        case SEEKING_VTR_STATE:
            return "Seeking state";
        case JOG_VTR_STATE:
            return "Jog state";
        case OTHER_VTR_STATE:
        default:
            return "Unknown state";
    }
}

string VTRControl::errorCodeToString(int errorCode)
{
    int videoNibble = (errorCode & 0x00000007);
    int audioNibble = (errorCode & 0x00000070) >> 4;

    string result;

    switch (videoNibble)
    {
        case 0: 
            result = "Video good, ";
            break;
        case 1:
            result = "Video almost good, ";
            break;
        case 2:
            result = "Video status cannot be determined, ";
            break;
        case 3:
            result = "Video status unclear, ";
            break;
        default: // case 4
            result = "Video not good, ";
    }

    switch (audioNibble)
    {
        case 0: 
            result += "Audio good";
            break;
        case 1:
            result += "Audio almost good";
            break;
        case 2:
            result += "Audio status cannot be determined";
            break;
        case 3:
            result += "Audio status unclear";
            break;
        default: // case 4
            result += "Audio not good";
    }

    return result;
}

string VTRControl::getDeviceTypeString(DeviceType type)
{
    REC_ASSERT((size_t)type < sizeof(g_vtrDeviceInfo) / sizeof(VTRDeviceInfo));
    
    return g_vtrDeviceInfo[type].name;
}

int VTRControl::deviceTypeToCode(DeviceType type)
{
    REC_ASSERT((size_t)type < sizeof(g_vtrDeviceInfo) / sizeof(VTRDeviceInfo));
    
    return g_vtrDeviceInfo[type].code;
}

string VTRControl::stateBytesToString(const unsigned char* stateBytes)
{
    string result;
    
    char buf[32];
    sprintf(buf, "(%02x %02x %02x %02x) ", 
        stateBytes[0], stateBytes[1], stateBytes[2], stateBytes[3]);
    result = buf;
    
    unsigned char mask;
    int i, j;
    bool first = true;
    for (i = 0; i < 4; i++)
    {
        mask = 0x80;
        for (j = 0; j < 8; j++)
        {
            if (stateBytes[i] & mask)
            {
                if (!first)
                {
                    result += ",";
                }
                first = false;
                result += g_vtrStateBitStrings[i][j];
            }
            mask >>= 1;
        }
    }

    return result;
}


namespace rec
{
    
class VTRStatePoller : public ThreadWorker
{
public:
    VTRStatePoller(DefaultVTRControl* vtrControl)
    : _vtrControl(vtrControl), _hasStopped(true), _stop(false),
    _pollExtState(false), _pollVitc(false),
    _pollLtc(false), _minPollInterval(10)
    {}
    virtual ~VTRStatePoller()
    {}

    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped); 

        Timer pollFailureTimer;
        VTRState state;
        unsigned char stateBytes[NUM_STATE_BYTES];
        bool haveDeviceType = false;
        int deviceTypeCode;
        int errorCode;
        bool requireReopenPort = false;
        bool logConnection = true;
        
        while (!_stop)
        {
            // reopen the serial port if it isn't open
            if (!_vtrControl->portIsOpen() || requireReopenPort)
            {
                sleep_sec(REOPEN_PORT_RETRY_SEC);

                if (!_vtrControl->reopenPort())
                {
                    // try again
                    continue;
                }
                
                logConnection = true;
                requireReopenPort = false;
            }

            // poll state
            if (_vtrControl->readState(&state, stateBytes))
            {
                pollFailureTimer.reset();
                
                // get the device type before sending state updates
                if (!haveDeviceType)
                {
                    if (_vtrControl->readDeviceType(&deviceTypeCode))
                    {
                        haveDeviceType = true;
                        size_t i;
                        bool knownDeviceType = false;
                        for (i = 0; i < sizeof(g_vtrDeviceInfo) / sizeof(VTRDeviceInfo); i++)
                        {
                            if (deviceTypeCode == g_vtrDeviceInfo[i].code)
                            {
                                knownDeviceType = true;
                                _vtrControl->updateDeviceType(deviceTypeCode, g_vtrDeviceInfo[i].type);
                                break;
                            }
                        }
                        if (!knownDeviceType)
                        {
                            _vtrControl->updateDeviceType(deviceTypeCode, UNKNOWN_DEVICE_TYPE);
                        }
                    }
                }
                
                _vtrControl->updateState(state, stateBytes);

                
                if (logConnection)
                {
                    Logging::info("Connected to VTR '%s' via port '%s'\n", 
                        VTRControl::getDeviceTypeString(_vtrControl->getDeviceType()).c_str(),
                        _vtrControl->getSerialPortName().c_str());
                    logConnection = false;
                }
            }
            else
            {
                haveDeviceType = false; // force a re-check of the device type
                
                if (!pollFailureTimer.isRunning())
                {
                    // start the timer
                    pollFailureTimer.start();
                }
                else
                {
                    if (pollFailureTimer.elapsedTime() > REOPEN_PORT_FAIL_PERIOD)
                    {
                        // if state polling is still failing after REOPEN_PORT_FAIL_PERIOD then we 
                        // close the port and set state to "not connected"
                        
                        state = NOT_CONNECTED_VTR_STATE;
                        memset(stateBytes, 0, NUM_STATE_BYTES);
                        _vtrControl->updateState(state, stateBytes);
                        
                        requireReopenPort = true;
                    }
                    else if (pollFailureTimer.elapsedTime() > CONNECTION_FAIL_PERIOD)
                    {
                        // if state polling is still failing after CONNECTION_FAIL_PERIOD then we set 
                        // state to "not connected" 
                        
                        if (_vtrControl->getState() != NOT_CONNECTED_VTR_STATE)
                        {
                            Logging::warning("Connection lost to VTR '%s' via port '%s'\n", 
                                VTRControl::getDeviceTypeString(_vtrControl->getDeviceType()).c_str(),
                                _vtrControl->getSerialPortName().c_str());
                            logConnection = true;
                        }
                        state = NOT_CONNECTED_VTR_STATE;
                        memset(stateBytes, 0, NUM_STATE_BYTES);
                        _vtrControl->updateState(state, stateBytes);

                        // don't reset the timer because REOPEN_PORT_FAIL_PERIOD > CONNECTION_FAIL_PERIOD
                    }
                }
                
                // don't hog the CPU
                sleep_msec(2 * _minPollInterval);
                continue;
            }

            
            errorCode = 0;
            
            // poll extended state for VTR errors
            if (_pollExtState)
            {
                if (_vtrControl->readExtState(&state, &errorCode))
                {
                    if (errorCode)
                    {
                        Timecode ltc;
                        _vtrControl->readLtc(&ltc);
                        _vtrControl->updateLtc(ltc);                        
                        _vtrControl->updatePlaybackError(errorCode, ltc);
                    }
                }
            }

            // poll timecodes
            if (_pollVitc) 
            {
                Timecode vtc;
                if (_vtrControl->readVitc(&vtc))
                {
                    _vtrControl->updateVitc(vtc);
                }
            }
            if (_pollLtc && !errorCode)
            {
                Timecode ltc;
                if (_vtrControl->readLtc(&ltc))
                {
                    _vtrControl->updateLtc(ltc);
                }
            }

            // sleep to give other threads a chance to operate the vtr
            sleep_msec(_minPollInterval);
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

    
    void pollExtState(bool poll)
    {
        _pollExtState = poll;
    }

    void pollVitc(bool poll)
    {
        _pollVitc = poll;
    }

    void pollLtc(bool poll)
    {
        _pollLtc = poll;
    }

    void minPollInterval(int interval)
    {
        _minPollInterval = interval;
    }
    
private:
    DefaultVTRControl* _vtrControl;
    bool _hasStopped;
    bool _stop;
    bool _pollExtState;
    bool _pollVitc;
    bool _pollLtc;
    int _minPollInterval;
};

}; // namespace rec





set<string> DefaultVTRControl::_serialPortInUse;
Mutex DefaultVTRControl::_serialPortInUseMutex;



void DefaultVTRControl::addSerialPort(string deviceName)
{
    if (!deviceName.empty())
    {
        _serialPortInUse.insert(deviceName);
    }
}

void DefaultVTRControl::removeSerialPort(string deviceName)
{
    _serialPortInUse.erase(deviceName);
}

bool DefaultVTRControl::serialPortIsAvailable(string deviceName)
{
    return _serialPortInUse.find(deviceName) == _serialPortInUse.end();
}



DefaultVTRControl::DefaultVTRControl(string deviceName)
: _debugSerialPortOpen(false), _stateThread(0), _serialPortDeviceName(deviceName), _protocol(0),
_state(NOT_CONNECTED_VTR_STATE), _deviceTypeCode(0), _deviceType(UNKNOWN_DEVICE_TYPE)
{
    memset(_stateBytes, 0, NUM_STATE_BYTES);
    
    _debugSerialPortOpen = Config::getBoolD("dbg_serial_port_open", false);

    
    // use reopen function to open the port 
    if (!reopenPort())
    {
        Logging::warning("Failed to open serial port '%s' at startup - will try again later\n", deviceName.c_str());
    }
    
    // create the state poll thread
    _stateThread = new Thread(new VTRStatePoller(this), true);
    _stateThread->start();
}

DefaultVTRControl::~DefaultVTRControl()
{
    delete _stateThread;
    
    delete _protocol;

    {    
        LOCK_SECTION(_serialPortInUseMutex);
        removeSerialPort(_actualSerialPortDeviceName);
    }
}

void DefaultVTRControl::registerListener(VTRControlListener* listener)
{
    LOCK_SECTION(_listenerMutex);

    ListenerInfo info;
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

void DefaultVTRControl::unregisterListener(VTRControlListener* listener)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).listener == listener)
        {
            _listeners.erase(iter);
            return;
        }
    }
}

void DefaultVTRControl::pollExtState(bool poll)
{
    VTRStatePoller* poller;
    poller = dynamic_cast<VTRStatePoller*>(_stateThread->getWorker());
    poller->pollExtState(poll);
}

void DefaultVTRControl::pollVitc(bool poll)
{
    VTRStatePoller* poller;
    poller = dynamic_cast<VTRStatePoller*>(_stateThread->getWorker());
    poller->pollVitc(poll);
}

void DefaultVTRControl::pollLtc(bool poll)
{
    VTRStatePoller*  poller;
    poller = dynamic_cast<VTRStatePoller*>(_stateThread->getWorker());
    poller->pollLtc(poll);
}

int DefaultVTRControl::getDeviceCode()
{
    return _deviceTypeCode;
}

DeviceType DefaultVTRControl::getDeviceType()
{
    return _deviceType;
}

bool DefaultVTRControl::isD3VTR()
{
    return _deviceType == AJ_D350_NTSC_DEVICE_TYPE || _deviceType == AJ_D350_PAL_DEVICE_TYPE;
}

bool DefaultVTRControl::isNonD3VTR()
{
    return !isD3VTR() && _deviceType != 0x00;
}

bool DefaultVTRControl::play()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x01, "Play") == 0;
}

bool DefaultVTRControl::stop()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x00, "Stop") == 0;
}

bool DefaultVTRControl::record()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x02, "Record") == 0;
}

bool DefaultVTRControl::fastForward()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x10, "Fast Forward") == 0;
}

bool DefaultVTRControl::fastRewind()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x20, "Fast Rewind") == 0;
}

bool DefaultVTRControl::standbyOn()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x05, "Standby On") == 0;
}

bool DefaultVTRControl::standbyOff()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x04, "Standby Off") == 0;
}

bool DefaultVTRControl::seekToTimecode(Timecode timecode)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    unsigned char data[4];
    data[0] = ((timecode.frame / 10) << 4) | (timecode.frame % 10);
    data[1] = ((timecode.sec / 10) << 4) | (timecode.sec % 10);
    data[2] = ((timecode.min / 10) << 4) | (timecode.min % 10);
    data[3] = ((timecode.hour / 10) << 4) | (timecode.hour % 10);
    
    CommandBlock request;
    request.setCMDs(0x20, 0x31);
    request.setData(data, 4);
 
    return _protocol->simpleCommand(&request, "Seek Timecode") == 0;
}

bool DefaultVTRControl::eject()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x0F, "Eject") == 0;
}

VTRState DefaultVTRControl::getState()
{
    LOCK_SECTION(_stateMutex);
    
    return _state;
}

void DefaultVTRControl::getDetailedState(VTRState* state, unsigned char* stateBytes)
{
    LOCK_SECTION(_stateMutex);
    
    *state = _state;
    memcpy(stateBytes, _stateBytes, NUM_STATE_BYTES);
}


// caller must have lock on _serialMutex and _serialPortInUseMutex
bool DefaultVTRControl::openPort(string deviceName)
{
    if (!serialPortIsAvailable(deviceName))
    {
        return false;
    }
    
    try
    {
        if (_debugSerialPortOpen)
        {
            Logging::debug("Opening VTR serial port '%s'...\n", deviceName.c_str());
        }

        _protocol = SonyProtocol::open(deviceName);
        if (_protocol == 0)
        {
            if (_debugSerialPortOpen)
            {
                Logging::debug("Failed to open serial port '%s'\n", deviceName.c_str());
            }
            return false;
        }
        
        _protocol->flush();

        _actualSerialPortDeviceName = deviceName;
        addSerialPort(_actualSerialPortDeviceName);

        if (_debugSerialPortOpen)
        {
            Logging::debug("Successfully opened VTR serial port '%s'\n", deviceName.c_str());
        }

        return true;
    }
    catch (...)
    {
        SAFE_DELETE(_protocol);
        return false;
    }
}

bool DefaultVTRControl::reopenPort()
{
    LOCK_SECTION(_serialMutex);
    LOCK_SECTION_2(_serialPortInUseMutex);
    
    // close port first
    string prevActualDeviceName = _actualSerialPortDeviceName;
    if (_protocol != 0)
    {
        removeSerialPort(_actualSerialPortDeviceName);
        _actualSerialPortDeviceName = "";
        SAFE_DELETE(_protocol);
    }

    if (_serialPortDeviceName.size() >= 2 && 
        _serialPortDeviceName.compare(_serialPortDeviceName.size() - 2, 2, "%d") == 0)
    {
        // try MAX_PORT_DEVICE_NUMBER devices, substituting %d with the number 0..MAX_PORT_DEVICE_NUMBER
        // try the previously opened port last

        int i;
        string actualDeviceName;
        for (i = 0; i < MAX_PORT_DEVICE_NUMBER; i++)
        {
            actualDeviceName = _serialPortDeviceName.substr(0, _serialPortDeviceName.size() - 2);
            actualDeviceName.append(1, (char)('0' + i));
            if (actualDeviceName.compare(prevActualDeviceName) != 0 &&
                openPort(actualDeviceName))
            {
                return true;
            }
        }
 
        // try the previously opened port
        if (!prevActualDeviceName.empty())
        {
            return openPort(prevActualDeviceName);
        }
        
        return false;
    }
    
    return openPort(_serialPortDeviceName);
}

bool DefaultVTRControl::portIsOpen()
{
    return _protocol != 0;
}

string DefaultVTRControl::getSerialPortName()
{
    return _actualSerialPortDeviceName;
}

void DefaultVTRControl::updateDeviceType(int deviceTypeCode, DeviceType deviceType)
{
    {
        LOCK_SECTION(_deviceTypeMutex);
        _deviceTypeCode = deviceTypeCode;
        _deviceType = deviceType;
    }
    
    {
        LOCK_SECTION(_listenerMutex);
        
        vector<ListenerInfo>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if ((*iter).deviceTypeCode != deviceTypeCode)
            {
                (*iter).listener->vtrDeviceType(this, deviceTypeCode, deviceType);
                (*iter).deviceTypeCode = deviceTypeCode;
                (*iter).deviceType = deviceType;
            }
        }
    }    
}

void DefaultVTRControl::updateState(VTRState state, const unsigned char* stateBytes)
{
    {
        LOCK_SECTION(_stateMutex);
        _state = state;
        memcpy(_stateBytes, stateBytes, NUM_STATE_BYTES);
    }
    
    {
        LOCK_SECTION(_listenerMutex);
        
        vector<ListenerInfo>::iterator iter;
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

void DefaultVTRControl::updatePlaybackError(int errorCode, Timecode ltc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
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

void DefaultVTRControl::updateVitc(Timecode vitc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).vitc != vitc)
        {
            (*iter).listener->vtrVITC(this, vitc);
            (*iter).vitc = vitc;
        }
    }
}

void DefaultVTRControl::updateLtc(Timecode ltc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).ltc != ltc)
        {
            (*iter).listener->vtrLTC(this, ltc);
            (*iter).ltc = ltc;
        }
    }
}

bool DefaultVTRControl::readDeviceType(int* deviceTypeCode)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x00, 0x11);
    int result = _protocol->issueCommand(&request, &response, "Device Type");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 2 || !response.checkCMDs(0x10, 0x11))
    {
        Logging::error("Invalid device type response from VTR\n");
        return false;
    }

    *deviceTypeCode = (((int)response.getData()[0]) << 8) | (int)response.getData()[1];
    return true;
}

bool DefaultVTRControl::readState(VTRState* state, unsigned char* stateBytes)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x20);
    const unsigned char requestData[] = {0x04};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "State");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != NUM_STATE_BYTES || !response.checkCMDs(0x70, 0x20))
    {
        Logging::error("Invalid state response from VTR\n");
        return false;
    }

    getStateFromData(response.getData(), state);
    memcpy(stateBytes, response.getData(), NUM_STATE_BYTES);
    return true;
}

bool DefaultVTRControl::readExtState(VTRState* state, int* errorCode)
{
    LOCK_SECTION(_serialMutex);

    *errorCode = 0;
    
    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x21);
    const unsigned char requestData[] = {0x07};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "Ext State");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 13 || !response.checkCMDs(0x70, 0x21))
    {
        Logging::error("Invalid extended state response from VTR\n");
        return false;
    }

    getStateFromData(response.getData(), state);
    getExtStateFromData(response.getData(), errorCode);
    return true;
}

bool DefaultVTRControl::readTimecodes(Timecode* vitc, Timecode* ltc)
{
    if (!readVitc(vitc) || !readLtc(ltc))
    {
        return false;
    }

    return true;
}

bool DefaultVTRControl::readVitc(Timecode* vitc)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x0C);
    const unsigned char requestData[] = {0x02};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "VITC");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 4 || response.getCMD1() != 0x70)
    {
        Logging::error("Invalid VITC response from VTR\n");
        return false;
    }
    // CMD2: timecode type indicator: ltc=0x04, vitc=0x06, hold=0x16, corrected ltc=0x14

    getTimecodeFromData(response.getData(), vitc);
    return true;
}

bool DefaultVTRControl::readLtc(Timecode* ltc)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x0C);
    const unsigned char requestData[] = {0x01};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "LTC");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 4 || response.getCMD1() != 0x70)
    {
        Logging::error("Invalid VITC response from VTR\n");
        return false;
    }
    // CMD2: timecode type indicator: ltc=0x04, vitc=0x06, hold=0x16, corrected ltc=0x14

    getTimecodeFromData(response.getData(), ltc);
    return true;
}

void DefaultVTRControl::getStateFromData(const unsigned char* data, VTRState* state)
{
    if (data[0] & 0x01)
    {
        *state = REMOTE_LOCKOUT_VTR_STATE;
        return;
    }

    if (data[0] & 0x20)
    {
        *state = TAPE_UNTHREADED_VTR_STATE;
        return;
    }

    // check before play state because 0x01 is also set when recording
    if (data[1] & 0x02)
    {
        *state = RECORDING_VTR_STATE;
        return;
    }

    if (data[1] & 0x01)
    {
        *state = PLAY_VTR_STATE;
        return;
    }

    if (data[1] & 0x04)
    {
        *state = FAST_FORWARD_VTR_STATE;
        return;
    }

    if (data[1] & 0x08)
    {
        *state = FAST_REWIND_VTR_STATE;
        return;
    }

    if (data[1] & 0x10)
    {
        *state = EJECTING_VTR_STATE;
        return;
    }

    // tape is stopped and standby is on = paused
    if ((data[1] & 0x20) && (data[1] & 0x80)) 
    {
        *state = PAUSED_VTR_STATE;
        return;
    }

    // standby is off = stopped
    if (!(data[1] & 0x80))
    {
        *state = STOPPED_VTR_STATE;
        return;
    }

    if (data[2] & 0x20)
    {
        *state = SEEKING_VTR_STATE;
        return;
    }

    if (data[2] & 0x10)
    {
        *state = JOG_VTR_STATE;
        return;
    }

    // TODO
    *state = OTHER_VTR_STATE;
}

void DefaultVTRControl::getExtStateFromData(const unsigned char* data, int* errorCode)
{
    *errorCode = data[8];
}

void DefaultVTRControl::getTimecodeFromData(const unsigned char* data, Timecode* timecode)
{
    timecode->frame = ((data[0] & 0x30) >> 4) * 10 + (data[0] & 0x0F);
    timecode->sec   = ((data[1] & 0x70) >> 4) * 10 + (data[1] & 0x0F);
    timecode->min   = ((data[2] & 0x70) >> 4) * 10 + (data[2] & 0x0F);
    timecode->hour  = ((data[3] & 0x30) >> 4) * 10 + (data[3] & 0x0F);
}


