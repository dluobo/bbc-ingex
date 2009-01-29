/*
 * $Id: shuttle_input.c,v 1.4 2009/01/29 07:10:27 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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
#include <assert.h>

#include <linux/input.h>

#include "shuttle_input.h"
#include "logging.h"
#include "utils.h"
#include "macros.h"


/* Note: the Contour ShuttlePro jog control does not produce an event at position 0.
         If you move 1 step from position 1 anti-clockwise you will get no
         response (event), and the same happens from 255 clockwise.

         This problem also affects the shuttle control, where a return to the neutral position
         fails to produce an event

         The cause is the following line in the linux kernel source code,
         drivers/input/input.c, line 129 (see http://lxr.linux.no/source/drivers/input/input.c#L129)):

                 case EV_REL:

                         if (code > REL_MAX || !test_bit(code, dev->relbit) || (value == 0))
                                 return;

                         break;

         The "value == 0" prevents events from being sent when the jog is at position 0, ie. when
         the jog is at position 0 the value == 0

         Another unfortunate thing is that hiddev interface is disabled for USB input applications.

         One solution would be to patch the kernel with a contour design shuttle pro hack,
         similar to what is already being done in the hidinput_hid_event function
         in drivers/usb/input/hid-input.c

         */


/* number of event device files to check */
#define EVENT_DEV_INDEX_MAX     32

/* maximum number of listeners supported */
#define MAX_LISTENERS           8

/* the min and max code for key presses */
#define KEY_CODE_MIN            256
#define KEY_CODE_MAX            270 /* 269 and 270 are for ShuttlePro V2 */

/* jog values range is 0..255 (0 is never emitted); a patched usbhid module will have a range 1..256 */
#define MAX_JOG_VALUE               256
#define UNPATCHED_MAX_JOG_VALUE     255

#define MIN_SHUTTLE_VALUE       1
#define MAX_SHUTTLE_VALUE       7

/* some number > max number of events we expect (all keys + jog + shuttle + sync = 17) before a sync event */
#define EVENT_BUFFER_SIZE       32

/* wait 4 seconds before trying to reopen the shuttle device */
#define DEVICE_REOPEN_WAIT      (4000 * 1000)



typedef struct
{
    void* data;
    shuttle_listener func;
} ShuttleListener;

typedef struct
{
    short vendor;
    short product;
    char* name;
} ShuttleData;

struct ShuttleInput
{
    int fd;
    const ShuttleData* shuttleData;
    ShuttleListener listeners[MAX_LISTENERS];
    int prevJogValue;
    int checkShuttleValue;
    ShuttleShuttleEvent prevShuttleShuttleEvent;
    int jogIsAtPosition0;
    int stopped;
    pthread_mutex_t listenerMutex;
};


static const char* g_eventDeviceTemplate = "/dev/input/event%d";

static const ShuttleData g_supportedShuttles[] =
{
    {0x0b33, 0x0010, "Contour ShuttlePro"},
    {0x0b33, 0x0030, "Contour ShuttlePro V2"}
};

static const size_t g_supportedShuttlesSize = sizeof(g_supportedShuttles) / sizeof(ShuttleData);


/* returns the number of events until and including the first sync event */
static int get_num_events(struct input_event* events, int numEvents)
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

static int handle_silence(ShuttleInput* shuttle, ShuttleEvent* outEvent)
{
    /* assume shuttle is back in neutral if we don't receive events
    Silence occurs when the jog is at position 0 */

    if (shuttle->prevShuttleShuttleEvent.speed != 0)
    {
        outEvent->type = SH_SHUTTLE_EVENT;
        outEvent->value.shuttle.clockwise = 1;
        outEvent->value.shuttle.speed = 0;

        shuttle->prevShuttleShuttleEvent = outEvent->value.shuttle;
        shuttle->checkShuttleValue = 0;
        return 1;
    }

    return 0;
}

static int handle_event(ShuttleInput* shuttle, struct input_event* inEvents, int numEvents, int eventIndex,
    ShuttleEvent* outEvent)
{
    int haveOutEvent = 0;
    struct input_event* inEvent = &inEvents[eventIndex];
    int i;
    int haveJogEvent;


    switch (inEvent->type)
    {
        /* synchronization event */
        case EV_SYN:

            /* assume shuttle is back in neutral if we receive 2 synchronization events
               with no shuttle event in-between */

            if (shuttle->prevShuttleShuttleEvent.speed != 0)
            {
                if (shuttle->checkShuttleValue)
                {
                    /* send event that shuttle is back in neutral */
                    outEvent->type = SH_SHUTTLE_EVENT;
                    outEvent->value.shuttle.clockwise = 1;
                    outEvent->value.shuttle.speed = 0;

                    shuttle->prevShuttleShuttleEvent = outEvent->value.shuttle;
                    shuttle->checkShuttleValue = 0;
                    haveOutEvent = 1;
                }
                else
                {
                    /* check in next synchronization event */
                    shuttle->checkShuttleValue = 1;
                }
            }
            else
            {
                shuttle->checkShuttleValue = 0;
            }
            break;

        /* key event */
        case EV_KEY:
            if (inEvent->code >= KEY_CODE_MIN && inEvent->code <= KEY_CODE_MAX)
            {
                outEvent->type = SH_KEY_EVENT;
                outEvent->value.key.number = inEvent->code - KEY_CODE_MIN + 1;
                outEvent->value.key.isPressed = (inEvent->value == 1);
                haveOutEvent = 1;
            }
            break;

        /* Jog or Shuttle event */
        case EV_REL:

            /* ignore jog events occurrences that occur directly after shuttle events
               assume the first one is a regular jog event and always ignore it */
            if (inEvent->code == REL_DIAL &&
                ((shuttle->prevJogValue == inEvent->value && !shuttle->jogIsAtPosition0) || /* ignore non-event */
                    shuttle->prevJogValue > MAX_JOG_VALUE)) /* ignore first */
            {
                shuttle->prevJogValue = inEvent->value;
                break;
            }


            /* Jog event */
            if (inEvent->code == REL_DIAL)
            {
                /* only in position 0 do we not receive jog events */
                if (shuttle->jogIsAtPosition0)
                {
                    ml_log_info("Jog is no longer in position 0\n");
                    shuttle->jogIsAtPosition0 = 0;
                }

                outEvent->type = SH_JOG_EVENT;
                if (inEvent->value == shuttle->prevJogValue + 1 ||
                    (inEvent->value == 1 && shuttle->prevJogValue >= UNPATCHED_MAX_JOG_VALUE))
                {
                    outEvent->value.jog.clockwise = 1;
                }
                else if (inEvent->value + 1 == shuttle->prevJogValue ||
                    (inEvent->value >= UNPATCHED_MAX_JOG_VALUE && shuttle->prevJogValue == 1))
                {
                    outEvent->value.jog.clockwise = 0;
                }
                /* handle missed events (is this possible?) */
                else if (inEvent->value > shuttle->prevJogValue)
                {
                    outEvent->value.jog.clockwise = 1;
                }
                else
                {
                    outEvent->value.jog.clockwise = 0;
                }
                outEvent->value.jog.position = inEvent->value;

                shuttle->prevJogValue = inEvent->value;
                haveOutEvent = 1;
            }

            /* Shuttle event */
            else if (inEvent->code == REL_WHEEL)
            {
                shuttle->checkShuttleValue = 0;

                outEvent->type = SH_SHUTTLE_EVENT;
                outEvent->value.shuttle.clockwise = inEvent->value >= 0;
                outEvent->value.shuttle.speed = abs(inEvent->value);
                assert(outEvent->value.shuttle.speed <= MAX_SHUTTLE_VALUE &&
                    outEvent->value.shuttle.speed >= MIN_SHUTTLE_VALUE);

                /* change behaviour to (try) compensate for fact that return to neutral shuttle
                events do not occur when the jog is at position 0 */
                haveJogEvent = 0;
                for (i = 0; i < numEvents; i++)
                {
                    if (inEvents[i].type == EV_REL && inEvent[i].code == REL_DIAL)
                    {
                        haveJogEvent = 1;
                        break;
                    }
                }
                if (!haveJogEvent)
                {
                    if (!shuttle->jogIsAtPosition0)
                    {
                        ml_log_warn("Jog is in position 0 - please rotate\n");
                        shuttle->jogIsAtPosition0 = 1;
                    }

                    /* inEvent value 0 and 1 are taken to be speed 0. A value
                    equal 1 will then trigger the speed to drop back to 0.
                    Also, jumps in speed will also trigger it
                    handle_silence() will deal with the case where speed value 1 is skipped */
                    if (outEvent->value.shuttle.speed <= 1 ||
                        (shuttle->prevShuttleShuttleEvent.speed > outEvent->value.shuttle.speed + 1))
                    {
                        outEvent->value.shuttle.speed = 0;
                        outEvent->value.shuttle.clockwise = 1;
                    }
                    else
                    {
                        outEvent->value.shuttle.speed -= 1;
                    }
                }
                else
                {
                    if (shuttle->jogIsAtPosition0)
                    {
                        ml_log_info("Jog is no longer in position 0\n");
                    }
                    shuttle->jogIsAtPosition0 = 0;
                }

                if (shuttle->prevShuttleShuttleEvent.speed != outEvent->value.shuttle.speed ||
                    shuttle->prevShuttleShuttleEvent.clockwise != outEvent->value.shuttle.clockwise)
                {
                    haveOutEvent = 1;
                }
                shuttle->prevShuttleShuttleEvent = outEvent->value.shuttle;
            }

            break;

    }


    return haveOutEvent;
}

static int open_shuttle_device(ShuttleInput* shuttle)
{
    int i;
    int fd;
    struct input_id deviceInfo;
    size_t j;
    char eventDevName[256];
    int grab = 1;

    /* close shuttle if open */
    if (shuttle->fd >= 0)
    {
        close(shuttle->fd);
        shuttle->fd = -1;
    }

    /* go through the event devices and check whether it is a supported shuttle */
    for (i = 0; i < EVENT_DEV_INDEX_MAX; i++)
    {
        sprintf(eventDevName, g_eventDeviceTemplate, i);

        if ((fd = open(eventDevName, O_RDONLY)) != -1)
        {
            /* get the device information */
            if (ioctl(fd, EVIOCGID, &deviceInfo) >= 0)
            {
                for (j = 0; j < g_supportedShuttlesSize; j++)
                {
                    /* check the vendor and product id */
                    if (g_supportedShuttles[j].vendor == deviceInfo.vendor &&
                        g_supportedShuttles[j].product == deviceInfo.product)
                    {
                        /* try grab the device for exclusive access */
                        if (ioctl(fd, EVIOCGRAB, &grab) == 0)
                        {
                            shuttle->prevJogValue = MAX_JOG_VALUE + 1;
                            shuttle->fd = fd;
                            shuttle->shuttleData = &g_supportedShuttles[j];
                            return 1;
                        }

                        /* failed to grab device */
                        break;
                    }
                }
            }

            close(fd);
        }
    }

    return 0;
}


int shj_open_shuttle(ShuttleInput** shuttle)
{
    ShuttleInput* newShuttle;

    CALLOC_ORET(newShuttle, ShuttleInput, 1);
    newShuttle->fd = -1;

    if (!open_shuttle_device(newShuttle))
    {
        ml_log_warn("Failed to open jog-shuttle device for exclusive access - will try again later\n");
    }

    CHK_OFAIL(init_mutex(&newShuttle->listenerMutex));

    *shuttle = newShuttle;
    return 1;

fail:
    shj_close_shuttle(&newShuttle);
    return 0;
}

int shj_register_listener(ShuttleInput* shuttle, shuttle_listener listener, void* data)
{
    int i;
    ShuttleListener shutListen;

    shutListen.data = data;
    shutListen.func = listener;

    PTHREAD_MUTEX_LOCK(&shuttle->listenerMutex)

    for (i = 0; i < MAX_LISTENERS; i++)
    {
        if (shuttle->listeners[i].func == NULL)
        {
            shuttle->listeners[i] = shutListen;
            break;
        }
    }

    PTHREAD_MUTEX_UNLOCK(&shuttle->listenerMutex)

    return i != MAX_LISTENERS; /* i == MAX_LISTENERS if max listeners exceeded */
}

void shj_unregister_listener(ShuttleInput* shuttle, shuttle_listener listener)
{
    int i;

    PTHREAD_MUTEX_LOCK(&shuttle->listenerMutex)

    for (i = 0; i < MAX_LISTENERS; i++)
    {
        if (shuttle->listeners[i].func == listener)
        {
            shuttle->listeners[i].func = NULL;
            break;
        }
    }

    PTHREAD_MUTEX_UNLOCK(&shuttle->listenerMutex)
}

void shj_start_shuttle(ShuttleInput* shuttle)
{
    struct input_event inEvent[EVENT_BUFFER_SIZE];
    ShuttleEvent outEvent;
    int i, j;
    fd_set rfds;
    struct timeval tv;
    int retval;
    ssize_t numRead;
    int numEvents;
    int eventsOffset;
    int processOffset;
    int processNumEvents;
    int selectCount;
    int waitTime;


    numEvents = 0;
    eventsOffset = 0;
    selectCount = 0;
    while (!shuttle->stopped)
    {
        if (shuttle->fd < 0)
        {
            /* try open device */
            if (!open_shuttle_device(shuttle))
            {
                /* failed - wait before trying again */
                waitTime = DEVICE_REOPEN_WAIT;
                while (!shuttle->stopped && waitTime > 0)
                {
                    usleep(100 * 1000);
                    waitTime -= 100 * 1000;
                }
                continue;
            }
        }

        FD_ZERO(&rfds);
        FD_SET(shuttle->fd, &rfds);
        /* TODO: is there a way to find out how many sec between sync events? */
        tv.tv_sec = 0;
        tv.tv_usec = 50000; /* 1/20 second */


        retval = select(shuttle->fd + 1, &rfds, NULL, NULL, &tv);
        selectCount++;

        if (shuttle->stopped)
        {
            /* thread stop was signalled so break out of the loop to allow join */
            break;
        }
        else if (retval == -1)
        {
            /* select error */
            selectCount = 0;
            if (errno == EINTR)
            {
                /* signal interrupt - try select again */
                continue;
            }
            else
            {
                /* something is wrong. */
                /* Close the device and try open again after waiting */

                close(shuttle->fd);
                shuttle->fd = -1;

                waitTime = DEVICE_REOPEN_WAIT;
                while (!shuttle->stopped && waitTime > 0)
                {
                    usleep(100 * 1000);
                    waitTime -= 100 * 1000;
                }
                continue;
            }
        }
        else if (retval)
        {
            selectCount = 0;

            numRead = read(shuttle->fd, &inEvent[eventsOffset], (EVENT_BUFFER_SIZE - eventsOffset) * sizeof(struct input_event));
            if (numRead > 0)
            {
                numEvents += numRead / sizeof(struct input_event);
                eventsOffset += numRead / sizeof(struct input_event);

                /* process the events if there is a sync event */
                processOffset = 0;
                if (numEvents == EVENT_BUFFER_SIZE)
                {
                    /* process all the events if we get an unexpected number of events before a sync event */
                    processNumEvents = numEvents;
                }
                else
                {
                    processNumEvents = get_num_events(&inEvent[processOffset], numEvents);
                }
                while (processNumEvents > 0)
                {
                    for (j = 0; j < processNumEvents; j++)
                    {
                        if (handle_event(shuttle, &inEvent[processOffset], processNumEvents, j, &outEvent))
                        {
                            /* call the listeners */
                            PTHREAD_MUTEX_LOCK(&shuttle->listenerMutex)
                            for (i = 0; i < MAX_LISTENERS; i++)
                            {
                                if (shuttle->listeners[i].func != NULL)
                                {
                                    shuttle->listeners[i].func(shuttle->listeners[i].data, &outEvent);
                                }
                            }
                            PTHREAD_MUTEX_UNLOCK(&shuttle->listenerMutex)
                        }
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
                        processNumEvents = get_num_events(&inEvent[processOffset], numEvents);
                    }
                }
            }
            else
            {
                /* num read zero indicates end-of-file, ie. the shuttle device was unplugged */
                /* close the device and try open again after waiting */

                close(shuttle->fd);
                shuttle->fd = -1;

                waitTime = DEVICE_REOPEN_WAIT;
                while (!shuttle->stopped && waitTime > 0)
                {
                    usleep(100 * 1000);
                    waitTime -= 100 * 1000;
                }
                continue;
            }
        }
        else if (selectCount > 80) /* > 4 seconds */
        {
            selectCount = 0;

            /* a timeout > 4 seconds means we are in jog position 0 where no events are sent */

            /* flush the events */
            for (j = 0; j < numEvents; j++)
            {
                if (handle_event(shuttle, &inEvent[0], numEvents, j, &outEvent))
                {
                    /* call the listeners */
                    PTHREAD_MUTEX_LOCK(&shuttle->listenerMutex)
                    for (i = 0; i < MAX_LISTENERS; i++)
                    {
                        if (shuttle->listeners[i].func != NULL)
                        {
                            shuttle->listeners[i].func(shuttle->listeners[i].data, &outEvent);
                        }
                    }
                    PTHREAD_MUTEX_UNLOCK(&shuttle->listenerMutex)
                }
            }

            numEvents = 0;
            eventsOffset = 0;

            /* handle silence */
            if (handle_silence(shuttle, &outEvent))
            {
                /* call the listeners */
                PTHREAD_MUTEX_LOCK(&shuttle->listenerMutex)
                for (i = 0; i < MAX_LISTENERS; i++)
                {
                    if (shuttle->listeners[i].func != NULL)
                    {
                        shuttle->listeners[i].func(shuttle->listeners[i].data, &outEvent);
                    }
                }
                PTHREAD_MUTEX_UNLOCK(&shuttle->listenerMutex)
            }
        }
        else if (selectCount > 1) /* ignore the first */
        {
            /* send a ping */
            outEvent.type = SH_PING_EVENT;
            PTHREAD_MUTEX_LOCK(&shuttle->listenerMutex)
            for (i = 0; i < MAX_LISTENERS; i++)
            {
                if (shuttle->listeners[i].func != NULL)
                {
                    shuttle->listeners[i].func(shuttle->listeners[i].data, &outEvent);
                }
            }
            PTHREAD_MUTEX_UNLOCK(&shuttle->listenerMutex)
        }

    }
}

void shj_close_shuttle(ShuttleInput** shuttle)
{
    if (*shuttle == NULL)
    {
        return;
    }

    if ((*shuttle)->fd >= 0)
    {
        close((*shuttle)->fd);
    }

    destroy_mutex(&(*shuttle)->listenerMutex);

    SAFE_FREE(shuttle);
}

void shj_stop_shuttle(void* arg)
{
    ShuttleInput* shuttle = (ShuttleInput*)arg;

    shuttle->stopped = 1;
}


