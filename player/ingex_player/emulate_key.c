/*
 * $Id: emulate_key.c,v 1.4 2009/01/29 07:10:26 stuart_hc Exp $
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

/* originally code and ideas were used from XFakeKey, produced by Adam Pierce (http://www.doctort.org/adam/) */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include "emulate_key.h"
#include "logging.h"
#include "macros.h"


struct EmulateKey
{
    Display* display;
    Window rootWin;
};


static int send_key(EmulateKey* emu, int down, int keysym, int modifiers)
{
#if 0

    XKeyEvent event;
    Window winFocus;
    int revert;

    /* get window to send event to */
    XGetInputFocus(emu->display, &winFocus, &revert);

    /* create event */
    memset(&event, 0, sizeof(XKeyEvent));
    event.type = (down) ? KeyPress : KeyRelease;
    /* event.serial set by XSendEvent */
    /* event.send_event set by XSendEvent */
    event.display = emu->display;
    event.window = winFocus;
    event.root = emu->rootWin;
    event.subwindow = None;
    event.time = CurrentTime;
    event.x = 1;
    event.y = 1;
    event.x_root = 1;
    event.y_root = 1;
    event.state = modifiers;
    event.keycode = XKeysymToKeycode(emu->display, keysym);
    event.same_screen = True;

    /* send event */
    int result = XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent*)&event);

    fprintf(stderr, "%d\n", event.keycode);
#else

    int result = XTestFakeKeyEvent(emu->display, XKeysymToKeycode(emu->display, keysym), down ? True : False, CurrentTime);

#endif

    return result != 0;
}



int create_emu(EmulateKey** emu)
{
    EmulateKey* newEmu;

    if ((newEmu = (EmulateKey*)malloc(sizeof(EmulateKey))) == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        return 0;
    }
    memset(newEmu, 0, sizeof(EmulateKey));


    if ((newEmu->display = XOpenDisplay(0)) == NULL)
    {
        fprintf(stderr, "Failed to open display\n");
        SAFE_FREE(&newEmu);
        return 0;
    }
    newEmu->rootWin = XDefaultRootWindow(newEmu->display);

    *emu = newEmu;
    return 1;
}

int emu_key_down(EmulateKey* emu, int keysym, int modifier)
{
    return send_key(emu, 1, keysym, modifier);
}

int emu_key_up(EmulateKey* emu, int keysym, int modifier)
{
    return send_key(emu, 0, keysym, modifier);
}

int emu_key(EmulateKey* emu, int keysym, int modifier)
{
    return emu_key_down(emu, keysym, modifier) && emu_key_up(emu, keysym, modifier);
}

void free_emu(EmulateKey** emu)
{
    if (*emu == NULL)
    {
        return;
    }

    if ((*emu)->display != NULL)
    {
        XCloseDisplay((*emu)->display);
    }

    SAFE_FREE(emu);
}

