/***************************************************************************
 *   $Id: dialogues.h,v 1.7 2009/05/01 13:41:34 john_f Exp $             *
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

#ifndef _DIALOGUES_H_
#define _DIALOGUES_H_
#include <wx/wx.h>
#include <wx/grid.h>
#include <vector>
#include "ingexgui.h"
#include "timepos.h"

/// Set preroll and postroll
class SetRollsDlg : public wxDialog
{
	public:
		SetRollsDlg(wxWindow *, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, wxXmlDocument &);
		int ShowModal();
		const ProdAuto::MxfDuration GetPreroll();
		const ProdAuto::MxfDuration GetPostroll();
	private:
		ProdAuto::MxfDuration SetRoll(const wxChar *, int, const ProdAuto::MxfDuration &, wxStaticBoxSizer *);
		void OnRollChange( wxScrollEvent& event );
		wxSlider * mPrerollCtrl;
		wxSlider * mPostrollCtrl;
		wxStaticBoxSizer * mPrerollBox;
		wxStaticBoxSizer * mPostrollBox;
		ProdAuto::MxfDuration mMaxPreroll;
		ProdAuto::MxfDuration mMaxPostroll;
		wxXmlDocument & mSavedState;

	enum
	{
		SLIDER_Preroll,
		SLIDER_Postroll,
	};

	DECLARE_EVENT_TABLE()
};

class wxGrid;
class wxGridEvent;

/// Set project names.
class SetProjectDlg : public wxDialog
{
	public:
		SetProjectDlg(wxWindow *, const wxSortedArrayString &, wxXmlDocument &);
		int ShowModal();
		const wxString GetSelectedProject();
		const wxSortedArrayString & GetProjectNames();
		static const wxString GetCurrentProjectName(const wxXmlDocument &);
	private:
		enum {
			ADD = wxID_HIGHEST + 1,
			DELETE,
			EDIT,
		};
		void OnAdd(wxCommandEvent&);
		void OnDelete(wxCommandEvent&);
		void OnEdit(wxCommandEvent&);
		void OnChoice(wxCommandEvent&);
		void EnterName(const wxString &, const wxString &, int = wxNOT_FOUND);
		void EnableButtons(bool);

//		wxListBox * mProjectList;
		wxButton * mDeleteButton;
		wxButton * mEditButton;
		wxButton * mOKButton;
		wxChoice * mProjectList;
		wxSortedArrayString mProjectNames; //needed because there is no sort style for a wxChoice
		wxXmlDocument & mSavedState;
	DECLARE_EVENT_TABLE()
};

/// A wxGrid which passes on character events - currently these aren't being used because they don't seem to be passed on
class MyGrid : public wxGrid
{
	public:
		MyGrid(wxWindow * parent, wxWindowID id, const wxPoint & pos = wxDefaultPosition, const wxSize & size = wxDefaultSize, long style = wxWANTS_CHARS, const wxString & name = wxPanelNameStr) : wxGrid(parent, id, pos, size, style, name) {};
	private:
		void OnChar(wxKeyEvent & event) { /* std::cerr << "grid char" << std::endl; ---this works*/
event.Skip(); };
	DECLARE_EVENT_TABLE()
};

/// Set tape IDs.
class SetTapeIdsDlg : public wxDialog
{
	public:
		SetTapeIdsDlg(wxWindow *, wxXmlDocument &, wxArrayString &, std::vector<bool> &);
		static wxXmlNode * GetTapeIdsNode(wxXmlDocument &);
		static const wxString GetTapeId(wxXmlNode *, const wxString &, wxArrayString * = 0);
		bool IsUpdated();
	private:
		enum {
			FILLINC = wxID_HIGHEST + 1,
			FILLCOPY,
			INCREMENT,
			GROUPINC,
			HELP,
			CLEAR,
		};
		void OnInitDlg(wxInitDialogEvent &);
		void OnEditorShown(wxGridEvent &);
		void OnEditorHidden(wxGridEvent &);
		void OnCellChange(wxGridEvent &);
		void OnCellRangeSelected(wxGridRangeSelectEvent &);
		void OnCellLeftClick(wxGridEvent &);
		void OnLabelLeftClick(wxGridEvent &);
		void OnIncrement(wxCommandEvent &);
		void OnGroupIncrement(wxCommandEvent &);
		void OnFillCopy(wxCommandEvent &);
		void OnFillInc(wxCommandEvent &);
		void OnHelp(wxCommandEvent &);
		void OnClear(wxCommandEvent &);
		bool ManipulateCells(const bool, const bool);
#if USING_ULONGLONG
		bool ManipulateCell(const int, const int, const bool, const bool, wxULongLong_t * = 0);
#else
		bool ManipulateCell(const int, const int, const bool, const bool, unsigned long * = 0);
#endif
		void FillCol(const bool, const bool);
		void UpdateRow(const int);
		void IncrementAsGroup(const bool commit);
		void SetBackgroundColour(int);
		void CheckForDuplicates();

		MyGrid * mGrid;
		wxButton * mIncrementButton;
		wxButton * mFillCopyButton;
		wxButton * mFillIncButton;
		wxButton * mGroupIncButton;
		wxButton * mClearButton;
		wxTextCtrl * mDupWarning;

		std::vector<bool> mEnabled;

		bool mEditing;
		bool mUpdated;

	DECLARE_EVENT_TABLE()
};

/// Set a timecode to jump to
class JumpToTimecodeDlg : public wxDialog
{
	public:
		JumpToTimecodeDlg(wxWindow *);
	private:
		enum
		{
			HRS = wxID_HIGHEST + 1,
			MINS,
			SECS,
			FRAMES
		};
		void OnTextChange(wxCommandEvent &);
		void OnEnter(wxCommandEvent &);
		void OnFocus(wxFocusEvent &);
		int CalcValue(wxCommandEvent &, unsigned int);
		MyTextCtrl * mHours, * mMins, * mSecs, * mFrames;
	DECLARE_EVENT_TABLE()
};

#define TEST_MAX_REC 180 //minutes
#define TEST_MAX_GAP 900 //seconds

DECLARE_EVENT_TYPE(wxEVT_TEST_DLG_MESSAGE, -1)


class wxSpinCtrl;
class wxSpinEvent;
class wxToggleButton;

/// Enable and set parameters for test mode
class TestModeDlg : public wxDialog
{
	public:
		TestModeDlg(wxWindow *);
		int ShowModal();
		~TestModeDlg();
		enum
		{
			RECORD,
			STOP,
		};
	private:
		enum
		{
			MIN_REC = wxID_HIGHEST + 1,
			MAX_REC,
			REC_RANDOM,
			MIN_GAP,
			MAX_GAP,
			GAP_RANDOM,
			RUN
		};
		void OnChangeMinRecTime(wxSpinEvent &);
		void OnChangeMaxRecTime(wxSpinEvent &);
		void OnChangeMinGapTime(wxSpinEvent &);
		void OnChangeMaxGapTime(wxSpinEvent &);
		void OnRun(wxCommandEvent &);
		void OnTimer(wxTimerEvent &);
		void Record(bool rec = true);
		wxSpinCtrl * mMinRecTime;
		wxSpinCtrl * mMaxRecTime;
		wxSpinCtrl * mMinGapTime;
		wxSpinCtrl * mMaxGapTime;
		wxToggleButton * mRunButton;
		wxStaticText * mRunStopMessage;
		wxStaticText * mRunStopCountdown;
		wxButton * mCancelButton;
		wxTimer * mTimer;
		wxTimeSpan mCountdown;
		bool mRecording;
	DECLARE_EVENT_TABLE()
};

#define N_CUE_POINT_COLOURS 9 //including default

/// Set cue point descriptions
class CuePointsDlg : public wxDialog
{
	public:
		CuePointsDlg(wxWindow *, wxXmlDocument &);
		int ShowModal(const wxString = wxT(""));
		void Shortcut(const int);
		const wxString GetDescription();
		size_t GetColourIndex();
		static ProdAuto::LocatorColour::EnumType GetColourCode(const size_t);
		static const wxColour GetColour(const size_t);
		static const wxColour GetLabelColour(const size_t);
	private:
		void OnEditorShown(wxGridEvent &);
		void OnEditorHidden(wxGridEvent &);
		void Load();
		void Save();
		void OnMenu(wxCommandEvent &);
		void OnOK(wxCommandEvent &);
		void OnCellLeftClick(wxGridEvent &);
		void OnSetGridRow(wxCommandEvent &);
		void OnLabelLeftClick(wxGridEvent &);
		void SetColour(const int, const int);
		void Reset();

		wxGrid * mGrid;
		wxButton * mOkButton;
		wxStaticText * mTimecodeDisplay;
		wxStaticText * mMessage;

		wxXmlNode * mCuePointsNode;
		wxString mDescription;
		wxColour mTextColour;
		wxColour mBackgroundColour;
		int mColour;
		int mCurrentRow;
		int mDescrColours[10];
	DECLARE_EVENT_TABLE()
};

#endif

/// Controls automatic chunking.  Active when not visible.  Changes when visible take immediate effect (i.e. no possibility to cancel)
class ChunkingDlg : public wxDialog
{
	public:
		ChunkingDlg(wxWindow *, wxButton *, Timepos *, wxXmlDocument &);
		int ShowModal();
		void RunFrom(const ProdAuto::MxfTimecode & = InvalidMxfTimecode, const ProdAuto::MxfDuration & = InvalidMxfDuration);
		void Reset();
	private:
		void OnChangeChunkSize(wxSpinEvent &);
		void OnEnable(wxCommandEvent &);
		void OnTimer(wxTimerEvent &);
		void SetCountdownLabel();

		wxSpinCtrl * mChunkSizeCtrl;
		wxToggleButton * mEnableButton;
		wxButton * mChunkButton;
		wxTimer * mCountdownTimer;
		Timepos * mTimepos;
		wxXmlDocument & mSavedState;
		unsigned int mCountdown;
		unsigned long mChunkLength;
		ProdAuto::MxfDuration mPostroll;
		bool mRecording;
	DECLARE_EVENT_TABLE()
};
