/*
 * $Id: x11_display_sink.h,v 1.8 2009/01/29 07:10:27 stuart_hc Exp $
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

#ifndef __X11_DISPLAY_SINK_H__
#define __X11_DISPLAY_SINK_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "media_sink.h"
#include "media_control.h"
#include "x11_common.h"
#include "video_switch_sink.h"


/* X11 display sink */

typedef struct X11DisplaySink X11DisplaySink;


int xsk_open(int reviewDuration, int disableOSD, const Rational* pixelAspectRatio,
    const Rational* monitorAspectRatio, float scale, int swScale, X11WindowInfo* windowInfo, X11DisplaySink** sink);
void xsk_set_media_control(X11DisplaySink* sink, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control);
void xsk_unset_media_control(X11DisplaySink* sink);
MediaSink* xsk_get_media_sink(X11DisplaySink* sink);

void xsk_set_window_name(X11DisplaySink* sink, const char* name);

void xsk_register_window_listener(X11DisplaySink* sink, X11WindowListener* listener);
void xsk_unregister_window_listener(X11DisplaySink* sink, X11WindowListener* listener);

void xsk_register_keyboard_listener(X11DisplaySink* sink, KeyboardInputListener* listener);
void xsk_unregister_keyboard_listener(X11DisplaySink* sink, KeyboardInputListener* listener);

void xsk_register_progress_bar_listener(X11DisplaySink* sink, ProgressBarInputListener* listener);
void xsk_unregister_progress_bar_listener(X11DisplaySink* sink, ProgressBarInputListener* listener);

void xsk_register_mouse_listener(X11DisplaySink* sink, MouseInputListener* listener);
void xsk_unregister_mouse_listener(X11DisplaySink* sink, MouseInputListener* listener);


#ifdef __cplusplus
}
#endif


#endif

