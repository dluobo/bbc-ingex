/*
 * $Id: x11_common.c,v 1.7 2008/11/07 14:24:55 philipn Exp $
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
#include <unistd.h>

#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "x11_common.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


/* process events at least every 1.5 frames at 25fps */
#define MIN_PROCESS_EVENT_INTERVAL      60000


/* make sure we process events regularly, at least every MIN_PROCESS_EVENT_INTERVAL useconds */
static void* process_event_thread(void* arg)
{
    X11Common* x11Common = (X11Common*)arg;
    struct timeval now;
    struct timeval last;
    long diff_usec;
    
    memset(&last, 0, sizeof(last));

    while (!x11Common->stopped)
    {
        gettimeofday(&now, NULL);
        last = x11Common->processEventTime;
        
        diff_usec = (now.tv_sec - last.tv_sec) * 1000000 + now.tv_usec - last.tv_usec;

        if (diff_usec > MIN_PROCESS_EVENT_INTERVAL)
        {
            x11c_process_events(x11Common, 0);
        }
        else if (diff_usec > 0)
        {
            usleep(MIN_PROCESS_EVENT_INTERVAL - diff_usec);
        }
    }
    
    pthread_exit((void*) 0);
}


static void x11c_set_pbar_listener(void* data, ProgressBarInputListener* listener)
{
    X11Common* x11Common = (X11Common*)data;

    x11Common->progressBarListener = listener;
}

static void x11c_unset_pbar_listener(void* data)
{
    X11Common* x11Common = (X11Common*)data;

    x11Common->progressBarListener = NULL;
}

static void x11c_close_pbar(void* data)
{
    X11Common* x11Common = (X11Common*)data;
    
    if (x11Common == NULL)
    {
        return;
    }
    
    x11Common->progressBarListener = NULL;
}


static void x11c_set_keyboard_listener(void* data, KeyboardInputListener* listener)
{
    X11Common* x11Common = (X11Common*)data;

    x11Common->keyboardListener = listener;
}

static void x11c_unset_keyboard_listener(void* data)
{
    X11Common* x11Common = (X11Common*)data;

    x11Common->keyboardListener = NULL;
}

static void x11c_close_keyboard(void* data)
{
    X11Common* x11Common = (X11Common*)data;
    
    if (x11Common == NULL)
    {
        return;
    }
    
    x11Common->keyboardListener = NULL;
}


static void x11c_set_mouse_listener(void* data, MouseInputListener* listener)
{
    X11Common* x11Common = (X11Common*)data;

    x11Common->mouseListener = listener;
}

static void x11c_unset_mouse_listener(void* data)
{
    X11Common* x11Common = (X11Common*)data;

    x11Common->mouseListener = NULL;
}

static void x11c_close_mouse(void* data)
{
    X11Common* x11Common = (X11Common*)data;
    
    if (x11Common == NULL)
    {
        return;
    }
    
    x11Common->mouseListener = NULL;
}


void x11wl_close_request(X11WindowListener* listener)
{
    if (listener && listener->close_request)
    {
        listener->close_request(listener->data);
    }
}


int x11c_initialise(X11Common* x11Common, int reviewDuration, OnScreenDisplay* osd, X11WindowInfo* windowInfo)
{
    memset(x11Common, 0, sizeof(x11Common));
    
    x11Common->reviewDuration = reviewDuration;
    x11Common->osd = osd;
    
    x11Common->progressBarInput.data = x11Common;
    x11Common->progressBarInput.set_listener = x11c_set_pbar_listener;
    x11Common->progressBarInput.unset_listener = x11c_unset_pbar_listener;
    x11Common->progressBarInput.close = x11c_close_pbar;
    
    x11Common->keyboardInput.data = x11Common;
    x11Common->keyboardInput.set_listener = x11c_set_keyboard_listener;
    x11Common->keyboardInput.unset_listener = x11c_unset_keyboard_listener;
    x11Common->keyboardInput.close = x11c_close_keyboard;

    x11Common->mouseInput.data = x11Common;
    x11Common->mouseInput.set_listener = x11c_set_mouse_listener;
    x11Common->mouseInput.unset_listener = x11c_unset_mouse_listener;
    x11Common->mouseInput.close = x11c_close_mouse;

    if (windowInfo)
    {
        x11Common->windowInfo = *windowInfo;
    }

    CHK_ORET(init_mutex(&x11Common->eventMutex));

    CHK_ORET(create_joinable_thread(&x11Common->processEventThreadId, process_event_thread, x11Common));
    
    return 1;
}

void x11c_clear(X11Common* x11Common)
{
    x11Common->stopped = 1;
    join_thread(&x11Common->processEventThreadId, NULL, NULL);

    
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* don't allow events to be processed */
    
    kic_free_keyboard_connect(&x11Common->keyboardConnect);
    pic_free_progress_bar_connect(&x11Common->progressBarConnect);
    mic_free_mouse_connect(&x11Common->mouseConnect);

    if (x11Common->createdWindowInfo)
    {
        x11c_close_window(&x11Common->windowInfo);
        x11Common->createdWindowInfo = 0;
    }
    
    SAFE_FREE(&x11Common->windowName);
    
    x11Common->osd = NULL;

    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
    
    
    destroy_mutex(&x11Common->eventMutex);
    
    memset(x11Common, 0, sizeof(x11Common));
}

int x11c_reset(X11Common* x11Common)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    kic_free_keyboard_connect(&x11Common->keyboardConnect);
    pic_free_progress_bar_connect(&x11Common->progressBarConnect);
    mic_free_mouse_connect(&x11Common->mouseConnect);
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
    
    return 1;
}

void x11c_register_window_listener(X11Common* x11Common, X11WindowListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    x11Common->windowListener = listener;
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_unregister_window_listener(X11Common* x11Common, X11WindowListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    if (x11Common->windowListener == listener)
    {
        x11Common->windowListener = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_register_keyboard_listener(X11Common* x11Common, KeyboardInputListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    x11Common->separateKeyboardListener = listener;
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_unregister_keyboard_listener(X11Common* x11Common, KeyboardInputListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    if (x11Common->separateKeyboardListener == listener)
    {
        x11Common->separateKeyboardListener = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_register_progress_bar_listener(X11Common* x11Common, ProgressBarInputListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    x11Common->separateProgressBarListener = listener;
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_unregister_progress_bar_listener(X11Common* x11Common, ProgressBarInputListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    if (x11Common->separateProgressBarListener == listener)
    {
        x11Common->separateProgressBarListener = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_register_mouse_listener(X11Common* x11Common, MouseInputListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    x11Common->separateMouseListener = listener;
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_unregister_mouse_listener(X11Common* x11Common, MouseInputListener* listener)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    if (x11Common->separateMouseListener == listener)
    {
        x11Common->separateMouseListener = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}



int x11c_prepare_display(X11Common* x11Common)
{
    if (x11Common->windowInfo.display != NULL)
    {
        return 1;
    }
    
    return x11c_open_display(&x11Common->windowInfo.display);
}

int x11c_get_screen_dimensions(X11Common* x11Common, int* width, int* height)
{
    if (x11Common->windowInfo.display == NULL)
    {
        ml_log_error("Can't get screen dimensions because display has not been opened\n");
        return 0;
    }
    
    *width = XWidthOfScreen(XDefaultScreenOfDisplay(x11Common->windowInfo.display));
    *height = XHeightOfScreen(XDefaultScreenOfDisplay(x11Common->windowInfo.display));
    
    return 1;
}

int x11c_init_window(X11Common* x11Common, unsigned int displayWidth, unsigned int displayHeight,
    unsigned int imageWidth, unsigned int imageHeight)
{
    if (x11Common->haveWindow)
    {
        return 1;
    }
    
    if (x11Common->windowInfo.display == NULL)
    {
        ml_log_error("Can't create X11 window because display has not been opened\n");
        return 0;
    }
    
    x11Common->displayWidth = displayWidth;
    x11Common->displayHeight = displayHeight;
    x11Common->imageWidth = imageWidth;
    x11Common->imageHeight = imageHeight;
    
    if (x11Common->windowName == NULL)
    {
        x11c_set_window_name(x11Common, NULL);
    }

    if (x11Common->windowInfo.window == 0)
    {
        x11c_create_window(&x11Common->windowInfo, displayWidth, displayHeight, x11Common->windowName);
        x11Common->createdWindowInfo = 1;
    }
    else
    {
        x11c_update_window(&x11Common->windowInfo, displayWidth, displayHeight, x11Common->windowName);
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(x11Common->windowInfo.display, x11Common->windowInfo.window, &attrs);
    x11Common->windowWidth = attrs.width;
    x11Common->windowHeight = attrs.height;
    
    x11Common->haveWindow = 1;
    return 1;
}

void x11c_set_media_control(X11Common* x11Common, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    
    if (x11Common->windowInfo.display != NULL)
    {
       if (!kic_create_keyboard_connect(x11Common->reviewDuration, control, 
           &x11Common->keyboardInput, mapping, &x11Common->keyboardConnect))
       {
           ml_log_warn("Failed to create X11 keyboard input connect\n"); 
       }
       if (!pic_create_progress_bar_connect(control, &x11Common->progressBarInput, 
           &x11Common->progressBarConnect))
       {
           ml_log_warn("Failed to create X11 progress bar input connect\n"); 
       }
       if (!mic_create_mouse_connect(control, videoSwitch, &x11Common->mouseInput, &x11Common->mouseConnect))
       {
           ml_log_warn("Failed to create X11 mouse input connect\n"); 
       }
    }
    else
    {
        ml_log_warn("Failed to connect X11 keyboard input because display not initialised\n"); 
    }

    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

void x11c_unset_media_control(X11Common* x11Common)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    
    kic_free_keyboard_connect(&x11Common->keyboardConnect);
    pic_free_progress_bar_connect(&x11Common->progressBarConnect);
    mic_free_mouse_connect(&x11Common->mouseConnect);
    
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
}

int x11c_process_events(X11Common* x11Common, int sync)
{
    XEvent event;
    int processedEvent = 0;
    float progressBarPosition = -1.0;
    int modifier;

    
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex)

    /* NOTE: if there isn't a window (haveWindow == 1), then a sync problem could
    occur. Using valgrind exposes te problem */
    if (x11Common->windowInfo.display == NULL || !x11Common->haveWindow)
    {
        goto fail;
    }

    
    gettimeofday(&x11Common->processEventTime, NULL);
    
    if (!XPending(x11Common->windowInfo.display))
    {
        if (sync)
        {
            /* wait until image has been processed by the X server */
            XSync(x11Common->windowInfo.display, False);
        }
        
        processedEvent = 0;
    }
    else
    {
        do
        {
            XNextEvent(x11Common->windowInfo.display, &event);
            
            if (event.type == FocusOut || event.type == FocusIn || event.type == Expose)
            {
                msl_refresh_required(x11Common->sinkListener);
            }
            else if (event.type == KeyPress)
            {
                modifier = 0;
                if (((XKeyEvent*)&event)->state & ShiftMask)
                {
                    modifier |= SHIFT_KEY_MODIFIER;
                }
                if (((XKeyEvent*)&event)->state & ControlMask)
                {
                    modifier |= CONTROL_KEY_MODIFIER;
                }
                if (((XKeyEvent*)&event)->state & Mod1Mask)
                {
                    modifier |= ALT_KEY_MODIFIER;
                }
                kil_key_pressed(x11Common->keyboardListener, XLookupKeysym((XKeyEvent*)&event, 0), modifier);
                kil_key_pressed(x11Common->separateKeyboardListener, XLookupKeysym((XKeyEvent*)&event, 0), modifier);
            }
            else if (event.type == KeyRelease)
            {
                modifier = 0;
                if (((XKeyEvent*)&event)->state & ShiftMask)
                {
                    modifier |= SHIFT_KEY_MODIFIER;
                }
                if (((XKeyEvent*)&event)->state & ControlMask)
                {
                    modifier |= CONTROL_KEY_MODIFIER;
                }
                if (((XKeyEvent*)&event)->state & Mod1Mask)
                {
                    modifier |= ALT_KEY_MODIFIER;
                }
                kil_key_released(x11Common->keyboardListener, XLookupKeysym((XKeyEvent*)&event, 0), modifier);
                kil_key_released(x11Common->separateKeyboardListener, XLookupKeysym((XKeyEvent*)&event, 0), modifier);
            }
            else if (event.type == ButtonPress)
            {
                if (((XButtonEvent*)&event)->button == 1)
                {
                    int xPos = (int)(((XButtonEvent*)&event)->x * x11Common->imageWidth / (float)x11Common->displayWidth + 0.5);
                    int yPos = (int)(((XButtonEvent*)&event)->y * x11Common->imageHeight / (float)x11Common->displayHeight + 0.5);
                    
                    progressBarPosition = osd_get_position_in_progress_bar(x11Common->osd, xPos, yPos);
                    if (progressBarPosition >= 0.0)
                    {
                        pil_position_set(x11Common->progressBarListener, progressBarPosition);
                        pil_position_set(x11Common->separateProgressBarListener, progressBarPosition);
                    }
                    else
                    {
                        mil_click(x11Common->mouseListener, x11Common->imageWidth, x11Common->imageHeight, xPos, yPos);
                        mil_click(x11Common->separateMouseListener, x11Common->imageWidth, x11Common->imageHeight, xPos, yPos);
                    }
                }
            }
            else if (event.type == MotionNotify)
            {
                int xPos = (int)(((XButtonEvent*)&event)->x * x11Common->imageWidth / (float)x11Common->displayWidth + 0.5);
                int yPos = (int)(((XButtonEvent*)&event)->y * x11Common->imageHeight / (float)x11Common->displayHeight + 0.5);
                
                progressBarPosition = osd_get_position_in_progress_bar(x11Common->osd, xPos, yPos); 
                if (progressBarPosition >= 0.0)
                {
                    pil_position_set(x11Common->progressBarListener, progressBarPosition);
                    pil_position_set(x11Common->separateProgressBarListener, progressBarPosition);
                }
                else
                {
                    mil_click(x11Common->mouseListener, x11Common->imageWidth, x11Common->imageHeight, xPos, yPos);
                    mil_click(x11Common->separateMouseListener, x11Common->imageWidth, x11Common->imageHeight, xPos, yPos);
                }
            }
            else if (event.type == ClientMessage)
            {
                if ((Atom)event.xclient.data.l[0] == x11Common->windowInfo.deleteAtom)
                {
                    x11wl_close_request(x11Common->windowListener);
                }
            }
            else if (event.type == ConfigureNotify)
            {
                x11Common->windowWidth = ((XConfigureEvent*)&event)->width;
                x11Common->windowHeight = ((XConfigureEvent*)&event)->height;
            }
        }
        while (XPending(x11Common->windowInfo.display));
        
        if (sync)
        {
            /* wait until image has been processed by the X server */
            XSync(x11Common->windowInfo.display, False);
        }
        
        processedEvent = 1;
    }
    
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
    
    return processedEvent;
    
fail:
    PTHREAD_MUTEX_UNLOCK(&x11Common->eventMutex)
    return 0;
}

int x11c_set_window_name(X11Common* x11Common, const char* name)
{
    char* newWindowName = NULL;
    char* oldWindowName;
    
    if (x11Common == NULL)
    {
        return 0;
    }
    
    /* store new name */
    if (name == NULL)
    {
        CALLOC_ORET(newWindowName, char, 1);
    }
    else
    {
        CALLOC_ORET(newWindowName, char, strlen(name) + 1);
        strcpy(newWindowName, name);
    }

    oldWindowName = x11Common->windowName;
    x11Common->windowName = newWindowName;
    SAFE_FREE(&oldWindowName);
    
    /* set name if we have a window */
    if (x11Common->windowInfo.display != NULL && x11Common->haveWindow)
    {
        XStoreName(x11Common->windowInfo.display, x11Common->windowInfo.window, x11Common->windowName);
    }
    
    return 1;
}

int x11c_shared_memory_available(X11Common* common)
{
    char* displayName;
    
    if (common == NULL || common->windowInfo.display == NULL)
    {
        return 0;
    }
    
    if (!XShmQueryExtension(common->windowInfo.display))
    {
        return 0;
    }
    
    displayName = XDisplayName(NULL);
    
    if (strncmp("unix", displayName, 4) == 0)
    {
        displayName += 4;
    }
    else if (strncmp("localhost", displayName, 9) == 0)
    {
        displayName += 9;
    }
    if (*displayName == ':' && atoi(displayName + 1) < 10)
    {
        return 1;
    }
    
    return 0;
}


int x11c_open_display(Display** display)
{
    Display* newDisplay = NULL;
    
    if (*display != NULL)
    {
        return 1;
    }
    
    newDisplay = XOpenDisplay(NULL);
    if (newDisplay == NULL)
    {
        ml_log_error("Cannot open Display.\n");
        return 0;
    }

    *display = newDisplay;
    return 1;
}

int x11c_create_window(X11WindowInfo* windowInfo, int displayWidth, int displayHeight, const char* windowName)
{
    XSetWindowAttributes x_attr;
    XEvent event;
    
    if (windowInfo->display == NULL)
    {
        ml_log_error("Can't create X11 window because display has not been opened\n");
        return 0;
    }
    if (windowInfo->window != 0)
    {
        ml_log_error("Can't create X11 window when one is already opened\n");
        return 0;
    }
    
    x_attr.border_pixel = BlackPixel(windowInfo->display, DefaultScreen(windowInfo->display));
    x_attr.background_pixel = BlackPixel(windowInfo->display, DefaultScreen(windowInfo->display));
    x_attr.backing_store = Always;
    x_attr.event_mask = ExposureMask | StructureNotifyMask;

    windowInfo->window = XCreateWindow(
        windowInfo->display, 
        DefaultRootWindow(windowInfo->display),
        0, 0, 
        displayWidth, displayHeight,
        0, 
        DefaultDepth(windowInfo->display, DefaultScreen(windowInfo->display)), 
        InputOutput, 
        CopyFromParent,
        CWBackingStore | CWBackPixel | CWBorderPixel | CWEventMask, &x_attr);

    XStoreName(windowInfo->display, windowInfo->window, windowName);

    /* XSelectInput will fail with BadAccess if more than one client
     * is trying to get ButtonPressMask and a few other event types */
    XSelectInput(windowInfo->display, windowInfo->window, 
        ExposureMask | StructureNotifyMask | FocusChangeMask | 
        KeyPressMask | KeyReleaseMask |
        ButtonPressMask | Button1MotionMask);

    /* we want to known when the window is deleted */
    windowInfo->deleteAtom = XInternAtom(windowInfo->display, "WM_DELETE_WINDOW", True);
    if (windowInfo->deleteAtom == 0 ||
        XSetWMProtocols(windowInfo->display, windowInfo->window, &windowInfo->deleteAtom, 1) == 0)
    {
        ml_log_warn("Failed to register interest in X11 window closing event\n");
    }

    XMapWindow(windowInfo->display, windowInfo->window);
    /* Wait until window is mapped */
    do 
    {
        XNextEvent(windowInfo->display, &event);
    } 
    while (event.type != MapNotify || event.xmap.event != windowInfo->window);


    windowInfo->gc = XCreateGC(windowInfo->display, windowInfo->window, 0, 0);
    
    
    return 1;
}

void x11c_update_window(X11WindowInfo* windowInfo, int displayWidth, int displayHeight, const char* windowName)
{
    if (windowInfo->display == NULL)
    {
        ml_log_error("Can't update X11 window because display has not been opened\n");
        return;
    }
    if (windowInfo->window == 0)
    {
        ml_log_error("Can't update X11 window because the window does not exist\n");
        return;
    }

    if (windowInfo->gc == 0)
    {
        windowInfo->gc = XCreateGC(windowInfo->display, windowInfo->window, 0, 0);
    }

    /* XSelectInput will fail with BadAccess if more than one client
     * is trying to get ButtonPressMask and a few other event types */
    XSelectInput(windowInfo->display, windowInfo->window, 
        ExposureMask | StructureNotifyMask | FocusChangeMask | 
        KeyPressMask | KeyReleaseMask |
        ButtonPressMask | Button1MotionMask);

    if (windowInfo->deleteAtom == 0)
    {
        /* we want to known when the window is deleted */
        windowInfo->deleteAtom = XInternAtom(windowInfo->display, "WM_DELETE_WINDOW", True);
        if (windowInfo->deleteAtom == 0 ||
            XSetWMProtocols(windowInfo->display, windowInfo->window, &windowInfo->deleteAtom, 1) == 0)
        {
            ml_log_warn("Failed to register interest in X11 window closing event\n");
        }
    }
    
    XStoreName(windowInfo->display, windowInfo->window, windowName);

    XResizeWindow(windowInfo->display, windowInfo->window, displayWidth, displayHeight);
}

void x11c_close_window(X11WindowInfo* windowInfo)
{
    if (windowInfo->display != NULL)
    {
        if (windowInfo->gc != 0)
        {
            XFreeGC(windowInfo->display, windowInfo->gc);
            windowInfo->gc = 0;
        }
        XCloseDisplay(windowInfo->display);
        windowInfo->display = NULL;
    }
    windowInfo->window = 0;
    windowInfo->deleteAtom = 0;
    windowInfo->gc = 0;
}

