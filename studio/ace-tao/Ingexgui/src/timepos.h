/***************************************************************************
 *   $Id: timepos.h,v 1.7 2009/09/18 16:10:16 john_f Exp $           *
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

//Timecode and position display controller
#ifndef _TIMEPOS_H_
#define _TIMEPOS_H_

#include <wx/wx.h>
#include "RecorderC.h"

#define NO_TIMECODE wxT("--:--:--:--")
#define NO_POSITION wxT("   --:--")
#define UNKNOWN_TIMECODE wxT("??:??:??:??")
#define UNKNOWN_POSITION wxT("   ??:??")
#define TC_DISPLAY_UPDATE_INTERVAL 50 //ms - must be short enough to allow timely triggers

DECLARE_EVENT_TYPE(wxEVT_TIMEPOS_EVENT, -1)

/// Controls timecode and position displays
class Timepos : public wxEvtHandler
{
	public:
		Timepos(wxEvtHandler *, wxStaticText *, wxStaticText *);
		void Reset();
		void SetTimecode(const ProdAuto::MxfTimecode, bool);
		void SetPosition(unsigned long);
		void SetDefaultEditRate(const ProdAuto::MxfTimecode);
		void SetPositionUnknown(bool = false);
		const wxString Record(const ProdAuto::MxfTimecode);
		void Stop(const ProdAuto::MxfTimecode);
		void DisableTimecode(const wxString & = NO_TIMECODE);
		const wxString GetTimecode(ProdAuto::MxfTimecode * = 0);
		const wxString GetStartTimecode(ProdAuto::MxfTimecode * = 0);
		const wxString GetStartPosition();
		static const wxString FormatPosition(const ProdAuto::MxfDuration);
		static const wxString FormatTimecode(const ProdAuto::MxfTimecode);
		unsigned long GetFrameCount();
		const ProdAuto::MxfTimecode GetDefaultEditRate();
		bool SetTrigger(const ProdAuto::MxfTimecode *, wxEvtHandler *, bool = false);
	private:
		void OnRefreshTimer(wxTimerEvent& WXUNUSED(event));
		static const wxString FormatPosition(const wxTimeSpan, const ProdAuto::MxfTimecode);
		wxDateTime TimeFromTimecode(const ProdAuto::MxfTimecode &, bool = false);

		bool mTimecodeRunning;
		bool mPositionRunning;
		bool mPostrolling;
		wxStaticText * mTimecodeDisplay;
		wxStaticText * mPositionDisplay;
		wxTimeSpan mTimecodeOffset;
		wxDateTime mStopTime;
		ProdAuto::MxfTimecode mStartTimecode;
		ProdAuto::MxfDuration mDuration;
		ProdAuto::MxfTimecode mLastKnownTimecode;
		ProdAuto::MxfTimecode mTriggerTimecode;
		wxEvtHandler * mTriggerHandler;
		bool mTriggerWrap;

	DECLARE_EVENT_TABLE()
};

#endif
