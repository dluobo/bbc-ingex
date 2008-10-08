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


int x11c_initialise(X11Common* x11Common, int reviewDuration, OnScreenDisplay* osd, X11PluginWindowInfo *pluginInfo)
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

    if (pluginInfo)
    {
        x11Common->pluginInfo.pluginDisplay = pluginInfo->pluginDisplay;
        x11Common->pluginInfo.pluginWindow = pluginInfo->pluginWindow;
    }
    else
    {
        x11Common->pluginInfo.pluginDisplay = NULL;
        x11Common->pluginInfo.pluginWindow = 0;
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

    if (x11Common->display != NULL)
    {
        if (x11Common->gc != None)
        {
            XFreeGC(x11Common->display, x11Common->gc);
        }
        XCloseDisplay(x11Common->display);
        x11Common->display = NULL; /* this will stop any attempts to process events */
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



int x11c_open_display(X11Common* x11Common)
{
    if (x11Common->display != NULL)
    {
        return 1;
    }

    if (x11Common->pluginInfo.pluginDisplay != NULL)
    {
        x11Common->display = x11Common->pluginInfo.pluginDisplay;
    }
    else
    {
        if ((x11Common->display = XOpenDisplay(NULL)) == NULL)
        {
            ml_log_error("Cannot open Display.\n");
            return 0;
        }
    }
    return 1;
}

int x11c_get_screen_dimensions(X11Common* x11Common, int* width, int* height)
{
    if (x11Common->display == NULL)
    {
        ml_log_error("Can't get screen dimensions because display has not been opened\n");
        return 0;
    }
    
    *width = XWidthOfScreen(XDefaultScreenOfDisplay(x11Common->display));
    *height = XHeightOfScreen(XDefaultScreenOfDisplay(x11Common->display));
    
    return 1;
}

int x11c_create_window(X11Common* x11Common, unsigned int displayWidth, unsigned int displayHeight,
    unsigned int imageWidth, unsigned int imageHeight)
{
    XSetWindowAttributes x_attr;
    XEvent event;
    
    if (x11Common->haveWindow)
    {
        return 1;
    }
    
    if (x11Common->display == NULL)
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

    x_attr.border_pixel = BlackPixel(x11Common->display, DefaultScreen(x11Common->display));
    x_attr.background_pixel = BlackPixel(x11Common->display, DefaultScreen(x11Common->display));
    x_attr.backing_store = Always;
    x_attr.event_mask = ExposureMask | StructureNotifyMask;

    Window embedWindowId = 0;
    if (x11Common->pluginInfo.pluginWindow != 0)
    {
        embedWindowId = x11Common->pluginInfo.pluginWindow;
    }

    if (embedWindowId)
    {
        /* embedded window should already exist so just use it */
        x11Common->window = embedWindowId;
    }
    else
    {
        /* create a new X11 window  */
        x11Common->window = XCreateWindow(
            x11Common->display, 
            DefaultRootWindow(x11Common->display),
            0, 0, 
            displayWidth, displayHeight,
            0, 
            DefaultDepth(x11Common->display, DefaultScreen(x11Common->display)), 
            InputOutput, 
            CopyFromParent,
            CWBackingStore | CWBackPixel | CWBorderPixel | CWEventMask, &x_attr);
    }

    XStoreName(x11Common->display, x11Common->window, x11Common->windowName);

    /* XSelectInput will fail with BadAccess is more than one client
     * is trying to get ButtonPressMask and a few other event types */
    XSelectInput(x11Common->display, x11Common->window, 
        ExposureMask | StructureNotifyMask | FocusChangeMask | 
        KeyPressMask | KeyReleaseMask |
        ButtonPressMask | Button1MotionMask);

    /* we want to known when the window is deleted */
    x11Common->deleteAtom = XInternAtom(x11Common->display, "WM_DELETE_WINDOW", True);
    if (x11Common->deleteAtom == None ||
        XSetWMProtocols(x11Common->display, x11Common->window, &x11Common->deleteAtom, 1) == 0)
    {
        ml_log_warn("Failed to register interest in X11 window closing event\n");
    }

    if (embedWindowId == 0)    /* not using embedded window */
    {
        XMapWindow(x11Common->display, x11Common->window);
        /* Wait until window is mapped */
        do 
        {
            XNextEvent(x11Common->display, &event);
        } 
        while (event.type != MapNotify || event.xmap.event != x11Common->window);
    }

    x11Common->gc = XCreateGC(x11Common->display, x11Common->window, 0, 0);
    
    XWindowAttributes attrs;
    XGetWindowAttributes(x11Common->display, x11Common->window, &attrs);
    x11Common->windowWidth = attrs.width;
    x11Common->windowHeight = attrs.height;
    
    x11Common->haveWindow = 1;
    return 1;
}

void x11c_set_media_control(X11Common* x11Common, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control)
{
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex) /* wait until events have been processed */
    
    if (x11Common->display != NULL)
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

    
    PTHREAD_MUTEX_LOCK(&x11Common->eventMutex)

    /* NOTE: if there isn't a window (haveWindow == 1), then a sync problem could
    occur. Using valgrind exposes te problem */
    if (x11Common->display == NULL || !x11Common->haveWindow)
    {
        goto fail;
    }

    
    gettimeofday(&x11Common->processEventTime, NULL);
    
    if (!XPending(x11Common->display))
    {
        if (sync)
        {
            /* wait until image has been processed by the X server */
            XSync(x11Common->display, False);
        }
        
        processedEvent = 0;
    }
    else
    {
        do
        {
            XNextEvent(x11Common->display, &event);
            
            if (event.type == FocusOut || event.type == FocusIn || event.type == Expose)
            {
                msl_refresh_required(x11Common->sinkListener);
            }
            else if (event.type == KeyPress)
            {
                kil_key_pressed(x11Common->keyboardListener, XLookupKeysym((XKeyEvent*)&event, 0));
                kil_key_pressed(x11Common->separateKeyboardListener, XLookupKeysym((XKeyEvent*)&event, 0));
            }
            else if (event.type == KeyRelease)
            {
                kil_key_released(x11Common->keyboardListener, XLookupKeysym((XKeyEvent*)&event, 0));
                kil_key_released(x11Common->separateKeyboardListener, XLookupKeysym((XKeyEvent*)&event, 0));
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
                if ((Atom)event.xclient.data.l[0] == x11Common->deleteAtom)
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
        while (XPending(x11Common->display));
        
        if (sync)
        {
            /* wait until image has been processed by the X server */
            XSync(x11Common->display, False);
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
    if (x11Common->display != NULL && x11Common->haveWindow)
    {
        XStoreName(x11Common->display, x11Common->window, x11Common->windowName);
    }
    
    return 1;
}

int x11c_shared_memory_available(X11Common* common)
{
    char* displayName;
    
    if (common == NULL || common->display == NULL)
    {
        return 0;
    }
    
    if (!XShmQueryExtension(common->display))
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

