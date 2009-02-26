/***************************************************************************
 *   $Id: timepos.cpp,v 1.4 2009/02/26 19:17:10 john_f Exp $           *
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

#include "timepos.h"

BEGIN_EVENT_TABLE( Timepos, wxEvtHandler )
	EVT_TIMER( -1, Timepos::OnRefreshTimer )
END_EVENT_TABLE()

/// Sets displays to default, and starts update timer.
/// @param timecodeDisplay The studio timecode display control.
/// @param positionDisplay The position display control.
Timepos::Timepos(wxStaticText * timecodeDisplay, wxStaticText * positionDisplay)
: mTimecodeDisplay(timecodeDisplay), mPositionDisplay(positionDisplay)
{
	Reset();
	wxTimer * refreshTimer = new wxTimer(this);
	refreshTimer->Start(TC_DISPLAY_UPDATE_INTERVAL);
}

/// Responds to the update timer event.
/// If timecode is running, updates the timecode display based the time since SetTimecode() was called.
/// If the position display is running, updates it based on the time since Record() was called, or if the end of postroll has been reached, stops it and displays the exact recording length.
/// @param event The timer event.
void Timepos::OnRefreshTimer(wxTimerEvent& WXUNUSED(event))
{
	if (mTimecodeRunning) {
		wxDateTime timecode = wxDateTime::UNow() + mTimecodeOffset;
		mTimecodeDisplay->SetLabel(wxString::Format(timecode.Format(wxT("%H:%M:%S:")) + wxString::Format(wxT("%02d"), (int) (timecode.GetMillisecond() * mLastTimecodeReceived.edit_rate.numerator / mLastTimecodeReceived.edit_rate.denominator / 1000))));
	}
	if (mPositionRunning) {
		if (mPostrolling && wxDateTime::UNow() >= mStopTime) { //finished postrolling
			//display exact duration and stop
			mPostrolling = false;
			mPositionRunning = false;
			mPositionDisplay->SetLabel(FormatPosition(mDuration));
		}
		else {
			//display position based on current time
			mPositionDisplay->SetLabel(FormatPosition(wxDateTime::UNow() - mPositionOrigin, mDuration));
		}
	}
}

/// Gets the position display as a frame number, or returns 0 if the position display is not running.
/// @return The frame count.
unsigned long Timepos::GetFrameCount()
{
	unsigned long frameCount = 0;
	if (mPositionRunning) {
		wxTimeSpan position = wxDateTime::UNow() - mPositionOrigin;
		frameCount = position.GetMilliseconds().GetLo() * mLastTimecodeReceived.edit_rate.numerator / mLastTimecodeReceived.edit_rate.denominator / 1000;
	}
	return frameCount;
}


/// Converts a duration into a string for display.
/// Default value if edit rate parameters not known.
const wxString Timepos::FormatPosition(const ProdAuto::MxfDuration duration)
{
	wxString msg = UNKNOWN_POSITION;
	if (!duration.undefined) {
		wxTimeSpan timespan(0, 0, 0, (unsigned long) 1000 * duration.samples * duration.edit_rate.denominator / duration.edit_rate.numerator); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
		msg = FormatPosition(timespan, duration);
	}
	return msg;
}

/// Converts a time span object into a string for display.
/// @param timespan Contains the duration.
/// @param duration Contains edit rate needed for conversion.
const wxString Timepos::FormatPosition(const wxTimeSpan timespan, const ProdAuto::MxfDuration duration)
{
	wxString msg;
	if (duration.undefined) {
		msg = UNKNOWN_POSITION;
	}
	else {
		//minutes
		msg = timespan.GetMinutes() ? wxString::Format(wxT("%02d:"), timespan.GetMinutes()) : wxT("    "); //Minutes count to >59
		//seconds
		msg += wxString::Format(wxT("%02d:"), (int) (timespan.GetSeconds().GetLo() % 60));
		//frames
		msg += wxString::Format(wxT("%02d"), (int) (timespan.GetMilliseconds().GetLo() % 1000 * duration.edit_rate.numerator / duration.edit_rate.denominator / 1000));
	}
	return msg;
}

/// If timecode isn't running (so we don't know studio timecode's offset from the system clock), or given timecode isn't valid,
/// shows default position indication.
/// Otherwise, sets position counter origin to the correct date and the given timecode, minus its offset from real time,
/// and starts position counter running.
/// @param tc The timecode of the start of the recording.
const wxString Timepos::Record(const ProdAuto::MxfTimecode tc)
{
	if (mTimecodeRunning && !tc.undefined && tc.edit_rate.numerator && tc.edit_rate.denominator) { //sensible values: no chance of divide by zero!
		mStartTimecode = tc; //for calculating exact duration
		mPositionRunning = true;
		mPostrolling = false; //could still be postrolling a previous recording
		//Work out the start time and date of the recording.  This allows recordings lasting >24 hours to display properly.
		mPositionOrigin = wxDateTime::UNow();
		mPositionOrigin.ResetTime(); //assume for the moment that recording started today
		mPositionOrigin += wxTimeSpan(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //start timecode with today's date; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
		mPositionOrigin -= mTimecodeOffset; //adjust to system clock
		if (mPositionOrigin.Subtract(wxDateTime::UNow()).GetHours()) { //the recording appears to be at least an hour long already so must have started yesterday
			mPositionOrigin.Subtract(wxDateSpan(0, 0, 0, 1));
		}
	}
	else {
		mStartTimecode.undefined = true;
		mPositionDisplay->SetLabel(UNKNOWN_POSITION);
	}
	return mPositionDisplay->GetLabel();
}

/// If position is not running, does nothing.
/// Otherwise, if given timecode is acceptable, works out the exact recording duration and the stop time in system clock time,
/// and sets the postrolling flag.
/// @param tc The timecode of the end of the recording (which can be in the future).  Must be compatible with the start timecode supplied to Record().
void Timepos::Stop(const ProdAuto::MxfTimecode tc)
{
	if (mPositionRunning) {
		if (!tc.undefined && tc.edit_rate.numerator == mStartTimecode.edit_rate.numerator && tc.edit_rate.denominator == mStartTimecode.edit_rate.denominator) { //sensible values and can calculate exact duration (as edit rate is the same)
			//Work out the stop time and date of the recording
			mStopTime = wxDateTime::UNow();
			mStopTime.ResetTime(); //assume for the moment that recording finishes today
			mStopTime += wxTimeSpan(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //finish timecode with today's date; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
			mStopTime -= mTimecodeOffset; //adjust to system clock
			if (wxDateTime::UNow().Subtract(mStopTime).GetHours() > 0) { //the recording appears to have stopped more than an hour ago so must be stopping tomorrow
				mStopTime.Add(wxDateSpan(0, 0, 0, 1));
			}
			mDuration.edit_rate.numerator = mStartTimecode.edit_rate.numerator;
			mDuration.edit_rate.denominator = mStartTimecode.edit_rate.denominator;
			mDuration.undefined = false;
			if (tc.samples <= mStartTimecode.samples) { //have rolled over midnight
				//add a day's worth of samples
				mDuration.samples = tc.samples - mStartTimecode.samples + 3600 * 24 * tc.edit_rate.denominator / tc.edit_rate.numerator;
			}
			else {
				mDuration.samples = tc.samples - mStartTimecode.samples;
			}
			mPostrolling = true; //start checking for stop time
		}
		else {
			//something very odd going on!
			mPositionRunning = false;
			mPositionDisplay->SetLabel(UNKNOWN_POSITION);
		}
	}
}

/// Supplies the timecode corresponding to now, both as a timecode structure and a formatted string.
/// @param tc Timecode structure to be populated.
/// @return The formatted string displayed on the timecode control.
const wxString Timepos::GetTimecode(ProdAuto::MxfTimecode * tc)
{
	if (tc) {
		if (mTimecodeRunning) {
			*tc = mLastTimecodeReceived; // for edit rate/defined
			wxDateTime timecode = wxDateTime::UNow() + mTimecodeOffset;
			tc->samples = (((((unsigned long) timecode.GetHour() * 60) + timecode.GetMinute()) * 60) + timecode.GetSecond()) * tc->edit_rate.numerator / tc->edit_rate.denominator;
			tc->samples += timecode.GetMillisecond() * tc->edit_rate.numerator / mLastTimecodeReceived.edit_rate.denominator / 1000;
		}
		else {
			tc->undefined = true;
		}
	}
	return mTimecodeDisplay->GetLabel();
}

/// Supplies the timecode corresponding to the value sent to Record(), both as a timecode structure and a formatted string.
/// @param tc Timecode structure to be populated.
/// @return Timecode as a formatted string.
const wxString Timepos::GetStartTimecode(ProdAuto::MxfTimecode * tc)
{
	if (tc) {
		*tc = mStartTimecode;
	}
	return FormatTimecode(mStartTimecode);
}

/// Formats the supplied timecode as a displayable string.
/// Wraps at 24 hours.
/// @param tc The timecode to format.
/// @return The formatted string.
const wxString Timepos::FormatTimecode(const ProdAuto::MxfTimecode tc)
{
	if (tc.undefined || !tc.edit_rate.numerator || !tc.edit_rate.denominator) { //arithmetic fault
		return UNKNOWN_TIMECODE;
	}
	else {
		long seconds = tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator;
		return wxString::Format(wxT("%02d:%02d:%02d:%02d"), (seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60, tc.samples % (tc.edit_rate.numerator / tc.edit_rate.denominator));
	}
}

/// @return The formatted string displayed on the position control, or a similarly formatted string showing the recording duration, if postrolling.
const wxString Timepos::GetPosition()
{
	if (mPostrolling) {
		//we're really at the end of the recording, even though the display is still moving
		return FormatPosition(mDuration);
	}
	else {
		return mPositionDisplay->GetLabel();
	}
}

/// @return The formatted string representing the very start of a recording.
const wxString Timepos::GetStartPosition()
{
	return FormatPosition(wxTimeSpan(0, 0, 0, 0), mDuration);
}

/// Returns the system to its initial state.
void Timepos::Reset()
{
	DisableTimecode();
	mLastKnownTimecode.undefined = true;
	mDuration.undefined = true;
	mPositionDisplay->SetLabel(NO_POSITION);
	mPostrolling = false;
	mPositionRunning = false;
}

/// Manually sets the position display to a given position, based on the edit rate in use.
/// @param samples The number of frames into the recording.
void Timepos::SetPosition(unsigned long samples) {
	if (!mLastKnownTimecode.undefined) {
		mPositionDisplay->SetLabel(FormatPosition(wxTimeSpan(0, 0, 0, (unsigned long) 1000 * samples * mLastKnownTimecode.edit_rate.denominator / mLastKnownTimecode.edit_rate.numerator), mDuration)); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
	}
}

/// Stops the position display running and shows the desired indication.
/// @param noPosition If true, the indication corresponds to no position; if false, unknown position.
void Timepos::SetPositionUnknown(bool noPosition)
{
	if (noPosition) {
		mPositionDisplay->SetLabel(NO_POSITION);
	}
	else {
		mPositionDisplay->SetLabel(UNKNOWN_POSITION);
	}
	mPositionRunning = false;
}

/// Stops the timecode display running, invalidates previously received timecode, and displays the given message.
/// @param message The string to display.
void Timepos::DisableTimecode(const wxString & message)
{
	mTimecodeRunning = false;
	mTimecodeDisplay->SetLabel(message);
	mLastTimecodeReceived.undefined = true;
}

/// Calculates given timecode's offset from the system clock and starts the timecode display running, or displays it as a stuck timecode.
/// If timecode has a problem, displays the appropriate indication instead.
/// @param tc Timecode reference.
/// @param stuck If true, indicates that the timecode is stuck so the display should not auto-update.
void Timepos::SetTimecode(const ProdAuto::MxfTimecode tc, bool stuck)
{
	if (tc.undefined) { //no timecode available
		DisableTimecode();
	}
	else if (tc.edit_rate.numerator && tc.edit_rate.denominator) { //sensible values: no chance of divide by zero!
		mLastTimecodeReceived = tc;
		mLastKnownTimecode = tc;
		mDuration.edit_rate.numerator = tc.edit_rate.numerator;
		mDuration.edit_rate.denominator = tc.edit_rate.denominator;
		mDuration.undefined = false;
		if (stuck) {
			mTimecodeRunning = false;
			//show the stuck value
			wxTimeSpan stuckTimecode(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
			mTimecodeDisplay->SetLabel(wxString::Format(stuckTimecode.Format(wxT("%H:%M:%S:")) + wxString::Format(wxT("%02d"), (int) ((stuckTimecode.GetMilliseconds().GetLo() % 1000) * tc.edit_rate.numerator / tc.edit_rate.denominator / 1000)))); //GetLo() is sufficient because 24 hours'-worth of milliseconds is < 32 bits
		}
		else {
			mTimecodeRunning = true;
			wxDateTime now = wxDateTime::UNow();
			//express timecode as a date time object with today's date
			wxDateTime timecodeTime = now;
			timecodeTime.ResetTime(); //midnight today
			timecodeTime += wxTimeSpan(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //timecode with today's date; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
			//work out offset from system time
			mTimecodeOffset = timecodeTime.Subtract(now); //how much later the timecode is than the clock time
		}
	}
	else {
		//duff data
		DisableTimecode(UNKNOWN_TIMECODE);
	}
}

/// Sets the edit rate if it's not already set
/// @param rate Edit rate - this is not checked for sensible values.
void Timepos::SetDefaultEditRate(const ProdAuto::MxfTimecode rate) {
	if (mLastKnownTimecode.undefined) {
		mLastKnownTimecode = rate;
	}
}
