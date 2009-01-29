/***************************************************************************
 *   $Id: jogshuttle.cpp,v 1.1 2009/01/29 07:45:26 stuart_hc Exp $           *
 *                                                                         *
 *   Copyright (C) 2006-2009 British Broadcasting Corporation              *
 *   - all rights reserved.                                                *
 *   Author: Matthew Marks                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/* Note - the jog/shuttle control will not work unless it is given non-root read access.
Refer to installation documentation for how to set up the host to provide this */

#include <wx/wx.h>
#include "jogshuttle.h"

DEFINE_EVENT_TYPE(wxEVT_JOGSHUTTLE_MESSAGE)

using namespace ingex; //jogshuttle stuff is in this namespace

/// @param The handler to accept events from this listener.
JSListener::JSListener(wxEvtHandler * evtHandler) : mEvtHandler(evtHandler)
{
	mButtonsPressed.Add(0 /* not pressed */, 16 /* 15 buttons but starting from index 1 */);
}

void JSListener::connected(JogShuttle *, JogShuttleDevice)
//void JSListener::connected(JogShuttle * jogShuttle, JogShuttleDevice device)
{
//	printf("Connected to '%s'\n", jogShuttle->getDeviceName(device).c_str());
}

/// Called when the jog/shuttle control is unplugged.
/// Sets all saved button states to unpressed.
void JSListener::disconnected(JogShuttle *, JogShuttleDevice)
//void JSListener::disconnected(JogShuttle * jogShuttle, JogShuttleDevice device)
{
	for (size_t i = 1; i < mButtonsPressed.GetCount(); i++) {
		mButtonsPressed[i] = 0;
	}
//	printf("Disconnected from '%s'\n", jogShuttle->getDeviceName(device).c_str());
}

void JSListener::ping(JogShuttle *)
{
	// do nothing
}

/// Called when the user presses a button on the jog/shuttle control.
/// Sends an event containing the button ID and notes the state change.
/// @param number The button ID.
void JSListener::buttonPressed(JogShuttle *, int number)
{
	if ((size_t) number < mButtonsPressed.GetCount()) { //sanity check
		mButtonsPressed[number] = 1;
	}
	wxCommandEvent event(wxEVT_JOGSHUTTLE_MESSAGE, JS_BUTTON_PRESSED);
	event.SetInt(number);
	mEvtHandler->AddPendingEvent(event);
}

/// Called when the user releases a button on the jog/shuttle control.
/// Sends an event containing the button ID and notes the state change.
/// @param number The button ID.
void JSListener::buttonReleased(JogShuttle *, int number)
{
	if ((size_t) number < mButtonsPressed.GetCount()) { //sanity check
		mButtonsPressed[number] = 0;
	}
	wxCommandEvent event(wxEVT_JOGSHUTTLE_MESSAGE, JS_BUTTON_RELEASED);
	event.SetInt(number);
	mEvtHandler->AddPendingEvent(event);
}

/// Called when the user twiddles the jog control.
/// Sends an event containing the direction.
/// @param clockwise True if twiddled clockwise.
/// @param position A rolling position number, mod 256.
void JSListener::jog(JogShuttle *, bool clockwise, int)
{
	wxCommandEvent event(wxEVT_JOGSHUTTLE_MESSAGE, JOG);
	event.SetInt(clockwise);
	mEvtHandler->AddPendingEvent(event);
}

/// Called when the user twiddles the shuttle control.
/// Sends an event containing the extent (-7 to +7; positive for clockwise).
/// @param clockwise True if twiddled clockwise.
/// @param position Absolute extent (0-7).
void JSListener::shuttle(JogShuttle *, bool clockwise, int position)
{
	wxCommandEvent event(wxEVT_JOGSHUTTLE_MESSAGE, SHUTTLE);
	event.SetInt(clockwise ? position : position * -1);
	mEvtHandler->AddPendingEvent(event);
}

/// Returns true if the requested button number is currently pressed
bool JSListener::IsPressed(const unsigned int number)
{
	bool isPressed = false;
	if (number < mButtonsPressed.GetCount()) { //sanity check
		isPressed = mButtonsPressed[number] != 0;
	}
	return isPressed;
}
