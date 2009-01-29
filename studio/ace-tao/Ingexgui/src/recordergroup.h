/***************************************************************************
 *   $Id: recordergroup.h,v 1.5 2009/01/29 07:36:58 stuart_hc Exp $         *
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

#ifndef _RECORDERGROUP_H_
#define _RECORDERGROUP_H_
#include <wx/wx.h>
#include "controller.h"
#include "ingexgui.h"
DECLARE_EVENT_TYPE(wxEVT_RECORDERGROUP_MESSAGE, -1)

class Comms;
class wxXmlDocument;

/// A control derived from a list box containing a list of recorders, connected and disconnected.
class RecorderGroupCtrl : public wxListBox
{
	public:
		RecorderGroupCtrl(wxWindow *, wxWindowID id, const wxPoint &, const wxSize &, int, wxChar**, const wxXmlDocument *);
		~RecorderGroupCtrl();
		void StartGettingRecorders();
		void SetPreroll(const ProdAuto::MxfDuration);
		void SetPostroll(const ProdAuto::MxfDuration);
		const ProdAuto::MxfDuration GetPreroll();
		const ProdAuto::MxfDuration GetPostroll();
		const ProdAuto::MxfDuration GetMaxPreroll();
		const ProdAuto::MxfDuration GetMaxPostroll();
		void SetTapeIds(const wxString &, const CORBA::StringSeq &, const CORBA::StringSeq &);
		void RecordAll(const ProdAuto::MxfTimecode);
		void Record(const wxString &, const CORBA::BooleanSeq &);
		void Stop(const ProdAuto::MxfTimecode &, const wxString &, const ProdAuto::LocatorSeq &);
		void EnableForInput(const bool = true);
		void SetProjectNames(const wxSortedArrayString &);
		const wxSortedArrayString & GetProjectNames();
		void SetCurrentProjectName(const wxString &);
		const wxString & GetCurrentProjectName();
		const wxString & GetCurrentDescription();
		void Deselect(unsigned int);
		enum RecorderGroupCtrlEventType {
			DISABLE_REFRESH,
			ENABLE_REFRESH,
			NEW_RECORDER,
			REMOVE_RECORDER,
			REQUEST_RECORD,
			RECORDING,
			TRACK_STATUS,
			STOPPED,
			DISPLAY_TIMECODE_SOURCE,
			DISPLAY_TIMECODE,
			DISPLAY_TIMECODE_STUCK,
			DISPLAY_TIMECODE_MISSING,
			TIMECODE_RUNNING,
			TIMECODE_STUCK,
			TIMECODE_MISSING,
			COMM_FAILURE,
		};
	private:
		void OnListRefreshed(wxCommandEvent &);
		void OnLMouseDown(wxMouseEvent &);
		void OnRightMouseDown(wxMouseEvent &);
		void OnUnwantedMouseDown(wxMouseEvent &);
		void OnControllerEvent(ControllerThreadEvent &);
		void Insert(const wxString &, unsigned int);
		const wxString GetName(unsigned int);
		void Connect(unsigned int);
		void Disconnect(unsigned int);
		Controller * GetController(unsigned int);
		void SetTimecodeRecorder(wxString name = wxT(""));
		Comms * mComms;
		bool mEnabledForInput;
		ProdAuto::MxfDuration mMaxPreroll, mMaxPostroll, mPreroll, mPostroll;
		wxString mTimecodeRecorder;
		bool mTimecodeRecorderStuck;
		ProdAuto::MxfTimecode mStartTimecode;
		const wxXmlDocument * mDoc;
		wxSortedArrayString mProjectNames;
		wxString mCurrentProject;
		wxString mCurrentDescription;
		DECLARE_EVENT_TABLE()
};

/// Class storing various information about a recorder, to be attached to events from the recorder group.
/// Various constructors for different information; all data constant.
class RecorderData
{
	public:
		RecorderData(ProdAuto::TrackStatusList_var trackStatusList, ProdAuto::TrackList_var trackList = 0) : mTrackStatusList(trackStatusList), mTrackList(trackList), mTimecode() {};
		RecorderData(ProdAuto::MxfTimecode tc) : mTimecode(tc) {};
		RecorderData(ProdAuto::TrackList_var trackList, CORBA::StringSeq_var fileList, ProdAuto::MxfTimecode tc = InvalidMxfTimecode) : mTrackList(trackList), mFileList(fileList), mTimecode(tc) {};
		ProdAuto::TrackList_var GetTrackList() {return mTrackList;};
		ProdAuto::TrackStatusList_var GetTrackStatusList() {return mTrackStatusList;};
		CORBA::StringSeq_var GetFileList() {return mFileList;};
		ProdAuto::MxfTimecode GetTimecode() {return mTimecode;};
	private:
		const ProdAuto::TrackStatusList_var mTrackStatusList;
		const ProdAuto::TrackList_var mTrackList;
		const CORBA::StringSeq_var mFileList;
		const ProdAuto::MxfTimecode mTimecode;
};

/// Class storing a recorder name and a controller object, to be attached to each entry in the list control.
/// This class is used to allow names in the list to be remembered while messages such as "Connecting..." are displayed.
/// Recorder name must be present; controller object is created and destroyed with the Start(), Stop() and Del() methods.
class ControllerContainer : public wxClientData
{
	public:
		ControllerContainer(const wxString & name) : mName(name), mController(0) {};
		~ControllerContainer() { if (mController) { delete mController; } };
		void Start(Comms * comms, wxEvtHandler * handler) { mController = new Controller(mName, comms, handler); }; //the controller's Destroy() method will send a message when ready to delete itself
		void Stop() { if (mController) { mController->Destroy(); } };
		void Del() { if (mController) { delete mController; mController = 0; } };
		const wxString GetName() { return mName; };
		Controller * GetController() { return mController; };
	private:
		const wxString mName;
		Controller * mController;
};

#endif
