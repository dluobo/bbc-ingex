/*
 * $Id: x11_xv_display_sink.h,v 1.7 2008/11/06 19:56:56 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham, <stuart_hc@users.sourceforge.net>
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

#ifndef __X11_XV_DISPLAY_SINK_H__
#define __X11_XV_DISPLAY_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"
#include "media_control.h"
#include "x11_common.h"
#include "video_switch_sink.h"


/* X11 XV display sink */

typedef struct X11XVDisplaySink X11XVDisplaySink;

int xvsk_check_is_available();

int xvsk_open(int reviewDuration, int disableOSD, const Rational* pixelAspectRatio, 
    const Rational* monitorAspectRatio, float scale, int swScale, X11WindowInfo* windowInfo, X11XVDisplaySink** sink);
void xvsk_set_media_control(X11XVDisplaySink* sink, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control);
void xvsk_unset_media_control(X11XVDisplaySink* sink);
MediaSink* xvsk_get_media_sink(X11XVDisplaySink* sink);

void xvsk_set_window_name(X11XVDisplaySink* sink, const char* name);

void xvsk_register_window_listener(X11XVDisplaySink* sink, X11WindowListener* listener);
void xvsk_unregister_window_listener(X11XVDisplaySink* sink, X11WindowListener* listener);

void xvsk_register_keyboard_listener(X11XVDisplaySink* sink, KeyboardInputListener* listener);
void xvsk_unregister_keyboard_listener(X11XVDisplaySink* sink, KeyboardInputListener* listener);

void xvsk_register_progress_bar_listener(X11XVDisplaySink* sink, ProgressBarInputListener* listener);
void xvsk_unregister_progress_bar_listener(X11XVDisplaySink* sink, ProgressBarInputListener* listener);

void xvsk_register_mouse_listener(X11XVDisplaySink* sink, MouseInputListener* listener);
void xvsk_unregister_mouse_listener(X11XVDisplaySink* sink, MouseInputListener* listener);



#ifdef __cplusplus
}
#endif


#endif

