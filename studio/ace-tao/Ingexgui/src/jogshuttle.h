/***************************************************************************
 *   $Id: jogshuttle.h,v 1.1 2009/01/29 07:45:26 stuart_hc Exp $             *
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

#ifndef JOGSHUTTLE_H_
#define JOGSHUTTLE_H_

#include "JogShuttle.h" //NB this is not this file!

DECLARE_EVENT_TYPE(wxEVT_JOGSHUTTLE_MESSAGE, -1)

/// Class with methods that are called by the jog/shuttle device
class JSListener : public ingex::JogShuttleListener
{
	public:
		JSListener(wxEvtHandler *);
		virtual void connected(ingex::JogShuttle * jogShuttle, ingex::JogShuttleDevice device);
		virtual void disconnected(ingex::JogShuttle * jogShuttle, ingex::JogShuttleDevice device);
		virtual void ping(ingex::JogShuttle * jogShuttle);
		virtual void buttonPressed(ingex::JogShuttle * jogShuttle, int number);
		virtual void buttonReleased(ingex::JogShuttle * jogShuttle, int number);
		virtual void jog(ingex::JogShuttle * jogShuttle, bool clockwise, int position);
		virtual void shuttle(ingex::JogShuttle * jogShuttle, bool clockwise, int position);
		bool IsPressed(const unsigned int);
	private:
		wxEvtHandler * mEvtHandler;
		wxArrayInt mButtonsPressed;
};

enum JogShuttleEventType {
	JOG,
	SHUTTLE,
	JS_BUTTON_PRESSED,
	JS_BUTTON_RELEASED
};

#endif
