/*
 * $Id: HTTPVTRControl.h,v 1.1 2008/07/08 16:27:15 philipn Exp $
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
 
#ifndef __RECORDER_HTTP_VTR_CONTROL_H__
#define __RECORDER_HTTP_VTR_CONTROL_H__


#include <vector>

#include "VTRControl.h"
#include "HTTPServer.h"
#include "Threads.h"


namespace rec
{

    
typedef enum
{
    INDEXED_VTR_TARGET,
    D3_VTR_TARGET,
    DIGIBETA_VTR_TARGET
} VTRTarget;


class SingleVTRControl
{
public:
    SingleVTRControl(VTRControl* vc);
    ~SingleVTRControl();
    
    VTRControl* vtrControl;
    
    int deviceTypeCode;
    DeviceType deviceType;
    VTRState state;
    int errorCode;
    Timecode errorLTC;
    Timecode vitc;
    Timecode ltc;
    Mutex vtrStateMutex;
    
    Thread* controlAgent;
    Mutex controlAgentMutex;
};


class HTTPVTRControl : public HTTPConnectionHandler, public VTRControlListener
{
public:
    HTTPVTRControl(HTTPServer* server, std::vector<VTRControl*> vcs);
    virtual ~HTTPVTRControl();

    // from HTTPConnectionHandler
    virtual void processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection);

    // from VTRControlListener
    virtual void vtrDeviceType(VTRControl* vtrControl, int deviceTypeCode, DeviceType deviceType);
    virtual void vtrState(VTRControl* vtrControl, VTRState state, const unsigned char* stateBytes);
    virtual void vtrPlaybackError(VTRControl* vtrControl, int errorCode, Timecode ltc);
    virtual void vtrVITC(VTRControl* vtrControl, Timecode vitc);
    virtual void vtrLTC(VTRControl* vtrControl, Timecode ltc);

    
    void getVTRStatus(HTTPConnection* connection, VTRTarget target, int index);
    
    void playCommand(HTTPConnection* connection, VTRTarget target, int index);
    void stopCommand(HTTPConnection* connection, VTRTarget target, int index);
    void standbyOffCommand(HTTPConnection* connection, VTRTarget target, int index);
    void standbyOnCommand(HTTPConnection* connection, VTRTarget target, int index);
    void frCommand(HTTPConnection* connection, VTRTarget target, int index);
    void ffCommand(HTTPConnection* connection, VTRTarget target, int index);
    void ejectCommand(HTTPConnection* connection, VTRTarget target, int index);
    void recordCommand(HTTPConnection* connection, VTRTarget target, int index);
    
    SingleVTRControl* getSingleVTRControl(VTRTarget target, int index);
    
private:
    SingleVTRControl* getSingleVTRControl(VTRControl* vtrControl);
    
    std::vector<SingleVTRControl*> _vtrControls;
};    



};





#endif

