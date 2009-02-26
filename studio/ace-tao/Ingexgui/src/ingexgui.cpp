/***************************************************************************
 *   $Id: ingexgui.cpp,v 1.11 2009/02/26 19:17:10 john_f Exp $            *
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

#include <wx/wx.h>
#include "wx/tooltip.h" //for SetDelay()
#include "wx/splitter.h"
#include "wx/stdpaths.h"
#include "wx/filename.h"
#include "wx/xml/xml.h"
#include "wx/file.h"
#include "wx/tglbtn.h"
#include "help.h"
#include "jogshuttle.h"
#include "JogShuttle.h"
#include "eventlist.h"

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
	EVT_MENU( MENU_StepBackwards, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_StepForwards, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_TogglePlayFile, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_Mute, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_Up, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_Down, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_PrevTake, IngexguiFrame::OnPrevTake )
	EVT_MENU( MENU_NextTake, IngexguiFrame::OnNextTake )
	EVT_MENU( MENU_FirstTake, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_LastTake, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_PlayBackwards, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_Pause, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_PlayForwards, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_PlayPause, IngexguiFrame::OnShortcut )
	EVT_MENU( MENU_ClearLog, IngexguiFrame::OnClearLog )
	EVT_MENU( MENU_PlayerDisable, IngexguiFrame::OnPlayerDisable )
	EVT_MENU( MENU_PlayMOV, IngexguiFrame::OnPlayerOpenFile )
	EVT_MENU( MENU_PlayMXF, IngexguiFrame::OnPlayerOpenFile )
	EVT_MENU( MENU_PlayerAbsoluteTimecode, IngexguiFrame::OnPlayerOSDChange )
	EVT_MENU( MENU_PlayerRelativeTimecode, IngexguiFrame::OnPlayerOSDChange )
	EVT_MENU( MENU_PlayerNoOSD, IngexguiFrame::OnPlayerOSDChange )
	EVT_MENU( MENU_PlayerMuteAudio, IngexguiFrame::OnPlayerMuteAudio )
	EVT_MENU( MENU_PlayerAudioFollowsVideo, IngexguiFrame::OnPlayerAudioFollowsVideo )
	EVT_MENU( MENU_PlayerAccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
#ifdef HAVE_DVS
	EVT_MENU( MENU_PlayerEnableSDIOSD, IngexguiFrame::OnPlayerSDIOSDChange )
	EVT_MENU( MENU_PlayerExtOutput, IngexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_PlayerExtAccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_PlayerExtUnaccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
#endif
	EVT_MENU( MENU_PlayerUnaccelOutput, IngexguiFrame::OnPlayerOutputTypeChange )
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
	EVT_BUTTON( BUTTON_PrevTake, IngexguiFrame::OnPrevTake )
	EVT_BUTTON( BUTTON_NextTake, IngexguiFrame::OnNextTake )
	EVT_BUTTON( BUTTON_JumpToTimecode, IngexguiFrame::OnJumpToTimecode )
	EVT_BUTTON( BUTTON_DeleteCue, IngexguiFrame::OnDeleteCue )
	EVT_TOGGLEBUTTON( BUTTON_PlayFile, IngexguiFrame::OnPlayFile )
	EVT_COMMAND( wxID_ANY, wxEVT_PLAYER_MESSAGE, IngexguiFrame::OnPlayerEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_TREE_MESSAGE, IngexguiFrame::OnTreeEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_RECORDERGROUP_MESSAGE, IngexguiFrame::OnRecorderGroupEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_TEST_DLG_MESSAGE, IngexguiFrame::OnTestDlgEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_JOGSHUTTLE_MESSAGE, IngexguiFrame::OnJogShuttleEvent )
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
//				guiMenuEvent.SetId(IngexguiFrame::MENU_PrevTake);
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
	: wxFrame((wxFrame *)0, wxID_ANY, wxT("ingexgui")/*, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS* - this doesn't prevent cursor keys being lost, as hoped */), mStatus(STOPPED), mDescriptionControlHasFocus(false), mFileModeSelectedTrack(0), mToday(wxDateTime::Today())

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
	menuPlayer->AppendCheckItem(MENU_PlayerDisable, wxT("Disable player"));
	menuPlayer->Append(MENU_PlayMOV, wxT("Play MOV file..."));
	menuPlayer->Append(MENU_PlayMXF, wxT("Play MXF file(s)..."));
	wxMenu * menuPlayerType = new wxMenu;
	wxMenu * menuPlayerOSD = new wxMenu;
#ifdef HAVE_DVS
	menuPlayerType->AppendRadioItem(MENU_PlayerExtAccelOutput, wxT("External monitor and computer screen (accelerated if possible)")); //first item is selected by default
#endif
	menuPlayerType->AppendRadioItem(MENU_PlayerAccelOutput, wxT("Computer screen (accelerated if possible)")); //selected by default if no DVS card
#ifdef HAVE_DVS
	menuPlayerType->AppendRadioItem(MENU_PlayerExtOutput, wxT("External monitor"));
	menuPlayerType->AppendRadioItem(MENU_PlayerExtUnaccelOutput, wxT("External monitor and computer screen unaccelerated (use if accelerated fails)"));
#endif
	menuPlayerType->AppendRadioItem(MENU_PlayerUnaccelOutput, wxT("Computer screen unaccelerated (use if accelerated fails)"));

	menuPlayerOSD->AppendRadioItem(MENU_PlayerAbsoluteTimecode, wxT("&Absolute timecode")); //first item is selected by default
	menuPlayerOSD->AppendRadioItem(MENU_PlayerRelativeTimecode, wxT("&Relative timecode"));
	menuPlayerOSD->AppendRadioItem(MENU_PlayerNoOSD, wxT("&OSD Off"));
	menuPlayer->Append(MENU_PlayerType, wxT("Player type"), menuPlayerType);
	menuPlayer->Append(MENU_PlayerOSD, wxT("Player On-Screen Display"), menuPlayerOSD);
	menuPlayer->AppendCheckItem(MENU_PlayerMuteAudio, wxT("Mute audio"));
	menuPlayer->AppendCheckItem(MENU_PlayerAudioFollowsVideo, wxT("Audio corresponds to video source displayed"));
#ifdef HAVE_DVS
	menuPlayer->AppendCheckItem(MENU_PlayerEnableSDIOSD, wxT("Enable external monitor On-screen Display"));
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
	menuShortcuts->Append(MENU_Up); //label set dynamically
	menuShortcuts->Append(MENU_Down); //label set dynamically
	menuShortcuts->Append(MENU_PrevTake, wxT("Move to previous take\tPGUP"));
	menuShortcuts->Append(MENU_NextTake, wxT("Move to next take\tPGDN"));
	menuShortcuts->Append(MENU_FirstTake, wxT("Move to start of first take\tHOME"));
	menuShortcuts->Append(MENU_LastTake, wxT("Move to start of last take\tEND"));
	menuShortcuts->Append(MENU_PlayBackwards, wxT("Play/fast backwards\tJ"));
	menuShortcuts->Append(MENU_Pause, wxT("Pause\tK"));
	menuShortcuts->Append(MENU_PlayForwards, wxT("Play/fast forwards\tL"));
	menuShortcuts->Append(MENU_PlayPause, wxT("Play/Pause\tSPACE"));
	menuShortcuts->Append(MENU_StepBackwards, wxT("Step backwards\t3"));
	menuShortcuts->Append(MENU_StepForwards, wxT("Step forwards\t4"));
	menuShortcuts->Append(MENU_TogglePlayFile, wxT("Toggle Play Files\tF9"));
	menuShortcuts->Append(MENU_Mute, wxT("Mute/unmute playback audio\tM"));
	menuHelp->Append(-1, wxT("Shortcuts"), menuShortcuts); //add this after adding menu items to it

	SetMenuBar(menuBar);
#ifdef HAVE_DVS
	mPlayer = new Player(this, true, prodauto::DUAL_DVS_AUTO_OUTPUT, SOURCE_TIMECODE); //must delete this explicitly on app exit
	if (!mPlayer->ExtOutputIsAvailable()) {
		GetMenuBar()->FindItem(MENU_PlayerExtAccelOutput)->Enable(false);
		GetMenuBar()->FindItem(MENU_PlayerExtOutput)->Enable(false);
		GetMenuBar()->FindItem(MENU_PlayerExtUnaccelOutput)->Enable(false);
		mPlayer->SetOutputType(prodauto::X11_AUTO_OUTPUT);
		GetMenuBar()->FindItem(MENU_PlayerAccelOutput)->Check();
	}
#else
	mPlayer = new Player(this, true, prodauto::X11_AUTO_OUTPUT, SOURCE_TIMECODE); //must delete this explicitly on app exit
#endif
	mPlayer->AudioFollowsVideo(GetMenuBar()->FindItem(MENU_PlayerAudioFollowsVideo)->IsChecked());
#ifdef HAVE_DVS
	mPlayer->EnableSDIOSD(GetMenuBar()->FindItem(MENU_PlayerEnableSDIOSD)->IsChecked());
#endif
	CreateStatusBar();

	//saved state
	mSavedStateFilename = wxStandardPaths::Get().GetUserConfigDir() + wxFileName::GetPathSeparator() + SAVED_STATE_FILENAME;
	if (!wxFile::Exists(mSavedStateFilename)) {
		wxMessageDialog dlg(this, wxT("No saved preferences file found.  Default values will be used."), wxT("No saved preferences")); //NB not using wxMessageBox because (in GTK) it doesn't stop the parent window from being selected, so it can end up hidden, making the app appear to have hanged
		dlg.ShowModal();
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode);
	}
	else if (!mSavedState.Load(mSavedStateFilename)) { //already produces a warning message box
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode);
	}
	else if (wxT("Ingexgui") != mSavedState.GetRoot()->GetName()) {
		wxMessageDialog dlg(this, wxT("Saved preferences file \"") + mSavedStateFilename + wxT("\": has unrecognised data: recreating with default values."), wxT("Unrecognised saved preferences"), wxICON_EXCLAMATION | wxOK);
		dlg.ShowModal();
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode); //dialogue will detect this as updated
	}
	mRecorderGroup = new RecorderGroupCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, argc, argv, &mSavedState);
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
	mRecProjectNameCtrl = new wxStaticText(recordPage, wxID_ANY, wxT("")); //name added later
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
	mEventList = new EventList(eventPanel, -1, wxDefaultPosition, wxSize(splitterWindow->GetClientSize().x, EVENT_LIST_HEIGHT));
	eventPanelSizer->Add(mEventList, 1, wxEXPAND + wxALL, CONTROL_BORDER);

	wxToolTip::SetDelay(TOOLTIP_DELAY);

	Fit();
	splitterWindow->SetMinimumPaneSize(1); //to allow it to be almost but not quite closed.  Do this after Fit() or the earlier value will not be reflected in the window layout.
	SetMinSize(GetSize()); //prevents scrollbar of event list disappearing oddly

	mCuePointsDlg = new CuePointsDlg(this, mSavedState); //this is done now so that a file doesn't have to be read each time a cue point is marked
	mTestModeDlg = new TestModeDlg(this); //this is done now so that it can remember its parameters between invocations

	//currently selected project
	wxString currentProject = SetProjectDlg::GetCurrentProjectName(mSavedState);
	if (currentProject.IsEmpty()) {
		SetTitle(TITLE);
	}
	else {
		SetTitle(wxString(TITLE) + wxT(": ") + currentProject);
		mRecorderGroup->SetCurrentProjectName(currentProject);
		mRecProjectNameCtrl->SetLabel(currentProject);
		GetMenuBar()->FindItem(MENU_ClearLog)->Enable(mEventList->SetCurrentProjectName(currentProject));
	}

	//jog/shuttle
        mJogShuttle = new ingex::JogShuttle();
        mJSListener = new JSListener(this);
        mJogShuttle->addListener(mJSListener);
        mJogShuttle->start();

	ResetToDisconnected();
	SetStatus(STOPPED);
}

/// Responds to an application close event by closing, preventing closure or seeking confirmation, depending on the circumstances.
/// @param event The close event.
void IngexguiFrame::OnClose(wxCloseEvent & event)
{
	if (event.CanVeto() && RECORDING == mStatus) {
		wxMessageDialog dlg(this, wxT("You cannot close the application while recording: stop the recording first"), wxT("Cannot close while recording"), wxICON_EXCLAMATION | wxOK);
		dlg.ShowModal();
		event.Veto();
	}
	else {
		wxMessageDialog dlg(this, wxT("Are you sure?  You are not disconnected"), wxT("Confirmation of Quit"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
		if (!event.CanVeto() || !mTree->HasRecorders() || wxID_YES == dlg.ShowModal()) {
			if (mTree->HasRecorders()) {
				delete mRecorderGroup; //or doesn't exit
			}
			delete mPlayer; //traffic control notification will be sent
			Destroy();
			Log(wxT("Application closed."));
		}
		else {
			event.Veto();
		}
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
		GetMenuBar()->FindItem(MENU_ClearLog)->Enable(mEventList->SetCurrentProjectName(dlg.GetSelectedProject()));
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
	if (GetMenuBar()->FindItem(MENU_PlayerAbsoluteTimecode)->IsChecked()) {
		mPlayer->SetOSD(SOURCE_TIMECODE);
	}
	else if (GetMenuBar()->FindItem(MENU_PlayerRelativeTimecode)->IsChecked()) {
		mPlayer->SetOSD(CONTROL_TIMECODE);
	}
	else { //OSD off
		mPlayer->SetOSD(OSD_OFF);
	}
}

#ifdef HAVE_DVS
/// Responds to SDI OSD disable menu requests by communicating with the player.
/// @param event The command event.
void IngexguiFrame::OnPlayerSDIOSDChange( wxCommandEvent& WXUNUSED(event) )
{
	mPlayer->EnableSDIOSD(GetMenuBar()->FindItem(MENU_PlayerEnableSDIOSD)->IsChecked());
}
#endif

/// Responds to a player type/size menu requests by communicating with the player and enabling/disabling other menu items as appropriate.
/// @param event The command event.
void IngexguiFrame::OnPlayerOutputTypeChange(wxCommandEvent& WXUNUSED(event))
{
	if (GetMenuBar()->FindItem(MENU_PlayerAccelOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::X11_AUTO_OUTPUT);
	}
#ifdef HAVE_DVS
	else if (GetMenuBar()->FindItem(MENU_PlayerExtOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::DVS_OUTPUT);
	}
	else if (GetMenuBar()->FindItem(MENU_PlayerExtAccelOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::DUAL_DVS_AUTO_OUTPUT);
	}
	else if (GetMenuBar()->FindItem(MENU_PlayerExtUnaccelOutput)->IsChecked()) {
		mPlayer->SetOutputType(prodauto::DUAL_DVS_X11_OUTPUT);
	}
#endif
	else {
		//on-screen unaccelerated
		mPlayer->SetOutputType(prodauto::X11_OUTPUT);
	}
}

/// Responds to player audio mute menu request
/// @param event The command event.
void IngexguiFrame::OnPlayerMuteAudio(wxCommandEvent& WXUNUSED(event))
{
	mPlayer->MuteAudio(GetMenuBar()->FindItem(MENU_PlayerMuteAudio)->IsChecked());
}

/// Responds to "player audio follows video" menu request
/// @param event The command event.
void IngexguiFrame::OnPlayerAudioFollowsVideo(wxCommandEvent& WXUNUSED(event))
{
	mPlayer->AudioFollowsVideo(GetMenuBar()->FindItem(MENU_PlayerAudioFollowsVideo)->IsChecked());
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
		case MENU_Down :
			//down one event
			if (mPlayFileButton->GetValue()) { //File Mode
				mPlayer->JumpToCue(MENU_Down == event.GetId() ? 1 : 0);
			}
			else {
				mEventList->SelectAdjacentEvent(MENU_Down == event.GetId());
			}
			break;
		case MENU_FirstTake :
			mEventList->Select(0);
			break;
		case MENU_LastTake :
			mEventList->SelectLastTake();
			break;
		case MENU_PlayBackwards :
			mPlayer->Play(true, true);
			break;
		case MENU_Pause :
			mPlayer->Pause();
			break;
		case MENU_PlayForwards :
			mPlayer->Play(true, false);
			break;
		case MENU_PlayPause :
			if (PAUSED == mStatus) {
				mPlayer->Play();
			}
			else {
				mPlayer->Pause();
			}
			break;
		case MENU_StepBackwards :
			mPlayer->Step(false);
			break;
		case MENU_StepForwards :
			mPlayer->Step(true);
			break;
		case MENU_TogglePlayFile :
			{
				mPlayFileButton->SetValue(!mPlayFileButton->GetValue());
				//doesn't create an event
				wxCommandEvent buttonEvent(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, BUTTON_PlayFile);
				AddPendingEvent(buttonEvent);
				break;
			}
		case MENU_Mute :
			GetMenuBar()->FindItem(MENU_PlayerMuteAudio)->Check(!GetMenuBar()->FindItem(MENU_PlayerMuteAudio)->IsChecked());
			OnPlayerMuteAudio(event); //event is not used
			break;
	}
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
	mEventList->SelectPrevTake(mPlayer->Within());
	UpdatePlayerAndEventControls(false, true); //force player to jump to cue, as this won't happen if the selected event hasn't changed
}

/// Responds to the Next Take button/menu item by moving to the start of the next take
/// @param event The command event.
void IngexguiFrame::OnNextTake( wxCommandEvent& WXUNUSED(event))
{
	mEventList->SelectNextTake(); //generates an event which will update controls
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
///	AT_START/WITHIN/AT_END: Update controls as some states etc will change.
///	CLOSE_REQ: Switch the player to external output only, or disable completely if external output is not being used.
///	QUADRANT_CLICK: Toggle between full-screen and single source defined by the quadrant.
///		Event int: the quadrant clicked on.
///	FILE_MODE: Reflect status in controls.
///		Event int: non-zero for file mode enabled.
///	KEYPRESS: Respond to the user pressing a shortcut key in the player window.
///		Event int: X-windows key value.
void IngexguiFrame::OnPlayerEvent(wxCommandEvent& event) {
	switch ((PlayerEventType) event.GetId()) {
		case NEW_FILESET :
			mPlaybackTrackSelector->EnableAndSelectTracks((std::vector<bool> *) event.GetClientData(), event.GetInt());
			break;
		case STATE_CHANGE :
			switch (event.GetInt()) {
				case PlayerMode::PLAY :
					SetStatus(PLAYING);
					break;
				case PlayerMode::PLAY_BACKWARDS :
					SetStatus(PLAYING_BACKWARDS);
					break;
				case PlayerMode::PAUSE :
					if (!IsRecording()) { //player is in control of state (This check prevents a sitaution where when toggling rapidly between play and record we get a paused status but recorders are still recording. Record button is not enabled, because recorder selector is not enabled, because RecorderGroup::Stop() hasn't been called.  Position is still running and tree/status are indicating problems.  The only way out of this is to quit - stopping the recorders with another GUI leaves the recorder selector and record buttons disabled.)
						SetStatus(PAUSED);
					}
					break;
				case PlayerMode::STOP :
					if (!IsRecording()) { //player is in control of state
						SetStatus(STOPPED);
					}
					break;
				case PlayerMode::CLOSE :
					//Issued on Enable(false) _and_ Reset()
					UpdatePlayerAndEventControls();
					break;
			}
			break;
		case SPEED_CHANGE :
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
		case CUE_POINT :
			//move to the right position in the list
			if (!mPlayFileButton->GetValue()) {
				mEventList->Select(event.GetInt(), true); //As it is the player selecting this event, stop the player being told to jump to it
			}
			break;
		case FRAME_DISPLAYED :
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
			break;
		case AT_START :
		case WITHIN :
		case AT_END :
			UpdatePlayerAndEventControls();
			break;
		case CLOSE_REQ : {
#ifdef HAVE_DVS
				if (GetMenuBar()->FindItem(MENU_PlayerExtAccelOutput)->IsChecked() || GetMenuBar()->FindItem(MENU_PlayerExtUnaccelOutput)->IsChecked()) {
					//external output is active so don't close the player, just switch to external only
					GetMenuBar()->FindItem(MENU_PlayerExtOutput)->Check(); //doesn't generate an event
					wxCommandEvent dummyEvent;
					OnPlayerOutputTypeChange(dummyEvent);
				}
				else {
#endif
					//close the player completely
					GetMenuBar()->FindItem(MENU_PlayerDisable)->Check(); //doesn't generate an event
					wxCommandEvent dummyEvent;
					OnPlayerDisable(dummyEvent);
#ifdef HAVE_DVS
				}
#endif
			}
			break;
		case QUADRANT_CLICK :
			mPlaybackTrackSelector->SelectQuadrant(event.GetInt());
			break;
		case KEYPRESS : {
//std::cerr << "key: " << event.GetInt() << std::endl;
//std::cerr << "modifier: " << event.GetExtraLong() << std::endl;
			if (mCuePointsDlg->IsModal() && event.GetInt() > 47 && event.GetInt() < 58) { //"add cue" shortcut
				mCuePointsDlg->Shortcut(event.GetInt() - 48);
			}
			else if (mCuePointsDlg->IsModal() && event.GetInt() == 65307) { //escape: "dismiss dialogue" shortcut
				mCuePointsDlg->Shortcut(-1);
			}
			else {
				wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED);
				menuEvent.SetId(wxID_ANY); //indicates no hit so far
				switch (event.GetInt()) {
					case 65470: //F1
						menuEvent.SetId(MENU_Record);
						break;
					case 65471: //F2
						menuEvent.SetId(MENU_MarkCue);
						break;
					case 65474: //F5 (NB shift is detected below)
						menuEvent.SetId(MENU_Stop);
						break;
					case 65476: //F7
						menuEvent.SetId(MENU_PrevTrack);
						break;
					case 65477: //F8
						menuEvent.SetId(MENU_NextTrack);
						break;
					case 65478: //F9
						menuEvent.SetId(MENU_TogglePlayFile);
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
						menuEvent.SetId(MENU_PrevTake);
						break;
					case 65366 : //PgDn
						menuEvent.SetId(MENU_NextTake);
						break;
					case 65360 : //Home
						//First Take
						menuEvent.SetId(MENU_FirstTake);
						break;
					case 65367 : //End
						menuEvent.SetId(MENU_LastTake);
						break;
					case 106 : //j
						menuEvent.SetId(MENU_PlayBackwards);
						break;
					case 107 : //k
						menuEvent.SetId(MENU_Pause);
						break;
					case 108 : //l
						menuEvent.SetId(MENU_PlayForwards);
						break;
					case 32 : //space
						menuEvent.SetId(MENU_PlayPause);
						break;
					case 51 : case 65361 : //3 and left
						menuEvent.SetId(MENU_StepBackwards);
						break;
					case 52 : case 65363 : //4 and right
						menuEvent.SetId(MENU_StepForwards);
						break;
					case 109 : //m
						menuEvent.SetId(MENU_Mute);
						break;
				}
				if ((MENU_Stop == menuEvent.GetId() && event.GetExtraLong() != 1) || (MENU_Stop != menuEvent.GetId() && event.GetExtraLong())) {
					//stop has been pressed without shift (only), or any other keypress has a modifier
					menuEvent.SetId(wxID_ANY); //ignore
				}
				//menu events are not blocked even if the menu is disabled, so check explicitly
				//some menus are disabled due to focus being on text boxes, but this can never happen if keystrokes are being sent to the player window
				if (wxID_ANY != menuEvent.GetId() && GetMenuBar()->FindItem(menuEvent.GetId())->IsEnabled()) { //recognised the keypress and the shortcut is enabled
					AddPendingEvent(menuEvent);
				}
			}
			break;
		}
		default:
			break;
	}
}

/// Responds to the an event from the jog/shuttle control.
/// @param event The command event.
void IngexguiFrame::OnJogShuttleEvent(wxCommandEvent& event)
{
	wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED);
	menuEvent.SetId(wxID_ANY); //indicates no hit so far
	switch ((JogShuttleEventType) event.GetId()) {
		case JOG :
			menuEvent.SetId(MENU_Pause);
			AddPendingEvent(menuEvent);
			menuEvent.SetId(event.GetInt() ? MENU_StepForwards : MENU_StepBackwards);
			break;
		case SHUTTLE :
			mPlayer->PlayAbsolute(event.GetInt());
			break;
		case JS_BUTTON_PRESSED :
			switch (event.GetInt()) {
				case 1: //1st row, left hand side
					menuEvent.SetId(MENU_Record);
					break;
				case 2: //1st row, second from left
					menuEvent.SetId(MENU_MarkCue);
					break;
				case 3: case 4: //1st row, right hand two
					if (mJSListener->IsPressed(14) || mJSListener->IsPressed(15) //eith black button on Shuttle Pro 2
					 || (mJSListener->IsPressed(4) && mJSListener->IsPressed(3))) { //or both buttons must be pressed
						if (GetMenuBar()->FindItem(MENU_Stop)->IsEnabled()) {
							//cancel any cue point entering operation
							mCuePointsDlg->EndModal(wxID_CANCEL);
						}
						menuEvent.SetId(MENU_Stop);
					}
					break;
				case 5: //2nd row, left hand side
					menuEvent.SetId(MENU_PlayBackwards);
					break;
				case 6: //2nd row, second from left
					menuEvent.SetId(MENU_Pause);
					break;
				case 7: //2nd row, middle
					menuEvent.SetId(MENU_PlayForwards);
					break;
				case 8: //2nd row, second from right
					menuEvent.SetId(MENU_Mute);
					break;
				case 9: //2nd row, right hand side
					menuEvent.SetId(MENU_TogglePlayFile);
					break;
				case 10: //top left long silver button
					menuEvent.SetId(MENU_Up);
					break;
				case 12: //bottom left long silver button
					menuEvent.SetId(MENU_Down);
					break;
				case 11: //top right long silver button
					menuEvent.SetId(MENU_PrevTake);
					break;
				case 13: //bottom right long silver button
					menuEvent.SetId(MENU_NextTake);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	if (wxID_ANY != menuEvent.GetId() && GetMenuBar()->FindItem(menuEvent.GetId())->IsEnabled()) { //recognised the keypress and the shortcut is enabled
		AddPendingEvent(menuEvent);
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
		mRecorderGroup->Stop(now, mDescriptionCtrl->GetLineText(0).Trim(false).Trim(true), mEventList->GetLocators());
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
				mTimepos->GetStartTimecode(&timecode);
				const int64_t frameCount = mTimepos->GetFrameCount();
				timecode.samples += frameCount; //will wrap automatically on display
				if (wxID_OK == mCuePointsDlg->ShowModal(Timepos::FormatTimecode(timecode))) {
					mEventList->AddEvent(EventList::CUE, 0, frameCount, mCuePointsDlg->GetDescription(), mCuePointsDlg->GetColourIndex());
					UpdatePlayerAndEventControls();
					mSavedState.Save(mSavedStateFilename);
				}
				break;
			}
		case PLAYING : case PLAYING_BACKWARDS :
			mPlayer->Pause();
			break;
		case PAUSED :
			mPlayer->Play(true, false); //always play forwards (more logical than following the direction last set by the keyboard)
			break;
		default :
			break;
	}
//	SetFocus();
}

/// Deletes the cue point corresponding to the currently selected event list item.
void IngexguiFrame::OnDeleteCue( wxCommandEvent& WXUNUSED( event ) )
{
	mEventList->DeleteCuePoint();
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
		mPlayer->Play();
	}
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
				ProdAuto::MxfTimecode startTimecode;
				mTimepos->GetStartTimecode(&startTimecode);
				mEventList->AddEvent(EventList::START, &startTimecode); //didn't start now - started before the preroll period
				UpdatePlayerAndEventControls();
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
			}
			break;
		case RecorderGroupCtrl::RECORDING :
			if (event.GetInt()) { //successful
				if (!mTree->IsRouterRecorder(event.GetString())) { //router recorders don't return sensible start timecodes
					mTimepos->Record(((RecorderData *) event.GetClientData())->GetTimecode());
					ProdAuto::MxfTimecode startTimecode;
					mTimepos->GetStartTimecode(&startTimecode);
					mEventList->AddEvent(EventList::START, &startTimecode); //don't automatically do this or if the first recorder to record is a router recorder we'll get an unknown start timecode
					UpdatePlayerAndEventControls();
				}
				mTree->SetRecorderStateOK(event.GetString());
				Log(wxT("RECORDING successfully on \"") + event.GetString() + wxT("\" @ ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
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
				if (((RecorderData *) event.GetClientData())->GetTrackList() && !mTree->IsRouterRecorder(event.GetString())) {
					mEventList->AddRecorderData((RecorderData *) event.GetClientData());
				}
				ProdAuto::MxfTimecode stopTimecode;
				mTimepos->GetTimecode(&stopTimecode);
				GetMenuBar()->FindItem(MENU_ClearLog)->Enable(); //there is stuff in the list so can be deleted
				//logging
				CORBA::StringSeq_var fileList = ((RecorderData *) event.GetClientData())->GetFileList();
				wxString msg = wxString::Format(wxT("%d element%s returned%s"), fileList->length(), 1 == fileList->length() ? wxT("") : wxT("s"), fileList->length() ? wxT(":") : wxT("."));
				for (size_t i = 0; i < fileList->length(); i++) {
					msg += wxT("\n\"") + wxString(fileList[i], *wxConvCurrent) + wxT("\"");
				}
				Log(msg);
				if (STOPPED != mStatus) {
					//use the first returned timecode as the stop position
					mTimepos->Stop(((RecorderData *) event.GetClientData())->GetTimecode());
					ProdAuto::MxfTimecode tc = ((RecorderData *) event.GetClientData())->GetTimecode();
					mEventList->AddEvent(EventList::STOP, &tc, mTimepos->GetFrameCount(), mRecorderGroup->GetCurrentDescription()); //will not add more than one
					SetStatus(STOPPED);
				}
				else {
					//need to reload the player as more files have appeared
					UpdatePlayerAndEventControls(true);
				}
			}
			else {
				Log(wxT("STOPPED failure on \"") + event.GetString() + wxT("\""));
				mTree->SetRecorderStateProblem(event.GetString(), wxT("Failed to stop: retrying"));
			}
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::TRACK_STATUS :
//spam			Log(wxT("TRACK_STATUS for \"") + event.GetString() + wxT("\""));
			mTree->SetTrackStatus(event.GetString(), IsRecording(), ((RecorderData *) event.GetClientData())->GetTrackStatusList()); //will set record button and status indicator
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::REMOVE_RECORDER :
			Log(wxT("REMOVE_RECORDER for \"") + event.GetString() + wxT("\""));
//			mTapeIdButton->Enable(mTree->RemoveRecorder(event.GetString()));
			EnableButtonReliably(mTapeIdButton, mTree->RemoveRecorder(event.GetString()));
			if (!mTree->HasRecorders() && IsRecording()) {//this check needed in case we disconnect from a recorder that's recording/running up and we can't contact
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
				Log(wxT("TIMECODE_STUCK for \"") + event.GetString() + wxT("\" @ ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
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
				ProdAuto::MxfTimecode timecode;
				mTimepos->GetTimecode(&timecode);
				mEventList->AddEvent(EventList::START, &timecode); //just in case the only recorder is a router recorder, so a start event won't already have been issued FIXME: GetStartPosition will return unknown
				UpdatePlayerAndEventControls();
			}
			mRecordButton->SetLabel(wxString::Format(wxT("Record (%d track%s)"), mTree->EnabledTracks(), mTree->EnabledTracks() == 1 ? wxT("") : wxT("s")));
		}
		else {
			ResetToDisconnected();
		}
	
		//Highlight the tape ID button if we can't record because of missing tape IDs
		mTapeIdButton->SetBackgroundColour(mTree->SomeEnabled() && !mTree->TapeIdsOK() ? BUTTON_WARNING_COLOUR : wxNullColour);
	}
	else { //recorder name supplied - tape ID updates
		CORBA::StringSeq packageNames, tapeIds;
		mTree->GetRecorderTapeIds(event.GetString(), packageNames, tapeIds);
		mRecorderGroup->SetTapeIds(event.GetString(), packageNames, tapeIds);
	}
}

/// Responds to the player enable/disable menu requests by telling the player and updating menu items.
/// @param event The command event.
void IngexguiFrame::OnPlayerDisable(wxCommandEvent& WXUNUSED( event ))
{
	bool enabled = !GetMenuBar()->FindItem(MENU_PlayerDisable)->IsChecked();
	mPlayer->Enable(enabled);
	if (enabled && mPlayFileButton->GetValue()) {
		mPlayer->SelectTrack(mFileModeSelectedTrack, false);
	}
	GetMenuBar()->FindItem(MENU_PlayerType)->Enable(enabled);
	GetMenuBar()->FindItem(MENU_PlayerOSD)->Enable(enabled);
	mPlayFileButton->Enable(enabled && mFileModeFiles.GetCount());
	GetMenuBar()->FindItem(MENU_TogglePlayFile)->Enable(enabled && mFileModeFiles.GetCount());
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
			mPlayFileButton->SetBackgroundColour(BUTTON_WARNING_COLOUR);
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
			mPlayFileButton->SetBackgroundColour(BUTTON_WARNING_COLOUR);
			UpdatePlayerAndEventControls(true); //reload player
		}
	}
}

/// Responds to the Play File toggle button being pressed
/// Loads the player with the correct set of files
void IngexguiFrame::OnPlayFile(wxCommandEvent & WXUNUSED( event ))
{
	mPlayFileButton->SetBackgroundColour(mPlayFileButton->GetValue() ? BUTTON_WARNING_COLOUR : wxNullColour);
	UpdatePlayerAndEventControls(true); //force player reload
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
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
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
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(false);
			mRecordButton->SetToolTip(wxT(""));
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable();
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
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(false);
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable();
			GetMenuBar()->FindItem(MENU_Stop)->Enable();
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable(false);
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
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(false);
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable();
			GetMenuBar()->FindItem(MENU_Stop)->Enable();
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable();
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
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
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
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
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
			GetMenuBar()->FindItem(MENU_TestMode)->Enable(enableRecordButton);
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			GetMenuBar()->FindItem(MENU_SetProjectName)->Enable();
			mStopButton->SetToolTip(wxT(""));
			mTree->EnableChanges();
			mPlayFileButton->Enable(mFileModeFiles.GetCount() || !mFileModeMovFile.IsEmpty());
			break;
	}
	GetMenuBar()->FindItem(MENU_Record)->Enable(mRecordButton->IsEnabled());
//	mStopButton->Enable(GetMenuBar()->FindItem(MENU_Stop)->IsEnabled());
	EnableButtonReliably(mStopButton, GetMenuBar()->FindItem(MENU_Stop)->IsEnabled());
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
				fileNames.push_back((const char*) mFileModeMovFile.mb_str(*wxConvCurrent));
				trackNames.push_back((const char*) mFileModeMovFile.mb_str(*wxConvCurrent));
				mPlayProjectNameBox->GetStaticBox()->SetLabel(wxT("Filename"));
				mPlayProjectNameCtrl->SetLabel(mFileModeMovFile);
				mPlaybackTrackSelector->Clear();
				mPlayer->Load(&fileNames, &trackNames, prodauto::FFMPEG_INPUT, mFileModeFrameOffset);
				mFileModeSelectedTrack = 1; //first source rather than quad split
			}
			mPlayer->SelectTrack(mFileModeSelectedTrack, false);
		}
		mPrevTakeButton->Disable();
		GetMenuBar()->FindItem(MENU_PrevTake)->Enable(false);
		mNextTakeButton->Disable();
		GetMenuBar()->FindItem(MENU_NextTake)->Enable(false);
		GetMenuBar()->FindItem(MENU_Up)->SetText(wxT("Move to start of file\tUP")); // TODO: this function to be deprecated - replace with line below for later versions of wx (somewhere between 2.8.4 and 2.8.9)
//		GetMenuBar()->FindItem(MENU_Up)->SetItemLabel(wxT("Move to start of file\tUP"));
		GetMenuBar()->FindItem(MENU_Down)->SetText(wxT("Move to end of file\tDOWN")); // TODO: this function to be deprecated - replace with line below for later versions of wx (somewhere between 2.8.4 and 2.8.9)
//		GetMenuBar()->FindItem(MENU_Down)->SetItemLabel(wxT("Move to end of file\tDOWN"));
		GetMenuBar()->FindItem(MENU_Up)->Enable(mPlayer->Within());
		GetMenuBar()->FindItem(MENU_Down)->Enable(!mPlayer->AtEnd());
		CanEditCues(false);
	}
	else { //not file mode
		if (mEventList->GetItemCount()) {
			CanEditCues(
				RECORDING == mStatus //no point in deleting cue points after recording, as the descriptions have already been sent to the recorder
				&& mEventList->LatestTakeCuePointIsSelected() //it's an appropriate cue point
			);
			//player control
			TakeInfo * currentTakeInfo = mEventList->GetCurrentTakeInfo();
			if (currentTakeInfo && currentTakeInfo->GetFiles()->GetCount()) { //the take is complete and there are at least some files available
				if (mEventList->SelectedTakeHasChanged() || forceLoad) { //a new take
					if (!forceLoad) { //not returning to previous situation
						mTakeModeFrameOffset = 0;
					}
					//update playback tracks notebook page
					mPlayProjectNameCtrl->SetLabel(currentTakeInfo->GetProjectName());
					std::vector<std::string> fileNames;
					std::vector<std::string> trackNames;
					mPlaybackTrackSelector->SetTracks(*currentTakeInfo, fileNames, trackNames);
					//load files and jump to current position
					mPlayer->Load(&fileNames, &trackNames, prodauto::MXF_INPUT, mTakeModeFrameOffset, &currentTakeInfo->GetCuePointFrames(), currentTakeInfo->GetStartIndex(), mEventList->GetFirstSelected() - currentTakeInfo->GetStartIndex());
				}
				else if (mEventList->SelectedEventHasChanged() || forceNewCuePoint) { //a different selected event
					mPlayer->JumpToCue(mEventList->GetFirstSelected() - currentTakeInfo->GetStartIndex());
				}
			}
			//"previous" button/"previous", "up" and "first" menu items
			if (!mEventList->AtTop() || mPlayer->Within()) {
				//not at the beginning of the first take
	//			mPrevTakeButton->Enable();
				EnableButtonReliably(mPrevTakeButton);
				GetMenuBar()->FindItem(MENU_PrevTake)->Enable();
				GetMenuBar()->FindItem(MENU_Up)->Enable();
				if (mEventList->AtStartOfTake() && !mPlayer->Within()) {
					//the start of this take is selected
					mPrevTakeButton->SetLabel(wxT("Previous Take"));
					GetMenuBar()->FindItem(MENU_PrevTake)->SetText(wxT("Move to start of previous take\tPGUP"));
				}
				else {
					//somewhere within a take
					mPrevTakeButton->SetLabel(wxT("Start of Take"));
					GetMenuBar()->FindItem(MENU_PrevTake)->SetText(wxT("Move to start of take\tPGUP"));
				}
			}
			else {
				mPrevTakeButton->Disable();
				GetMenuBar()->FindItem(MENU_PrevTake)->Enable(false);
				GetMenuBar()->FindItem(MENU_Up)->Enable(false);
			}
			//"next" button/"next", "down" and "last" menu items
			if (!mEventList->AtBottom()) {
	//			mNextTakeButton->Enable();
				EnableButtonReliably(mNextTakeButton);
				GetMenuBar()->FindItem(MENU_NextTake)->Enable();
				GetMenuBar()->FindItem(MENU_Down)->Enable();
				if (mEventList->InLastTake()) {
					mNextTakeButton->SetLabel(wxT("End of Take"));
					GetMenuBar()->FindItem(MENU_NextTake)->SetText(wxT("Move to end of take\tPGDN"));
				}
				else {
					mNextTakeButton->SetLabel(wxT("Next Take"));
					GetMenuBar()->FindItem(MENU_NextTake)->SetText(wxT("Move to start of next take\tPGDN"));
				}
			}
			else {
				mNextTakeButton->Disable();
				GetMenuBar()->FindItem(MENU_NextTake)->Enable(false);
				GetMenuBar()->FindItem(MENU_Down)->Enable(false);
			}
		}
		else {
			//empty list
			mPrevTakeButton->Disable();
			GetMenuBar()->FindItem(MENU_PrevTake)->Enable(false);
			mNextTakeButton->Disable();
			GetMenuBar()->FindItem(MENU_NextTake)->Enable(false);
			GetMenuBar()->FindItem(MENU_Up)->Enable(false);
			GetMenuBar()->FindItem(MENU_Down)->Enable(false);
			mDeleteCueButton->Disable();
			mEventList->CanEdit(false);
			mPlayProjectNameCtrl->SetLabel(wxT(""));
			mPlayer->Reset();
			mPlaybackTrackSelector->Clear();
		}
		GetMenuBar()->FindItem(MENU_Up)->SetText(wxT("Move up cue point list\tUP")); // TODO: this function to be deprecated - replace with line below for later versions of wx (somewhere between 2.8.4 and 2.8.9)
//		GetMenuBar()->FindItem(MENU_Up)->SetItemLabel(wxT("Move up cue point list\tUP"));
		GetMenuBar()->FindItem(MENU_Down)->SetText(wxT("Move down cue point list\tDOWN")); // TODO: this function to be deprecated - replace with line below for later versions of wx (somewhere between 2.8.4 and 2.8.9)
//		GetMenuBar()->FindItem(MENU_Down)->SetItemLabel(wxT("Move down cue point list\tDOWN"));
		mPlayProjectNameBox->GetStaticBox()->SetLabel(wxT("Project"));
	} //not file mode
	//player shortcuts
	UpdateTextShortcutStates();
	GetMenuBar()->FindItem(MENU_PrevTrack)->Enable(mPlaybackTrackSelector->EarlierTrack(false));
	GetMenuBar()->FindItem(MENU_NextTrack)->Enable(mPlaybackTrackSelector->LaterTrack(false));
	GetMenuBar()->FindItem(MENU_TogglePlayFile)->Enable(mPlayFileButton->IsEnabled());
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
	GetMenuBar()->FindItem(MENU_PlayBackwards)->Enable(!mDescriptionControlHasFocus && mPlayer->Within() && !mPlayer->AtMaxReverseSpeed());
	GetMenuBar()->FindItem(MENU_Pause)->Enable(!mDescriptionControlHasFocus && mPlayer->IsOK() && PAUSED != mStatus);
	GetMenuBar()->FindItem(MENU_PlayForwards)->Enable(!mDescriptionControlHasFocus && mPlayer->IsOK() && !mPlayer->AtMaxForwardSpeed());
	GetMenuBar()->FindItem(MENU_PlayPause)->Enable(!mDescriptionControlHasFocus && mPlayer->IsOK() && (PLAYING == mStatus || PLAYING_BACKWARDS == mStatus || (PAUSED == mStatus && (!mPlayer->LastPlayingBackwards() || mPlayer->Within()))));
	GetMenuBar()->FindItem(MENU_StepBackwards)->Enable(!mDescriptionControlHasFocus && PAUSED == mStatus && mPlayer->Within());
	GetMenuBar()->FindItem(MENU_StepForwards)->Enable(!mDescriptionControlHasFocus && PAUSED == mStatus && !mPlayer->AtEnd());
	GetMenuBar()->FindItem(MENU_Mute)->Enable(!mDescriptionControlHasFocus);
	GetMenuBar()->FindItem(MENU_FirstTake)->Enable(!mDescriptionControlHasFocus && (!mEventList->AtTop() || mPlayer->Within()));
	GetMenuBar()->FindItem(MENU_LastTake)->Enable(!mDescriptionControlHasFocus && !mEventList->AtBottom());
}

/// Clears the list of events, the track buttons and anything being displayed by the player
void IngexguiFrame::ClearLog()
{
	ResetPlayer();
	mEventList->Clear();
	GetMenuBar()->FindItem(MENU_ClearLog)->Enable(false);
}

/// Clears the track buttons and anything being displayed by the player
void IngexguiFrame::ResetPlayer()
{
	mPlayer->Reset();
	mPlaybackTrackSelector->Clear();
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
	if (wxDateTime::Today() != mToday) {
		mToday = wxDateTime::Today();
	        wxLogMessage(wxT("Date: ") + mToday.FormatISODate());
	}
	// Contrary to the documentation, wxLogMessage does in fact want
	// a const wxChar* argument so this works fine with unicode.
	wxLogMessage(message);
}

void IngexguiFrame::OnJumpToTimecode(wxCommandEvent & WXUNUSED(event))
{
	JumpToTimecodeDlg dlg(this);
}

/// Returns true if in a state associated with recording.
bool IngexguiFrame::IsRecording()
{
	return RECORDING == mStatus || RUNNING_UP == mStatus || RUNNING_DOWN == mStatus;
}

/// Enables or disable editing and deletion of cue points
void IngexguiFrame::CanEditCues(const bool canEdit)
{
	mDeleteCueButton->Enable(canEdit);
	mEventList->CanEdit(canEdit);
}
