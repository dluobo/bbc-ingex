/*
 * $Id: JogShuttle.h,v 1.2 2009/01/29 07:10:27 stuart_hc Exp $
 *
 * Report jog shuttle controller events
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __INGEX_JOGSHUTTLE_H__
#define __INGEX_JOGSHUTTLE_H__


#include <vector>
#include <string>


namespace ingex
{


typedef enum
{
    UNKNOWN_DEVICE = 0,
    CONTOUR_SHUTTLE_PRO_DEVICE,
    CONTOUR_SHUTTLE_PRO_V2_DEVICE
} JogShuttleDevice;


class JogShuttle;

class JogShuttleListener
{
public:
    virtual ~JogShuttleListener() {}

    virtual void connected(JogShuttle* jogShuttle, JogShuttleDevice device) = 0;
    virtual void disconnected(JogShuttle* jogShuttle, JogShuttleDevice device) = 0;

    virtual void buttonPressed(JogShuttle* jogShuttle, int number) = 0;
    virtual void buttonReleased(JogShuttle* jogShuttle, int number) = 0;

    virtual void jog(JogShuttle* jogShuttle, bool clockwise, int position) = 0;
    virtual void shuttle(JogShuttle* jogShuttle, bool clockwise, int position) = 0;

    virtual void ping(JogShuttle* jogShuttle) = 0;
};



// internal only forward declarations
class Mutex;
void* jog_shuttle_thread(void* arg);


class JogShuttle
{
public:
    JogShuttle();
    virtual ~JogShuttle();

    // listeners
    void addListener(JogShuttleListener* listener);
    void removeListener(JogShuttleListener* listener);

    // device name
    std::string getDeviceName(JogShuttleDevice device);

    // open the device and start reading
    void start();

    // stop reading and close the device
    void stop();



public:
    // allow thread start function to access private stuff
    friend void* jog_shuttle_thread(void* arg);

private:
    bool openDevice();
    void closeDevice();

    void signalDeviceConnected(JogShuttleDevice device);
    void signalDeviceDisconnected(JogShuttleDevice device);
    void signalButtonPressed(int number);
    void signalButtonReleased(int number);
    void signalJog(bool clockwise, int position);
    void signalShuttle(bool clockwise, int position);
    void signalPing();

    void threadStart();
    int getNumEvents(struct input_event* events, int numEvents);
    void handleEvent(struct input_event* inEvents, int numEvents, int eventIndex);


    // listeners
    Mutex* _listenersMutex;
    std::vector<JogShuttleListener*> _listeners;

    // device
    Mutex* _deviceMutex;
    int _deviceFile;
    JogShuttleDevice _deviceType;

    // reading thread
    pthread_t _thread;
    bool _pauseThread;
    bool _stopThread;
    int _prevJogPosition;
    bool _checkShuttleValue;
    bool _prevShuttleClockwise;
    int _prevShuttlePosition;
    bool _jogIsAtPosition0;
};


};


#endif

