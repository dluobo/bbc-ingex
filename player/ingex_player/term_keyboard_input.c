/*
 * $Id: term_keyboard_input.c,v 1.2 2008/10/29 17:47:42 john_f Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>	
#include <termios.h>

#include <X11/keysym.h>

#include "term_keyboard_input.h"
#include "logging.h"
#include "utils.h"
#include "macros.h"


struct TermKeyboardInput
{
    KeyboardInput input;
    KeyboardInputListener* listener;
    struct termios tio_orig;
    int stopped;
    pthread_mutex_t listenerMutex;
};
    
    

static void tki_set_listener(void* data, KeyboardInputListener* listener)
{
    TermKeyboardInput* termInput = (TermKeyboardInput*)data;

    PTHREAD_MUTEX_LOCK(&termInput->listenerMutex);
    termInput->listener = listener;
    PTHREAD_MUTEX_UNLOCK(&termInput->listenerMutex);
}

static void tki_unset_listener(void* data)
{
    TermKeyboardInput* termInput = (TermKeyboardInput*)data;

    PTHREAD_MUTEX_LOCK(&termInput->listenerMutex);
    termInput->listener = NULL;
    PTHREAD_MUTEX_UNLOCK(&termInput->listenerMutex);
}

static void tki_close(void* data)
{
    TermKeyboardInput* termInput = (TermKeyboardInput*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    /* reset original attributes */
    tcsetattr(0, TCSANOW, &termInput->tio_orig);
    
	destroy_mutex(&termInput->listenerMutex);
    
    SAFE_FREE(&termInput);
}


int tki_create_term_keyboard(TermKeyboardInput** termInput)
{
    TermKeyboardInput* newInput;
    struct termios tio_new;

    CALLOC_ORET(newInput, TermKeyboardInput, 1);

    newInput->input.data = newInput;    
    newInput->input.set_listener = tki_set_listener;    
    newInput->input.unset_listener = tki_unset_listener;    
    newInput->input.close = tki_close;    

    
    /* save attributes for restoring later */
    tcgetattr(STDIN_FILENO, &newInput->tio_orig);
    
    /* set new attributes */
    /* VMIN=1 and VTIME=0 means that the read func returns when at least 1 
    character is present */
    tio_new = newInput->tio_orig;
    tio_new.c_lflag &= ~(ICANON|ECHO);
    tio_new.c_cc[VMIN] = 1;
    tio_new.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio_new);

    CHK_OFAIL(init_mutex(&newInput->listenerMutex));
    
    *termInput = newInput;
    return 1;
    
fail:
    tki_close(newInput);
    return 0;
}

KeyboardInput* tki_get_keyboard_input(TermKeyboardInput* termInput)
{
    return &termInput->input;
}

/* read() sometimes returns partial escape sequences on some terminals
so we concatenate the next read() for the escape sequences that we know about */
void tki_start_term_keyboard(TermKeyboardInput* termInput)
{
    char buf[256];
    ssize_t numRead;
    char* bufPtr;
    fd_set rfds;
    struct timeval tv;
    int retval;

    bufPtr = buf;
    while (!termInput->stopped)
    {
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 50000; /* 1/20 second */
            
        retval = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);

        if (termInput->stopped)
        {
            /* thread stop was signalled so break out of the loop to allow join */
            break;
        }
        else if (retval)
        {
            numRead = read(STDIN_FILENO, bufPtr, 100);
            if (numRead == 0)
            {
                continue;
            }
            numRead += bufPtr - buf;

            if ((numRead == 1 && buf[0] == 27) || /* "ESC" */
                (numRead == 2 && buf[0] == 27 && buf[1] == 91) ||  /* "ESC[" */
                (numRead == 3 && buf[0] == 27 && buf[1] == 91 && buf[2] == 54) || /* "ESC[T" */
                (numRead == 3 && buf[0] == 27 && buf[1] == 91 && buf[2] == 53)) /* "ESC[S" */
            {
                /* wait for the remaining characters in the escape sequence */
                bufPtr = buf + numRead;
                continue;
            }
            bufPtr = buf;
            
            PTHREAD_MUTEX_LOCK(&termInput->listenerMutex);
            if (numRead == 1)
            {
                if (buf[0] == 10) /* return */
                {
                    kil_key_pressed(termInput->listener, XK_Return);
                    kil_key_released(termInput->listener, XK_Return);
                }
                else
                {
                    kil_key_pressed(termInput->listener, buf[0]);
                    kil_key_released(termInput->listener, buf[0]);
                }
            }
            else if (numRead == 3)
            {
                if (buf[0] == 27 && buf[1] == 91 && buf[2] == 67) /* right */
                {
                    kil_key_pressed(termInput->listener, XK_Right);
                    kil_key_released(termInput->listener, XK_Right);
                }
                else if (buf[0] == 27 && buf[1] == 91 && buf[2] == 68) /* left */
                {
                    kil_key_pressed(termInput->listener, XK_Left);
                    kil_key_released(termInput->listener, XK_Left);
                }
                else if (buf[0] == 27 && buf[1] == 91 && buf[2] == 65) /* up */
                {
                    kil_key_pressed(termInput->listener, XK_Up);
                    kil_key_released(termInput->listener, XK_Up);
                }
                else if (buf[0] == 27 && buf[1] == 91 && buf[2] == 66) /* down */
                {
                    kil_key_pressed(termInput->listener, XK_Down);
                    kil_key_released(termInput->listener, XK_Down);
                }
                else if (buf[0] == 27 && buf[1] == 91 && buf[2] == 72) /* home */
                {
                    kil_key_pressed(termInput->listener, XK_Home);
                    kil_key_released(termInput->listener, XK_Home);
                }
                else if (buf[0] == 27 && buf[1] == 91 && buf[2] == 70) /* end */
                {
                    kil_key_pressed(termInput->listener, XK_End);
                    kil_key_released(termInput->listener, XK_End);
                }
            }
            else if (numRead == 4)
            {
                if (buf[0] == 27 && buf[1] == 91 && buf[2] == 51 && buf[3] == 126) /* delete */
                {
                    kil_key_pressed(termInput->listener, XK_Delete);
                    kil_key_released(termInput->listener, XK_Delete);
                }
                else if (buf[0] == 27 && buf[1] == 91 && buf[2] == 53 && buf[3] == 126) /* page up */
                {
                    kil_key_pressed(termInput->listener, XK_Page_Up);
                    kil_key_released(termInput->listener, XK_Page_Up);
                }
                else if (buf[0] == 27 && buf[1] == 91 && buf[2] == 54 && buf[3] == 126) /* page down */
                {
                    kil_key_pressed(termInput->listener, XK_Page_Down);
                    kil_key_released(termInput->listener, XK_Page_Down);
                }
            }
            PTHREAD_MUTEX_UNLOCK(&termInput->listenerMutex);
        }
    }

    /* just in case free_term_keyboard_connect is not called we reset to the original attributes */
    tcsetattr(0, TCSANOW, &termInput->tio_orig);
}

void tki_restore_term_settings(TermKeyboardInput* termInput)
{
    if (termInput == NULL)
    {
        return;
    }

    /* reset original attributes */
    tcsetattr(0, TCSANOW, &termInput->tio_orig);
}

void tki_stop_term_keyboard(void* arg)
{
    TermKeyboardInput* termInput = (TermKeyboardInput*)arg;
    
    termInput->stopped = 1;
}

