/*
 * $Id: quartzRouter.h,v 1.5 2008/11/06 11:08:37 john_f Exp $
 *
 * Class to handle communication with Quartz router.
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

#ifndef quartzRouter_h
#define quartzRouter_h

#include "CommunicationPort.h"

#include <ace/Task.h>

#include <string>
#include <vector>

const int qbufsize = 128;

class RouterObserver;

/**
Interface to Quartz router
*/
class Router : public ACE_Task_Base 
{
public:
// methods
    Router();
    Router(std::string rp);
    ~Router();
    bool Init(const std::string & port, Transport::EnumType transport);
    void Stop();
    bool Connected() { return mConnected; }

    virtual int svc ();

    void AddObserver(RouterObserver * obs) { mObservers.push_back(obs); }

    void QuerySrc(unsigned int dest);

private:
// methods
    void ProcessMessage(const std::string & message);
    void readUpdate();
    bool readReply();

// members
    char mBuffer[qbufsize];
    char * mWritePtr;
    char * mBufferEnd;

    std::vector<RouterObserver *> mObservers;
    CommunicationPort * mpCommunicationPort;
    bool mConnected;
    bool mRun;
};

#endif //#ifndef quartzRouter_h

