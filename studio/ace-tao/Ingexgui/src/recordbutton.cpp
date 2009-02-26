/***************************************************************************
 *   $Id: recordbutton.cpp,v 1.5 2009/02/26 19:17:10 john_f Exp $            *
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

#include "recordbutton.h"

BEGIN_EVENT_TABLE(RecordButton, wxButton)
	EVT_LEFT_DOWN(RecordButton::OnLMouseDown)
	EVT_TIMER(wxID_ANY, RecordButton::OnTimer)
END_EVENT_TABLE()

/// @param parent The parent window.
/// @param id The button's window ID.
RecordButton::RecordButton(wxWindow * parent, wxWindowID id, const wxString & label) : wxButton(parent, id, label), mLabel(label)
{
	SetNextHandler(parent);
	mInitialColour = GetBackgroundColour(); //on some machines disabling doesn't grey out the button
	mTimer = new wxTimer(this);
	Enable();
}

/// Intercepts mouse clicks to prevent them propagating if the button is "disabled".
/// @param event The mouse event.
void RecordButton::OnLMouseDown(wxMouseEvent & event)
{
	if (mClickable) {
		//act on the click
		event.Skip();
	}
}

/// Prevents the button being clicked.
void RecordButton::Disable()
{
//std::cerr << "disable" << std::endl;
	Enable(false);
}

/// Makes the button clickable or not.
/// If enabled, the button is dull red; if disabled, greyed out.
/// @param state True to enable.
/// @return Not much use.
bool RecordButton::Enable(bool state)
{
	wxButton::SetLabel(mLabel);
	mTimer->Stop();
	if (state) {
//std::cerr << "enable true" << std::endl;
		SetBackgroundColour(wxColour(0xB0, 0x00, 0x00)); //dull red
	}
	else {
//std::cerr << "enable false" << std::endl;
		SetBackgroundColour(mInitialColour);
	}
	mClickable = true; //let the underlying button handle this
	return wxButton::Enable(state); //logic of return value is a bit broken
}

/// Changes the label displayed on the button (except in recording state)
/// @param label The label to be displayed on the button
void RecordButton::SetLabel(const wxString & label)
{
	mLabel = label;
	if (wxColour(wxT("RED")) != GetBackgroundColour()) {
		wxButton::SetLabel(mLabel);
	}
}

/// Puts the button into the bright red textless and disabled "record" state.
void RecordButton::Record() {
//std::cerr << "record" << std::endl;
	wxButton::Enable(); //so it's not greyed out
	mClickable = false;
	SetLabel(wxT(""));
	SetBackgroundColour(wxColour(wxT("RED")));
	mTimer->Stop();
}

/// Puts the button into the flashing bright red textless and disabled "pending" state.
void RecordButton::Pending() {
//std::cerr << "pending" << std::endl;
	wxButton::Enable(); //so it's not greyed out
	mClickable = false; //so it doesn't respond to mouse clicks
	wxButton::SetLabel(wxT(""));
	SetBackgroundColour(wxColour(wxT("RED")));
	mTimer->Start(125);
}

/// Responds to flash timer events by toggling colour
void RecordButton::OnTimer(wxTimerEvent & WXUNUSED(event)) {
//std::cerr << "timer" << std::endl;
	if (mTimer->IsRunning()) { //trap timer being stopped with event in the system - if this ever happens...
		if (wxColour(wxT("RED")) != GetBackgroundColour()) {
			SetBackgroundColour(wxColour(wxT("RED")));
		}
		else {
			SetBackgroundColour(mInitialColour);
		}
	}
}

/// Returns true if the button is clickable
bool RecordButton::IsEnabled() {
	return mClickable && wxButton::IsEnabled();
}
