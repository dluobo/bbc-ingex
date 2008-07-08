/*
 * $Id: HTTPVTRControl.cpp,v 1.1 2008/07/08 16:27:00 philipn Exp $
 *
 * Provides access to a VTR through HTTP requests 
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
 
#include "HTTPVTRControl.h"
#include "JSONObject.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"


using namespace std;
using namespace rec;


typedef enum
{
    HTTP_PLAY_COMMAND,
    HTTP_STOP_COMMAND,
    HTTP_STANDBY_ON_COMMAND,
    HTTP_STANDBY_OFF_COMMAND,
    HTTP_FR_COMMAND,
    HTTP_FF_COMMAND,
    HTTP_EJECT_COMMAND,
    HTTP_RECORD_COMMAND
} HTTPControlCommand;


// all the HTTP service URLs handled by the HTTPVTRControl
// access to first VTR in vector
static const char* g_vtrStatusURL = "/vtr/status.json";
static const char* g_playCommandURL = "/vtr/control/play";
static const char* g_stopCommandURL = "/vtr/control/stop";
static const char* g_standbyOnCommandURL = "/vtr/control/standbyon";
static const char* g_standbyOffCommandURL = "/vtr/control/standbyoff";
static const char* g_frCommandURL = "/vtr/control/fr";
static const char* g_ffCommandURL = "/vtr/control/ff";
static const char* g_ejectCommandURL = "/vtr/control/eject";
static const char* g_recordCommandURL = "/vtr/control/record";

// access to D3 VTR
static const char* g_d3VTRStatusURL = "/vtr/d3/status.json";
static const char* g_d3PlayCommandURL = "/vtr/d3/control/play";
static const char* g_d3StopCommandURL = "/vtr/d3/control/stop";
static const char* g_d3StandbyOnCommandURL = "/vtr/d3/control/standbyon";
static const char* g_d3StandbyOffCommandURL = "/vtr/d3/control/standbyoff";
static const char* g_d3FRCommandURL = "/vtr/d3/control/fr";
static const char* g_d3FFCommandURL = "/vtr/d3/control/ff";
static const char* g_d3EjectCommandURL = "/vtr/d3/control/eject";
static const char* g_d3RecordCommandURL = "/vtr/d3/control/record";

// access to Digibeta VTR
static const char* g_digibetaVTRStatusURL = "/vtr/digibeta/status.json";
static const char* g_digibetaPlayCommandURL = "/vtr/digibeta/control/play";
static const char* g_digibetaStopCommandURL = "/vtr/digibeta/control/stop";
static const char* g_digibetaStandbyOnCommandURL = "/vtr/digibeta/control/standbyon";
static const char* g_digibetaStandbyOffCommandURL = "/vtr/digibeta/control/standbyoff";
static const char* g_digibetaFRCommandURL = "/vtr/digibeta/control/fr";
static const char* g_digibetaFFCommandURL = "/vtr/digibeta/control/ff";
static const char* g_digibetaEjectCommandURL = "/vtr/digibeta/control/eject";
static const char* g_digibetaRecordCommandURL = "/vtr/digibeta/control/record";


static void timecode_to_json(JSONObject* json, Timecode tc)
{
    json->setNumber("hour", tc.hour);
    json->setNumber("min", tc.min);
    json->setNumber("sec", tc.sec);
    json->setNumber("frame", tc.frame);
    json->setBool("dropFrame", false); // TODO
}



class ControlAgent : public ThreadWorker
{
public:
    ControlAgent(HTTPControlCommand command, VTRControl* vtrControl, HTTPConnection* connection)
    : _command(command), _vtrControl(vtrControl), _connection(connection), _hasStopped(false) 
    {}
    
    virtual ~ControlAgent()
    {}
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        bool result;
        switch (_command)
        {
            case HTTP_PLAY_COMMAND:
                result = _vtrControl->play();
                Logging::debug("Sent play command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            case HTTP_STOP_COMMAND:
                result = _vtrControl->stop();
                Logging::debug("Sent stop command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            case HTTP_STANDBY_ON_COMMAND:
                result = _vtrControl->standbyOn();
                Logging::debug("Sent standby on command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            case HTTP_STANDBY_OFF_COMMAND:
                result = _vtrControl->standbyOff();
                Logging::debug("Sent standby off command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            case HTTP_FR_COMMAND:
                result = _vtrControl->fastRewind();
                Logging::debug("Sent fast rewind command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            case HTTP_FF_COMMAND:
                result = _vtrControl->fastForward();
                Logging::debug("Sent fast forward command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            case HTTP_EJECT_COMMAND:
                result = _vtrControl->eject();
                Logging::debug("Sent eject command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            case HTTP_RECORD_COMMAND:
                result = _vtrControl->record();
                Logging::debug("Sent record command to VTR: %s\n", result ? "Success" : "Failed");
                break;
            default:
                Logging::warning("Unknown HTTP command\n");
                result = false;
                break;
        }
        
        if (result)
        {
            _connection->sendOk();
        }
        else
        {
            _connection->sendServerError("Command failed");
        }
    }
    
    virtual void stop()
    {
        // nothing to stop
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    HTTPControlCommand _command;
    VTRControl* _vtrControl;
    HTTPConnection* _connection;
    bool _hasStopped;
};


SingleVTRControl::SingleVTRControl(VTRControl* vc)
: vtrControl(vc), deviceTypeCode(0), deviceType(UNKNOWN_DEVICE_TYPE), 
state(NOT_CONNECTED_VTR_STATE), errorCode(0), controlAgent(0)
{
    
    // TODO/NOTE: no guarantee that the state or device type will not change after
    // calling the getter and before a listener callback is made to HTTPVTRControl
    // Do we need to duplicate the state in this class? It would be easier to keep
    // it in VTRControl and thus avoid possibility of getting out of sync
    
    deviceTypeCode = vc->getDeviceCode();
    deviceType = vc->getDeviceType();
    state = vc->getState();
}

SingleVTRControl::~SingleVTRControl()
{
    delete controlAgent;
}
    




HTTPVTRControl::HTTPVTRControl(HTTPServer* server, vector<VTRControl*> vcs)
{
    vector<VTRControl*>::const_iterator iter;
    for (iter = vcs.begin(); iter != vcs.end(); iter++)
    {
        _vtrControls.push_back(new SingleVTRControl(*iter));
        (*iter)->registerListener(this);
    }
    
    HTTPServiceDescription* service;
    
    service = server->registerService(new HTTPServiceDescription(g_vtrStatusURL), this);
    service->setDescription("Returns the VTR status");
    
    service = server->registerService(new HTTPServiceDescription(g_playCommandURL), this);
    service->setDescription("Sends play command to VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_stopCommandURL), this);
    service->setDescription("Sends stop command to VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_standbyOnCommandURL), this);
    service->setDescription("Sends standby on command to VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_standbyOffCommandURL), this);
    service->setDescription("Sends standby off command to VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_frCommandURL), this);
    service->setDescription("Sends fast rewind command to VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_ffCommandURL), this);
    service->setDescription("Sends fast forward command to VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_ejectCommandURL), this);
    service->setDescription("Sends eject command to VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_recordCommandURL), this);
    service->setDescription("Sends record command to VTR");
    
    
    service = server->registerService(new HTTPServiceDescription(g_d3VTRStatusURL), this);
    service->setDescription("Returns the D3 VTR status");
    
    service = server->registerService(new HTTPServiceDescription(g_d3PlayCommandURL), this);
    service->setDescription("Sends play command to D3 VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_d3StopCommandURL), this);
    service->setDescription("Sends stop command to D3 VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_d3StandbyOnCommandURL), this);
    service->setDescription("Sends standby on command to D3 VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_d3StandbyOffCommandURL), this);
    service->setDescription("Sends standby off command to D3 VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_d3FRCommandURL), this);
    service->setDescription("Sends fast rewind command to D3 VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_d3FFCommandURL), this);
    service->setDescription("Sends fast forward command to D3 VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_d3EjectCommandURL), this);
    service->setDescription("Sends eject command to D3 VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_d3RecordCommandURL), this);
    service->setDescription("Sends record command to D3 VTR");
    
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaVTRStatusURL), this);
    service->setDescription("Returns the Digibeta VTR status");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaPlayCommandURL), this);
    service->setDescription("Sends play command to Digibeta VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaStopCommandURL), this);
    service->setDescription("Sends stop command to Digibeta VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaStandbyOnCommandURL), this);
    service->setDescription("Sends standby on command to Digibeta VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaStandbyOffCommandURL), this);
    service->setDescription("Sends standby off command to Digibeta VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaFRCommandURL), this);
    service->setDescription("Sends fast rewind command to Digibeta VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaFFCommandURL), this);
    service->setDescription("Sends fast forward command to Digibeta VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaEjectCommandURL), this);
    service->setDescription("Sends eject command to Digibeta VTR");
    
    service = server->registerService(new HTTPServiceDescription(g_digibetaRecordCommandURL), this);
    service->setDescription("Sends record command to Digibeta VTR");
}

HTTPVTRControl::~HTTPVTRControl()
{
    vector<SingleVTRControl*>::const_iterator iter;
    for (iter = _vtrControls.begin(); iter != _vtrControls.end(); iter++)
    {
        (*iter)->vtrControl->unregisterListener(this);
        delete *iter;
    }
}

SingleVTRControl* HTTPVTRControl::getSingleVTRControl(VTRTarget target, int index)
{
    switch (target)
    {
        case INDEXED_VTR_TARGET:
            if (index < (int)_vtrControls.size())
            {
                return _vtrControls[index];
            }
            break;
        case D3_VTR_TARGET:
        case DIGIBETA_VTR_TARGET:
            {
                vector<SingleVTRControl*>::const_iterator iter;
                int count = 0;
                for (iter = _vtrControls.begin(); iter != _vtrControls.end(); iter++)
                {
                    if ((target == D3_VTR_TARGET && (*iter)->vtrControl->isD3VTR()) ||
                        (target == DIGIBETA_VTR_TARGET && (*iter)->vtrControl->isNonD3VTR()))
                    {
                        if (count == index)
                        {
                            return *iter;
                        }
                        count++;
                    }
                }
            }
            break;
    }
    
    return 0;
}


void HTTPVTRControl::processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection)
{
    if (serviceDescription->getURL().compare(g_vtrStatusURL) == 0)
    {
        getVTRStatus(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_playCommandURL) == 0)
    {
        playCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_stopCommandURL) == 0)
    {
        stopCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_standbyOnCommandURL) == 0)
    {
        standbyOnCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_standbyOffCommandURL) == 0)
    {
        standbyOffCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_frCommandURL) == 0)
    {
        frCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_ffCommandURL) == 0)
    {
        ffCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_ejectCommandURL) == 0)
    {
        ejectCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_recordCommandURL) == 0)
    {
        recordCommand(connection, INDEXED_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3VTRStatusURL) == 0)
    {
        getVTRStatus(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3PlayCommandURL) == 0)
    {
        playCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3StopCommandURL) == 0)
    {
        stopCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3StandbyOnCommandURL) == 0)
    {
        standbyOnCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3StandbyOffCommandURL) == 0)
    {
        standbyOffCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3FRCommandURL) == 0)
    {
        frCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3FFCommandURL) == 0)
    {
        ffCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3EjectCommandURL) == 0)
    {
        ejectCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_d3RecordCommandURL) == 0)
    {
        recordCommand(connection, D3_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaVTRStatusURL) == 0)
    {
        getVTRStatus(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaPlayCommandURL) == 0)
    {
        playCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaStopCommandURL) == 0)
    {
        stopCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaStandbyOnCommandURL) == 0)
    {
        standbyOnCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaStandbyOffCommandURL) == 0)
    {
        standbyOffCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaFRCommandURL) == 0)
    {
        frCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaFFCommandURL) == 0)
    {
        ffCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaEjectCommandURL) == 0)
    {
        ejectCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else if (serviceDescription->getURL().compare(g_digibetaRecordCommandURL) == 0)
    {
        recordCommand(connection, DIGIBETA_VTR_TARGET, 0);
    }
    else
    {
        REC_ASSERT(false);
    }
}

void HTTPVTRControl::vtrDeviceType(VTRControl* vtrControl, int deviceTypeCode, DeviceType deviceType)
{
    SingleVTRControl* svtrControl = getSingleVTRControl(vtrControl);
    REC_ASSERT(svtrControl != 0);
    
    {
        LOCK_SECTION(svtrControl->vtrStateMutex);
        
        svtrControl->deviceTypeCode = deviceTypeCode;
        svtrControl->deviceType = deviceType;
    }
}

void HTTPVTRControl::vtrState(VTRControl* vtrControl, VTRState state, const unsigned char* stateBytes)
{
    SingleVTRControl* svtrControl = getSingleVTRControl(vtrControl);
    REC_ASSERT(svtrControl != 0);
    
    {
        LOCK_SECTION(svtrControl->vtrStateMutex);
        
        svtrControl->state = state;
    }
}

void HTTPVTRControl::vtrPlaybackError(VTRControl* vtrControl, int errorCode, Timecode ltc)
{
    SingleVTRControl* svtrControl = getSingleVTRControl(vtrControl);
    REC_ASSERT(svtrControl != 0);
    
    {
        LOCK_SECTION(svtrControl->vtrStateMutex);
        
        svtrControl->errorCode = errorCode;
        svtrControl->errorLTC = ltc;
    }
}

void HTTPVTRControl::vtrVITC(VTRControl* vtrControl, Timecode vitc)
{
    SingleVTRControl* svtrControl = getSingleVTRControl(vtrControl);
    REC_ASSERT(svtrControl != 0);
    
    {
        LOCK_SECTION(svtrControl->vtrStateMutex);
        
        svtrControl->vitc = vitc;
    }
}

void HTTPVTRControl::vtrLTC(VTRControl* vtrControl, Timecode ltc)
{
    SingleVTRControl* svtrControl = getSingleVTRControl(vtrControl);
    REC_ASSERT(svtrControl != 0);
    
    {
        LOCK_SECTION(svtrControl->vtrStateMutex);
        
        svtrControl->ltc = ltc;
    }
}


void HTTPVTRControl::getVTRStatus(HTTPConnection* connection, VTRTarget target, int index)
{
    int deviceTypeCode;
    DeviceType deviceType;
    VTRState state;
    int errorCode;
    Timecode errorLTC;
    Timecode vitc;
    Timecode ltc;
    
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }
    
    // copy the state to local variables
    {
        LOCK_SECTION(svtrControl->vtrStateMutex);
        
        deviceTypeCode = svtrControl->deviceTypeCode;
        deviceType = svtrControl->deviceType;
        state = svtrControl->state;
        errorCode = svtrControl->errorCode;
        errorLTC = svtrControl->errorLTC;
        vitc = svtrControl->vitc;
        ltc = svtrControl->ltc;
    }
    
    
    // generate JSON response
    
    JSONObject json;
    json.setNumber("deviceTypeCode", deviceTypeCode);
    json.setString("deviceType", VTRControl::getDeviceTypeString(deviceType));
    json.setBool("recordEnabled", !svtrControl->vtrControl->isD3VTR());
    json.setNumber("state", state);
    timecode_to_json(json.setObject("vitc"), vitc);
    timecode_to_json(json.setObject("ltc"), ltc);
    json.setNumber("errorCode", errorCode);
    timecode_to_json(json.setObject("errorLTC"), errorLTC);
    

    connection->sendJSON(&json);
}

void HTTPVTRControl::playCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_PLAY_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}

void HTTPVTRControl::stopCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_STOP_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}

void HTTPVTRControl::standbyOnCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_STANDBY_ON_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}

void HTTPVTRControl::standbyOffCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_STANDBY_OFF_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}

void HTTPVTRControl::frCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_FR_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}

void HTTPVTRControl::ffCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_FF_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}

void HTTPVTRControl::ejectCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_EJECT_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}

void HTTPVTRControl::recordCommand(HTTPConnection* connection, VTRTarget target, int index)
{
    // get the target VTR control
    SingleVTRControl* svtrControl = getSingleVTRControl(target, index);
    if (svtrControl == 0)
    {
        connection->sendBadRequest("Unknown target VTR");
        return;
    }

    
    {    
        LOCK_SECTION(svtrControl->controlAgentMutex);
        
        if (svtrControl->controlAgent != 0 &&
            svtrControl->controlAgent->isRunning())
        {
            connection->sendServerBusy("Server is busy with the previous request");
            return;
        }
        
        // clean-up
        if (svtrControl->controlAgent != 0)
        {
            SAFE_DELETE(svtrControl->controlAgent);
        }
        
        // only allow record for non-D3 VTRs
        if (svtrControl->vtrControl->isD3VTR())
        {
            connection->sendBadRequest("D3 VTR Record is not permitted");
            return;
        }
        
        // start the agent
        svtrControl->controlAgent = new Thread(new ControlAgent(HTTP_RECORD_COMMAND, svtrControl->vtrControl, connection), true);
        svtrControl->controlAgent->start();
    }
}


SingleVTRControl* HTTPVTRControl::getSingleVTRControl(VTRControl* vtrControl)
{
    vector<SingleVTRControl*>::const_iterator iter;
    for (iter = _vtrControls.begin(); iter != _vtrControls.end(); iter++)
    {
        if ((*iter)->vtrControl == vtrControl)
        {
            return *iter;
        }
    }
    
    return 0;
}


