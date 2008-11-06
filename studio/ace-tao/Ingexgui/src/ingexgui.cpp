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

#include <wx/wx.h>
#include "wx/tooltip.h" //for SetDelay()
#include "wx/splitter.h"
#include "wx/stdpaths.h"
#include "wx/filename.h"
#include "wx/xml/xml.h"
#include "wx/file.h"
#include "wx/tglbtn.h"
#include "help.h"
#include "ingexgui.xpm"
#include "stop.xpm"
#include "record.xpm"
#include "play.xpm"
#include "play_backwards.xpm"
#include "paused.xpm"
#include "happy.xpm"
#include "concerned.xpm"
#include "exclamation.xpm"
#include "ffx2.xpm"
#include "ffx4.xpm"
#include "ffx8.xpm"
#include "ffx16.xpm"
#include "ffx32.xpm"
#include "ffx64.xpm"
#include "frx2.xpm"
#include "frx4.xpm"
#include "frx8.xpm"
#include "frx16.xpm"
#include "frx32.xpm"
#include "frx64.xpm"
//#include "question.xpm"
#include "blank.xpm"

#include "ticktree.h"
#include "timepos.h"
#include "dialogues.h"
#include "dragbuttonlist.h"
#include "player.h"
#include "recordbutton.h"
#include "recordergroup.h"

//#include <wx/tooltip.h>

#include <wx/arrimpl.cpp>
#include <fstream>
WX_DEFINE_OBJARRAY(TakeInfoArray);
WX_DEFINE_OBJARRAY(ArrayOfTrackList_var);
WX_DEFINE_OBJARRAY(ArrayOfStringSeq_var);

DEFINE_EVENT_TYPE (wxEVT_RESTORE_LIST_LABEL);
//IMPLEMENT_DYNAMIC_CLASS(wxControllerThreadEvent, wxEvent)

BEGIN_EVENT_TABLE( MyTextCtrl, wxTextCtrl )
	EVT_SET_FOCUS( MyTextCtrl::OnFocus )
	EVT_KILL_FOCUS( MyTextCtrl::OnFocus )
END_EVENT_TABLE()


BEGIN_EVENT_TABLE( IngexguiFrame, wxFrame )
	EVT_CLOSE( IngexguiFrame::OnClose )
	EVT_MENU( MENU_Help, IngexguiFrame::OnHelp )
	EVT_MENU( MENU_About, IngexguiFrame::OnAbout )
	EVT_MENU( MENU_Record, IngexguiFrame::OnRecord )
	EVT_MENU( MENU_MarkCue, IngexguiFrame::OnCue )
	EVT_MENU( MENU_Stop, IngexguiFrame::OnStop )
	EVT_MENU( MENU_PrevTrack, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_NextTrack, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_3, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_4, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_Up, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_Down, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_PageUp, IngexguiFrame::OnPrevTake )
	EVT_MENU( MENU_PageDown, IngexguiFrame::OnNextTake )
	EVT_MENU( MENU_Home, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_End, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_J, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_K, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_L, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_Space, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_ClearLog, IngexguiFrame::OnClearLog )
	EVT_MENU( MENU_DisablePlayer, IngexguiFrame::OnDisablePlayer )
	EVT_MENU( MENU_PlayMOV, IngexguiFrame::OnPlayerOpenFile )
	EVT_MENU( MENU_PlayMXF, IngexguiFrame::OnPlayerOpenFile )
	EVT_MENU( MENU_AbsoluteTimecode, IngexguiFrame::OnPlayerOSDChange )
	EVT_MENU( MENU_RelativeTimecode, IngexguiFrame::OnPlayerOSDChange )
	EVT_MENU( MENU_NoOSD, IngexguiFrame::OnPlayerOSDChange )
	EVT_MENU( MENU_DisablePlayerSDIOSD, IngexguiFrame::OnPlayerOSDChange )
	EVT_MENU( MENU_AudioFollowsVideo, IngexguiFrame::OnAudioFollowsVideo )
	EVT_MENU( MENU_ExtOutput, IngexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_AccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_ExtAccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_ExtUnaccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_UnaccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( wxID_CLOSE, IngexguiFrame::OnQuit )
	EVT_MENU( MENU_SetProjectName, IngexguiFrame::OnSetProjectName )
	EVT_MENU( MENU_SetRolls, IngexguiFrame::OnSetRolls )
	EVT_MENU( MENU_SetCues, IngexguiFrame::OnSetCues )
	EVT_MENU( MENU_TestMode, IngexguiFrame::OnTestMode )
	EVT_BUTTON( BUTTON_RecorderListRefresh, IngexguiFrame::OnRecorderListRefresh )
	EVT_BUTTON( BUTTON_TapeId, IngexguiFrame::OnSetTapeIds )
	EVT_BUTTON( BUTTON_Record, IngexguiFrame::OnRecord )
	EVT_BUTTON( BUTTON_Stop, IngexguiFrame::OnStop )
	EVT_BUTTON( BUTTON_Cue, IngexguiFrame::OnCue )
	EVT_BUTTON( BUTTON_ClearDescription, IngexguiFrame::OnClearDescription )
	EVT_TEXT( TEXTCTRL_Description, IngexguiFrame::OnDescriptionChange )
	EVT_TEXT_ENTER( TEXTCTRL_Description, IngexguiFrame::OnDescriptionEnterKey )
	EVT_SET_FOCUS( IngexguiFrame::OnFocusGot )
	EVT_KILL_FOCUS( IngexguiFrame::OnFocusLost )
	EVT_LIST_ITEM_SELECTED( wxID_ANY, IngexguiFrame::OnEventSelection )
	EVT_LIST_ITEM_ACTIVATED( wxID_ANY, IngexguiFrame::OnEventActivated )
	EVT_LIST_BEGIN_LABEL_EDIT( wxID_ANY, IngexguiFrame::OnEventBeginEdit )
	EVT_LIST_END_LABEL_EDIT( wxID_ANY, IngexguiFrame::OnEventEndEdit )
	EVT_COMMAND( wxID_ANY, wxEVT_RESTORE_LIST_LABEL, IngexguiFrame::OnRestoreListLabel )
	EVT_BUTTON( BUTTON_PrevTake, IngexguiFrame::OnPrevTake )
	EVT_BUTTON( BUTTON_NextTake, IngexguiFrame::OnNextTake )
	EVT_BUTTON( BUTTON_JumpToTimecode, IngexguiFrame::OnJumpToTimecode )
	EVT_BUTTON( BUTTON_DeleteCue, IngexguiFrame::OnDeleteCue )
	EVT_TOGGLEBUTTON( BUTTON_PlayFile, IngexguiFrame::OnPlayFile )
	EVT_COMMAND( wxID_ANY, wxEVT_PLAYER_MESSAGE, IngexguiFrame::OnPlayerEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_TREE_MESSAGE, IngexguiFrame::OnTreeEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_RECORDERGROUP_MESSAGE, IngexguiFrame::OnRecorderGroupEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_TEST_DLG_MESSAGE, IngexguiFrame::OnTestDlgEvent )
	EVT_RADIOBUTTON( wxID_ANY, IngexguiFrame::OnPlaybackTrackSelect )

//	EVT_NOTEBOOK_PAGE_CHANGED(wxID_ANY, IngexguiFrame::OnNotebookPageChange)
//	EVT_SPINCTRL( -1, IngexguiFrame::OnSpinCtrl )
END_EVENT_TABLE()

IMPLEMENT_APP(IngexguiApp)

/// Application initialisation: creates and displays the main frame.
/// @return Always true.
bool IngexguiApp::OnInit()
{
	IngexguiFrame *frame = new IngexguiFrame(argc, argv);

	frame->Show(true);
	SetTopWindow(frame);
	return true;
} 

//Need to do this because accelerator/menu shortcuts don't always work
//Even so, sometimes (when control that takes cursor keys doesn't have focus?), no key events are generated for cursor keys
int IngexguiApp::FilterEvent(wxEvent& event)
{
//return -1;
	bool intercepted = false;
	if (wxEVT_CHAR == event.GetEventType()) {
		wxKeyEvent * keyEvent = (wxKeyEvent *) &event;
		intercepted = true;
		wxCommandEvent guiMenuEvent(wxEVT_COMMAND_MENU_SELECTED);
		switch (keyEvent->GetKeyCode()) {
			case WXK_UP : //controls get this before menu (at least in GTK)
				guiMenuEvent.SetId(IngexguiFrame::MENU_Up);
				break;
			case WXK_DOWN : //controls get this before menu (at least in GTK)
				guiMenuEvent.SetId(IngexguiFrame::MENU_Down);
				break;
//			case WXK_PAGEUP : //menus don't recognise this as a shortcut (at least in GTK)
//				guiMenuEvent.SetId(IngexguiFrame::MENU_PageUp);
//				GetTopWindow()->AddPendingEvent(guiMenuEvent);
////				guiButtonEvent.SetId(IngexguiFrame::BUTTON_PrevTake);
////				GetTopWindow()->AddPendingEvent(guiButtonEvent);
//				break;
//			case WXK_PAGEDOWN : //menus don't recognise this as a shortcut (at least in GTK)
//				guiButtonEvent.SetId(IngexguiFrame::BUTTON_NextTake);
//				GetTopWindow()->AddPendingEvent(guiButtonEvent);
//				break;

//			case ' ' :
//				break;
			default:
				intercepted = false;
		}
		if (intercepted) {
			GetTopWindow()->AddPendingEvent(guiMenuEvent);
		}
	}


	if (intercepted) {
		return true; //if false is returned, the event is processed again
	}
	else {
		return -1; //process as normal
	}
}


/// Lays out all controls and menus, and creates persistent objects.
/// @param argc Command line argument count.
/// @param argv Command line argument vector.
IngexguiFrame::IngexguiFrame(int argc, wxChar** argv)
	: wxFrame((wxFrame *)0, wxID_ANY, wxT("ingexgui")/*, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS* - this doesn't prevent cursor keys being lost, as hoped */), mStatus(STOPPED), mEndEvent(-1), mCurrentTakeInfo(0), mDescriptionControlHasFocus(false), mFileModeSelectedTrack(0)
{
//	SetWindowStyle(GetWindowStyle() | wxWANTS_CHARS);

	wxLog::SetActiveTarget(new wxLogStream(new ofstream(wxDateTime::Now().Format(wxT("ingexguiLog-%y%m%d-%H%M%S")).mb_str(*wxConvCurrent))));

        Log(wxT("Controller started on ") + wxDateTime::Now().FormatISODate());
	mHelpDlg = new HelpDlg(this); //modeless to allow it to remain visible while using the app
	//icon

	wxIcon icon(ingexgui_xpm);
	SetIcon(icon);
	//menu
	wxMenuBar * menuBar = new wxMenuBar;

	//Misc menu
	wxMenu * menuMisc = new wxMenu;
	menuMisc->Append(MENU_SetProjectName, wxT("Set project name..."));
	menuMisc->Append(MENU_SetRolls, wxT("Set pre- and post-roll..."));
	menuMisc->Append(MENU_SetCues, wxT("Edit cue point descriptions..."));
	wxMenuItem * clearLogItem = menuMisc->Append(MENU_ClearLog, wxT("Clear recording log"));
	clearLogItem->Enable(false);
	menuMisc->AppendCheckItem(MENU_AutoClear, wxT("Don't log multiple recordings"));
	menuMisc->Append(MENU_TestMode, wxT("Test mode..."));
	menuMisc->Append(wxID_CLOSE, wxT("Quit"));
	menuBar->Append(menuMisc, wxT("&Misc"));

	//Player menu
	wxMenu * menuPlayer = new wxMenu;
	menuPlayer->AppendCheckItem(MENU_DisablePlayer, wxT("Disable player"));
	menuPlayer->Append(MENU_PlayMOV, wxT("Play MOV file..."));
	menuPlayer->Append(MENU_PlayMXF, wxT("Play MXF file(s)..."));
	wxMenu * menuPlayerType = new wxMenu;
	wxMenu * menuPlayerOSD = new wxMenu;
#ifdef HAVE_DVS
	menuPlayerType->AppendRadioItem(MENU_ExtAccelOutput, wxT("External monitor and computer screen (accelerated if possible)")); //first item is selected by default
#endif
	menuPlayerType->AppendRadioItem(MENU_AccelOutput, wxT("Computer screen (accelerated if possible)")); //selected by default if no DVS card
#ifdef HAVE_DVS
	menuPlayerType->AppendRadioItem(MENU_ExtOutput, wxT("External monitor"));
	menuPlayerType->AppendRadioItem(MENU_ExtUnaccelOutput, wxT("External monitor and computer screen unaccelerated (use if accelerated fails)"));
#endif
	menuPlayerType->AppendRadioItem(MENU_UnaccelOutput, wxT("Computer screen unaccelerated (use if accelerated fails)"));

	menuPlayerOSD->AppendRadioItem(MENU_AbsoluteTimecode, wxT("&Absolute timecode")); //first item is selected by default
	menuPlayerOSD->AppendRadioItem(MENU_RelativeTimecode, wxT("&Relative timecode"));
	menuPlayerOSD->AppendRadioItem(MENU_NoOSD, wxT("&OSD Off"));
	menuPlayer->Append(MENU_PlayerType, wxT("Player type"), menuPlayerType);
	menuPlayer->Append(MENU_PlayerOSD, wxT("Player On-Screen Display"), menuPlayerOSD);
	menuPlayer->AppendCheckItem(MENU_AudioFollowsVideo, wxT("Audio corresponds to video source displayed"));
#ifdef HAVE_DVS
	menuPlayer->AppendCheckItem(MENU_DisablePlayerSDIOSD, wxT("Disable external monitor On-screen Display"));
#endif
	menuBar->Append(menuPlayer, wxT("&Player"));

	//Help menu
	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(MENU_Help, wxT("&Help..."));
	menuHelp->Append(MENU_About, wxT("&About..."));
	menuBar->Append(menuHelp, wxT("&Help"));

	//Shortcuts menu
	wxMenu *menuShortcuts = new wxMenu;
	menuShortcuts->Append(MENU_Record, wxT("Record\tF1"));
	menuShortcuts->Append(MENU_MarkCue, wxT("Mark Cue\tF2")); //NB this must correspond to the "insert blank cue" key in CuePointsDlg
	menuShortcuts->Append(MENU_Stop, wxT("Stop\tShift+F5"));
	menuShortcuts->Append(MENU_PrevTrack, wxT("Select previous playback track\tF7"));
	menuShortcuts->Append(MENU_NextTrack, wxT("Select next playback track\tF8"));
	menuShortcuts->Append(MENU_Up, wxT("Move up cue point list\tUP"));
	menuShortcuts->Append(MENU_Down, wxT("Move down cue point list\tDOWN"));
	menuShortcuts->Append(MENU_PageUp, wxT("Move to previous take\tPGUP"));
	menuShortcuts->Append(MENU_PageDown, wxT("Move to next take\tPGDN"));
	menuShortcuts->Append(MENU_Home, wxT("Move to start of first take\tHOME"));
	menuShortcuts->Append(MENU_End, wxT("Move to start of last take\tEND"));
	menuShortcuts->Append(MENU_J, wxT("Play/fast backwards\tJ"));
	menuShortcuts->Append(MENU_K, wxT("Pause\tK"));
	menuShortcuts->Append(MENU_L, wxT("Play/fast forwards\tL"));
	menuShortcuts->Append(MENU_Space, wxT("Play/Pause\tSPACE"));
	menuShortcuts->Append(MENU_3, wxT("Step backwards\t3"));
	menuShortcuts->Append(MENU_4, wxT("Step forwards\t4"));
	menuHelp->Append(-1, wxT("Shortcuts"), menuShortcuts); //add this after adding menu items to it

	SetMenuBar(menuBar);
#ifdef HAVE_DVS
	mPlayer = new prodauto::Player(this, true, prodauto::DUAL_DVS_AUTO_OUTPUT, prodauto::SOURCE_TIMECODE); //must delete this explicitly on app exit
	if (!mPlayer->ExtOutputIsAvailable()) {
		GetMenuBar()->FindItem(MENU_ExtAccelOutput)->Enable(false);
		GetMenuBar()->FindItem(MENU_ExtOutput)->Enable(false);
		GetMenuBar()->FindItem(MENU_ExtUnaccelOutput)->Enable(false);
		mPlayer->SetOutputType(prodauto::X11_AUTO_OUTPUT);
		GetMenuBar()->FindItem(MENU_AccelOutput)->Check();
	}
#else
	mPlayer = new prodauto::Player(this, true, prodauto::X11_AUTO_OUTPUT, prodauto::SOURCE_TIMECODE); //must delete this explicitly on app exit
#endif
	mPlayer->AudioFollowsVideo(GetMenuBar()->FindItem(MENU_AudioFollowsVideo)->IsChecked());
	CreateStatusBar();

	//saved state
	mSavedStateFilename = wxStandardPaths::Get().GetUserConfigDir() + wxFileName::GetPathSeparator() + SAVED_STATE_FILENAME;
	if (!wxFile::Exists(mSavedStateFilename)) {
		wxMessageBox(wxT("No saved preferences file found.  Default values will be used."), wxT("No saved preferences"));
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode);
	}
	else if (!mSavedState.Load(mSavedStateFilename)) { //already produces a warning message box
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode);
	}
	else if (wxT("Ingexgui") != mSavedState.GetRoot()->GetName()) {
		wxMessageBox(wxT("Saved preferences file \"") + mSavedStateFilename + wxT("\": has unrecognised data: recreating with default values."), wxT("Unrecognised saved preferences"), wxICON_EXCLAMATION);
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode); //dialogue will detect this as updated
	}
	mRecorderGroup = new RecorderGroupCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, argc, argv, &mSavedState);
	//currently selected project
	wxString currentProject = SetProjectDlg::GetCurrentProjectName(mSavedState);
	if (currentProject.IsEmpty()) {
		SetTitle(TITLE);
	}
	else {
		SetTitle(wxString(TITLE) + wxT(": ") + currentProject);
		mRecorderGroup->SetCurrentProjectName(currentProject);
	}
	//window
	wxBoxSizer * sizer1V = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer1V);

	//recorder selection row
	wxBoxSizer * sizer2aH = new wxBoxSizer(wxHORIZONTAL);
	sizer1V->Add(sizer2aH, 0, wxEXPAND);
	wxStaticBoxSizer * recorderSelectionBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Recorder Selection"));
	sizer2aH->Add(recorderSelectionBox, 1, wxALL, CONTROL_BORDER);

	recorderSelectionBox->Add(mRecorderGroup, 1, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
	mRecorderListRefreshButton = new wxButton(this, BUTTON_RecorderListRefresh, wxT("Refresh list"));
	recorderSelectionBox->Add(mRecorderListRefreshButton, 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
	mTapeIdButton = new wxButton(this, BUTTON_TapeId, wxT("Set tape IDs"));
	sizer2aH->Add(mTapeIdButton, 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);

	//indicator row
	wxBoxSizer * sizer2bH = new wxBoxSizer(wxHORIZONTAL);
	sizer1V->Add(sizer2bH);
	mStatusCtrl = new wxStaticBitmap(this, -1, wxBitmap(blank));
	sizer2bH->Add(mStatusCtrl, 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);

	mTimecodeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Studio timecode"));
	sizer2bH->Add(mTimecodeBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	wxStaticText * timecodeDisplay = new wxStaticText(this, -1, UNKNOWN_TIMECODE); //need to set the contents to read the correct size for the position box and display
	wxFont * font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
	timecodeDisplay->SetFont(*font);
	timecodeDisplay->SetToolTip(wxT("Time-of-day timecode"));
	mTimecodeBox->Add(timecodeDisplay, 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);
	mAlertCtrl = new wxStaticBitmap(this, -1, wxBitmap(blank));
	mAlertCtrl->SetToolTip(wxT(""));
	sizer2bH->Add(mAlertCtrl, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE, CONTROL_BORDER);
	wxStaticBoxSizer * positionBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Position"));
	sizer2bH->Add(positionBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	wxStaticText * positionDisplay = new wxStaticText(this, -1, NO_POSITION, wxDefaultPosition, timecodeDisplay->GetSize());
	positionDisplay->SetFont(*font);
	delete font;
	positionDisplay->SetToolTip(wxT("Current recording's position"));
	positionBox->Add(positionDisplay, 0, wxALIGN_CENTRE);

	mTimepos = new Timepos(timecodeDisplay, positionDisplay);

	//transport controls
	wxBoxSizer * sizer2cH = new wxBoxSizer(wxHORIZONTAL);
	sizer1V->Add(sizer2cH);
	mRecordButton = new RecordButton(this, BUTTON_Record, wxT("Record (999 sources)")); //to set a minimum size
	mRecordButton->SetMinSize(mRecordButton->GetSize());
	sizer2cH->Add(mRecordButton, 0, wxALL, CONTROL_BORDER);
	mStopButton = new wxButton(this, BUTTON_Stop, wxT("Stop"));
	sizer2cH->Add(mStopButton, 0, wxALL, CONTROL_BORDER);
	mCueButton = new wxButton(this, BUTTON_Cue, wxT("Mark Cue"));
	sizer2cH->Add(mCueButton, 0, wxALL, CONTROL_BORDER);

	//splitter window containing everything else
	wxSplitterWindow * splitterWindow = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
	sizer1V->Add(splitterWindow, 1, wxEXPAND | wxALL);

	//notebook panel and notebook
	wxPanel * notebookPanel = new wxPanel(splitterWindow); //need a panel so we can have a sizer so we can have a control border...
	notebookPanel->SetNextHandler(this);
	wxBoxSizer * notebookPanelSizer = new wxBoxSizer(wxVERTICAL);
	notebookPanel->SetSizer(notebookPanelSizer);
	mNotebook = new wxNotebook(notebookPanel, wxID_ANY);
	notebookPanelSizer->Add(mNotebook, 1, wxEXPAND + wxALL, CONTROL_BORDER);

	//notebook record page
	wxPanel * recordPage = new wxPanel(mNotebook); //need this as can't add a sizer directly to a notebook
	recordPage->SetNextHandler(this);
	mNotebook->AddPage(recordPage, wxT("Record"));
	wxBoxSizer * recordPageSizer = new wxBoxSizer(wxVERTICAL);
	recordPage->SetSizer(recordPageSizer);
	wxStaticBoxSizer * recProjectNameBox = new wxStaticBoxSizer(wxHORIZONTAL, recordPage, wxT("Project"));
	recordPageSizer->Add(recProjectNameBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mRecProjectNameCtrl = new wxStaticText(recordPage, wxID_ANY, mRecorderGroup->GetCurrentProjectName());
	recProjectNameBox->Add(mRecProjectNameCtrl, 1, wxEXPAND);
	wxStaticBoxSizer * descriptionBox = new wxStaticBoxSizer(wxHORIZONTAL, recordPage, wxT("Description"));
	recordPageSizer->Add(descriptionBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mDescriptionCtrl = new MyTextCtrl(recordPage, TEXTCTRL_Description, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	mDescClearButton = new wxButton(recordPage, BUTTON_ClearDescription, wxT("Clear"));
	mDescClearButton->Disable();
	descriptionBox->Add(mDescriptionCtrl, 1, wxEXPAND);
	descriptionBox->Add(mDescClearButton, 0, wxLEFT, CONTROL_BORDER);
	mTree = new TickTreeCtrl(recordPage, TREE, wxDefaultPosition, wxSize(100, 100), wxT("Recorders")); //the y value of the size is the significant one.  Makes it a bit taller than default
	recordPageSizer->Add(mTree, 1, wxEXPAND | wxALL, CONTROL_BORDER);

	//display a sensible amount (i.e total initial size of record tab) in the upper part of the splitter window 
	splitterWindow->SetMinimumPaneSize(notebookPanel->GetBestSize().y);

	//notebook playback page
	wxPanel * playbackPage = new wxPanel(mNotebook); //need this as can't add a sizer directly to a notebook
	playbackPage->SetNextHandler(this); //allow track select radio button events to propagate here
	mNotebook->AddPage(playbackPage, wxT("Playback"));
	wxBoxSizer * playbackPageSizer = new wxBoxSizer(wxVERTICAL);
	playbackPage->SetSizer(playbackPageSizer);
	mPlayProjectNameBox = new wxStaticBoxSizer(wxHORIZONTAL, playbackPage, wxT("Project"));
	playbackPageSizer->Add(mPlayProjectNameBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mPlayProjectNameCtrl = new wxStaticText(playbackPage, wxID_ANY, wxT(""));
	mPlayProjectNameBox->Add(mPlayProjectNameCtrl, 1, wxEXPAND);
	wxStaticBoxSizer * playbackTracksBox = new wxStaticBoxSizer(wxHORIZONTAL, playbackPage, wxT("Tracks"));
	playbackPageSizer->Add(playbackTracksBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPlaybackTrackSelector = new DragButtonList(playbackPage);
	playbackTracksBox->Add(mPlaybackTrackSelector, 1, wxEXPAND);

	//event panel
	wxPanel * eventPanel = new wxPanel(splitterWindow);
	splitterWindow->SplitHorizontally(notebookPanel, eventPanel); //default 50% split
	eventPanel->SetNextHandler(this);
	wxBoxSizer * eventPanelSizer = new wxBoxSizer(wxVERTICAL);
	eventPanel->SetSizer(eventPanelSizer);

	//take navigation buttons
	wxBoxSizer * sizer2dH = new wxBoxSizer(wxHORIZONTAL);
	eventPanelSizer->Add(sizer2dH, 0, wxEXPAND);
	mPrevTakeButton = new wxButton(eventPanel, BUTTON_PrevTake, wxT("Start of Take")); //init with longest string to make button big enough
	sizer2dH->Add(mPrevTakeButton, 0, wxALL, CONTROL_BORDER);
	mNextTakeButton = new wxButton(eventPanel, BUTTON_NextTake, wxT("End of Take"));
	sizer2dH->Add(mNextTakeButton, 0, wxALL, CONTROL_BORDER);
//	mJumpToTimecodeButton = new wxButton(eventPanel, BUTTON_JumpToTimecode, wxT("Jump to Timecode"));
//	sizer2dH->Add(mJumpToTimecodeButton, 0, wxALL, CONTROL_BORDER);
	mPlayFileButton = new wxToggleButton(eventPanel, BUTTON_PlayFile, wxT("Play files"));
	sizer2dH->Add(mPlayFileButton, 0, wxALL, CONTROL_BORDER);
	sizer2dH->AddStretchSpacer();
	mDeleteCueButton = new wxButton(eventPanel, BUTTON_DeleteCue, wxT("Delete Cue Point"));
	sizer2dH->Add(mDeleteCueButton, 0, wxALL, CONTROL_BORDER);

	//event list
	mEventList = new wxListView(eventPanel, -1, wxDefaultPosition, wxSize(splitterWindow->GetClientSize().x, EVENT_LIST_HEIGHT), wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER|wxLC_EDIT_LABELS/*|wxALWAYS_SHOW_SB*/); //ALWAYS_SHOW_SB results in a disabled scrollbar on GTK (wx 2.8)
	//set up the columns
	wxListItem itemCol;
	itemCol.SetText(wxT("Event"));
	itemCol.SetWidth(EVENT_COL_WIDTH);
	mEventList->InsertColumn(0, itemCol);
#ifdef __WIN32__
	mEventList->SetColumnWidth(0, 140);
#endif
	itemCol.SetText(wxT("Timecode"));
	mEventList->InsertColumn(1, itemCol); //will set width of this automatically later
#ifdef __WIN32__
	mEventList->SetColumnWidth(1, 125);
#endif
	itemCol.SetText(wxT("Position"));
	mEventList->InsertColumn(2, itemCol); //will set width of this automatically later
#ifdef __WIN32__
	mEventList->SetColumnWidth(2, 125);
#endif
	itemCol.SetText(wxT("Description"));
	mEventList->InsertColumn(3, itemCol); //will set width of this automatically later
	eventPanelSizer->Add(mEventList, 1, wxEXPAND + wxALL, CONTROL_BORDER);

	wxToolTip::SetDelay(TOOLTIP_DELAY);

	Fit();
	splitterWindow->SetMinimumPaneSize(1); //to allow it to be almost but not quite closed.  Do this after Fit() or the earlier value will not be reflected in the window layout.
	SetMinSize(GetSize()); //prevents scrollbar of event list disappearing oddly

	mCuePointsDlg = new CuePointsDlg(this, mSavedState); //this is done now so that a file doesn't have to be read each time a cue point is marked
	mTestModeDlg = new TestModeDlg(this); //this is done now so that it can remember its parameters between invocations

	ResetToDisconnected();
	SetStatus(STOPPED);
}

/// Responds to an application close event by closing, preventing closure or seeking confirmation, depending on the circumstances.
/// @param event The close event.
void IngexguiFrame::OnClose(wxCloseEvent & event)
{
	if (event.CanVeto() && RECORDING == mStatus) {
		wxMessageBox(wxT("You cannot close the application while recording: stop the recording first"), wxT("Cannot close while recording"), wxICON_EXCLAMATION);
		event.Veto();
	}	
	else if (!event.CanVeto() || !mTree->HasRecorders() || wxYES == wxMessageBox(wxT("Are you sure?  You are not disconnected"), wxT("Confirmation of Quit"), wxYES_NO | wxICON_QUESTION)) {
		if (mTree->HasRecorders()) {
			delete mRecorderGroup; //or doesn't exit
		}
		Destroy();
		Log(wxT("Application closed."));
	}
	else {
		event.Veto();
	}
}

/// Responds to an About menu request by showing the appropriate dialogue.
/// @param event The command event.
void IngexguiFrame::OnAbout( wxCommandEvent& WXUNUSED( event ) )
{
	AboutDlg dlg(this);
	dlg.ShowModal();
}

/// Responds to a Help menu request by displaying the help dialogue modelessly.
/// @param event The command event.
void IngexguiFrame::OnHelp( wxCommandEvent& WXUNUSED( event ) )
{
	//modeless help dialog
	mHelpDlg->Show();
}

/// Responds to a Project Names menu request by showing the appropriate dialogue and updating state if user makes changes.
/// @param event The command event.
void IngexguiFrame::OnSetProjectName( wxCommandEvent& WXUNUSED( event ) )
{
	SetProjectName();
}

/// Shows the set project name dialogue and updates state if user makes changes.
void IngexguiFrame::SetProjectName()
{
	SetProjectDlg dlg(this, mRecorderGroup->GetProjectNames(), mSavedState);
	if (wxID_OK == dlg.ShowModal()) {
		mRecorderGroup->SetProjectNames(dlg.GetProjectNames());
		mRecorderGroup->SetCurrentProjectName(dlg.GetSelectedProject());
		SetTitle(wxString(TITLE) + wxT(": ") + dlg.GetSelectedProject());
		mRecProjectNameCtrl->SetLabel(dlg.GetSelectedProject());
		mSavedState.Save(mSavedStateFilename);
	}
}

/// Responds to a Test mode request by showing the appropriate dialogue, which uses events to send commands back to the frame.
/// @param event The command event.
void IngexguiFrame::OnTestMode( wxCommandEvent& WXUNUSED( event ) )
{
	mTestModeDlg->ShowModal();
}

/// Responds to a Set Rolls menu request by showing the appropriate dialogue and informing the recorder group if user makes changes.
/// @param event The command event.
void IngexguiFrame::OnSetRolls( wxCommandEvent& WXUNUSED( event ) )
{
	SetRollsDlg dlg(this, mRecorderGroup->GetPreroll(), mRecorderGroup->GetMaxPreroll(), mRecorderGroup->GetPostroll(), mRecorderGroup->GetMaxPostroll(), mSavedState);
	if (wxID_OK == dlg.ShowModal()) {
		mSavedState.Save(mSavedStateFilename);
		//Update current values
		mRecorderGroup->SetPreroll(dlg.GetPreroll());
		mRecorderGroup->SetPostroll(dlg.GetPostroll());
	}
}

/// Responds to a Set Cue Descriptions menu request by showing the appropriate dialogue.
/// @param event The command event.
void IngexguiFrame::OnSetCues( wxCommandEvent& WXUNUSED( event ) )
{
	if (wxID_OK == mCuePointsDlg->ShowModal()) {
		mSavedState.Save(mSavedStateFilename);
	}
}

/// Responds to a Refresh Recorder List request by initiating the process of obtaining a list of recorders.
/// @param event The command event.
void IngexguiFrame::OnRecorderListRefresh( wxCommandEvent& WXUNUSED( event ) )
{
	mRecorderGroup->StartGettingRecorders();
}

/// Responds to the tape IDs button being pressed by showing the appropriate dialogue and updating state if user makes changes.
/// @param event The button event.
void IngexguiFrame::OnSetTapeIds( wxCommandEvent& WXUNUSED( event ) )
{
	wxArrayString names;
	std::vector<bool> enabled;
	mTree->GetPackageNames(names, enabled);
	SetTapeIdsDlg dlg(this, mSavedState, names, enabled);
	if (dlg.IsUpdated()) {
		mSavedState.Save(mSavedStateFilename);
		mTree->UpdateTapeIds(mSavedState); //will issue an event to update the record and tape ID buttons
	}
}

/// Responds to a Clear Log menu request.
/// @param event The command event.
void IngexguiFrame::OnClearLog( wxCommandEvent& WXUNUSED(event) )
{
	ClearLog();
}

/// Responds to OSD-related menu requests by communicating with the player.
/// @param event The command event.
void IngexguiFrame::OnPlayerOSDChange( wxCommandEvent& WXUNUSED(event) )
{
	if (GetMenuBar()->FindItem(MENU_AbsoluteTimecode)->IsChecked()) {
		mPlayer->SetOSD(prodauto::SOURCE_TIMECODE);
#ifdef HAVE_DVS
		GetMenuBar()->FindItem(MENU_DisablePlayerSDIOSD)->Enable();
#endif
	}
	else if (GetMenuBar()->FindItem(MENU_RelativeTimecode)->IsChecked()) {
		mPlayer->SetOSD(prodauto::CONTROL_TIMECODE);
#ifdef HAVE_DVS
		GetMenuBar()->FindItem(MENU_DisablePlayerSDIOSD)->Enable();
#endif
	}
	else { //OSD off
		mPlayer->SetOSD(prodauto::OSD_OFF);
#ifdef HAVE_DVS
		GetMenuBar()->FindItem(MENU_DisablePlayerSDIOSD)->Enable(false);
#endif
	}
#ifdef HAVE_DVS
	mPlayer->EnableSDIOSD(!GetMenuBar()->FindItem(MENU_DisablePlayerSDIOSD)->IsChecked());
#endif
}

/// Responds to a player type/size menu requests by communicating with the player and enabling/disabling other menu items as appropriate.
/// @param event The command event.
void IngexguiFrame::OnPlayerOutputTypeChange(wxCommandEvent& WXUNUSED(event))
{
	if (GetMenuBar()->FindItem(MENU_AccelOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::X11_AUTO_OUTPUT);
	}
#ifdef HAVE_DVS
	else if (GetMenuBar()->FindItem(MENU_ExtOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::DVS_OUTPUT);
	}
	else if (GetMenuBar()->FindItem(MENU_ExtAccelOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::DUAL_DVS_AUTO_OUTPUT);
	}
	else if (GetMenuBar()->FindItem(MENU_ExtUnaccelOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::DUAL_DVS_X11_OUTPUT);
	}
#endif
	else {
		//on-screen unaccelerated
		mPlayer->SetOutputType(prodauto::X11_OUTPUT);
	}
}

/// Responds to player audio follows video menu request
/// @param event The command event.
void IngexguiFrame::OnAudioFollowsVideo(wxCommandEvent& WXUNUSED(event))
{
	mPlayer->AudioFollowsVideo(GetMenuBar()->FindItem(MENU_AudioFollowsVideo)->IsChecked());
}

/// Responds to a menu quit event by attempting to close the app.
/// @param event The command event.
void IngexguiFrame::OnQuit( wxCommandEvent& WXUNUSED( event ) )
{
	Close();
}

/// Responds to various shortcut menu events.
/// @param event The command event.
void IngexguiFrame::OnShortcut( wxCommandEvent& event )
{
	switch (event.GetId()) {
		case MENU_PrevTrack :
			//previous track
			mPlaybackTrackSelector->EarlierTrack(true);
			break;
		case MENU_NextTrack :
			//next track
			mPlaybackTrackSelector->LaterTrack(true);
			break;
		case MENU_Up :
			//up one event
			SelectAdjacentEvent(false);
			break;
		case MENU_Down :
			//down one event
			SelectAdjacentEvent(true);
			break;
		case MENU_Home :
			//start of first take
			mEventList->Select(0);
			break;
		case MENU_End :
			//start of last take
			mEventList->Select(mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartIndex()); //Menu item is disabled if no takes!!
			break;
		case MENU_J :
			//play backwards
			Play(true);
			break;
		case MENU_K :
			//pause
			mPlayer->Pause();
			break;
		case MENU_L :
			//play
			Play();
			break;
		case MENU_Space :
			//toggle play in current direction and pause
			if (PAUSED == mStatus) {
				Play(mLastPlayingBackwards);
			}
			else {
				mPlayer->Pause();
			}
			break;
		case MENU_3 :
			mPlayer->Step(false);
			break;
		case MENU_4 :
			mPlayer->Step(true);
			break;
	}
}

/// Moves up or down the event list by one item, if possible
/// @param down True to move downwards; false to move upwards.
void IngexguiFrame::SelectAdjacentEvent(bool down) {
	long item = mEventList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (-1 == item) {
		//nothing selected
		mEventList->Select(down ? mEventList->GetItemCount() - 1 : 0); //select first item
	}
	else if (!down && 0 != item) {
		//not the first item
		mEventList->Select(--item); //select next item up
	}
	else if (down && mEventList->GetItemCount() - 1 != item) {
		//not the last item
		mEventList->Select(++item); //select next item up
	}
	mEventList->EnsureVisible(item); //scroll if necessary
}

/// Responds to record button being pressed.
/// @param event The button event.
void IngexguiFrame::OnRecord( wxCommandEvent& WXUNUSED( event ) )
{
	if (GetMenuBar()->FindItem(MENU_AutoClear)->IsChecked()) {
		//clear everything away because we're starting again
		ClearLog(); //resets player
	}
	else {
		ResetPlayer();
	}
	ProdAuto::MxfTimecode now;
	mTimepos->GetTimecode(&now);
	Log(wxT("Record button pressed @ ") + Timepos::FormatTimecode(now));
	mRecorderGroup->RecordAll(now);
	SetStatus(RUNNING_UP);
}

/// Responds to the Previous Take button/menu item by moving to the start of the take or the start of the previous.
/// @param event The command event.
void IngexguiFrame::OnPrevTake( wxCommandEvent & WXUNUSED(event))
{
	//get the index of the take info corresponding to the current selection
	wxListItem item;
	item.SetId(mEventList->GetFirstSelected());
	mEventList->GetItem(item); //the take info index is in this item's data

	//set the selection

	if (mCurrentTakeInfo && (unsigned long) mEventList->GetFirstSelected() == mCurrentTakeInfo->GetStartIndex() && !mPlayer->Within()) {
		//the start of this take is selected: select the start of the previous
		if (item.GetData()) {
			//not the first take
			mEventList->Select(mTakeInfoArray.Item(item.GetData() - 1).GetStartIndex());
		}
	}
	else {
		//somewhere within a take: go to the start of it
		mEventList->Select(mTakeInfoArray.Item(item.GetData()).GetStartIndex());
	}
	mEventList->EnsureVisible(mEventList->GetFirstSelected());

	UpdatePlayerAndEventControls(false, true); //force player to jump to cue, as this won't happen if the selected event hasn't changed
}

/// Responds to the Next Take button/menu item by moving to the start of the next take
/// @param event The command event.
void IngexguiFrame::OnNextTake( wxCommandEvent& WXUNUSED(event))
{
	//get the index of the take info corresponding to the current selection
	wxListItem item;
	item.SetId(mEventList->GetFirstSelected());
	mEventList->GetItem(item); //the take info index is in this item's data

	//set the selection
	if (item.GetData() == mTakeInfoArray.GetCount() - 1) {
		//In the last take: move to the end
		mEventList->Select(mEventList->GetItemCount() - 1);
	}
	else {
		//Not in the last take: move to the start of the next
		mEventList->Select(mTakeInfoArray.Item(item.GetData() + 1).GetStartIndex());
	}
	mEventList->EnsureVisible(mEventList->GetFirstSelected());

	UpdatePlayerAndEventControls();
}

/// Responds to the an event from the player.
/// @param event The command event.
/// The following event IDs are recognised, with corresponding data values (all enumerations being in the prodauto namespace):
///	NEW_FILESET: Sets enable state of track select buttons and selects one.
///		Event client data: vector of enable states (tracks present)
///		Event int: the selected tracks.
///	STATE_CHANGE: Reflect the player's status.
///		Event int: PLAY, PLAY_BACKWARDS, PAUSE, STOP, CLOSE.
///	CUE_POINT: Select indicated point in the event list and pause player if at end/start of file depending on playback direction
///		Event int: Event list index for cue point.
///	FRAME_DISPLAYED: Update time counter.
///		Event int: Non-zero for valid frame number.
///		Event extra long: Frame number.
///	WITHIN_TAKE: Update "Previous Take" button because it will now cause a jump to the start of the current take.
///	CLOSE_REQ: Switch the player to external output only, or disable completely if external output is not being used.
///	QUADRANT_CLICK: Toggle between full-screen and single source defined by the quadrant.
///		Event int: the quadrant clicked on.
///	FILE_MODE: Reflect status in controls.
///		Event int: non-zero for file mode enabled.
///	KEYPRESS: Respond to the user pressing a shortcut key in the player window.
///		Event int: X-windows key value.
void IngexguiFrame::OnPlayerEvent(wxCommandEvent& event) {
	switch ((prodauto::PlayerEventType) event.GetId()) {
		case prodauto::NEW_FILESET :
			mPlaybackTrackSelector->EnableAndSelectTracks((std::vector<bool> *) event.GetClientData(), event.GetInt());
			break;
		case prodauto::STATE_CHANGE :
			switch (event.GetInt()) {
				case prodauto::PLAY :
					SetStatus(PLAYING);
					break;
				case prodauto::PLAY_BACKWARDS :
					SetStatus(PLAYING_BACKWARDS);
					break;
				case prodauto::PAUSE :
					if (RECORDING != mStatus && RUNNING_UP != mStatus && RUNNING_DOWN != mStatus) { //player is in control of state (This check prevents a sitaution where when toggling rapidly between play and record we get a paused status but recorders are still recording. Record button is not enabled, because recorder selector is not enabled, because RecorderGroup::Stop() hasn't been called.  Position is still running and tree/status are indicating problems.  The only way out of this is to quit - stopping the recorders with another GUI leaves the recorder selector and record buttons disabled.)
						SetStatus(PAUSED);
					}
					break;
				case prodauto::STOP :
					if (RECORDING != mStatus && RUNNING_UP != mStatus && RUNNING_DOWN != mStatus) { //player is in control of state
						SetStatus(STOPPED);
					}
					break;
				case prodauto::CLOSE :
					//Issued on Enable(false) _and_ Reset()
					UpdatePlayerAndEventControls();
					break;
			}
			break;
		case prodauto::SPEED_CHANGE :
			switch (event.GetInt()) {
				case 1 :
					mStatusCtrl->SetBitmap(play);
					break;
				case 2 :
					mStatusCtrl->SetBitmap(ffx2);
					break;
				case 4 :
					mStatusCtrl->SetBitmap(ffx4);
					break;
				case 8 :
					mStatusCtrl->SetBitmap(ffx8);
					break;
				case 16 :
					mStatusCtrl->SetBitmap(ffx16);
					break;
				case 32 :
					mStatusCtrl->SetBitmap(ffx32);
					break;
				case 64 :
					mStatusCtrl->SetBitmap(ffx64);
					break;
				case -1 :
					mStatusCtrl->SetBitmap(play_backwards);
					break;
				case -2 :
					mStatusCtrl->SetBitmap(frx2);
					break;
				case -4 :
					mStatusCtrl->SetBitmap(frx4);
					break;
				case -8 :
					mStatusCtrl->SetBitmap(frx8);
					break;
				case -16 :
					mStatusCtrl->SetBitmap(frx16);
					break;
				case -32 :
					mStatusCtrl->SetBitmap(frx32);
					break;
				case -64 :
					mStatusCtrl->SetBitmap(frx64);
					break;
				default :
					break;
			}
			UpdatePlayerAndEventControls();
			break;
		case prodauto::CUE_POINT :
			//move to the right position in the list
			if (!mPlayFileButton->GetValue()) {
				mCurrentSelectedEvent = event.GetInt(); //As it is the player selecting this event, stop the player being told to jump to it
				mEventList->Select(event.GetInt()); //generates and processes a selection event immediately
			}
			break;
		case prodauto::FRAME_DISPLAYED :
			if (event.GetInt()) {
				//valid position
				mTimepos->SetPosition(event.GetExtraLong());
				if (mPlayFileButton->GetValue()) { //file mode
					mFileModeFrameOffset = event.GetExtraLong(); //so that we can reinstate the correct position if we come out of file mode
				}
				else {
					mTakeModeFrameOffset = event.GetExtraLong(); //so that we can reinstate the correct position if we come out of take mode
				}
			}
			else {
				mTimepos->SetPositionUnknown(true); //display "no position"
			}
			if ((mPlayer->AtEnd() && PLAYING == mStatus) || (!mPlayer->Within() && PLAYING_BACKWARDS == mStatus)) {
				mPlayer->Pause(); //player is already banging its head against the end stop, but this will issue a state change event
			}
			break;
		case prodauto::WITHIN_TAKE :
			//have just moved into a take: update the "previous take" button
			UpdatePlayerAndEventControls();
			break;
		case prodauto::CLOSE_REQ : {
#ifdef HAVE_DVS
				if (GetMenuBar()->FindItem(MENU_ExtAccelOutput)->IsChecked() || GetMenuBar()->FindItem(MENU_ExtUnaccelOutput)->IsChecked()) {
					//external output is active so don't close the player, just switch to external only
					GetMenuBar()->FindItem(MENU_ExtOutput)->Check(); //doesn't generate an event
					wxCommandEvent dummyEvent;
					OnPlayerOutputTypeChange(dummyEvent);
				}
				else {
#endif
					//close the player completely
					GetMenuBar()->FindItem(MENU_DisablePlayer)->Check(); //doesn't generate an event
					wxCommandEvent dummyEvent;
					OnDisablePlayer(dummyEvent);
#ifdef HAVE_DVS
				}
#endif
			}
			break;
		case prodauto::QUADRANT_CLICK :
			mPlaybackTrackSelector->SelectQuadrant(event.GetInt());
			break;
		case prodauto::KEYPRESS : {
			wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED);
			menuEvent.SetId(wxID_ANY);
			//menu events are not blocked even if the menu is disabled
//std::cerr << "key: " << event.GetInt() << std::endl;
//std::cerr << "modifier: " << event.GetExtraLong() << std::endl;
			switch (event.GetInt()) {
				case 65470: //F1
					//Record
					menuEvent.SetId(MENU_Record);
					break;
				case 65471: //F2
					//Mark cue
					menuEvent.SetId(MENU_MarkCue);
					break;
				case 65474: //F5 (NB shift is detected below)
					//Stop
					menuEvent.SetId(MENU_Stop);
					break;
				case 65476: //F7
					//Prev Track
					menuEvent.SetId(MENU_PrevTrack);
					break;
				case 65477: //F8
					//Next Track
					menuEvent.SetId(MENU_NextTrack);
					break;
				case 65362 : //Up
					//Prev Event
					menuEvent.SetId(MENU_Up);
					break;
				case 65364 : //Down
					//Next Event
					menuEvent.SetId(MENU_Down);
					break;
				case 65365 : //PgUp
					//Prev Take
					menuEvent.SetId(MENU_PageUp);
					break;
				case 65366 : //PgDn
					//Next Take
					menuEvent.SetId(MENU_PageDown);
					break;
				case 65360 : //Home
					//First Take
					menuEvent.SetId(MENU_Home);
					break;
				case 65367 : //Home
					//Last Take
					menuEvent.SetId(MENU_End);
					break;
				case 106 : //j
					//play backwards
					menuEvent.SetId(MENU_J);
					break;
				case 107 : //k
					//pause
					menuEvent.SetId(MENU_K);
					break;
				case 108 : //l
					//play forwards
					menuEvent.SetId(MENU_L);
					break;
				case 32 : //space
					// toggle play in whatever was the last direction, and pause.
					menuEvent.SetId(MENU_Space);
					break;
				case 51 : case 65361 : //3 and left
					//step backwards
					menuEvent.SetId(MENU_3);
					break;
				case 52 : case 65363 : //4 and right
					//step forwards
					menuEvent.SetId(MENU_4);
					break;
			}
			if ((MENU_Stop == menuEvent.GetId() && event.GetExtraLong() != 1) || (MENU_Stop != menuEvent.GetId() && event.GetExtraLong())) {
				//stop has been pressed without shift (only), or any other keypress has a modifier
				menuEvent.SetId(wxID_ANY); //ignore
			}
			if (wxID_ANY != menuEvent.GetId() && GetMenuBar()->FindItem(menuEvent.GetId())->IsEnabled()) { //recognised the keypress and the shortcut is enabled
				AddPendingEvent(menuEvent);
			}
			break;
		}
		default:
			break;
	}
}

/// Responds to a track select radio button being pressed (the event being passed on from the track select buttons object).
/// Informs the player of the new track, and updates the enable states of the previous/next track shortcuts
/// @param event The command event.
void IngexguiFrame::OnPlaybackTrackSelect(wxCommandEvent & event)
{
	if (mPlayFileButton->GetValue()) { //File Mode
		mFileModeSelectedTrack = event.GetId();
		mPlayer->SelectTrack(event.GetId(), false); //don't remember the selection
	}
	else {
		mPlayer->SelectTrack(event.GetId(), true); //remember the selection
	}
	GetMenuBar()->FindItem(MENU_PrevTrack)->Enable(mPlaybackTrackSelector->EarlierTrack(false));
	GetMenuBar()->FindItem(MENU_NextTrack)->Enable(mPlaybackTrackSelector->LaterTrack(false));
}

/// Responds to the stop button being pressed.
/// @param event The command event.
void IngexguiFrame::OnStop( wxCommandEvent& WXUNUSED( event ) )
{
	if (RECORDING == mStatus || RUNNING_UP == mStatus) {
		ProdAuto::MxfTimecode now;
		mTimepos->GetTimecode(&now);
		Log(wxT("Stop button pressed during recording or run-up @ ") + Timepos::FormatTimecode(now));
		//Create a locator sequence
		ProdAuto::LocatorSeq locators;
		if (mTakeInfoArray.GetCount()) { //sanity check
			TakeInfo * currentTakeInfo = &(mTakeInfoArray[mTakeInfoArray.GetCount() - 1]);
			locators.length(currentTakeInfo->GetCueColourCodes()->size());
			wxListItem item;
			for (unsigned int i = 0; i < locators.length(); i++) {
				item.SetId(currentTakeInfo->GetStartIndex() + i + 1); //the 1 is to jump over the start event
				item.SetColumn(3); //description column
				mEventList->GetItem(item);
				locators[i].comment = item.GetText().mb_str(*wxConvCurrent);
				locators[i].colour = (*(currentTakeInfo->GetCueColourCodes()))[i];
				locators[i].timecode = (*(currentTakeInfo->GetCueTimecodes()))[i];
			}
		}
		mRecorderGroup->Stop(now, mDescriptionCtrl->GetLineText(0).Trim(false).Trim(true), locators);
		SetStatus(RUNNING_DOWN);
		UpdatePlayerAndEventControls(true); //force reloading of the player if in file mode
	}
	else if (PLAYING == mStatus || PLAYING_BACKWARDS == mStatus || PAUSED == mStatus) {
		SetStatus(STOPPED);
	}
}

/// Responds to the Play/Pause/Cue button being pressed.
/// Adds a cue point, or tells the player.
/// @param event The command event.
void IngexguiFrame::OnCue( wxCommandEvent& WXUNUSED( event ) )
{
	switch (mStatus) {
		case RECORDING :
			{
				ProdAuto::MxfTimecode timecode;
				const wxString timecodeString = mTimepos->GetTimecode(&timecode);
				const wxString positionString = mTimepos->GetPosition();
				const int64_t frameCount = mTimepos->GetFrameCount();
				if (wxID_OK == mCuePointsDlg->ShowModal(timecodeString)) {
					AddEvent(CUE, positionString, timecode, frameCount, mCuePointsDlg->GetDescription(), mCuePointsDlg->GetColour(), mCuePointsDlg->GetLabelColour(), mCuePointsDlg->GetColourCode());
				}
				break;
			}
		case PLAYING : case PLAYING_BACKWARDS :
			mPlayer->Pause();
			break;
		case PAUSED :
			Play();
			break;
		default :
			break;
	}
//	SetFocus();
}

/// Deletes the cue point corresponding to the currently selected event list item.
/// Assumes that this is a cue point, and one which can be deleted!
void IngexguiFrame::OnDeleteCue( wxCommandEvent& WXUNUSED( event ) )
{
	//remove cue point from take info
	long selected = mEventList->GetFirstSelected();
	mTakeInfoArray[mTakeInfoArray.GetCount() - 1].DeleteCuePoint(selected - mTakeInfoArray[mTakeInfoArray.GetCount() - 1].GetStartIndex() - 1); //the last 1 is to jump over the start event
	//remove displayed cue point
	wxListItem item;
	item.SetId(selected);
	mEventList->DeleteItem(item);
	//Selected row has gone so select something sensible (the next row, or the last row if the last row was deleted)
	mEventList->Select(mEventList->GetItemCount() == selected ? selected - 1 : selected); //Generates an event which will update the delete cue button state
}

/// Responds to an event (cue point) being selected.
/// Updates controls and informs player.
/// @param event The command event.
void IngexguiFrame::OnEventSelection(wxListEvent& WXUNUSED(event))
{
	//NB a list event is generated by OnNotebookPageChange() so this must be changed if this function is modified to do other things than call UpdatePlayerAndEventControls()
	UpdatePlayerAndEventControls();
}

//When double-clicked, jumps to the selected event, and starts playing if paused; replaying if at stop point

/// Responds to an event (cue point) being double-clicked.
/// Updates controls and informs player, and starts playback if paused.
/// @param event The command event.
void IngexguiFrame::OnEventActivated(wxListEvent& WXUNUSED(event))
{
	UpdatePlayerAndEventControls();
	if (PAUSED == mStatus) {
		Play();
	}
}

/// Responds to an attempt to edit in the event list.
/// Allows editing only on cue points on a recording currently in progress.
/// Sets the selected label to the text of the description, as you can only edit the labels (left hand column) of a list control.
/// Dispatches an event to restore the label text in case the user presses ENTER without making changes.
/// @param event The list control event.
void IngexguiFrame::OnEventBeginEdit(wxListEvent& event)
{
	if (mDeleteCueButton->IsEnabled()) { //if you can delete it, you can edit it!
		//want to edit the description text (as opposed to the column 0 label text), so transfer it to the label
		wxListItem item;
		item.SetId(event.GetIndex());
		item.SetColumn(3);
		mEventList->GetItem(item);
		item.SetColumn(0);
		item.SetMask(wxLIST_MASK_TEXT); //so it will update the text property
		mEventList->SetItem(item);
		//If user presses ENTER without editing, no end editing event will be generated, so restore the label once editing has commenced (and so the modified label text has been grabbed)
		wxCommandEvent restoreLabelEvent(wxEVT_RESTORE_LIST_LABEL, wxID_ANY);
		restoreLabelEvent.SetExtraLong(event.GetIndex());
		AddPendingEvent(restoreLabelEvent);

		event.Skip();
	}
	else { //can't edit this
		event.Veto();
	}
}

/// Responds to editing finishing in the event list by pressing ENTER after making changes, or ESCAPE.
/// If ESCAPE was not pressed, copies the edited text to the description column and vetoes the event to prevent the label being overwritten.
/// @param event The list control event.
void IngexguiFrame::OnEventEndEdit(wxListEvent& event)
{
	if (!event.IsEditCancelled()) { //user hasn't pressed ESCAPE - otherwise the event text is the label text
		//put the edited text in the right column
		wxListItem item;
		item.SetId(event.GetIndex());
		item.SetColumn(3);
		item.SetText(event.GetText()); //the newly edited string
		mEventList->SetItem(item);
		//prevent the label being overwritten
		event.Veto();
#ifndef __WIN32__
		mEventList->SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
	}
}

/// Responds to commencement of editing by restoring the default label.
/// @param event The command event.
void IngexguiFrame::OnRestoreListLabel(wxCommandEvent& event)
{
	wxListItem item;
	item.SetId(event.GetExtraLong());
	item.SetColumn(0);
	item.SetText(CUE_LABEL);
	mEventList->SetItem(item);
}

/// Responds to the an event from the group of recorders.
/// @param event The command event.
/// The following event IDs are recognised, the enumerations being in the RecorderGroupCtrl namespace:
/// 	DISABLE_REFRESH: Disables the list refresh button.
///	ENABLE_REFRESH: Enables the list refresh button.
///	NEW_RECORDER: Adds the recorder to the source tree, and sets mode to connected.  Sets status to recording if the new recorder is recording.
///		Event string: The recorder name.
///		Event int: The index of the recorder so it can be disconnected easily
///		Event client data: Ptr to a RecorderData object, containing a track list and a track status list.  Deletes this.
///	REQUEST_RECORD: Asks the source tree for a list of enable states and passes it to the recorder group.
///		Event string: The recorder name (an event will be received for each recorder).
///	RECORDING: If successful, set status to recording and set timecode counter to given timecode if not already recording.
///		Otherwise, report failure.
///		Event string: The recorder name.
///		Event int: Non-zero for success.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	STOPPED: If successful, set status to stopped and set timecode counter to given timecode if not already stopped.
///		Otherwise, report failure.
///		Event string: The recorder name.
///		Event int: Non-zero for success.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	TRACK_STATUS: Informs source tree of tracks' status (recording or not) for given recorder.
///		Event string: The recorder name.
///		Event client data: Ptr to a RecorderData object, containing a track status list.  Deletes this.
///	REMOVE_RECORDER: Removes the given recorder from the tree, and sets mode to disconnected if no recorders remaining.
///		Event string: The recorder name.
///	DISPLAY_TIMECODE: Displays the given timecode, enabling auto-increment.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	DISPLAY_TIMECODE_STUCK: Displays the given timecode, disabling auto-increment.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	DISPLAY_TIMECODE_MISSING: Displays timecode as being missing.
///	DISPLAY_TIMECODE_SOURCE: Labels the timecode source as the given recorder name (which may be empty).
///		Event string: The recorder name.
///	COMM_FAILURE: Informs the source tree.
///		Event string: The recorder name.
///	TIMECODE_STUCK: Informs the source tree.
///		Event string: The recorder name.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	TIMECODE_MISSING: Informs the source tree.
///		Event string: The recorder name.
///	TIMECODE_SOURCE: Informs the source tree.
///		Event string: The recorder name.

void IngexguiFrame::OnRecorderGroupEvent(wxCommandEvent& event) {
	switch ((RecorderGroupCtrl::RecorderGroupCtrlEventType) event.GetId()) {
		case RecorderGroupCtrl::DISABLE_REFRESH :
			mRecorderListRefreshButton->Disable();
			SetStatus(mStatus); //to disable record button if necessary
			break;
		case RecorderGroupCtrl::ENABLE_REFRESH :
			EnableButtonReliably(mRecorderListRefreshButton);
			SetStatus(mStatus); //to re-enable record button if necessary
			break;
		case RecorderGroupCtrl::NEW_RECORDER :
			Log(wxT("NEW_RECORDER: \"") + event.GetString() + wxT("\""));
			//event contains all the details about the recorder
			EnableButtonReliably(mTapeIdButton, mTree->AddRecorder(event.GetString(), ((RecorderData *) event.GetClientData())->GetTrackList(), ((RecorderData *) event.GetClientData())->GetTrackStatusList(), event.GetInt(), mSavedState));
			if (mRecorderGroup->GetCurrentProjectName().IsEmpty()) { //not yet selected a project and we should have some project names now as we've got a recorder
				SetProjectName();
				if (mRecorderGroup->GetCurrentProjectName().IsEmpty()) {
					Log(wxT("Disconnecting because no project name set"));
					mRecorderGroup->Deselect(event.GetInt());
				}
			}
			if (mTree->RecordingSuccessfully()) {
				Log(wxT(" ...already recording"));
				//this recorder is already recording
				ResetPlayer();
				SetStatus(RECORDING); //will prevent any more recorders being added
				ProdAuto::MxfTimecode tc;
				mTimepos->GetStartTimecode(&tc);
				AddEvent(START, mTimepos->GetStartPosition(), tc); //didn't start now - started before the preroll period
				mTimepos->SetPositionUnknown(); //don't know when recording started
			}
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::REQUEST_RECORD : {
			//record enable list required
			CORBA::BooleanSeq enableList;
			if (mTree->GetRecordEnables(event.GetString(), enableList)) { //something's enabled for recording, and not already recording
				Log(wxT("REQUEST_RECORD for \"") + event.GetString() + wxT("\" and track(s) enabled to record"));
				mRecorderGroup->Record(event.GetString(), enableList);
				mTree->SetRecorderStateUnknown(event.GetString(), wxT("Awaiting response..."));
			}
/*				ProdAuto::MxfTimecode now;
				if (RecorderGroupCtrl::REQUEST_RECORD == (RecorderGroupCtrl::RecorderGroupCtrlEventType) event.GetId()) { //initial record attempt
					Log(wxT("REQUEST_RECORD for \"") + event.GetString() + wxT("\" and track(s) enabled to record"));
					//use timecode already supplied to recordergroup, so that all recorders start at the same point
					now.undefined = true; //use timecode
					mTree->SetRecorderStateUnknown(event.GetString(), wxT("Awaiting response..."));
				}
				else { //later record attempt - record now
					mTimepos->GetTimecode(&now);
					Log(wxT("RECORD_RETRY for \"") + event.GetString() + wxT("\" @ ") + Timepos::FormatTimecode(now) + wxT(" and track(s) enabled to record"));
				}
				mRecorderGroup->Record(event.GetString(), enableList, tapeIds, now); */
			}
			break;
		case RecorderGroupCtrl::RECORDING :
			if (event.GetInt()) { //successful
				if (!mTree->IsRouterRecorder(event.GetString())) { //router recorders don't return sensible start timecodes
					mTimepos->Record(((RecorderData *) event.GetClientData())->GetTimecode());
					AddEvent(START); //don't automatically do this or if the first recorder to record is a router recorder we'll get an unknown start timecode
				}
				mTree->SetRecorderStateOK(event.GetString());
//				if (mTree->AllRecording()) {
//					AddEvent(START); //just in case the only recorder is a router recorder!
//				}
				Log(wxT("RECORDING successfully on \"") + event.GetString() + wxT("\" @ ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
/*				ProdAuto::MxfTimecode startTimecode;
				mTimepos->GetStartTimecode(&startTimecode);
				if (((RecorderData *) event.GetClientData())->GetTimecode().samples == startTimecode.samples) { //this recorder started when the other recorders did
					mTree->SetRecorderStateOK(event.GetString());
//std::cerr << "record: matching" << std::endl;
					Log(wxT("RECORDING successfully on \"") + event.GetString() + wxT("\" - timecode matches first recorder"));
				}
				else {
					mTree->SetRecorderStateOK(event.GetString());
//					mTree->SetRecorderStateProblem(event.GetString(), wxT("Started at ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
					Log(wxT("RECORDING successfully on \"") + event.GetString() + wxT("\" - timecode mismatch with first recorder - @ ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
				} */
			}
			else {
				Log(wxT("RECORDING failed on \"") + event.GetString() + wxT("\""));
				mTree->SetRecorderStateProblem(event.GetString(), wxT("Failed to record: retrying"));
			}
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::STOPPED :
			if (event.GetInt()) { //successful
				Log(wxT("STOPPED successfully on \"") + event.GetString() + wxT("\""));
				mTree->SetRecorderStateOK(event.GetString());
				//Add the recorded files to the take info
				if (mTakeInfoArray.GetCount()) { //sanity check
					if (((RecorderData *) event.GetClientData())->GetTrackList() && !mTree->IsRouterRecorder(event.GetString())) {
						mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).AddRecorder(((RecorderData *) event.GetClientData())->GetTrackList(), ((RecorderData *) event.GetClientData())->GetFileList());
					}
					AddEvent(STOP); //will not add more than one
					CORBA::StringSeq_var fileList = ((RecorderData *) event.GetClientData())->GetFileList();
					wxString msg = wxString::Format(wxT("%d element%s returned%s"), fileList->length(), 1 == fileList->length() ? wxT("") : wxT("s"), fileList->length() ? wxT(":") : wxT("."));
					for (size_t i = 0; i < fileList->length(); i++) {
						msg += wxT("\n\"") + wxString(fileList[i], *wxConvCurrent) + wxT("\"");
					}
					Log(msg);
				}
				if (STOPPED != mStatus) {
					//use the first returned timecode as the stop position
					mTimepos->Stop(((RecorderData *) event.GetClientData())->GetTimecode());
					SetStatus(STOPPED);
				}
				else {
					//need to reload the player as more files have appeared
					mCurrentTakeInfo = 0; //forces player reload
					UpdatePlayerAndEventControls();
				}
			}
			else {
				Log(wxT("STOPPED failure on \"") + event.GetString() + wxT("\""));
				mTree->SetRecorderStateProblem(event.GetString(), wxT("Failed to stop: retrying"));
			}
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::TRACK_STATUS :
			Log(wxT("TRACK_STATUS for \"") + event.GetString() + wxT("\""));
			mTree->SetTrackStatus(event.GetString(), RECORDING == mStatus || RUNNING_UP == mStatus || RUNNING_DOWN == mStatus, ((RecorderData *) event.GetClientData())->GetTrackStatusList()); //will set record button and status indicator
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::REMOVE_RECORDER :
			Log(wxT("REMOVE_RECORDER for \"") + event.GetString() + wxT("\""));
//			mTapeIdButton->Enable(mTree->RemoveRecorder(event.GetString()));
			EnableButtonReliably(mTapeIdButton, mTree->RemoveRecorder(event.GetString()));
			if (!mTree->HasRecorders() && (RECORDING == mStatus || RUNNING_UP == mStatus || RUNNING_DOWN == mStatus)) {//this check needed in case we disconnect from a recorder that's recording/running up and we can't contact
				SetStatus(STOPPED);
			}
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE :
			Log(wxT("DISPLAY_TIMECODE ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
			mTimepos->SetTimecode(((RecorderData *) event.GetClientData())->GetTimecode(), false);
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE_STUCK :
			Log(wxT("DISPLAY_TIMECODE_STUCK ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
			mTimepos->SetTimecode(((RecorderData *) event.GetClientData())->GetTimecode(), true);
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE_MISSING :
			Log(wxT("DISPLAY_TIMECODE_MISSING"));
			mTimepos->DisableTimecode(UNKNOWN_TIMECODE);
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE_SOURCE :
			Log(wxT("DISPLAY_TIMECODE_SOURCE \"") + event.GetString() + wxT("\""));
			if (event.GetString().IsEmpty()) {
				mTimecodeBox->GetStaticBox()->SetLabel(wxT("Studio Timecode"));
			}
			else {
				mTimecodeBox->GetStaticBox()->SetLabel(wxT("Studio Timecode (") + event.GetString() + wxT(")"));
			}
			break;
		case RecorderGroupCtrl::COMM_FAILURE :
			Log(wxT("COMM_FAILURE for \"") + event.GetString() + wxT("\""));
			mTree->SetRecorderStateUnknown(event.GetString(), wxT("Uncontactable: retrying"));
			break;
		case RecorderGroupCtrl::TIMECODE_STUCK :
				Log(wxT("TIMECODE_STUCK for ") + event.GetString() + wxT("\" @ ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
				mTree->SetRecorderStateProblem(event.GetString(), wxT("Timecode stuck at ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
				delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::TIMECODE_MISSING :
				Log(wxT("TIMECODE_MISSING for \"") + event.GetString() + wxT("\""));
				mTree->SetRecorderStateProblem(event.GetString(), wxT("Timecode missing"));
			break;
		case RecorderGroupCtrl::TIMECODE_RUNNING :
				Log(wxT("TIMECODE_RUNNING for \"") + event.GetString() + wxT("\""));
				mTree->SetRecorderStateOK(event.GetString());
			break;
	}
}

/// Responds to an event from the test dialogue.
/// Attempts to record or stop as appropriate.
/// @param event The command event.
void IngexguiFrame::OnTestDlgEvent(wxCommandEvent& event) {
	if (TestModeDlg::RECORD == event.GetId()) {
		if (GetMenuBar()->FindItem(MENU_Record)->IsEnabled()) {
			wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED, MENU_Record);
			AddPendingEvent(menuEvent);
		}
	}
	else {
		if (GetMenuBar()->FindItem(MENU_Stop)->IsEnabled()) {
			wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED, MENU_Stop);
			AddPendingEvent(menuEvent);
		}
	}
}

/// Responds to an event from the source tree.
/// Updates various controls (if the event string is empty)
/// Requests and sets tape IDs for the recorder indicated by the event's string (if the event string is not empty)
/// @param event The command event.
void IngexguiFrame::OnTreeEvent(wxCommandEvent& event)
{
	if (event.GetString().IsEmpty()) { //status
		switch (mStatus) {
			case RUNNING_UP:
				if (mTree->AllRecording()) { //all recorders that are supposed to are recording
					SetStatus(RECORDING);
				}
				break;
			case RECORDING: case RUNNING_DOWN:
				break;
			default: {
				bool enableRecordButton = mTree->SomeEnabled() && mTree->TapeIdsOK() && mRecorderListRefreshButton->IsEnabled();
	//			mRecordButton->Enable(enableRecordButton);
				EnableButtonReliably(mRecordButton, enableRecordButton);
				GetMenuBar()->FindItem(MENU_Record)->Enable(enableRecordButton);
				GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			}
		}
	
		if (mTree->HasRecorders()) {
			if (mTree->HasProblem()) {
				mAlertCtrl->SetBitmap(exclamation);
				mAlertCtrl->SetToolTip(wxT("Recorder problem"));
			}
			else if (!mTree->HasAllSignals()) {
				mAlertCtrl->SetBitmap(concerned);
				mAlertCtrl->SetToolTip(wxT("Signals missing on enabled tracks"));
			}
			else if (mTree->IsUnknown()) {
				mAlertCtrl->SetBitmap(blank);
				mAlertCtrl->SetToolTip(wxT("Awaiting response"));
			}
			else {
				mAlertCtrl->SetBitmap(happy);
				mAlertCtrl->SetToolTip(wxT("Status OK"));
			}
			GetMenuBar()->FindItem(MENU_SetRolls)->Enable();
			if (mTree->AllRecording()) {
				AddEvent(START); //just in case the only recorder is a router recorder, so a start event won't already have been issued
			}
			mRecordButton->SetLabel(wxString::Format(wxT("Record (%d track%s)"), mTree->EnabledTracks(), mTree->EnabledTracks() == 1 ? wxT("") : wxT("s")));
		}
		else {
			ResetToDisconnected();
		}
	
		//Highlight the tape ID button if we can't record because of missing tape IDs
		mTapeIdButton->SetBackgroundColour(mTree->SomeEnabled() && !mTree->TapeIdsOK() ? wxColour(0xFF, 0x80, 0x00) : wxNullColour);
	}
	else { //recorder name supplied
		if (event.GetExtraLong()) { //signal present state has changed
			mRecorderGroup->PollRapidly(event.GetString(), event.GetInt() == 0);
		}
		else { //Tape ID updates
			CORBA::StringSeq packageNames, tapeIds;
			mTree->GetRecorderTapeIds(event.GetString(), packageNames, tapeIds);
			mRecorderGroup->SetTapeIds(event.GetString(), packageNames, tapeIds);
		}
	}
}

/// Responds to the player enable/disable menu requests by telling the player and updating menu items.
/// @param event The command event.
void IngexguiFrame::OnDisablePlayer(wxCommandEvent& WXUNUSED( event ))
{
	bool enabled = !GetMenuBar()->FindItem(MENU_DisablePlayer)->IsChecked();
	mPlayer->Enable(enabled);
	if (enabled && mPlayFileButton->GetValue()) {
		mPlayer->SelectTrack(mFileModeSelectedTrack, false);
	}
	GetMenuBar()->FindItem(MENU_PlayerType)->Enable(enabled);
	GetMenuBar()->FindItem(MENU_PlayerOSD)->Enable(enabled);
	mPlayFileButton->Enable(enabled && mFileModeFiles.GetCount());
}

/// Responds to the "Play File..." menu command
/// Opens a file select dialogue
void IngexguiFrame::OnPlayerOpenFile(wxCommandEvent & event )
{
	if (MENU_PlayMOV == event.GetId()) {
		wxFileDialog dlg(this, wxT("Choose a MOV file"), wxT(""), wxT(""), wxT("MOV files |*.mov;*.MOV|All files|*.*"), wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR); //select single file only
		if (wxID_OK == dlg.ShowModal()) {
			mFileModeMovFile = dlg.GetPath();
			mFileModeFiles.Clear();
			mFileModeFrameOffset = 0; //remembers offset when toggling between file and event views
			mPlayFileButton->SetValue(true); //go into file mode if not already in that mode
			UpdatePlayerAndEventControls(true); //reload player
		}
	}
	else { //MXF
		wxFileDialog dlg(this, wxT("Choose one or more MXF files"), wxT(""), wxT(""), wxT("MXF files|*.mxf;*.MXF|All files|*.*"), wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR | wxFD_MULTIPLE); //select multiple files
		if (wxID_OK == dlg.ShowModal()) { //at least one file has been chosen
			dlg.GetPaths(mFileModeFiles);
			mFileModeMovFile.Clear();
			mFileModeFrameOffset = 0; //remembers offset when toggling between file and event views
			mPlayFileButton->SetValue(true); //go into file mode if not already in that mode
			UpdatePlayerAndEventControls(true); //reload player
		}
	}
}

/// Responds to the Play File toggle button being pressed
/// Loads the player with the correct set of files
void IngexguiFrame::OnPlayFile(wxCommandEvent & WXUNUSED( event ))
{
	UpdatePlayerAndEventControls(true); //force player reload
}

/// Adds a stop/start/cue point event to the event list.
/// Start and stop events are ignored if already started/stopped
/// For start events, creates a new TakeInfo object.
/// Updates the other controls and the player.
/// @param type The event type: START, CUE, STOP or PROBLEM.
/// @param position A string to use instead of the current position
/// @param timecode A timecode to use instead of the current timecode (if position is blank)
/// @param frameCount The frame count for CUE events
/// @param description A description for CUE events
/// @param colour The colour of a CUE event
/// @param labelColour The colour of text for a CUE event
/// @param colourCode The colour code of a CUE event, to be sent to the recorder
void IngexguiFrame::AddEvent(EventType type, const wxString positionString, ProdAuto::MxfTimecode timecode, const int64_t frameCount, const wxString description, const wxColour colour, const wxColour labelColour, const ProdAuto::LocatorColour::EnumType colourCode)
{
//std::cerr << mEventList->GetColumnWidth(3) << std::endl;
	wxListItem item;
	if (mEventList->GetItemCount()) {
		item.SetId(mEventList->GetItemCount() - 1);
		mEventList->GetItem(item);
		if ((START == type && wxT("Stop") != item.GetText()) || (STOP == type && wxT("Stop") == item.GetText())) {
			return;
		}
	}
	wxFont font(EVENT_FONT_SIZE, wxFONTFAMILY_DEFAULT, wxFONTFLAG_UNDERLINED, wxFONTFLAG_UNDERLINED);
//	font.SetUnderlined(true);
//	font.SetPointSize(FONT_SIZE); //Default rather small in MSW
	switch (type) {
		case START :
			item.SetText(wxT("Start")); //NB text matched elsewhere
			item.SetTextColour(wxColour(0xFF, 0x20, 0x00));
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
//			item.SetBackgroundColour(wxColour(wxT("GREEN")));
//			mStartEvent = mEventList->GetItemCount();
			{
				TakeInfo * info = new TakeInfo(mEventList->GetItemCount(), mRecorderGroup->GetCurrentProjectName()); //this will be the index of the current event
				mTakeInfoArray.Add(info); //deleted by array (object array)
			}
//			font.SetWeight(wxFONTWEIGHT_BOLD); breaks auto col width
//			font.SetStyle(wxFONTSTYLE_ITALIC); breaks auto col width
			break;
		case CUE :
			item.SetText(CUE_LABEL);
			item.SetTextColour(labelColour);
			item.SetBackgroundColour(colour);
			break;
//		case REVIEW :
//			item.SetText(wxString::Format(wxT("Last %d s"), REVIEW_THRESHOLD));
//			item.SetTextColour(wxColour(wxT("CORNFLOWER BLUE")));
//			break;
		case STOP :
			item.SetText(wxT("Stop")); //NB text matched elsewhere
			item.SetTextColour(wxColour(wxT("BLACK"))); //needed with cygwin for text to appear
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
			mEndEvent = mEventList->GetItemCount();
			break;
		case PROBLEM : //FIXME: putting these in will break the creation of the list of locators in OnStop()
			item.SetText(wxT("Problem"));
			item.SetTextColour(wxColour(wxT("RED")));
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
			break;
	}

	item.SetColumn(0);
	item.SetId(mEventList->GetItemCount()); //will insert at end
	item.SetData(mTakeInfoArray.GetCount() - 1); //the index of the take info for this take
	mEventList->InsertItem(item); //insert (at end)
	mEventList->EnsureVisible(item.GetId()); //scroll down if necessary
	if (START == type) {
		mEventList->Select(item.GetId());
	}
	else if (STOP == type) {
		GetMenuBar()->FindItem(MENU_ClearLog)->Enable();
	}
#ifndef __WIN32__
	mEventList->SetColumnWidth(0, wxLIST_AUTOSIZE);
#endif
	item.SetFont(font); //point size ignored on GTK and row height doesn't adjust on Win32

	//timecode
	item.SetColumn(1);
	wxString timecodeString;
	if (positionString.IsEmpty()) { //assumed no timecode argument supplied
		timecodeString = mTimepos->GetTimecode(&timecode);
	}
	else {
		timecodeString = Timepos::FormatTimecode(timecode);
	}
	item.SetText(timecodeString);
	font.SetFamily(wxFONTFAMILY_MODERN); //fixed pitch so fields don't move about
	mEventList->SetItem(item);
#ifndef __WIN32__
	mEventList->SetColumnWidth(1, wxLIST_AUTOSIZE);
#endif

	//position
	item.SetColumn(2);
	if (positionString.IsEmpty()) {
		item.SetText(mTimepos->GetPosition());
	}
	else {
		item.SetText(positionString);
	}

	//cue points
	if (START != type && STOP != type) {
		mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).AddCuePoint(frameCount, timecode, colourCode);
	}

	mEventList->SetItem(item); //additional to previous data
#ifndef __WIN32__
	mEventList->SetColumnWidth(2, wxLIST_AUTOSIZE);
#endif

	//description
	if (STOP == type) {
		item.SetColumn(3);
		item.SetText(mDescriptionCtrl->GetLineText(0).Trim(false).Trim(true));
		if (item.GetText().Len()) { //kludge to stop desc field being squashed if all descriptions are empty
			mEventList->SetItem(item); //additional to previous data
#ifndef __WIN32__
			mEventList->SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
		}
	}
	else if (CUE == type) {
		item.SetColumn(3);
		item.SetText(description);
		mEventList->SetItem(item); //additional to previous data
#ifndef __WIN32__
		mEventList->SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
	}
	if (-1 == mEventList->GetFirstSelected()) {
		//First item in the list
		mEventList->Select(0);
	}
	UpdatePlayerAndEventControls();
}

/// Sets the gui status.
/// @param stat The new status:
/// 	STOPPED: Not recording and not playing.
///	RECORDING
///	PLAYING
///	PLAYING_BACKWARDS
///	RUNNING_UP: Waiting for recorders to respond to record commands.
///	RUNNING_DOWN: Waiting for recorders to respond to stop commands.
///	PAUSED
//	UNKNOWN: Waiting for a response from a recorder.
void IngexguiFrame::SetStatus(Stat status)
{
	bool changed = false;
	if (mStatus != status) {
		mStatus = status;
		changed = true;
	}
	bool enableRecordButton = mTree->SomeEnabled() && mTree->TapeIdsOK() && mRecorderListRefreshButton->IsEnabled();
	switch (mStatus) {
		case STOPPED:
			if (changed) {
				Log(wxT("status: STOPPED"));
			}
			mStatusCtrl->SetBitmap(stop);
			mStatusCtrl->SetToolTip(wxT("Stopped"));
			SetStatusText(wxT("Stopped."));
			EnableButtonReliably(mRecordButton, enableRecordButton);
			GetMenuBar()->FindItem(MENU_Record)->Enable(enableRecordButton);
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable();
			mTree->EnableChanges();
			mPlayFileButton->Enable(mFileModeFiles.GetCount() || !mFileModeMovFile.IsEmpty());
			break;
		case RUNNING_UP:
			if (changed) {
				Log(wxT("status: RUNNING_UP"));
			}
			mStatusCtrl->SetBitmap(blank);
			mStatusCtrl->SetToolTip(wxT("Running up"));
			SetStatusText(wxT("Running up."));
			mRecordButton->Pending();
			GetMenuBar()->FindItem(MENU_Record)->Enable(false);
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(false);
			mRecordButton->SetToolTip(wxT(""));
			mStopButton->Enable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable(false);
			mTree->EnableChanges(false);
			mPlayFileButton->Enable(false);
			break;
		case RECORDING:
			if (changed) {
				Log(wxT("status: RECORDING"));
				mRecorderGroup->EnableForInput(false); //catches connecting to a recorder that's already recording
			}
			mStatusCtrl->SetBitmap(record);
			mStatusCtrl->SetToolTip(wxT("Recording"));
			SetStatusText(wxT("Recording."));
//			if (mTree->RecordingSuccessfully()) {
				mRecordButton->Record();
//				mRecordButton->SetToolTip(wxT("Recording"));
//			}
//			else {
//				mRecordButton->Enable();
//				mRecordButton->SetToolTip(wxT("Re-send record command"));
//			}
			GetMenuBar()->FindItem(MENU_Record)->Enable(false);
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(false);
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable();
			GetMenuBar()->FindItem(MENU_Stop)->Enable();
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable(false);
//			mStopButton->Enable();
			EnableButtonReliably(mStopButton);
			mStopButton->SetToolTip(wxT("Stop recording after postroll"));
			mTree->EnableChanges(false);
			mPlayFileButton->Enable(false);
			break;
		case RUNNING_DOWN:
			if (changed) {
				Log(wxT("status: RUNNING_DOWN"));
			}
			mStatusCtrl->SetBitmap(record);
			mStatusCtrl->SetToolTip(wxT("Running down"));
			SetStatusText(wxT("Running down."));
//			if (mTree->RecordingSuccessfully()) {
				mRecordButton->Record();
//				mRecordButton->SetToolTip(wxT("Recording"));
//			}
//			else {
//				mRecordButton->Enable();
//				mRecordButton->SetToolTip(wxT("Re-send record command"));
//			}
			GetMenuBar()->FindItem(MENU_Record)->Enable(false);
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(false);
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable();
			GetMenuBar()->FindItem(MENU_Stop)->Enable();
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable();
//			mStopButton->Enable();
			EnableButtonReliably(mStopButton);
			mStopButton->SetToolTip(wxT("Stop recording after postroll"));
			mTree->EnableChanges(false);
			mPlayFileButton->Enable(false);
			break;
		case PLAYING:
			if (changed) {
				Log(wxT("status: PLAYING"));
			}
			mStatusCtrl->SetBitmap(play);
			mStatusCtrl->SetToolTip(wxT("Playing"));
			SetStatusText(wxT("Playing."));
			EnableButtonReliably(mRecordButton, enableRecordButton);
			GetMenuBar()->FindItem(MENU_Record)->Enable(enableRecordButton);
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable();
			mStopButton->SetToolTip(wxT(""));
			mTree->EnableChanges();
			mPlayFileButton->Enable(mFileModeFiles.GetCount() || !mFileModeMovFile.IsEmpty());
			break;
		case PLAYING_BACKWARDS:
			if (changed) {
				Log(wxT("status: PLAYING_BACKWARDS"));
			}
			mStatusCtrl->SetBitmap(play_backwards);
			mStatusCtrl->SetToolTip(wxT("Playing backwards"));
			SetStatusText(wxT("Playing backwards."));
			EnableButtonReliably(mRecordButton, enableRecordButton);
			GetMenuBar()->FindItem(MENU_Record)->Enable(enableRecordButton);
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			mStopButton->SetToolTip(wxT(""));
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable();
			mTree->EnableChanges();
			mPlayFileButton->Enable(mFileModeFiles.GetCount() || !mFileModeMovFile.IsEmpty());
			break;
		case PAUSED:
			if (changed) {
				Log(wxT("status: PAUSED"));
			}
			mStatusCtrl->SetBitmap(paused);
			mStatusCtrl->SetToolTip(wxT("Paused"));
			SetStatusText(wxT("Paused."));
			EnableButtonReliably(mRecordButton, enableRecordButton);
			GetMenuBar()->FindItem(MENU_Record)->Enable(enableRecordButton);
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable();
			mStopButton->SetToolTip(wxT(""));
			mTree->EnableChanges();
			mPlayFileButton->Enable(mFileModeFiles.GetCount() || !mFileModeMovFile.IsEmpty());
			break;
	}
	UpdatePlayerAndEventControls();
}

/// Updates player and various controls.
/// @param forceLoad Force player to load new file(s).
/// @param forceCueJump Force player to jump to current cue point.
/// If a new valid take has been selected in the event list, updates the playback notebook page and loads the player with the new files.
/// If a new cue point has been selected in the event list, or if force is true, instruct player to jump to this cue point.
void IngexguiFrame::UpdatePlayerAndEventControls(bool forceLoad, bool forceNewCuePoint)
{
	if (mPlayFileButton->GetValue()) { //File Mode
		if (forceLoad) {
			//load player with file mode data
			std::vector<std::string> fileNames;
			std::vector<std::string> trackNames;
			if (mFileModeMovFile.IsEmpty()) { //MXF files
				ProdAuto::MxfTimecode editRate;
				mPlayProjectNameBox->GetStaticBox()->SetLabel(wxT("Project"));
				mPlayProjectNameCtrl->SetLabel(mPlaybackTrackSelector->SetMXFFiles(mFileModeFiles, fileNames, trackNames, editRate));
				mTimepos->SetDefaultEditRate(editRate); //allows position display to work in case we haven't got it from anywhere else
				mPlayer->Load(&fileNames, &trackNames, prodauto::MXF_INPUT, mFileModeFrameOffset);
			}
			else { //MOV file
				fileNames.push_back((const char*)mFileModeMovFile.mb_str(*wxConvCurrent));
				trackNames.push_back((const char*)mFileModeMovFile.mb_str(*wxConvCurrent));
				mPlayProjectNameBox->GetStaticBox()->SetLabel(wxT("Filename"));
				mPlayProjectNameCtrl->SetLabel(mFileModeMovFile);
				mPlaybackTrackSelector->Clear();
				mPlayer->Load(&fileNames, &trackNames, prodauto::FFMPEG_INPUT, mFileModeFrameOffset);
				mFileModeSelectedTrack = 1; //first source rather than quad split
			}
			mPlayer->SelectTrack(mFileModeSelectedTrack, false);
			mLastPlayingBackwards = false; //no point continuing the default play direction of the previous take
		}
		mPrevTakeButton->Disable();
		GetMenuBar()->FindItem(MENU_PageUp)->Enable(false);
		mNextTakeButton->Disable();
		GetMenuBar()->FindItem(MENU_PageDown)->Enable(false);
		GetMenuBar()->FindItem(MENU_Up)->Enable(false);
		GetMenuBar()->FindItem(MENU_Down)->Enable(false);
		mDeleteCueButton->Disable();
	}
	else { //not file mode
		if (mTakeInfoArray.GetCount()) {
			//work out the current take info
			wxListItem item;
			item.SetId(mEventList->GetFirstSelected());
			item.SetColumn(0);
			mEventList->GetItem(item);
			TakeInfo * currentTakeInfo = &(mTakeInfoArray.Item(item.GetData()));
			//"Delete Cue" button
			mDeleteCueButton->Enable(
				RECORDING == mStatus //no point in deleting cue points after recording, as the descriptions have already been sent to the recorder
				&& (long) mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartIndex() < mEventList->GetFirstSelected() //an event in the current (latest) recording
				&& CUE_LABEL == item.GetText() //a cue event
			);
			//player control
			if (currentTakeInfo->GetFiles()->GetCount()) { //the take is complete and there are at least some files available
				if (currentTakeInfo != mCurrentTakeInfo || forceLoad) { //a new take
					if (!forceLoad) { //not returning to previous situation
						mTakeModeFrameOffset = 0;
					}
					//load player with current take data
					mCurrentTakeInfo = currentTakeInfo;
					//update playback tracks notebook page
					mPlayProjectNameCtrl->SetLabel(currentTakeInfo->GetProjectName());
					std::vector<std::string> fileNames;
					std::vector<std::string> trackNames;
					mPlaybackTrackSelector->SetTracks(*currentTakeInfo, fileNames, trackNames);
					//load files and jump to current position
					mPlayer->Load(&fileNames, &trackNames, prodauto::MXF_INPUT, mTakeModeFrameOffset, mCurrentTakeInfo->GetCuePointFrames(), mCurrentTakeInfo->GetStartIndex(), mEventList->GetFirstSelected() - mCurrentTakeInfo->GetStartIndex());
					mLastPlayingBackwards = false; //no point continuing the default play direction of the previous take
				}
				else if ((unsigned long) mEventList->GetFirstSelected() != mCurrentSelectedEvent || forceNewCuePoint) { //only the selected event has changed
					mPlayer->JumpToCue(mEventList->GetFirstSelected() - mCurrentTakeInfo->GetStartIndex());
				}
				mCurrentSelectedEvent = mEventList->GetFirstSelected();
			}
			//"previous" button/"previous", "up" and "first" menu items
			if (mCurrentSelectedEvent || mPlayer->Within()) {
				//not at the beginning of the first take
	//			mPrevTakeButton->Enable();
				EnableButtonReliably(mPrevTakeButton);
				GetMenuBar()->FindItem(MENU_PageUp)->Enable();
				GetMenuBar()->FindItem(MENU_Up)->Enable();
				if (mCurrentSelectedEvent == mCurrentTakeInfo->GetStartIndex() && !mPlayer->Within()) {
					//the start of this take is selected
					mPrevTakeButton->SetLabel(wxT("Previous Take"));
					GetMenuBar()->FindItem(MENU_PageUp)->SetText(wxT("Move to start of previous take\tPGUP"));
				}
				else {
					//somewhere within a take
					mPrevTakeButton->SetLabel(wxT("Start of Take"));
					GetMenuBar()->FindItem(MENU_PageUp)->SetText(wxT("Move to start of take\tPGUP"));
				}
			}
			else {
				mPrevTakeButton->Disable();
				GetMenuBar()->FindItem(MENU_PageUp)->Enable(false);
				GetMenuBar()->FindItem(MENU_Up)->Enable(false);
			}
			//"next" button/"next", "down" and "last" menu items
			if (mCurrentSelectedEvent != (unsigned long) mEventList->GetItemCount() - 1) {
				//not at the end of the last take
	//			mNextTakeButton->Enable();
				EnableButtonReliably(mNextTakeButton);
				GetMenuBar()->FindItem(MENU_PageDown)->Enable();
				GetMenuBar()->FindItem(MENU_Down)->Enable();
				if (item.GetData() != mTakeInfoArray.GetCount() - 1) {
					//not in the last take
					mNextTakeButton->SetLabel(wxT("Next Take"));
					GetMenuBar()->FindItem(MENU_PageDown)->SetText(wxT("Move to start of next take\tPGDN"));
				}
				else {
					//somewhere in the last take
					mNextTakeButton->SetLabel(wxT("End of Take"));
					GetMenuBar()->FindItem(MENU_PageDown)->SetText(wxT("Move to end of take\tPGDN"));
				}
			}
			else {
				mNextTakeButton->Disable();
				GetMenuBar()->FindItem(MENU_PageDown)->Enable(false);
				GetMenuBar()->FindItem(MENU_Down)->Enable(false);
			}
		}
		else {
			//empty list
			mPrevTakeButton->Disable();
			GetMenuBar()->FindItem(MENU_PageUp)->Enable(false);
			mNextTakeButton->Disable();
			GetMenuBar()->FindItem(MENU_PageDown)->Enable(false);
			GetMenuBar()->FindItem(MENU_Up)->Enable(false);
			GetMenuBar()->FindItem(MENU_Down)->Enable(false);
			mDeleteCueButton->Disable();
			mPlayProjectNameCtrl->SetLabel(wxT(""));
			mCurrentTakeInfo = 0; //no takes
			mCurrentSelectedEvent = 0;
			mPlayer->Reset();
			mPlaybackTrackSelector->Clear();
		}
		mPlayProjectNameBox->GetStaticBox()->SetLabel(wxT("Project"));
	} //not file mode
	//player shortcuts
	UpdateTextShortcutStates();
	GetMenuBar()->FindItem(MENU_PrevTrack)->Enable(mPlaybackTrackSelector->EarlierTrack(false));
	GetMenuBar()->FindItem(MENU_NextTrack)->Enable(mPlaybackTrackSelector->LaterTrack(false));
	//cue button
	switch (mStatus) {
//		case UNKNOWN:
		case RUNNING_UP: case RUNNING_DOWN:
			mCueButton->Disable();
			mCueButton->SetToolTip(wxT(""));
			break;
		case STOPPED:
			mCueButton->SetLabel(wxT("Play"));
			mCueButton->SetToolTip(wxT("Play from current position"));
			mCueButton->Disable();
			break;
		case RECORDING:
//			mCueButton->Enable();
			EnableButtonReliably(mCueButton);
			mCueButton->SetLabel(wxT("Mark cue"));
			mCueButton->SetToolTip(wxT("Store a cue point for subsequent review"));
			break;
		case PLAYING: case PLAYING_BACKWARDS:
//			mCueButton->Enable();
			EnableButtonReliably(mCueButton);
			mCueButton->SetLabel(wxT("Pause"));
			mCueButton->SetToolTip(wxT("Freeze playback"));
			break;
		case PAUSED:
			if (mPlayer->AtEnd()) {
				mCueButton->SetLabel(wxT("Replay"));
				mCueButton->SetToolTip(wxT("Playback from start"));
			}
			else {
				mCueButton->SetLabel(wxT("Play"));
				mCueButton->SetToolTip(wxT("Play from current position"));
			}
//			mCueButton->Enable(mPlayer->IsOK());
			EnableButtonReliably(mCueButton, mPlayer->IsOK());
			break;
	}
}

/// Enable or disable the menu items which have shortcut keys that conflict with a text control
void IngexguiFrame::UpdateTextShortcutStates()
{
	GetMenuBar()->FindItem(MENU_J)->Enable(!mDescriptionControlHasFocus && mPlayer->Within() && !mPlayer->AtMaxReverseSpeed());
	GetMenuBar()->FindItem(MENU_K)->Enable(!mDescriptionControlHasFocus && mPlayer->IsOK() && PAUSED != mStatus);
	GetMenuBar()->FindItem(MENU_L)->Enable(!mDescriptionControlHasFocus && mPlayer->IsOK() && !mPlayer->AtMaxForwardSpeed());
	GetMenuBar()->FindItem(MENU_Space)->Enable(!mDescriptionControlHasFocus && mPlayer->IsOK() && (PLAYING == mStatus || PLAYING_BACKWARDS == mStatus || (PAUSED == mStatus && (!mLastPlayingBackwards || mPlayer->Within()))));
	GetMenuBar()->FindItem(MENU_3)->Enable(!mDescriptionControlHasFocus && PAUSED == mStatus && mPlayer->Within());
	GetMenuBar()->FindItem(MENU_4)->Enable(!mDescriptionControlHasFocus && PAUSED == mStatus && !mPlayer->AtEnd());
	GetMenuBar()->FindItem(MENU_Home)->Enable(!mDescriptionControlHasFocus && mTakeInfoArray.GetCount() && (mCurrentSelectedEvent || mPlayer->Within()));
	GetMenuBar()->FindItem(MENU_End)->Enable(!mDescriptionControlHasFocus && mTakeInfoArray.GetCount() && (mCurrentSelectedEvent != (unsigned long) mEventList->GetItemCount() - 1));
}

/// Clears the list of events, the track buttons and anything being displayed by the player
void IngexguiFrame::ClearLog()
{
	ResetPlayer();
	mEventList->DeleteAllItems();
	mTakeInfoArray.Empty();
	mCurrentTakeInfo = 0;
	GetMenuBar()->FindItem(MENU_ClearLog)->Enable(false);
}

/// Clears the track buttons and anything being displayed by the player
void IngexguiFrame::ResetPlayer()
{
	mPlayer->Reset();
	mPlaybackTrackSelector->Clear();
}

/// Starts playing forwards or backwards, remembering the direction supplied
/// @param backwards True to play backwards
void IngexguiFrame::Play(const bool backwards)
{
	mLastPlayingBackwards = backwards;
	if (backwards) {
		mPlayer->PlayBackwards();
	}
	else {
		if (mPlayer->AtEnd()) {
			mPlayer->JumpToCue(0); //the start - replay
		}
		mPlayer->Play();
	}
}

/// Use instead of wxButton::Enable() to ensure a button is clickable if enabled while the mouse pointer is over it.  Otherwise, it appears enabled but click has no effect until the pointer is moved off the button and on again.  Apparently a GTK bug (wxWidgets bug tracker ID 1735025)
/// @param button The button to enable or disable.
/// @param state True to enable.
void IngexguiFrame::EnableButtonReliably(wxButton * button, bool state)
{
//bugtracker says call CaptureMouse() and then ReleaseMouse() to work round - but on which object?  Need to test this some time
	if (state) { //enabling
		wxPoint mousePosition;
		if (wxFindWindowAtPointer(mousePosition) == button) { //the mouse pointer is over the button
			//move it out of the button before enabling
			WarpPointer(0, 0); //assumes no button at extreme top left of the frame!
			button->Enable();
			//move it back
			mousePosition = ScreenToClient(mousePosition);
			WarpPointer(mousePosition.x, mousePosition.y);
		}
		else {
			button->Enable();
		}
	}
	else { //Disabling
		button->Disable();
	}
}

/// Responds to Clear Description button being pressed.
/// Clears the description text box and puts the focus in it for immediate editing.
/// @param event The button event.
void IngexguiFrame::OnClearDescription(wxCommandEvent & WXUNUSED(event))
{
	mDescriptionCtrl->Clear(); //generates a text change event which will disable the button
	mDescriptionCtrl->SetFocus(); //generates a focus event
}

/// Responds to text changing in the Description box.
/// Enables/disables Clear button.
/// @param event The text ctrl event.
void IngexguiFrame::OnDescriptionChange(wxCommandEvent & WXUNUSED(event))
{
	EnableButtonReliably(mDescClearButton, mDescriptionCtrl->GetLineLength(0));
}

/// Called when ENTER is pressed on the text description.
/// Gives the focus to something else so that conflicting shortcuts can be re-enabled.
/// @param event The command event.
void IngexguiFrame::OnDescriptionEnterKey(wxCommandEvent & WXUNUSED(event))
{
	mNotebook->SetFocus(); //seems as good a thing as any - has to be something which is enabled
}

/// Called when something gets focus.
/// If it's the description control, disables conflicting shortcuts so that they go to the control.
/// @param event The focus event.
void IngexguiFrame::OnFocusGot(wxFocusEvent & event)
{
	if (TEXTCTRL_Description == event.GetId()) {
		mDescriptionControlHasFocus = true;
		UpdateTextShortcutStates();
	}
}

/// Called when something loses focus.
/// If it's the description control, enables conflicting shortcuts so that they can be used.
/// @param event The focus event.
void IngexguiFrame::OnFocusLost(wxFocusEvent & event)
{
	if (TEXTCTRL_Description == event.GetId()) {
		mDescriptionControlHasFocus = false;
		UpdateTextShortcutStates();
	}
}

/// Sets controls to the disconnected state (does not affect any player controls)
void IngexguiFrame::ResetToDisconnected()
{
	mTapeIdButton->Disable();
	mAlertCtrl->SetBitmap(blank);
	mAlertCtrl->SetToolTip(wxT(""));
	GetMenuBar()->FindItem(MENU_SetRolls)->Enable(false);
	mTimepos->DisableTimecode();
	mRecordButton->SetLabel(wxT("Record"));
}

/// Deal with log messages: send to log stream
/// @param message The string to display.
void IngexguiFrame::Log(const wxString & message)
{
//	std::cerr << wxDateTime::Now().FormatISOTime().mb_str(*wxConvCurrent) << " " << message.mb_str(*wxConvCurrent) << std::endl;

	// Contrary to the documentation, wxLogMessage does in fact want
	// a const wxChar* argument so this works fine with unicode.
	wxLogMessage(message);
}

void IngexguiFrame::OnJumpToTimecode(wxCommandEvent & WXUNUSED(event))
{
	JumpToTimecodeDlg dlg(this);
}
