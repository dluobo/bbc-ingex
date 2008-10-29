/*
 * $Id: console_monitor.c,v 1.2 2008/10/29 17:47:41 john_f Exp $
 *
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "console_monitor.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


struct ConsoleMonitor
{
    MediaPlayerListener playerListener;
    int64_t lastDroppedFramePosition;
    int64_t lastFrameCount;
};




static void csm_frame_displayed_event(void* data, const FrameInfo* frameInfo)
{
    //ConsoleMonitor* monitor = (ConsoleMonitor*)data;
}

static void csm_frame_dropped_event(void* data, const FrameInfo* lastFrameInfo)
{
    ConsoleMonitor* monitor = (ConsoleMonitor*)data;
    int i;

    if (lastFrameInfo->position != monitor->lastDroppedFramePosition || 
        lastFrameInfo->frameCount != monitor->lastFrameCount)
    {
        printf("Dropped: last frame displayed: ");
        for (i = 0; i < lastFrameInfo->numTimecodes; i++)
        {
            print_timecode(lastFrameInfo->timecodes[i].timecodeType, lastFrameInfo->timecodes[i].timecodeSubType, 
                &lastFrameInfo->timecodes[i].timecode);
            printf(" ");
        }
        printf("\n");
        monitor->lastDroppedFramePosition = lastFrameInfo->position;
        monitor->lastFrameCount = lastFrameInfo->frameCount;
    }
}

static void csm_state_change_event(void* data, const MediaPlayerStateEvent* event)
{
    //ConsoleMonitor* monitor = (ConsoleMonitor*)data;
    int i;
    
    if (event->lockedChanged)
    {
        if (event->locked)
        {
            printf("Locked\n");
        }
        else
        {
            printf("Unlocked\n");
        }
    }
    if (event->playChanged)
    {
        if (event->play)
        {
            printf("Playing\n");
        }
        else
        {
            printf("Paused\n");
        }
    }
    if (event->stopChanged)
    {
        if (event->stop)
        {
            printf("Stopped\n");
        }
        else
        {
            printf("Started\n");
        }
    }
    if (event->speedChanged)
    {
        printf("Speed %dx\n", event->speed);
    }
    
    if (event->displayedFrameInfo.numTimecodes > 0)
    {
        printf("Displayed: ");
        for (i = 0; i < event->displayedFrameInfo.numTimecodes; i++)
        {
            print_timecode(event->displayedFrameInfo.timecodes[i].timecodeType, 
                event->displayedFrameInfo.timecodes[i].timecodeSubType, 
                &event->displayedFrameInfo.timecodes[i].timecode);
            printf(" ");
        }
        printf("\n");
    }
}

static void csm_end_of_source_event(void* data, const FrameInfo* lastReadFrameInfo)
{
    int i;
    
    printf("End of source reached: ");
    for (i = 0; i < lastReadFrameInfo->numTimecodes; i++)
    {
        print_timecode(lastReadFrameInfo->timecodes[i].timecodeType, lastReadFrameInfo->timecodes[i].timecodeSubType, 
            &lastReadFrameInfo->timecodes[i].timecode);
        printf(" ");
    }
    printf("\n");
}

static void csm_start_of_source_event(void* data, const FrameInfo* firstReadFrameInfo)
{
    int i;
    
    printf("Start of source reached: ");
    for (i = 0; i < firstReadFrameInfo->numTimecodes; i++)
    {
        print_timecode(firstReadFrameInfo->timecodes[i].timecodeType, firstReadFrameInfo->timecodes[i].timecodeSubType, 
            &firstReadFrameInfo->timecodes[i].timecode);
        printf(" ");
    }
    printf("\n");
}

static void csm_player_closed(void* data)
{
    //ConsoleMonitor* monitor = (ConsoleMonitor*)data;

    printf("Player was closed\n");
}


int csm_open(MediaPlayer* player, ConsoleMonitor** monitor)
{
    ConsoleMonitor* newMonitor = NULL;
    
    CALLOC_ORET(newMonitor, ConsoleMonitor, 1);
    
    newMonitor->lastDroppedFramePosition = -1;

    newMonitor->playerListener.data = newMonitor;
    newMonitor->playerListener.frame_displayed_event = csm_frame_displayed_event;
    newMonitor->playerListener.frame_dropped_event = csm_frame_dropped_event;
    newMonitor->playerListener.state_change_event = csm_state_change_event;
    newMonitor->playerListener.end_of_source_event = csm_end_of_source_event;
    newMonitor->playerListener.start_of_source_event = csm_start_of_source_event;
    newMonitor->playerListener.player_closed = csm_player_closed;
    if (!ply_register_player_listener(player, &newMonitor->playerListener))
    {
        ml_log_error("Failed to register console monitor as player listener\n");
        goto fail;
    }
    
    *monitor = newMonitor;
    return 1;
    
fail:
    csm_close(&newMonitor);
    return 0;
}

void csm_close(ConsoleMonitor** monitor)
{
    if (*monitor == NULL)
    {
        return;
    }
    
    SAFE_FREE(monitor);
}


