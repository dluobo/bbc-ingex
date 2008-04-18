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

#ifndef _DIALOGUES_H_
#define _DIALOGUES_H_
#include <wx/wx.h>
#include <wx/grid.h>
#include "ingexgui.h"

/// Set preroll and postroll
class SetRollsDlg : public wxDialog
{
	public:
		SetRollsDlg(wxWindow *, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration);
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
		SetProjectDlg(wxWindow *, wxXmlDocument &, bool);
		bool IsUpdated();
		const wxString GetSelectedProject();
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
		bool mUpdated;
		wxString mSelectedProject;
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
		void OnChar(wxKeyEvent &);
		bool ManipulateCells(const bool, const bool);
		bool ManipulateCell(const int, const int, const bool, const bool, wxULongLong_t * = 0);
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

class TestModeDlg : public wxDialog
{
	public:
		TestModeDlg(wxWindow *);
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
		wxButton * mCancelButton;
		wxTimer * mTimer;
		bool mRecording;
	DECLARE_EVENT_TABLE()
};

#endif
