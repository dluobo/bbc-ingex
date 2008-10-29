/*
 * $Id: ffmpeg_shuttle_emu.c,v 1.2 2008/10/29 17:47:41 john_f Exp $
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

#include <X11/X.h>
#include <X11/keysym.h>

#include "emulate_key.h"
#include "shuttle_input.h"


#define CHECK_FATAL(cond) \
    if (!(cond)) \
    { \
        fprintf(stderr, "%s failed in %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    }

#define CHECK(cond) \
    if (!(cond)) \
    { \
        fprintf(stderr, "%s failed in %s:%d\n", #cond, __FILE__, __LINE__); \
    }

    
static void listener(void* data, ShuttleEvent* event)
{
    EmulateKey* emu = (EmulateKey*)data;
    
    switch (event->type)
    {
        case SH_KEY_EVENT:
            switch (event->value.key.number)
            {
                case 1: /* quit */
                    if (event->value.key.isPressed)
                    {
                        CHECK(emu_key_down(emu, 'q', 0));
                    }
                    else
                    {
                        CHECK(emu_key_up(emu, 'q', 0));
                    }
                    break;
                case 2: /* pause */
                    if (event->value.key.isPressed)
                    {
                        CHECK(emu_key_down(emu, 'p', 0));
                    }
                    else
                    {
                        CHECK(emu_key_up(emu, 'p', 0));
                    }
                    break;
            }
            break;
            
        case SH_SHUTTLE_EVENT:
            if (event->value.shuttle.clockwise)
            {
                /* seek forward 1 minute */
                CHECK(emu_key(emu, XK_Up, 0));
            }
            else
            {
                /* seek back 1 minute */
                CHECK(emu_key(emu, XK_Down, 0));
            }
            break;
            
        case SH_JOG_EVENT:
            if (event->value.jog.clockwise)
            {
                /* seek forward 10 seconds */
                CHECK(emu_key(emu, XK_Right, 0));
            }
            else
            {
                /* seek back 10 seconds */
                CHECK(emu_key(emu, XK_Left, 0));
            }
            break;
    }
}
    
    
int main (int argc, const char** argv)
{
    ShuttleInput* shuttle;
    EmulateKey* emu;

    
    CHECK_FATAL(create_emu(&emu));
    
    CHECK_FATAL(shj_open_shuttle(&shuttle));
    CHECK_FATAL(register_listener(shuttle, listener, (void *)emu));
    
    shj_start_shuttle(shuttle);
    
    shj_close_shuttle(&shuttle);
    free_emu(&emu);
    
    
    return 0;
}

