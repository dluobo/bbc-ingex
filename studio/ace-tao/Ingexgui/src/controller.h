/***************************************************************************
 *   Copyright (C) 2006-2008 British Broadcasting Corporation              *
 *   - all rights reserved.                                                *
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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <wx/wx.h>
#include "RecorderC.h"

#define DEFAULT_MAX_POSTROLL 250 //frames
//Watchdog timer needs to check twice in quick succession to detect stuck timecode immediately
#define WATCHDOG_TIMER_LONG_INTERVAL 10000 //ms
#define WATCHDOG_TIMER_SHORT_INTERVAL 250 //ms


class Comms;
class ControllerThreadEvent;

/// Handles communication with a recorder in a threaded manner.
class Controller : public wxEvtHandler, wxThread //each controller handles its own messages
{

public:
	Controller(const wxString &, Comms *, wxEvtHandler *);
	~Controller();
	bool IsOK();
	void SetTapeIds(const CORBA::StringSeq &, const CORBA::StringSeq &);
	void AddProjectNames(const CORBA::StringSeq &);
	void Record(const ProdAuto::MxfTimecode &, const ProdAuto::MxfDuration &, const wxString &, const CORBA::BooleanSeq &);
	void Stop(const ProdAuto::MxfTimecode &, const ProdAuto::MxfDuration &, const wxString &, const ProdAuto::LocatorSeq &);
	void PollRapidly(bool = true);
	bool IsRouterRecorder();
	void Destroy();
	const ProdAuto::MxfDuration GetMaxPreroll();
	const ProdAuto::MxfDuration GetMaxPostroll();
	void EnableTrack(unsigned int, bool = true);

	enum Command
	{
		NONE,
		CONNECT,
		SET_TAPE_IDS,
		ADD_PROJECT_NAMES,
		RECONNECT,
		STATUS,
		RECORD,
		STOP,
		DIE,
	};
	enum Result
	{
		SUCCESS,
		FAILURE,
		COMM_FAILURE,
	};
	enum TimecodeState
	{
		ABSENT,
		UNCONFIRMED,
		STUCK,
		RUNNING,
	};
private:
	ExitCode Entry();
	void Signal(Command);
	void OnThreadEvent(ControllerThreadEvent &);
	void OnTimer(wxTimerEvent& WXUNUSED(event));

	ProdAuto::Recorder_var mRecorder;
	Comms * mComms;
	wxMutex mMutex;
	wxTimer * mWatchdogTimer;
	const wxString mName;
	bool mTimecodeRunning;
	bool mReconnecting;
	bool mRouterRecorder;
	bool mPollRapidly;
	Command mPendingCommand;
	wxDateTime mLastTimecodeRequest;
	ProdAuto::MxfTimecode mLastTimecodeReceived;
	//vbls written to in both contexts while thread is running
	wxCondition* mCondition;
	Command mCommand;
	CORBA::BooleanSeq mEnableList;
	CORBA::StringSeq mSourceNames;
	CORBA::StringSeq mProjectNames;
	CORBA::StringSeq mTapeIds;
	wxString mProject;
	wxString mDescription;
	ProdAuto::MxfDuration mPreroll, mPostroll;
	ProdAuto::MxfDuration mMaxPreroll, mMaxPostroll;
	ProdAuto::SourceList mSourceList;
	ProdAuto::TrackList_var mTrackList;
	ProdAuto::LocatorSeq mLocators; 
	ProdAuto::MxfTimecode mStartTimecode;
	ProdAuto::MxfTimecode mStopTimecode;
	DECLARE_EVENT_TABLE()
};

/// Event to accommodate the output from a controller thread.
/// The method is from http://www.xmule.ws/phpnuke/modules.php?name=News&file=article&sid=76 - without the typedef, and making the last cast
/// in the macro wxNotifyEventFunction (i.e. not following the instructions completely), it works with gcc but doesn't compile under Win32.
/// Members are all very simple.
typedef void (wxEvtHandler::*ControllerThreadEventFunction)(ControllerThreadEvent &);
DECLARE_EVENT_TYPE(wxEVT_CONTROLLER_THREAD, -1)

#define EVT_CONTROLLER_THREAD(fn) \
	DECLARE_EVENT_TABLE_ENTRY( wxEVT_CONTROLLER_THREAD, wxID_ANY, wxID_ANY, \
	(wxObjectEventFunction) (wxEventFunction) (ControllerThreadEventFunction) &fn, \
	(wxObject*) NULL \
	),

//Custom event to carry a TrackStatusList etc.  Each instance must only be cloned once!
class ControllerThreadEvent : public wxNotifyEvent //Needs to be a Notify type event
{
public:
	ControllerThreadEvent() : mTrackStatusList(0), mTimecodeState(Controller::ABSENT), mTimecodeStateChanged(false) {};
	ControllerThreadEvent(const wxEventType & type) : wxNotifyEvent(type), mTrackStatusList(0), mTimecodeState(Controller::ABSENT), mTimecodeStateChanged(false) {};
	~ControllerThreadEvent() {};
	virtual wxEvent * Clone() const { return new ControllerThreadEvent(*this); }; //Called when posted to the event queue - must have exactly this prototype or base class one will be called which won't copy extra stuff
	const CORBA::ULong GetNTracks() { if (mTrackStatusList.operator->()) return mTrackStatusList->length(); else return 0; };
	void SetName(const wxString & name) { mName = name; };
	void SetMessage(const wxString & msg) { mMessage = msg; };
	void SetTrackList(ProdAuto::TrackList_var trackList) { mTrackList = trackList; };
	void SetTrackStatusList(ProdAuto::TrackStatusList_var trackStatusList) { mTrackStatusList = trackStatusList; };
	void SetCommand(Controller::Command command) { mCommand = command; };
	void SetResult(Controller::Result result) { mResult = result; };
	void SetStrings(CORBA::StringSeq_var strings) { mStrings = strings; };
	void SetTimecode(ProdAuto::MxfTimecode_var timecode) { mTimecode = timecode; };
	void SetTimecodeState(Controller::TimecodeState state) { mTimecodeState = state; };
	void SetTimecodeStateChanged(bool changed = true) { mTimecodeStateChanged = changed; };
	const wxString GetName() { return mName; };
	const wxString GetMessage() { return mMessage; };
	ProdAuto::TrackList_var GetTrackList() { return mTrackList; };
	ProdAuto::TrackStatusList GetTrackStatusList() { return mTrackStatusList; };
	const Controller::Command GetCommand() { return mCommand; };
	const Controller::Result GetResult() { return mResult; };
	CORBA::StringSeq_var GetStrings() { return mStrings; };
	const ProdAuto::MxfTimecode GetTimecode() { return mTimecode; };
	const Controller::TimecodeState GetTimecodeState() { return mTimecodeState; };
	bool TimecodeStateHasChanged() { return mTimecodeStateChanged; };
	DECLARE_DYNAMIC_CLASS(ControllerThreadEvent)
private:
	wxString mName;
	wxString mMessage;
	ProdAuto::TrackList_var mTrackList; //deletes itself
	ProdAuto::TrackStatusList_var mTrackStatusList; //deletes itself
	Controller::Command mCommand;
	Controller::Result mResult;
	CORBA::StringSeq_var mStrings; //deletes itself
	ProdAuto::MxfTimecode mTimecode; //deletes itself
	Controller::TimecodeState mTimecodeState;
	bool mTimecodeStateChanged;
};
#endif
