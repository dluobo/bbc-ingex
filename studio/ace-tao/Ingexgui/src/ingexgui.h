/***************************************************************************
 *   $Id: ingexgui.h,v 1.12 2009/02/27 12:19:16 john_f Exp $              *
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

#ifndef _INGEXGUI_H_
#define _INGEXGUI_H_
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

#define BUTTON_WARNING_COLOUR wxColour(0xFF, 0x80, 0x00)

#define UNKNOWN_TIMECODE wxT("??:??:??:??")

#define SAVED_STATE_FILENAME wxT(".ingexguirc")

#define USING_ULONGLONG 0 //this allows larger numbers to be incrementable in tape ID cells but if the C runtime library does not support 64-bit numbers the increment buttons will not work at all

static const ProdAuto::MxfTimecode InvalidMxfTimecode = { {0, 0}, 0, true};
static const ProdAuto::MxfDuration InvalidMxfDuration = { {0, 0}, 0, true};

class TickTreeCtrl;
class DragButtonList;
namespace ingex {
	class JogShuttle;
}
class JSListener;

class 
IngexguiApp : public wxApp
{
	public:
		virtual bool OnInit();
		int FilterEvent(wxEvent&);
};

class Timepos;
class Player;
class RecordButton;
class HelpDlg;
class RecorderGroupCtrl;
class CuePointsDlg;
class TestModeDlg;
class wxToggleButton;
class EventList;

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
		MENU_StepBackwards,
		MENU_StepForwards,
		MENU_Mute,
		MENU_Up,
		MENU_Down,
		MENU_PrevTake,
		MENU_NextTake,
		MENU_FirstTake,
		MENU_LastTake,
		MENU_PlayBackwards,
		MENU_Pause,
		MENU_PlayForwards,
		MENU_PlayPause,
		MENU_AutoClear,
		MENU_ClearLog,
		MENU_PlayerDisable,
		MENU_PlayMOV,
		MENU_PlayMXF,
		MENU_TogglePlayFiles,
#ifndef DISABLE_SHARED_MEM_SOURCE
		MENU_ToggleEtoE,
#endif
		MENU_PlayerType,
		MENU_PlayerAccelOutput,
#ifdef HAVE_DVS
		MENU_PlayerEnableSDIOSD,
		MENU_PlayerExtOutput,
		MENU_PlayerExtAccelOutput,
		MENU_PlayerExtUnaccelOutput,
#endif
		MENU_PlayerUnaccelOutput,
		MENU_PlayerOSD,
		MENU_PlayerAbsoluteTimecode,
		MENU_PlayerRelativeTimecode,
		MENU_PlayerNoOSD,
		MENU_PlayerMuteAudio,
		MENU_PlayerAudioFollowsVideo,
		MENU_TestMode,
#ifndef DISABLE_SHARED_MEM_SOURCE
		BUTTON_EtoE,
#endif
		BUTTON_PlayRecordings,
		BUTTON_PlayFiles,
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
		TEXTCTRL_Description,
		TREE,
		BUTTON_JumpToTimecode,
	};
	private:
	enum Stat
	{
		STOPPED,
		RUNNING_UP, //Waiting for recorders to respond to start commands
		RECORDING,
		RUNNING_DOWN, //Waiting for recorders to respond to stop commands
		PLAYING,
		PAUSED,
		PLAYING_BACKWARDS,
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
		void OnPlayerOSDChange(wxCommandEvent&);
#ifdef HAVE_DVS
		void OnPlayerSDIOSDChange(wxCommandEvent&);
#endif
		void OnPlayerOutputTypeChange(wxCommandEvent&);
		void OnPlayerAudioFollowsVideo(wxCommandEvent& WXUNUSED(event));
		void OnPlayerMuteAudio(wxCommandEvent& WXUNUSED(event));
		void OnQuit(wxCommandEvent&);
		void OnEventSelection(wxListEvent&);
		void OnEventActivated(wxListEvent&);
		void OnRefreshTimer(wxTimerEvent&);
		void OnDeleteCue(wxCommandEvent&);
		void OnJumpToTimecode(wxCommandEvent&);
		void OnPlayerEvent(wxCommandEvent&);
		void OnRecorderGroupEvent(wxCommandEvent&);
		void OnTestDlgEvent(wxCommandEvent&);
		void OnTreeEvent(wxCommandEvent&);
		void OnPlaybackTrackSelect(wxCommandEvent&);
		void OnShortcut(wxCommandEvent&);
		void OnPlayerDisable(wxCommandEvent&);
		void OnPlayerOpenFile(wxCommandEvent&);
		void OnClearDescription(wxCommandEvent&);
		void OnDescriptionChange(wxCommandEvent&);
		void OnDescriptionEnterKey(wxCommandEvent&);
		void OnFocusGot(wxFocusEvent&);
		void OnFocusLost(wxFocusEvent&);
		void OnTestMode(wxCommandEvent&);
		void OnPlayerMode(wxCommandEvent&);
		void OnJogShuttleEvent(wxCommandEvent&);
		void OnPrevTake(wxCommandEvent&);
		void OnNextTake(wxCommandEvent&);

		void UpdatePlayerAndEventControls(bool = false, bool = false);
		void UpdateTextShortcutStates();
		void SetStatus(Stat);
		ProdAuto::MxfDuration SetRoll(const wxChar *, int, const ProdAuto::MxfDuration &, wxStaticBoxSizer *);
		void ClearLog();
		void ResetToDisconnected();
		void Log(const wxString &);
		void SetProjectName();
		bool IsRecording();
		void EnableButtonReliably(wxButton *, bool = true);
		void CanEditCues(const bool);
		void EtoE();

		wxStaticBitmap * mStatusCtrl, * mAlertCtrl;
		RecorderGroupCtrl * mRecorderGroup;
		wxButton * mRecorderListRefreshButton;
		wxButton * mTapeIdButton;
		wxButton * mStopButton, * mCueButton;
		RecordButton * mRecordButton;
		EventList * mEventList;
		wxNotebook * mNotebook;
		wxButton * mDescClearButton;
		TickTreeCtrl * mTree;
		wxStaticText * mRecProjectNameCtrl;
		wxStaticBoxSizer * mPlayProjectNameBox;
		wxStaticText * mPlayProjectNameCtrl;
		wxTextCtrl * mDescriptionCtrl;
		wxButton * mPrevTakeButton, * mNextTakeButton;
		wxButton * mJumpToTimecodeButton;
		wxButton * mDeleteCueButton;
		DragButtonList * mPlaybackTrackSelector;
		HelpDlg * mHelpDlg;
		CuePointsDlg * mCuePointsDlg;
		TestModeDlg * mTestModeDlg;
		wxStaticBoxSizer * mTimecodeBox;
		wxToggleButton * mPlayRecordingsButton;
		wxToggleButton * mPlayFilesButton;
#ifndef DISABLE_SHARED_MEM_SOURCE
		wxToggleButton * mEtoEButton;
#endif
		Stat mStatus;
		long mEditRateNumerator, mEditRateDenominator;
		Timepos * mTimepos;
		Player * mPlayer;
		wxXmlDocument mSavedState;
		wxString mSavedStateFilename;
		bool mDescriptionControlHasFocus;
		wxLogStream * mLogStream;
		wxArrayString mFileModeFiles;
		unsigned int mFilesModeSelectedTrack;
		wxString mFileModeMovFile;
		int64_t mFileModeFrameOffset;
		int64_t mRecordingModeFrameOffset;
		ingex::JogShuttle * mJogShuttle;
		JSListener * mJSListener;
		wxDateTime mToday;
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
