/***************************************************************************
 *   $Id: dragbuttonlist.cpp,v 1.8 2009/02/27 12:19:16 john_f Exp $             *
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

#include "dragbuttonlist.h"
#include "ingexgui.h"
#include "eventlist.h"
#include "avid_mxf_info.h"

/// @param parent The parent window.
DragButtonList::DragButtonList(wxWindow * parent)
: wxScrolledWindow(parent)
{
	SetNextHandler(parent); //the radio button events are not handled here
	mSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mSizer);
}

/// Replaces current state with a new column of video track radio buttons, one for each track with a video file, and with a quad split button at the top.
/// Returns information about the current state.
/// Single track buttons are labelled with the track name and have a tool tip showing the associated filename.
/// All buttons are disabled.
/// @param takeInfo The file names, the track names and the track types.  Gets all info from here rather than examining the files themselves, because they may not be available yet
/// @param fileNames Returns the file name associated with each video track which has a file, with the audio filenames at the end.
/// @param trackNames Returns corresponding names of tracks.
void DragButtonList::SetTracks(TakeInfo & takeInfo, std::vector<std::string> & fileNames, std::vector<std::string> & trackNames)
{
	mSizer->Clear(true); //delete all buttons
	fileNames.clear();
	trackNames.clear();
	wxRadioButton * quadSplit = new wxRadioButton(this, 0, wxT("Quad Split")); //the quad split is always the first video track (id = 0)
	quadSplit->SetToolTip(wxT("Up to the first four successfully opened files"));
	mSizer->Add(quadSplit, -1, wxEXPAND);
	quadSplit->Enable(false); //enable later if any files successfully loaded
	std::vector<std::string> audioFileNames;
	if (takeInfo.GetFiles()->GetCount()) { //this take has files associated
		for (size_t i = 0; i < takeInfo.GetFiles()->GetCount(); i++) { //recorder loop
			for (size_t j = 0; j < (*takeInfo.GetFiles())[i]->length(); j++) { //file loop
				if (ProdAuto::VIDEO == takeInfo.GetTracks()[i][j].type && strlen((*takeInfo.GetFiles())[i][j])) {
					fileNames.push_back((*takeInfo.GetFiles())[i][j].in());
					wxRadioButton * rb = new wxRadioButton(this, fileNames.size(), wxString(takeInfo.GetTracks()[i][j].src.package_name, *wxConvCurrent)); //ID corresponds to file index
					rb->Enable(false); //we don't know whether the player can open this file yet
					rb->SetToolTip(wxString((*takeInfo.GetFiles())[i][j], *wxConvCurrent));
					mSizer->Add(rb, -1, wxEXPAND);
					trackNames.push_back(takeInfo.GetTracks()[i][j].src.package_name.in());
				}
				else if (ProdAuto::AUDIO == takeInfo.GetTracks()[i][j].type && strlen((*takeInfo.GetFiles())[i][j])) {
					audioFileNames.push_back((*takeInfo.GetFiles())[i][j].in());
				}
			}
		}
	}
	Layout();
	//add audio files at the end
	for (size_t i = 0; i < audioFileNames.size(); i++) {
		fileNames.push_back(audioFileNames.at(i));
	}
}

/// Alternative to SetTracks for MXF file mode
/// Replaces current state with a new column of video file radio buttons, and with a quad split button at the top.
/// Returns information about the current state.
/// Single track buttons are labelled with the Clip Track String and have a tool tip showing the associated filename.
/// All buttons are disabled.
/// @param paths The file names.
/// @param fileNames Returns the file name associated with each video file, with the audio filenames at the end.
/// @param trackNames Returns corresponding Clip Track Strings.
/// @param editRate Returns an edit rate
/// @return Project name if possible to get.
const wxString DragButtonList::SetMXFFiles(wxArrayString & paths, std::vector<std::string> & fileNames, std::vector<std::string> & trackNames, ProdAuto::MxfTimecode & editRate)
{
	mSizer->Clear(true); //delete all buttons
	fileNames.clear();
	trackNames.clear();
	wxRadioButton * quadSplit = new wxRadioButton(this, 0, wxT("Quad Split")); //the quad split is always the first video track (id = 0)
	quadSplit->SetToolTip(wxT("Up to the first four successfully opened files"));
	mSizer->Add(quadSplit, -1, wxEXPAND);
	quadSplit->Enable(false); //enable later if any files successfully loaded
	std::vector<std::string> audioFileNames;
	wxString projName;
	AvidMXFInfo info;
	editRate.undefined = true;
	editRate.samples = 0;
	for (size_t i = 0; i < paths.GetCount(); i++) {
		std::string path = (const char *) paths[i].mb_str(*wxConvCurrent);
		if (!ami_read_info(path.c_str(), &info, 0)) { //recognises the file
			if (info.isVideo) {
				fileNames.push_back(path);
				wxRadioButton * rb = new wxRadioButton(this, fileNames.size(), wxString(info.tracksString, *wxConvCurrent)); //ID corresponds to file index
				rb->Enable(false); //we don't know whether the player can open this file yet
				rb->SetToolTip(paths[i]);
				mSizer->Add(rb, -1, wxEXPAND);
				trackNames.push_back(info.tracksString);
				wxString p = wxString(info.projectName, *wxConvCurrent);
				if (info.projectName && p != projName) {
					if (projName.Length()) {
						projName = wxT("<Various projects>");
					}
					else {
						projName = p;
					}
				}
				editRate.edit_rate.numerator = info.editRate.numerator;
				editRate.edit_rate.denominator = info.editRate.denominator;
				editRate.undefined = false;
			}
			else {
				audioFileNames.push_back(path);
			}
			ami_free_info(&info);
		}
	}
	Layout();
	//add audio files at the end
	for (size_t i = 0; i < audioFileNames.size(); i++) {
		fileNames.push_back(audioFileNames.at(i));
	}
	return projName;
}

/// Alternative to SetTracks for E to E mode.
/// Replaces current state with a new column of video source radio buttons, and with a quad split button at the top.
/// Sources are given fixed names.
/// All buttons are disabled.
/// @param sources Returns the source name associated with each source.
/// @param names Returns corresponding displayed names.
void DragButtonList::SetEtoE(std::vector<std::string> & sources, std::vector<std::string> & names)
{
	mSizer->Clear(true); //delete all buttons
	sources.clear();
	names.clear();
	wxRadioButton * quadSplit = new wxRadioButton(this, 0, wxT("Quad Split")); //the quad split is always the first video source (id = 0)
	mSizer->Add(quadSplit, -1, wxEXPAND);
	quadSplit->Enable(false); //enable later if any sources successfully opened
	char source[3];
	wxString name;
	for (size_t i = 0; i < 4; i++) {
		sprintf(source, "%dp", i);
		sources.push_back(source);
		name.Printf(wxT("Live %d"), i + 1);
		names.push_back((const char *) name.mb_str(*wxConvCurrent));
		wxRadioButton * rb = new wxRadioButton(this, sources.size(), name);
		rb->Enable(false); //we don't know whether the player can open this source yet
		mSizer->Add(rb, -1, wxEXPAND);
	}
	Layout();
}


/// Enables/disables and selects the track select buttons.
/// COMMENTED OUT: Hides the quad split button if there is only one track enabled.
/// @param enables Enable state of each button.
/// @param selected The button to select.
void DragButtonList::EnableAndSelectTracks(std::vector<bool> * enables, const unsigned int selected)
{
	bool someOK = false;
	for (size_t i = 0; i < enables->size(); i++) {
		if (mSizer->GetItem(i + 1)) { //sanity check (the quad split button shifts all the rest down)
			mSizer->GetItem(i + 1)->GetWindow()->Enable(enables->at(i));
			someOK |= enables->at(i);
			if (selected == i + 1) {
				wxRadioButton * selectedButton = (wxRadioButton *) mSizer->GetItem(i + 1)->GetWindow();
				selectedButton->SetValue(true);
			}
		}
	}
	if (mSizer->GetItem((size_t) 0)) { //sanity check
//		if (enables->size() < 2) {
//			//no point in having a quad split if only one track to show
//			mSizer->GetItem((size_t) 0)->GetWindow()->Hide();
//		}
//		else {
//			//enable quad split if any files OK
			mSizer->GetItem((size_t) 0)->GetWindow()->Enable(someOK);
//		}
	}
}

/// Removes all buttons.
void DragButtonList::Clear()
{
	mSizer->Clear(true); //delete all buttons
}

/// Determines whether there are selectable tracks before the current one, or selects the next earlier track.
/// @param select True to select (if possible).
/// @return If select is false, true if there are earlier selectable tracks than that currently selected.  If select is true, true if a new track was selected.
bool DragButtonList::EarlierTrack(bool select)
{

	int previous = -1; //no previous button found
	bool more = false;
	for (size_t i = 0; i < mSizer->GetChildren().GetCount(); i++) {
		wxRadioButton * radioButton = (wxRadioButton *) mSizer->GetItem(i)->GetWindow();
		if (radioButton->GetValue()) { //selected
			if (previous > -1) { //there is a previous button to select
				if (select) { //we want to select a track
					//select it
					radioButton = (wxRadioButton *) mSizer->GetItem(previous)->GetWindow();
					radioButton->SetValue(true);
					//this doesn't generate an event, so do so manually
					wxCommandEvent event(wxEVT_COMMAND_RADIOBUTTON_SELECTED, previous);
					AddPendingEvent(event);
				}
				more = true;
			}
			//finished, whether or not we can select
			break;
		}
		else if (radioButton->IsEnabled()) { //selectable
			//the latest previous button
			previous = i;
		}
	}
	return more;
}

/// Determines whether there are selectable tracks after the current one, or selects the next track.
/// @param select True to select (if possible).
/// @return If select is false, true if there are later selectable tracks than that currently selected.  If select is true, true if a new track was selected.
bool DragButtonList::LaterTrack(bool select)
{
	bool selected_found = false; //not found the selected track yet
	bool more = false;
	for (size_t i = 0; i < mSizer->GetChildren().GetCount(); i++) {
		wxRadioButton * radioButton = (wxRadioButton *) mSizer->GetItem(i)->GetWindow();
		if (radioButton->GetValue()) { //selected
			selected_found = true;
		}
		else if (selected_found && radioButton->IsEnabled()) { //the next selectable track after the selected track
			more = true;
			if (select) { //we want to select a track
				//select it
				radioButton->SetValue(true);
				//this doesn't generate an event, so do so manually
				wxCommandEvent event(wxEVT_COMMAND_RADIOBUTTON_SELECTED, i);
				AddPendingEvent(event);
			}
			break;
		}
	}
	return more;
}

/// If the quad split is displayed, switches source to that corresponding to the quadrant value given (if it is available).
/// If an individual source is displayed, switches to the quad split (thus providing toggling behaviour).
/// @param source The quadrant (1-4).
void DragButtonList::SelectQuadrant(unsigned int source)
{
	if (mSizer->GetItem((size_t) 0)) { //sanity check (may have been an event hanging around while the sizer was cleared?)
		if (((wxRadioButton *) mSizer->GetItem((size_t) 0)->GetWindow())->GetValue()) { //quad split is displayed: display the individual source
			if (mSizer->GetItem(source) && ((wxRadioButton *) mSizer->GetItem(source)->GetWindow())->IsEnabled()){ //a source exists in this quadrant, and its file is successfully loaded
				//press the button (this deselects the others)
				((wxRadioButton *) mSizer->GetItem(source)->GetWindow())->SetValue(true);
				//this doesn't generate an event, so do so manually
				wxCommandEvent event(wxEVT_COMMAND_RADIOBUTTON_SELECTED, source);
				AddPendingEvent(event);
			}
		}
		else { //source is displayed: display the quad split
			((wxRadioButton *) mSizer->GetItem((size_t) 0)->GetWindow())->SetValue(true);
			//this doesn't generate an event, so do so manually
			wxCommandEvent event(wxEVT_COMMAND_RADIOBUTTON_SELECTED, 0);
			AddPendingEvent(event);
		}
	}
}

//drag drop experiment

/*	wxArrayString names;
	names.Add(wxT("First"));
	names.Add(wxT("Second"));
	names.Add(wxT("Third"));
	names.Add(wxT("Fourth"));

	for (unsigned int i = 0; i < names.GetCount(); i++) {
		wxBoxSizer * bs = new wxBoxSizer(wxHORIZONTAL);
		playbackPageSizer->Add(bs);
		wxRadioButton * rb = new wxRadioButton(playbackPage, -1, wxT(""));
		bs->Add(rb);
		wxStaticText * tb = new wxStaticText(playbackPage, -1, names[i]);
		bs->Add(tb);

void DragButtonList::SelectSource(unsigned int source) {
	wxWindow * button = FindWindowById(source, this);
	if (button) { //assumption that button will be NULL if not found!
		//select the button
		((wxRadioButtonbutton
		//send an event as if the button had been pressed by the user
	}
}

//drag drop experiment

	} */
