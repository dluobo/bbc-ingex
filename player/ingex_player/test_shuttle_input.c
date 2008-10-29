/*
 * $Id: test_shuttle_input.c,v 1.2 2008/10/29 17:47:42 john_f Exp $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "shuttle_input.h"


#define CHECK_FATAL(cond) \
    if (!(cond)) \
    { \
        fprintf(stderr, "%s failed in %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    }

    
static void listener(void* data, ShuttleEvent* event)
{
    switch (event->type)
    {
        case SH_KEY_EVENT:
            printf("Key: number = %u, is pressed = %d\n", event->value.key.number, 
                event->value.key.isPressed); 
            break;
        case SH_SHUTTLE_EVENT:
            printf("Shuttle: is clockwise = %d, speed = %u\n", event->value.shuttle.clockwise, 
                event->value.shuttle.speed); 
            break;
        case SH_JOG_EVENT:
            printf("Jog: is clockwise = %d, position = %d\n", event->value.jog.clockwise,
                 event->value.jog.position); 
            break;
        default:
            fprintf(stderr, "Unknown event type %d\n", event->type);
    }
}
    
    
int main (int argc, const char** argv)
{
    ShuttleInput* shuttle;

    
    CHECK_FATAL(shj_open_shuttle(&shuttle));
    CHECK_FATAL(register_listener(shuttle, listener, NULL));
    
    shj_start_shuttle(shuttle);
    
    shj_close_shuttle(&shuttle);
    
    
    return 0;
}


