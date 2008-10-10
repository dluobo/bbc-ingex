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

#ifndef _INGEXGUI_H_
#define _INGEXGUI_H_
#include <vector>
#include "comms.h"
#include "controller.h"
#include <wx/listctrl.h> //for Win32
#include <wx/datetime.h> //for Win32
#include <wx/xml/xml.h>
#include <wx/notebook.h> //for wx 2.7+

#define DEFAULT_PREROLL 10 //frames
#define DEFAULT_POSTROLL 10 //frames

#define CONTROL_BORDER 5 //pixels to space controls from edge, each other, etc
#define EVENT_COL_WIDTH 100
#define EVENT_LIST_HEIGHT 150
#define NOTEBOOK_HEIGHT 300
#define SOURCE_TREE_HEIGHT 150
#define TIME_FONT_SIZE 20 //point
#define EVENT_FONT_SIZE 10 //point - if >10 then text is cut off in the list ctrl

#define TOOLTIP_DELAY 1000 //ms
#define TITLE wxT("Ingex Control")

#define CUE_LABEL wxT("Cue")

#define UNKNOWN_TIMECODE wxT("??:??:??:??")

#define SAVED_STATE_FILENAME wxT(".ingexguirc")

#define USING_ULONGLONG 0 //this allows larger numbers to be incrementable in tape ID cells but if the C runtime library does not support 64-bit numbers the increment buttons will not work at all


static const ProdAuto::MxfTimecode InvalidMxfTimecode = { {0, 0}, 0, true};
static const ProdAuto::MxfDuration InvalidMxfDuration = { {0, 0}, 0, true};

class TickTreeCtrl;
class DragButtonList;

class 
IngexguiApp : public wxApp
{
	public:
		virtual bool OnInit();
		int FilterEvent(wxEvent&);
};

WX_DECLARE_OBJARRAY(ProdAuto::TrackList_var, ArrayOfTrackList_var);
WX_DECLARE_OBJARRAY(CORBA::StringSeq_var, ArrayOfStringSeq_var);

/// Class holding information about a take, for later replay.
/// Holds the position of the take within the displayed list of events, the project name, an array of lists of files
/// (one for each recorder) and an array of lists of track names (ditto).
/// Also arrays of cue point colour codes, timecodes and frame numbers.
class TakeInfo
{
	public:
		TakeInfo(const unsigned long startIndex, const wxString & projectName) : mStartIndex(startIndex), mProjectName(projectName) {};
		void AddRecorder(ProdAuto::TrackList_var trackList, CORBA::StringSeq_var fileList) {mFiles.Add(fileList); mTracks.Add(trackList);}; //adds a set of tracks provided by a recorder at the end of a recording
		void AddCuePoint(const int64_t frame, const ProdAuto::MxfTimecode timecode, const ProdAuto::LocatorColour::EnumType colourCode) {mCuePointFrames.push_back(frame); mCueTimecodes.push_back(timecode); mCueColourCodes.push_back(colourCode);}; //no text here because it's stored in the event list (which means it can be edited)
		void DeleteCuePoint(const unsigned long index) {mCuePointFrames.erase(mCuePointFrames.begin() + index); mCueColourCodes.erase(mCueColourCodes.begin() + index); mCueTimecodes.erase(mCueTimecodes.begin() + index);};

		const unsigned long GetStartIndex() {return mStartIndex;};
		ArrayOfStringSeq_var * GetFiles() {return &mFiles;};
		const wxString GetProjectName() {return mProjectName;};
		ArrayOfTrackList_var * GetTracks() {return &mTracks;};
		std::vector<int64_t> * GetCuePointFrames() {return &mCuePointFrames;};
		std::vector<ProdAuto::LocatorColour::EnumType> * GetCueColourCodes() {return &mCueColourCodes;};
		std::vector<ProdAuto::MxfTimecode> * GetCueTimecodes() {return &mCueTimecodes;};
	private:
		const unsigned long mStartIndex; //the index in the event list for the start of this take
		ArrayOfStringSeq_var mFiles;
		const wxString mProjectName;
		ArrayOfTrackList_var mTracks; //deletes itself
		std::vector<int64_t> mCuePointFrames;
		std::vector<ProdAuto::LocatorColour::EnumType> mCueColourCodes;
		std::vector<ProdAuto::MxfTimecode> mCueTimecodes;
};

WX_DECLARE_OBJARRAY(TakeInfo, TakeInfoArray);

class Timepos;
namespace prodauto {
	class Player;
}
class RecordButton;
class HelpDlg;
class RecorderGroupCtrl;
class CuePointsDlg;
class TestModeDlg;
class wxToggleButton;

/// The main displayed frame, at the heart of the application
class IngexguiFrame : public wxFrame
{
	public:
		IngexguiFrame(int, wxChar**);
	enum
	{
		MENU_About = wxID_HIGHEST + 1,
		MENU_Help,
		MENU_SetRolls,
		MENU_SetCues,
		MENU_SetProjectName,
		MENU_Record,
		MENU_MarkCue,
		MENU_Stop,
		MENU_PrevTrack,
		MENU_NextTrack,
		MENU_3,
		MENU_4,
		MENU_Up,
		MENU_Down,
		MENU_PageUp,
		MENU_PageDown,
		MENU_Home,
		MENU_End,
		MENU_J,
		MENU_K,
		MENU_L,
		MENU_Space,
		MENU_AutoClear,
		MENU_ClearLog,
		MENU_DisablePlayer,
		MENU_PlayMOV,
		MENU_PlayMXF,
		MENU_PlayerType,
		MENU_ExtOutput,
		MENU_AccelOutput,
		MENU_ExtAccelOutput,
		MENU_ExtUnaccelOutput,
		MENU_UnaccelOutput,
		MENU_PlayerOSD,
		MENU_AbsoluteTimecode,
		MENU_RelativeTimecode,
		MENU_NoOSD,
		MENU_TestMode,
		BUTTON_Record,
		BUTTON_Stop,
		BUTTON_Cue,
		TIMER_Refresh,
		BUTTON_RecorderListRefresh,
		BUTTON_TapeId,
		BUTTON_ClearDescription,
		CHOICE_RecorderSelector,
		CHECKLIST_Tracks,
		BUTTON_PrevTake,
		BUTTON_NextTake,
		BUTTON_DeleteCue,
		BUTTON_PlayFile,
		TEXTCTRL_Description,
		TREE,
		BUTTON_JumpToTimecode,
	};
	private:
	enum EventType
	{
		START,
		CUE,
//		REVIEW,
		STOP,
		PROBLEM
	};
	enum Stat
	{
		STOPPED,
		RUNNING_UP,
		RECORDING,
		RUNNING_DOWN,
		PLAYING,
		PAUSED,
		PLAYING_BACKWARDS,
//		UNKNOWN,
	};
		void OnClose(wxCloseEvent&);
		void OnHelp(wxCommandEvent&);
		void OnAbout(wxCommandEvent&);
		void OnSetRolls(wxCommandEvent&);
		void OnSetCues(wxCommandEvent&);
		void OnSetProjectName(wxCommandEvent&);
		void OnRecord(wxCommandEvent&);
		void OnRecorderListRefresh(wxCommandEvent&);
		void OnSetTapeIds(wxCommandEvent&);
		void OnStop(wxCommandEvent&);
		void OnCue(wxCommandEvent&);
		void OnClearLog(wxCommandEvent&);
		void OnPlayerOSDTypeChange(wxCommandEvent&);
		void OnPlayerOutputTypeChange(wxCommandEvent&);
		void OnQuit(wxCommandEvent&);
		void OnEventSelection(wxListEvent&);
		void OnEventActivated(wxListEvent&);
		void OnEventBeginEdit(wxListEvent&);
		void OnEventEndEdit(wxListEvent&);
		void OnRestoreListLabel(wxCommandEvent&);
		void OnRefreshTimer(wxTimerEvent&);
		void OnPrevTake(wxCommandEvent&);
		void OnNextTake(wxCommandEvent&);
		void OnDeleteCue(wxCommandEvent&);
		void OnJumpToTimecode(wxCommandEvent&);
		void OnPlayerEvent(wxCommandEvent&);
		void OnRecorderGroupEvent(wxCommandEvent&);
		void OnTestDlgEvent(wxCommandEvent&);
		void OnTreeEvent(wxCommandEvent&);
		void OnPlaybackTrackSelect(wxCommandEvent&);
		void OnShortcut(wxCommandEvent&);
		void OnDisablePlayer(wxCommandEvent&);
		void OnPlayerOpenFile(wxCommandEvent&);
		void OnClearDescription(wxCommandEvent&);
		void OnDescriptionChange(wxCommandEvent&);
		void OnDescriptionEnterKey(wxCommandEvent&);
		void OnFocusGot(wxFocusEvent&);
		void OnFocusLost(wxFocusEvent&);
		void OnTestMode(wxCommandEvent&);
		void OnPlayFile(wxCommandEvent&);

		void UpdatePlayerAndEventControls(bool = false, bool = false);
		void UpdateTextShortcutStates();
		void AddEvent(EventType, const wxString = wxT(""), ProdAuto::MxfTimecode = InvalidMxfTimecode, const int64_t = 0, const wxString = wxT(""), const wxColour = wxT("WHITE"), const wxColour = wxT("BLACK"), const ProdAuto::LocatorColour::EnumType = ProdAuto::LocatorColour::DEFAULT_COLOUR);
		void SetStatus(Stat);
		ProdAuto::MxfDuration SetRoll(const wxChar *, int, const ProdAuto::MxfDuration &, wxStaticBoxSizer *);
		void ClearLog();
		void SelectAdjacentEvent(bool down);
		void ResetPlayer();
		void Play(const bool = false);
		void ResetToDisconnected();
		void Log(const wxString &);
		void SetProjectName();
		void EnableButtonReliably(wxButton *, bool = true);

		wxStaticBitmap * mStatusCtrl, * mAlertCtrl;
		RecorderGroupCtrl * mRecorderGroup;
		wxButton * mRecorderListRefreshButton;
		wxButton * mTapeIdButton;
		wxButton * mStopButton, * mCueButton;
		RecordButton * mRecordButton;
		wxListView * mEventList; //used wxListCtrl for a while because wxListView crashed when you added an item - seems ok with 2.8
		wxNotebook * mNotebook;
		wxButton * mDescClearButton;
		TickTreeCtrl * mTree;
		wxStaticText * mRecProjectNameCtrl;
		wxStaticText * mPlayProjectNameCtrl;
		wxTextCtrl * mDescriptionCtrl;
		wxButton * mPrevTakeButton, * mNextTakeButton;
		wxButton * mJumpToTimecodeButton;
		wxToggleButton * mPlayFileButton;
		wxButton * mDeleteCueButton;
		DragButtonList * mPlaybackTrackSelector;
		HelpDlg * mHelpDlg;
		CuePointsDlg * mCuePointsDlg;
		TestModeDlg * mTestModeDlg;
		wxStaticBoxSizer * mTimecodeBox;

		Stat mStatus;
		long mStartEvent, mEndEvent;
		long mEditRateNumerator, mEditRateDenominator;
		TakeInfoArray mTakeInfoArray;
		Timepos * mTimepos;
		TakeInfo * mCurrentTakeInfo;
		unsigned long mCurrentSelectedEvent;
		prodauto::Player * mPlayer;
		wxXmlDocument mSavedState;
		wxString mSavedStateFilename;
		bool mLastPlayingBackwards;
		bool mDescriptionControlHasFocus;
		wxLogStream * mLogStream;
		wxArrayString mFileModeFiles;
		int64_t mFileModeFrameOffset;
		int64_t mTakeModeFrameOffset;
		DECLARE_EVENT_TABLE()
};

/// As the stock text ctrl but passes on focus set and kill events
class MyTextCtrl : public wxTextCtrl
{
	public:
		MyTextCtrl(wxWindow * parent, wxWindowID id, const wxString & value = wxT(""), const wxPoint & pos = wxDefaultPosition, const wxSize & size = wxDefaultSize, long style = 0) : wxTextCtrl(parent, id, value, pos, size, style) { SetNextHandler(parent); };
	private:
		void OnFocus(wxFocusEvent & event) { event.Skip(); };

		DECLARE_EVENT_TABLE()
};

#endif // _INGEXGUI_H_
