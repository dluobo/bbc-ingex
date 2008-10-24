/*
 * $Id: JogShuttle.cpp,v 1.1 2008/10/24 19:09:22 john_f Exp $
 *
 * Report jog shuttle controller events
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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
 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <linux/input.h>


#include "JogShuttle.h"


using namespace std;
using namespace ingex;



// number of event device files to check
#define EVENT_DEV_INDEX_MAX     32

// the min and max code for key presses
#define KEY_CODE_MIN            256
#define KEY_CODE_MAX            270     // 269 and 270 are for ShuttlePro V2

// jog values range is 0..255 (0 is never emitted); a patched usbhid module will have a range 1..256
#define MAX_JOG_VALUE               256
#define UNPATCHED_MAX_JOG_VALUE     255

#define MIN_SHUTTLE_VALUE       1
#define MAX_SHUTTLE_VALUE       7

// some number > max number of events we expect (all keys + jog + shuttle + sync = 17) before a sync event
#define EVENT_BUFFER_SIZE       32 

// wait 4 seconds before trying to reopen the shuttle device
#define DEVICE_REOPEN_WAIT      (4000 * 1000)



// supported Contour Design devices

typedef struct
{
    short vendor;
    short product;
    const char* name;
    JogShuttleDevice device;
} ShuttleData;

static const ShuttleData g_supportedShuttles[] = 
{
    {0x0b33, 0x0010, "Contour ShuttlePro", CONTOUR_SHUTTLE_PRO_DEVICE},
    {0x0b33, 0x0030, "Contour ShuttlePro V2", CONTOUR_SHUTTLE_PRO_V2_DEVICE}
};

static const size_t g_supportedShuttlesSize = sizeof(g_supportedShuttles) / sizeof(ShuttleData);


// the linux event device filename template
static const char* g_eventDeviceTemplate = "/dev/input/event%d";




// mutex helper classes

#define LOCK_SECTION(m)             MutexLocker __lock(m);

namespace ingex
{
    class Mutex
    {
    public:
        Mutex()
        {
            if (pthread_mutex_init(&_mutex, NULL) != 0)
            {
                fprintf(stderr, "Failed to init mutex: %s\n", strerror(errno));
                throw;
            }
        }
        ~Mutex()
        {
            if (pthread_mutex_destroy(&_mutex) != 0)
            {
                fprintf(stderr, "Failed to destroy mutex: %s\n", strerror(errno));
            }
        }
        
        void lock()
        {
            if (pthread_mutex_lock(&_mutex) != 0)
            {
                fprintf(stderr, "Failed to lock mutex: %s\n", strerror(errno));
                throw;
            }
        }
    
        void unlock()
        {
            if (pthread_mutex_unlock(&_mutex) != 0)
            {
                fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(errno));
                throw;
            }
        }
        
    private:
        pthread_mutex_t _mutex;
    };
    
    class MutexLocker
    {
    public:
        MutexLocker(Mutex* mutex) : _mutex(mutex)
        {
            _mutex->lock();
        }
    
        ~MutexLocker()
        {
            _mutex->unlock();
        }
        
    private:
        Mutex* _mutex;
    };
}



// the jog shuttle thread

void* ingex::jog_shuttle_thread(void* arg)
{
    JogShuttle* jogShuttle = (JogShuttle*)arg;
    
    jogShuttle->threadStart();
    
    pthread_exit((void*) 0);
}






JogShuttle::JogShuttle()
: _listenersMutex(0), _deviceMutex(0), _deviceFile(-1), _deviceType(UNKNOWN_DEVICE), 
_pauseThread(false), _stopThread(false), _prevJogPosition(MAX_JOG_VALUE + 1), _checkShuttleValue(false), 
_prevShuttleClockwise(false), _prevShuttlePosition(0), _jogIsAtPosition0(false)
{
    _listenersMutex = new Mutex();
    _deviceMutex = new Mutex();


    // create the shuttle device thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    int result = pthread_create(&_thread, &attr, jog_shuttle_thread, (void*)this);
    if (result != 0)
    {
        fprintf(stderr, "Failed to create joinable thread: %s\n", strerror(result));
        throw;
    }
    
    pthread_attr_destroy(&attr);
}

JogShuttle::~JogShuttle()
{
    // don't signal to listeners anymore
    {
        LOCK_SECTION(_listenersMutex);
        _listeners.clear();
    }

    // signal the thread to stop and join
    _stopThread = true;
    
    int result;
    void* status;
    if ((result = pthread_join(_thread, (void **)&status)) != 0)
    {
        fprintf(stderr, "Failed to join thread: %s", strerror(result));
    }

    // close the event device
    closeDevice();
    
    delete _listenersMutex;
    delete _deviceMutex;
}

void JogShuttle::addListener(JogShuttleListener* listener)
{
    LOCK_SECTION(_listenersMutex);

    // check not already registered
    vector<JogShuttleListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if (listener == *iter)
        {
            return;
        }
    }

    _listeners.push_back(listener);
}

void JogShuttle::removeListener(JogShuttleListener* listener)
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if (listener == *iter)
        {
            _listeners.erase(iter);
            return;
        }
    }
}

string JogShuttle::getDeviceName(JogShuttleDevice device)
{
    size_t i;
    for (i = 0; i < g_supportedShuttlesSize; i++)
    {
        if (device == g_supportedShuttles[i].device)
        {
            return g_supportedShuttles[i].name;
        }
    }
    
    return "Unknown";
}

void JogShuttle::start()
{
    _pauseThread = false;
}

void JogShuttle::stop()
{
    _pauseThread = true;
}

bool JogShuttle::openDevice()
{
    LOCK_SECTION(_deviceMutex);
    
    int i;
    int fd;
    struct input_id deviceInfo;
    size_t j;
    char eventDevName[256];
    int grab = 1;

    
    // loop through the event devices, check whether it is a supported shuttle
    // and then try opening it
    for (i = 0; i < EVENT_DEV_INDEX_MAX; i++)
    {
        sprintf(eventDevName, g_eventDeviceTemplate, i);
        
        if ((fd = open(eventDevName, O_RDONLY)) != -1)
        {
            // get the device information
            if (ioctl(fd, EVIOCGID, &deviceInfo) >= 0)
            {
                for (j = 0; j < g_supportedShuttlesSize; j++)
                {
                    // check the vendor and product id
                    if (g_supportedShuttles[j].vendor == deviceInfo.vendor &&
                        g_supportedShuttles[j].product == deviceInfo.product)
                    {
                        // try grab the device for exclusive access
                        if (ioctl(fd, EVIOCGRAB, &grab) == 0)
                        {
                            _prevJogPosition = MAX_JOG_VALUE + 1;
                            _deviceFile = fd;
                            _deviceType = g_supportedShuttles[j].device;
                            
                            signalDeviceConnected(_deviceType);
                            return true;
                        }
                        
                        // failed to grab device
                        break;
                    }
                }
            }
            
            close(fd);
        }
    }
    
    return false;
}

void JogShuttle::closeDevice()
{
    LOCK_SECTION(_deviceMutex);
    
    JogShuttleDevice deviceType;

    if (_deviceFile >= 0)
    {
        deviceType = _deviceType;
        
        close(_deviceFile);
        _deviceFile = -1;
        _deviceType = UNKNOWN_DEVICE;
        
        signalDeviceDisconnected(deviceType);
    }
}

void JogShuttle::signalDeviceConnected(JogShuttleDevice device)
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->connected(this, device);
    }
}

void JogShuttle::signalDeviceDisconnected(JogShuttleDevice device)
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->disconnected(this, device);
    }
}

void JogShuttle::signalButtonPressed(int number)
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->buttonPressed(this, number);
    }
}

void JogShuttle::signalButtonReleased(int number)
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->buttonReleased(this, number);
    }
}

void JogShuttle::signalJog(bool clockwise, int position)
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->jog(this, clockwise, position);
    }
}

void JogShuttle::signalShuttle(bool clockwise, int position)
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->shuttle(this, clockwise, position);
    }
}

void JogShuttle::signalPing()
{
    LOCK_SECTION(_listenersMutex);

    vector<JogShuttleListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->ping(this);
    }
}


void JogShuttle::threadStart()
{
    struct input_event inEvent[EVENT_BUFFER_SIZE]; 
    int j;
    fd_set rfds;
    struct timeval tv;
    int retval;
    ssize_t numRead;
    int numEvents;
    int eventsOffset;
    int processOffset;
    int processNumEvents;
    int selectCount;
    int waitTime = 0;

    
    numEvents = 0;
    eventsOffset = 0;
    selectCount = 0;
    while (!_stopThread)
    {
        // pause with device closed
        if (_pauseThread)
        {
            if (_deviceFile >= 0)
            {
                closeDevice();
            }
            
            while (!_stopThread && _pauseThread)
            {
                usleep(100 * 1000);
            }
            continue;
        }
        
        // do waits
        if (waitTime > 0)
        {
            while (!_stopThread && !_pauseThread && waitTime > 0)
            {
                usleep(100 * 1000);
                waitTime -= 100 * 1000;
            }
            
            if (_stopThread || _pauseThread)
            {
                continue;
            }
        }
        
        // open the device if closed
        if (_deviceFile < 0)
        {
            if (!openDevice())
            {
                waitTime = DEVICE_REOPEN_WAIT;
                continue;
            }
        }
        
        
        FD_ZERO(&rfds);
        FD_SET(_deviceFile, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 1/20 second
        
        retval = select(_deviceFile + 1, &rfds, NULL, NULL, &tv);
        selectCount++;

        
        if (_stopThread)
        {
            // thread stop was signalled so break out of the loop to allow join
            break;
        }
        else if (retval == -1)
        {
            // select error
            selectCount = 0;
            if (errno == EINTR)
            {
                // signal interrupt - try select again
                continue;
            }
            else
            {
                // something is wrong
                // close the device and try open again after waiting
                
                closeDevice();
                
                waitTime = DEVICE_REOPEN_WAIT;
                continue;
            }
        }
        else if (retval)
        {
            selectCount = 0;
            
            numRead = read(_deviceFile, &inEvent[eventsOffset], (EVENT_BUFFER_SIZE - eventsOffset) * sizeof(struct input_event));
            if (numRead > 0)
            {
                numEvents += numRead / sizeof(struct input_event);
                eventsOffset += numRead / sizeof(struct input_event);
                
                // process the events if there is a sync event
                processOffset = 0;
                if (numEvents == EVENT_BUFFER_SIZE)
                {
                    // process all the events if we get an unexpected number of events before a sync event
                    processNumEvents = numEvents;
                }
                else
                {
                    processNumEvents = getNumEvents(&inEvent[processOffset], numEvents);
                }
                while (processNumEvents > 0)
                {
                    for (j = 0; j < processNumEvents; j++)
                    {
                        handleEvent(&inEvent[processOffset], processNumEvents, j);
                    }

                    processOffset += processNumEvents;
                    numEvents -= processNumEvents;
                    eventsOffset -= processNumEvents;

                    if (numEvents == EVENT_BUFFER_SIZE)
                    {
                        processNumEvents = 0;
                    }
                    else
                    {
                        processNumEvents = getNumEvents(&inEvent[processOffset], numEvents);
                    }
                }
            }
            else
            {
                // num read zero indicates end-of-file, ie. the shuttle device was unplugged
                // close the device and try open again after waiting
                
                closeDevice();
                
                waitTime = DEVICE_REOPEN_WAIT;
                continue;
            }
        }
        else if (selectCount > 80) // > 4 seconds
        {
            selectCount = 0;
            
            // a timeout > 4 seconds means we are in jog position 0 where no events are sent

            // flush the events
            for (j = 0; j < numEvents; j++)
            {
                handleEvent(&inEvent[0], numEvents, j);
            }
            
            numEvents = 0;
            eventsOffset = 0;

            // handle silence            
            // assume shuttle is back in neutral if we don't receive events. Silence occurs 
            // when the jog is at position 0
        
            if (_prevShuttlePosition != 0)
            {
                signalShuttle(true, 0);
                
                _prevShuttleClockwise = true;
                _prevShuttlePosition = 0;
            }
        }
        else if (selectCount > 1) // ignore the first
        {
            signalPing();
        }
    }
}

int JogShuttle::getNumEvents(struct input_event* events, int numEvents)
{
    int i;
    
    for (i = 0; i < numEvents; i++)
    {
        if (events[i].type == EV_SYN)
        {
            return i + 1;
        }
    }
    
    return 0;
}

void JogShuttle::handleEvent(struct input_event* inEvents, int numEvents, int eventIndex)
{
    struct input_event* inEvent = &inEvents[eventIndex];
    int i;
    bool haveJogEvent;
    bool clockwise;
    int position;

    
    switch (inEvent->type)
    {
        // synchronization event
        case EV_SYN:
        
            // assume shuttle is back in neutral if we receive 2 synchronization events
            // with no shuttle event in-between

            if (_prevShuttlePosition != 0)
            {
                if (_checkShuttleValue)
                {
                    // shuttle is back in neutral
                    signalShuttle(true, 0);
                    
                    _prevShuttleClockwise = true;
                    _prevShuttlePosition = 0;
                    _checkShuttleValue = false;
                }
                else
                {
                    // check in next synchronization event
                    _checkShuttleValue = true;
                }
            }
            else
            {
                _checkShuttleValue = false;
            }
            break;
            
        // key event
        case EV_KEY:
            if (inEvent->code >= KEY_CODE_MIN && inEvent->code <= KEY_CODE_MAX)
            {
                if (inEvent->value == 1)
                {
                    signalButtonPressed(inEvent->code - KEY_CODE_MIN + 1);
                }
                else
                {
                    signalButtonReleased(inEvent->code - KEY_CODE_MIN + 1);
                }
            }
            break;
            
        // Jog or Shuttle event            
        case EV_REL:

            // ignore jog events occurrences that occur directly after shuttle events
            // assume the first one is a regular jog event and always ignore it
            if (inEvent->code == REL_DIAL &&
                ((_prevJogPosition == inEvent->value && !_jogIsAtPosition0) || // ignore non-event
                    _prevJogPosition > MAX_JOG_VALUE)) // ignore first
            {
                _prevJogPosition = inEvent->value;
                break;
            }
            
                
            // Jog event
            if (inEvent->code == REL_DIAL)
            {
                // only in position 0 do we not receive jog events
                if (_jogIsAtPosition0)
                {
                    _jogIsAtPosition0 = false;
                }
                
                if (inEvent->value == _prevJogPosition + 1 ||
                    (inEvent->value == 1 && _prevJogPosition >= UNPATCHED_MAX_JOG_VALUE))
                {
                    clockwise = true;
                }
                else if (inEvent->value + 1 == _prevJogPosition ||
                    (inEvent->value >= UNPATCHED_MAX_JOG_VALUE && _prevJogPosition == 1))
                {
                    clockwise = false;
                }
                // handle missed events (is this possible?) 
                else if (inEvent->value > _prevJogPosition)
                {
                    clockwise = true;
                }
                else
                {
                    clockwise = false;
                }
                position = inEvent->value;
                
                signalJog(clockwise, position);

                
                _prevJogPosition = position;
            }
            
            // Shuttle event
            else if (inEvent->code == REL_WHEEL)
            {
                _checkShuttleValue = false;
                
                clockwise = (inEvent->value >= 0);
                position = abs(inEvent->value);

                // change behaviour to (try) compensate for fact that return to neutral shuttle
                // events do not occur when the jog is at position 0
                haveJogEvent = false;                    
                for (i = 0; i < numEvents; i++)
                {
                    if (inEvents[i].type == EV_REL && inEvent[i].code == REL_DIAL)
                    {
                        haveJogEvent = true;
                        break;
                    }
                }
                if (!haveJogEvent) 
                {
                    if (!_jogIsAtPosition0)
                    {
                        _jogIsAtPosition0 = true;
                    }
                    
                    // inEvent value 0 and 1 are taken to be speed 0. A value
                    // equal 1 will then trigger the speed to drop back to 0.
                    // Also, jumps in speed will also trigger it
                    // handleSilence() will deal with the case where speed value 1 is skipped 
                    if (position <= 1 || _prevShuttlePosition > position + 1)
                    {
                        position = 0;
                        clockwise = true;
                    }
                    else
                    {
                        position -= 1;
                    }
                }
                else
                {
                    _jogIsAtPosition0 = false;
                }

                if (_prevShuttlePosition != position || _prevShuttleClockwise != clockwise)
                {
                    signalShuttle(clockwise, position);
                }
                
                
                _prevShuttleClockwise = clockwise;
                _prevShuttlePosition = position;
            }

            break;
    }
}



