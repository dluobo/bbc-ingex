/***************************************************************************
 *   $Id: dialogues.cpp,v 1.11 2009/09/18 16:10:15 john_f Exp $           *
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

#include "dialogues.h"
#include "help.h" //for StyleAndWrite()
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>
#include <wx/colordlg.h>
#include <wx/filename.h>
#include <wx/dir.h>

#include "up.xpm"
#include "down.xpm"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SetRollsDlg, wxDialog)
	EVT_SCROLL(SetRollsDlg::OnRollChange)
END_EVENT_TABLE()

/// Sets up dialogue.
/// @param parent The parent window.
/// @param preroll The current preroll value.
/// @param maxPreroll The maximum allowed preroll.
/// @param postroll The current postroll value.
/// @param maxPostroll The maximum allowed postroll.
SetRollsDlg::SetRollsDlg(wxWindow * parent, const ProdAuto::MxfDuration preroll, const ProdAuto::MxfDuration maxPreroll, const ProdAuto::MxfDuration postroll, const ProdAuto::MxfDuration maxPostroll, wxXmlDocument & savedState)
: wxDialog(parent, wxID_ANY, (const wxString &) wxT("Pre-roll and Post-roll")), mMaxPreroll(maxPreroll), mMaxPostroll(maxPostroll), mSavedState(savedState)
{
	wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	//sliders
	wxBoxSizer * sliderSizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(sliderSizer, 0, wxEXPAND | wxALL);
	mPrerollBox = new wxStaticBoxSizer(wxHORIZONTAL, this);
	sliderSizer->Add(mPrerollBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPrerollCtrl = new wxSlider(this, SLIDER_Preroll, 0, 0, 1000); //slider can't handle floats so calculate real value elsewhere
	mPrerollCtrl->SetValue(((preroll.samples * 2) + 1) * 500 / mMaxPreroll.samples); //prevent rounding down by putting in centre of range
	mPrerollBox->Add(mPrerollCtrl, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPostrollBox = new wxStaticBoxSizer(wxHORIZONTAL, this);
	sliderSizer->Add(mPostrollBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPostrollCtrl = new wxSlider(this, SLIDER_Postroll, 0, 0, 1000); //slider can't handle floats so calculate real value elsewhere
	mPostrollCtrl->SetValue(((postroll.samples * 2) + 1) * 500 / mMaxPostroll.samples); //prevent rounding down by putting in centre of range
	mPostrollBox->Add(mPostrollCtrl, 1, wxEXPAND | wxALL, CONTROL_BORDER);

	//buttons
	wxBoxSizer * buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(buttonSizer, 0, wxEXPAND | wxALL);
	buttonSizer->AddStretchSpacer();
	wxButton * okButton = new wxButton(this, wxID_OK, wxT("OK"));
	buttonSizer->Add(okButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	buttonSizer->AddStretchSpacer();
	wxButton * cancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttonSizer->Add(cancelButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	buttonSizer->AddStretchSpacer();

	//update box text
	wxScrollEvent event;
	event.SetId(SLIDER_Preroll);
	OnRollChange(event);
	event.SetId(SLIDER_Postroll);
	OnRollChange(event);

	Fit(); //Makes the height correct but the width too small
	SetSize(wxDefaultCoord, wxDefaultCoord, 500, wxDefaultCoord); //makes the width sensible
}

/// Responds to a slider event by updating the relevant text.
/// @param event The slider event.
void SetRollsDlg::OnRollChange( wxScrollEvent& event )
{
	if (SLIDER_Preroll == event.GetId()) {
		GetPreroll();
	}
	else if (SLIDER_Postroll == event.GetId()) {
		GetPostroll();
	}
}

/// Sets the preroll box label to the current preroll slider value.
/// @return The preroll value.
const ProdAuto::MxfDuration SetRollsDlg::GetPreroll() {
	return SetRoll(wxT("Pre-roll"), mPrerollCtrl->GetValue(), mMaxPreroll, mPrerollBox);
}

/// Sets the postroll box label to the current postroll slider value.
/// @return The postroll value.
const ProdAuto::MxfDuration SetRollsDlg::GetPostroll() {
	return SetRoll(wxT("Post-roll"), mPostrollCtrl->GetValue(), mMaxPostroll, mPostrollBox);
}

/// Sets a control's text.
/// @param label The displayed name of the parameter.
/// @param proportion The slider's position from 0-1000.
/// @param max The maximum value for the parameter.
/// @param control The control to be modified.
ProdAuto::MxfDuration SetRollsDlg::SetRoll(const wxChar * label, int proportion, const ProdAuto::MxfDuration & max, wxStaticBoxSizer * control)
{
	ProdAuto::MxfDuration roll = max; //for edit rates and defined flag
	roll.samples = max.samples * proportion / 1000;
	control->GetStaticBox()->SetLabel(wxString::Format(wxT("%s: %d fr (%3.1f s)"), label, roll.samples, roll.samples * roll.edit_rate.denominator / (float) roll.edit_rate.numerator));
	return roll;
}

/// Overloads Show Modal to update the XML document
int SetRollsDlg::ShowModal()
{
	int rc = wxDialog::ShowModal();
	if (wxID_OK == rc) {
		//Remove old nodes
		wxXmlNode * node = mSavedState.GetRoot()->GetChildren();
		while (node) {
			if (wxT("Preroll") == node->GetName() || wxT("Postroll") == node->GetName()) {
				wxXmlNode * deadNode = node;
				node = node->GetNext();
				mSavedState.GetRoot()->RemoveChild(deadNode);
				delete deadNode;
			}
			else {
				node = node->GetNext();
			}
		}
		//Create new nodes (element nodes containing text nodes)
		new wxXmlNode(new wxXmlNode(mSavedState.GetRoot(), wxXML_ELEMENT_NODE, wxT("Preroll")), wxXML_TEXT_NODE, wxT(""), wxString::Format(wxT("%d"), GetPreroll().samples));
		new wxXmlNode(new wxXmlNode(mSavedState.GetRoot(), wxXML_ELEMENT_NODE, wxT("Postroll")), wxXML_TEXT_NODE, wxT(""), wxString::Format(wxT("%d"), GetPostroll().samples));
	}
	return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( SetProjectDlg, wxDialog )
	EVT_BUTTON(ADD, SetProjectDlg::OnAdd)
	EVT_BUTTON(DELETE, SetProjectDlg::OnDelete)
	EVT_BUTTON(EDIT, SetProjectDlg::OnEdit)
	EVT_CHOICE(wxID_ANY, SetProjectDlg::OnChoice)
END_EVENT_TABLE()

/// Displays list of project names from the recorders and the selected project from the XML doc and allows the list to be added to
/// Updates the document if user changes the selected project.
/// @param parent The parent window.
/// @param projectNamees The current list of project names (can be empty)
/// @param doc The XML document to read and modify.
/// @param force When true, disables the cancel button etc to force the user to press OK.
SetProjectDlg::SetProjectDlg(wxWindow * parent, const wxSortedArrayString & projectNames, wxXmlDocument & savedState) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Select a project name"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mProjectNames(projectNames), mSavedState(savedState)
{
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	wxStaticBoxSizer * mListSizer = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Project name for new recordings"));
	mainSizer->Add(mListSizer, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mProjectList = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize); //ListBox is nicer but can't scroll it to show a new project name
	mListSizer->Add(mProjectList, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	wxBoxSizer * buttonSizer1 = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(buttonSizer1, 0, wxEXPAND);
	wxButton * addButton = new wxButton(this, ADD, wxT("Add"));
	buttonSizer1->Add(addButton, 0, wxALL, CONTROL_BORDER);
//	mDeleteButton = new wxButton(this, DELETE, wxT("Delete"));
//	buttonSizer1->AddStretchSpacer();
//	buttonSizer1->Add(mDeleteButton, 0, wxALL, CONTROL_BORDER);
//	mEditButton = new wxButton(this, EDIT, wxT("Edit"));
//	buttonSizer1->AddStretchSpacer();
//	buttonSizer1->Add(mEditButton, 0, wxALL, CONTROL_BORDER);
	wxBoxSizer * buttonSizer2 = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(buttonSizer2, 1, wxEXPAND);
	mOKButton = new wxButton(this, wxID_OK, wxT("OK"));
	buttonSizer2->Add(mOKButton, 1, wxALL, CONTROL_BORDER);
	wxButton * CancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttonSizer2->Add(CancelButton, 1, wxALL, CONTROL_BORDER);
	mProjectList->Append(mProjectNames);
	wxString selectedProject = GetCurrentProjectName(mSavedState);
	if (selectedProject.IsEmpty() && mProjectList->GetCount()) { //recorder has returned project names but no selected project from the file
		mProjectList->SetSelection(0); //just select something as a default project
	}
	else {
		mProjectList->SetSelection(mProjectNames.Index(selectedProject)); //could be nothing
	}
	Fit();
	SetMinSize(GetSize());
	mOKButton->SetFocus(); //especially useful to get through the dialogue quickly when it immediately brings up an add project box when there are no project names
}

/// Obtains the currently selected project name from the supplied XML document; returns an empty string if not found
const wxString SetProjectDlg::GetCurrentProjectName(const wxXmlDocument & doc)
{
	wxXmlNode * projectNode = doc.GetRoot()->GetChildren();
	while (projectNode && projectNode->GetName() != wxT("CurrentProject")) {
		projectNode = projectNode->GetNext();
	}
	if (projectNode) {
		return projectNode->GetNodeContent();
	}
	else {
		return wxT("");
	}
}

/// Overloads Show Modal to prompt the addition of one project name if there are none, and to update the XML document
int SetProjectDlg::ShowModal()
{
	if (!mProjectList->GetCount()) { //must enter at least one project name
		EnterName(wxT("No project names stored: enter a project name"), wxT("New Project"));
	}
	if (mProjectList->GetCount()) { //names obtained from recorder(s), or selected name from rc file, or name has just been entered
		int rc = wxDialog::ShowModal();
		if (wxID_OK == rc) {
			//Remove old node(s)
			wxXmlNode * node = mSavedState.GetRoot()->GetChildren();
			while (node) {
				if (wxT("CurrentProject") == node->GetName()) {
					wxXmlNode * deadNode = node;
					node = node->GetNext();
					mSavedState.GetRoot()->RemoveChild(deadNode);
					delete deadNode;
				}
				else {
					node = node->GetNext();
				}
			}
			//create a new element node "CurrentProject" containing a new text node with the section of the tape ID
			new wxXmlNode(new wxXmlNode(mSavedState.GetRoot(), wxXML_ELEMENT_NODE, wxT("CurrentProject"), wxT("")), wxXML_TEXT_NODE, wxT(""), mProjectList->GetStringSelection());
			//projectNode->SetContent(dlg.GetSelectedProject()); this doesn't seem to work - creates a new node with no content
		}
		return rc;
	}
	else { //cancel has been pressed in the EnterName box above
		return wxID_CANCEL;
	}
}

/// Responds to the "Add" button.
/// @param event The button event.
void SetProjectDlg::OnAdd( wxCommandEvent& WXUNUSED( event ) )
{
	EnterName(wxT("Enter a new project name"), wxT("New Project"));
}

/// Responds to the "Edit" button.
/// @param event The button event.
void SetProjectDlg::OnEdit( wxCommandEvent& WXUNUSED( event ) )
{
	EnterName(wxT("Edit project name"), wxT("Edit Project"), mProjectList->GetSelection());
}

/// Adds or edits a project name via a sub-dialogue, checking for duplicate names.
/// Adjusts button enable states accordingly.
/// @param msg The instruction to the user.
/// @param caption The dialogue title.
/// @param item The item to edit, if editing.
void SetProjectDlg::EnterName(const wxString & msg, const wxString & caption, int item) //item defaults to wxNOT_FOUND
{
	while (true) {
		wxString name = wxGetTextFromUser(msg, caption, mProjectList->GetString(item));
		name = name.Trim(true).Trim(false);
		if (name.IsEmpty()) {
			break;
		}
		mProjectList->Delete(item); //so that searching for duplicates is accurate
		if (wxNOT_FOUND == mProjectList->FindString(name)) { //case-insensitive match
			if (wxNOT_FOUND == item) { //adding a new name
				int index = mProjectNames.Add(name);
				mProjectList->Insert(name, index);
				mProjectList->Select(index);
			}
			else {
				mProjectList->Insert(name, item);
				mProjectList->Select(item);
			}
			Fit();
			SetMinSize(GetSize());
			break;
		}
		else {
			wxMessageDialog dlg(this, wxT("There is already a project named \"") + mProjectList->GetString(mProjectList->FindString( name)) + wxT("\"."), wxT("Name Clash"), wxICON_HAND | wxOK); //prints existing project name (may have case differences)  (NB not using wxMessageBox because (in GTK) it doesn't stop the parent window from being selected, so it can end up hidden, making the app appear to have hanged)
			dlg.ShowModal();
		}
	}
	EnableButtons(wxNOT_FOUND != mProjectList->GetSelection());
}

/// Responds to the "Delete" button.
/// Adjusts button enable states accordingly.
/// @param event The button event.
void SetProjectDlg::OnDelete( wxCommandEvent& WXUNUSED( event ) )
{
	mProjectList->Delete(mProjectList->GetSelection());
	//select the latest remaining project if possible
	if (mProjectList->GetCount()) {
		mProjectList->SetSelection(mProjectList->GetCount() - 1);
	}
	else {
		EnableButtons(false);
	}
}

/// Responds to a project name being chosen.
/// Adjusts button enable states accordingly.
/// @param event The choice event.
void SetProjectDlg::OnChoice( wxCommandEvent& WXUNUSED( event ) )
{
	//always a valid choice
	EnableButtons(true);
}

/// @return The name of the project that was selected when the dialogue was closed.
const wxString SetProjectDlg::GetSelectedProject()
{
	return mProjectList->GetStringSelection();
}

/// @return Sorted list of project names.
const wxSortedArrayString & SetProjectDlg::GetProjectNames()
{
	return mProjectNames;
}

/// Enable or disable all dialogue's "Add", "Delete" and "Edit" buttons.
/// @param state True to enable.
void SetProjectDlg::EnableButtons(bool state)
{
//	mDeleteButton->Enable(state);
//	mEditButton->Enable(state);
	mOKButton->Enable(state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(MyGrid, wxGrid)
	EVT_CHAR(MyGrid::OnChar)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(SetTapeIdsDlg, wxDialog)
	EVT_GRID_EDITOR_HIDDEN(SetTapeIdsDlg::OnEditorHidden)
	EVT_GRID_EDITOR_SHOWN(SetTapeIdsDlg::OnEditorShown)
	EVT_GRID_CELL_CHANGE(SetTapeIdsDlg::OnCellChange)
	EVT_GRID_CELL_LEFT_CLICK(SetTapeIdsDlg::OnCellLeftClick)
	EVT_GRID_LABEL_LEFT_CLICK(SetTapeIdsDlg::OnLabelLeftClick)
	EVT_GRID_RANGE_SELECT(SetTapeIdsDlg::OnCellRangeSelected) //for an area being dragged to select several cells
	EVT_BUTTON(FILLCOPY, SetTapeIdsDlg::OnFillCopy)
	EVT_BUTTON(FILLINC, SetTapeIdsDlg::OnFillInc)
	EVT_BUTTON(INCREMENT, SetTapeIdsDlg::OnIncrement)
	EVT_BUTTON(GROUPINC, SetTapeIdsDlg::OnGroupIncrement)
	EVT_BUTTON(HELP, SetTapeIdsDlg::OnHelp)
	EVT_BUTTON(CLEAR, SetTapeIdsDlg::OnClear)
	EVT_INIT_DIALOG(SetTapeIdsDlg::OnInitDlg)
END_EVENT_TABLE()
WX_DECLARE_STRING_HASH_MAP(wxString, StringHash);

/// Reads tape IDs names from the XML document and displays a dialogue to manipulate them.
/// Displays in package name order within three groups: enabled for recording and no tape ID; not enabled and no tape ID; the rest.
/// Updates the document if user changes data.
/// @param parent The parent window.
/// @param doc The XML document to read and modify.
/// @param currentPackages Package names of connected recorders, in display order
/// @param enabled Value corresponding to each package name, which is true if any sources within that package are enabled for recording
SetTapeIdsDlg::SetTapeIdsDlg(wxWindow * parent, wxXmlDocument & doc, wxArrayString & currentPackages, std::vector<bool> & enabled) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Set tape IDs"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mEditing(false), mUpdated(false)
{
	//controls
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	wxBoxSizer * upperSizer = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(upperSizer, 0, wxEXPAND);
	mGrid = new MyGrid(this, wxID_ANY);
	upperSizer->Add(mGrid, 0, wxALL, CONTROL_BORDER);
	mGrid->CreateGrid(currentPackages.GetCount(), 7);
	mGrid->SetColLabelValue(0, wxT("Source Name"));
	mGrid->SetColLabelValue(1, wxT("Tape ID:"));
	mGrid->SetColLabelValue(2, wxT("Part 1"));
	mGrid->SetColLabelValue(3, wxT("Part 2"));
	mGrid->SetColLabelValue(4, wxT("Part 3"));
	mGrid->SetColLabelValue(5, wxT("Part 4"));
	mGrid->SetColLabelValue(6, wxT("Part 5"));
	mGrid->SetRowLabelSize(0); //can't autoadjust this (?) so don't use it
	mGrid->SetGridCursor(0, 2); //first two columns can't be edited so don't leave the cursor in the top left hand corner
	wxBoxSizer * rightHandSizer = new wxBoxSizer(wxVERTICAL);
	upperSizer->Add(rightHandSizer, 0, wxALL, CONTROL_BORDER);
	mDupWarning = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxTE_CENTRE | wxBORDER_NONE); //don't think you can set background colour of static text - it would be much easier...
	mDupWarning->SetDefaultStyle(wxTextAttr(*wxRED, wxColour(wxT("YELLOW"))));
	mDupWarning->AppendText(wxT("Duplicate tape IDs"));
	mDupWarning->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BACKGROUND));
	rightHandSizer->Add(mDupWarning, 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
	mIncrementButton = new wxButton(this, INCREMENT, wxT("Increment selected"));
	mIncrementButton->Disable();
	rightHandSizer->Add(mIncrementButton, 0, wxEXPAND);
	rightHandSizer->AddSpacer(CONTROL_BORDER);
	mGroupIncButton = new wxButton(this, GROUPINC, wxT("Increment as group"));
	mGroupIncButton->Disable();
	rightHandSizer->Add(mGroupIncButton, 0, wxEXPAND);
	wxGridSizer * buttonSizer = new wxGridSizer(2, 3, CONTROL_BORDER, CONTROL_BORDER);
	mainSizer->Add(buttonSizer, 0, wxALL, CONTROL_BORDER);
	wxButton * cancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttonSizer->Add(cancelButton, 0, wxEXPAND);
	wxButton * helpButton = new wxButton(this, HELP, wxT("Help"));
	buttonSizer->Add(helpButton, 0, wxEXPAND);
	wxButton * OKButton = new wxButton(this, wxID_OK, wxT("OK"));
	buttonSizer->Add(OKButton, 0, wxEXPAND);
	mFillCopyButton = new wxButton(this, FILLCOPY, wxT("Fill column with copies"));
	mFillCopyButton->Disable();
	buttonSizer->Add(mFillCopyButton, 0, wxEXPAND);
	mFillIncButton = new wxButton(this, FILLINC, wxT("Fill column by incrementing"));
	mFillIncButton->Disable();
	buttonSizer->Add(mFillIncButton, 0, wxEXPAND);
	mClearButton = new wxButton(this, CLEAR, wxT("Clear selected"));
	mClearButton->Disable();
	buttonSizer->Add(mClearButton, 0, wxEXPAND);

	//find the tape IDs in the XML
	wxXmlNode * tapeIdsNode = GetTapeIdsNode(doc);

	//populate the table
	for (size_t i = 0; i < currentPackages.GetCount(); i++) {
		mGrid->SetCellValue(i, 0, currentPackages[i]); //source name
		mGrid->SetReadOnly(i, 0);
		wxArrayString sections;
		sections.Insert(wxT(""), 0, mGrid->GetNumberCols() - 2); //set the number of sections
		mGrid->SetCellValue(i, 1, GetTapeId(tapeIdsNode, currentPackages[i], &sections));
		mGrid->SetReadOnly(i, 1);
		mGrid->SetCellBackgroundColour(i, 1, wxColour(wxT("GREY")));
		for (size_t j = 0; j < sections.GetCount(); j++) {
			mGrid->SetCellValue(i, j + 2, sections[j]);
		}
		mEnabled.push_back(enabled.at(i));
		SetBackgroundColour(i);
	}

	mGrid->AutoSizeColumns();

	Fit();
	SetMinSize(GetSize());
	//show dialogue and save results
	if (wxID_OK == ShowModal()) {
		for (int i = 0; i < mGrid->GetNumberRows(); i++) {
			//remove any existing node(s) for this source
			wxXmlNode * tapeIdNode = tapeIdsNode->GetChildren();
			while (tapeIdNode) {
				if (mGrid->GetCellValue(i, 0) == tapeIdNode->GetPropVal(wxT("PackageName"), wxT(""))) {
					wxXmlNode * deadNode = tapeIdNode;
					tapeIdNode = tapeIdNode->GetNext();
					tapeIdsNode->RemoveChild(deadNode);
					delete deadNode;
				}
				else {
					tapeIdNode = tapeIdNode->GetNext();
				}
			}
			if (!mGrid->GetCellValue(i, 1).IsEmpty()) {
				//a new element node "TapeId" with property "PackageName"
				tapeIdNode = new wxXmlNode(tapeIdsNode, wxXML_ELEMENT_NODE, wxT("TapeId"), wxT(""), new wxXmlProperty(wxT("PackageName"), mGrid->GetCellValue(i, 0)));
				for (int sectionCol = 2; sectionCol < mGrid->GetNumberCols(); sectionCol++) {
					//a new element node "Section" containing a new text node with the section of the tape ID
					new wxXmlNode(new wxXmlNode(tapeIdNode, wxXML_ELEMENT_NODE, wxT("Section"), wxT(""), new wxXmlProperty(wxT("Number"), wxString::Format(wxT("%d"), sectionCol - 1))), wxXML_TEXT_NODE, wxT(""), mGrid->GetCellValue(i, sectionCol));
				}
			}
		}
		if (!tapeIdsNode->GetChildren()) {
			doc.GetRoot()->RemoveChild(tapeIdsNode);
			delete tapeIdsNode;
		}
		mUpdated = true;
	}
}

/// Retrieves or creates the node in the given document containing tape IDs.
/// Removes any child nodes not called "TapeId".
/// @param doc The XML document.
/// @return The tape ID root node (always valid).
wxXmlNode * SetTapeIdsDlg::GetTapeIdsNode(wxXmlDocument & doc)
{
	wxXmlNode * tapeIdsNode = doc.GetRoot()->GetChildren();
	while (tapeIdsNode && tapeIdsNode->GetName() != wxT("TapeIds")) {
		tapeIdsNode = tapeIdsNode->GetNext();
	}
	if (!tapeIdsNode) {
		tapeIdsNode = new wxXmlNode(doc.GetRoot(), wxXML_ELEMENT_NODE, wxT("TapeIds"));
	}
	else {
		//clean up
		wxXmlNode * childNode = tapeIdsNode->GetChildren();
		while (childNode) {
//std::cerr << childNode->GetName() << std::endl;
			if (wxT("TapeId") != childNode->GetName()) {
				wxXmlNode * deadNode = childNode;
				childNode = childNode->GetNext();
				tapeIdsNode->RemoveChild(deadNode);
				delete deadNode;
			}
			else {
				childNode = childNode->GetNext();
			}
		}
	}
	return tapeIdsNode;
}

/// Gets the tape ID for the given package name from the children of the given node.
/// @param tapeIdsNode The node to search from.  Until it finds the valid data, removes any child nodes with the same package ID which contain invalid data.
/// @param packageName The package name to search for.
/// @param sections Array to be populated with the sections of the tape ID.  If there are more sections than the size of the array, the extras will be placed in the last element.
/// @return The complete tape ID (empty string if not found).
const wxString SetTapeIdsDlg::GetTapeId(wxXmlNode * tapeIdsNode, const wxString & packageName, wxArrayString * sections)
{
	wxString fullName;
	wxArrayString internal;
	size_t nSections;
	if (sections) { //array supplied
		nSections = sections->GetCount();
	}
	else {
		//use our own array
		sections = &internal;
		nSections = 0; //indicates unlimited
	}
	wxXmlNode * tapeIdNode = tapeIdsNode->GetChildren();
	while (tapeIdNode) { //assumes all nodes at this level are called "TapeId"
		if (packageName == tapeIdNode->GetPropVal(wxT("PackageName"), wxT(""))) { //found it!
			wxXmlNode * sectionNode = tapeIdNode->GetChildren();
			while (sectionNode) {
				long sectionNumber;
				if (wxT("Section") == sectionNode->GetName() && sectionNode->GetPropVal(wxT("Number"), wxT("X")).ToLong(&sectionNumber) && sectionNumber > 0) {
					sections->SetCount(sectionNumber); //make sure the array's big enough
					(*sections)[sectionNumber - 1] = sectionNode->GetNodeContent().Trim(false).Trim(true);
				}
				sectionNode = sectionNode->GetNext();
			}
			for (size_t i = 0; i < sections->GetCount(); i++) {
				fullName += (*sections)[i];
			}
			if (!fullName.IsEmpty()) {
				if (nSections) { //a section limit
					//put any extra sections in the last section
					while (sections->GetCount() > nSections) {
						(*sections)[nSections - 1] += (*sections)[nSections];
						sections->RemoveAt(nSections);
					}
				}
				break; //no need to look further for the XML tape ID
			}
			else { //duff data for this node
				//remove the node
				wxXmlNode * deadNode = tapeIdNode;
				tapeIdNode = tapeIdNode->GetNext();
				tapeIdsNode->RemoveChild(deadNode);
				delete deadNode;
			}
		}
		else { //not found it yet
			tapeIdNode = tapeIdNode->GetNext();
		}
	}
	return fullName;
}

/// Sets the background colour of the full tape ID cell of the given row, depending on presence/absence of content, and enabled status
/// @param row The row to affect.
void SetTapeIdsDlg::SetBackgroundColour(int row) {
	if (mGrid->GetCellValue(row, 1).IsEmpty() && mEnabled.at(row)) { //no tape ID and enabled for recording, so preventing a recording
			mGrid->SetCellBackgroundColour(row, 0, wxColour(wxT("RED")));
	}
	else if (mGrid->GetCellValue(row, 1).IsEmpty()) { //no tape ID but not enabled for recording
		mGrid->SetCellBackgroundColour(row, 0, wxColour(wxT("YELLOW")));
	}
	else { //all OK
		mGrid->SetCellBackgroundColour(row, 0, wxColour(wxT("GREY")));
	}
}

/// Responds to the dialogue intialising by showing or hiding the duplicate tape warning.
/// Need to call it here because it doesn't subsequently display in the right place if hidden before dialogue initialisation.
/// @param event The dialog init event.
void SetTapeIdsDlg::OnInitDlg(wxInitDialogEvent & WXUNUSED(event))
{
	CheckForDuplicates();
}

/// Responds to a cell starting to be edited.
/// @param event The grid event.
void SetTapeIdsDlg::OnEditorShown(wxGridEvent & WXUNUSED(event))
{
	mEditing = true; //gets round a bug (?) which causes two wxEVT_GRID_EDITOR_HIDDEN events to be emitted when editing stops
}

/// Responds to cell content changing (only called when cell loses focus)
/// Removes leading and trailing spaces, updates the corresponding complete tape ID cell and the source name background colour, and checks for duplicate tape Ids
/// @param event The grid event.
void SetTapeIdsDlg::OnCellChange(wxGridEvent & event)
{
	mGrid->SetCellValue(event.GetRow(), event.GetCol(), mGrid->GetCellValue(event.GetRow(), event.GetCol()).Trim(false).Trim(true));
	UpdateRow(event.GetRow());
	CheckForDuplicates();
}

/// Updates the complete name cell and background colour from its row values etc.
/// @param row The grid row to update.
void SetTapeIdsDlg::UpdateRow(const int row) {
	wxString fullName;
	for (int i = 2; i < mGrid->GetNumberCols(); i++) {
		fullName += mGrid->GetCellValue(row, i);
	}
	if (mGrid->GetCellValue(row, 1) != fullName) {
		mGrid->SetCellValue(row, 1, fullName);
		SetBackgroundColour(row);
	}
}

/// Checks grid for duplicate tape IDs and shows or hides warning accordingly
void SetTapeIdsDlg::CheckForDuplicates()
{
	for (int i = 0; i < mGrid->GetNumberRows() - 1; i++) {
		if (!mGrid->GetCellValue(i, 1).IsEmpty()) {
			for (int j = i + 1; j < mGrid->GetNumberRows(); j++) {
				if (mGrid->GetCellValue(i, 1) == mGrid->GetCellValue(j, 1)) {
					mDupWarning->Show();
					return;
				}
			}
		}
	}
	mDupWarning->Hide();
}


/// Responds to completion of cell editing.
/// Works round an apparent bug in the grid control which results in two events being issued when this happens.
/// Warns of duplicate tape IDs.
/// Sets cell background colour according to contents.
/// @param event The grid event.
void SetTapeIdsDlg::OnEditorHidden(wxGridEvent & WXUNUSED(event))
{
	mGrid->AutoSizeColumns();
//	mGrid->ForceRefresh(); //sometimes makes a mess if you don't
	Fit();
}

/// Responds to a left click in a cell
/// Prevents the cursor moving to the first two columns (which aren't editable), moving it to the third column instead.
/// @param event The grid event.
void SetTapeIdsDlg::OnCellLeftClick(wxGridEvent & event)
{
	if (event.GetCol() < 2) {
		mGrid->SetGridCursor(event.GetRow(), 2);
	}
	else {
		event.Skip();
	}
}

/// Responds to a left click in a label
/// Traps the click if it's in the first two columns, as these can't be selected.
/// @param event The grid event.
void SetTapeIdsDlg::OnLabelLeftClick(wxGridEvent & event)
{
	if (event.GetCol() >= 2) {
		event.Skip();
	}
}

/// Responds to the fill copy button being pressed, by filling empty cells in the selected column with copies of its top cell
void SetTapeIdsDlg::OnFillCopy(wxCommandEvent & WXUNUSED(event))
{
	FillCol(true, false);
	CheckForDuplicates();
}

/// Responds to the fill increment button being pressed, by filling empty cells in the selected column with copies of its top cell, incrementing each time
void SetTapeIdsDlg::OnFillInc(wxCommandEvent & WXUNUSED(event))
{
	FillCol(false, true);
	CheckForDuplicates();
}

/// Responds to the increment button being pressed, by incrementing all suitable selected cells.
void SetTapeIdsDlg::OnIncrement(wxCommandEvent & WXUNUSED(event))
{
	ManipulateCells(true, true); //do changes
	CheckForDuplicates();
}

/// Responds to the group increment button being pressed, by incrementing all non-blank cells in selected column as a group.
void SetTapeIdsDlg::OnGroupIncrement(wxCommandEvent & WXUNUSED(event))
{
	IncrementAsGroup(true); //do changes
	CheckForDuplicates();
}

/// Responds to the clear button being pressed, by clearing all selected cells.
void SetTapeIdsDlg::OnClear(wxCommandEvent & WXUNUSED(event))
{
	ManipulateCells(false, true); //do changes
	mClearButton->Disable();
	CheckForDuplicates();
}

/// Responds to a range of cells being selected.
/// Sets enable states of the buttons.
/// @param event The grid event.
void SetTapeIdsDlg::OnCellRangeSelected(wxGridRangeSelectEvent & WXUNUSED(event))
{
	mGrid->HideCellEditControl(); //so that when you initially enter a value in the cell you don't have to press enter or click in another cell before you clicking a column heading enables the fill buttons
	mIncrementButton->Enable(ManipulateCells(true, false)); //just check
	mClearButton->Enable(ManipulateCells(false, false)); //just check
	FillCol(false, false); //just check
	IncrementAsGroup(false); //just check
}

/// Checks to see if a column can be filled with copies or filled with incremented copies of the top cell, setting the enable states of the corresponding buttons accordingly.
/// Filling with copies is possible if the only selection on the grid is one column, the top cell of the column is not empty, and at least one other cell in the column is empty.  Additional requirements for increment filling are that the top cell contains a non-negative number (leading zeros allowed and maintained), or a single character 'A'..'Z' or 'a'..'z'.
/// Optionally fills with copies or incremented copies, working downwards and jumping over non-empty cells.  Incrementing occurs as a cell is filled.  Alphabetical increments wrap, maintaining the case.
/// @param copy True to fill empty cells with a copy of the top cell, if possible
/// @param inc True to fill empty cells with incremented copies of the top cell, if possible (ignored if copy is true)
void SetTapeIdsDlg::FillCol(const bool copy, const bool inc) {
	bool canCopy = false;
	bool canInc = false;
	//Find out whether we can copy or increment-copy, where to copy from, and whether this cell is numerical
	bool numerical;
	int sourceRow;
	if (mGrid->GetSelectionBlockTopLeft().IsEmpty() //no blocks selected
	 && mGrid->GetSelectedCells().IsEmpty() //no individual cells selected
	 && 1 == mGrid->GetSelectedCols().GetCount()) { //exactly 1 column selected
		for (sourceRow = 0; sourceRow < mGrid->GetNumberRows(); sourceRow++) {
			if (!mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).IsEmpty()) { //we've found the row to copy from
				break;
			}
		}
		if (sourceRow < mGrid->GetNumberRows()) { //there are some cells below the source row
			for (int row = sourceRow; row < mGrid->GetNumberRows(); row++) {
				if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //at least one empty cell in the column
					canCopy = true;
					if ((numerical = ManipulateCell(sourceRow, mGrid->GetSelectedCols()[0], true, false)) //cell contains a number
					 ||
					 (1 == mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).Len()
					  && mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).CmpNoCase(wxT("A")) >= 0
	 				  && mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).CmpNoCase(wxT("Z")) < 0)) { //cell contains a single letter
						canInc = true;
					}
					break; //we can fill at least one cell
				}
			}
		}
	}
	//copy or increment if possible
	if (canCopy && copy) {
		//fill up the empty cells with the initial contents
		for (int row = sourceRow + 1; row < mGrid->GetNumberRows(); row++) {
			if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //destination cell is empty
				//copy contents of source cell to dest cell
				mGrid->SetCellValue(row, mGrid->GetSelectedCols()[0], mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]));
				UpdateRow(row);
			}
		}
		canCopy = false;
		canInc = false;
	}
	else if (canInc && inc) {
		//fill up the empty cells with an incremented value
		if (numerical) {
#if USING_ULONGLONG
			wxULongLong_t number;
			mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).ToULongLong(&number);
			wxString format = wxString::Format(wxT("%%0%d"), mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).Len()) + wxLongLongFmtSpec + wxT("u"); //handles leading zeros
#else
			unsigned long number;
			mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).ToULong(&number);
			wxString format = wxString::Format(wxT("%%0%du"), mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).Len()); //handles leading zeros
#endif
			for (int row = sourceRow + 1; row < mGrid->GetNumberRows(); row++) {
				if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //destination cell is empty
					mGrid->SetCellValue(row, mGrid->GetSelectedCols()[0], wxString::Format(format, ++number));
					UpdateRow(row);
				}
			}
		}
		else { //alphabetical
			wxChar value = mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0])[0];
			for (int row = sourceRow + 1; row < mGrid->GetNumberRows(); row++) {
				if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //destination cell is empty
					if (wxT('Z') == value) {
						//upper case wrap
						value = wxT('A');
					}
					else if (wxT('z') == value) {
						//lower case wrap
						value = wxT('a');
					}
					else {
						value++;
					}
					mGrid->SetCellValue(row, mGrid->GetSelectedCols()[0], wxString(value));
					UpdateRow(row);
				}
			}
		}
		canCopy = false;
		canInc = false;
	}
	mFillCopyButton->Enable(canCopy);
	mFillIncButton->Enable(canInc);
}

/// Checks for exactly one column being selected, and its (numerical) incrementability as a group (highest non-blank cell becomes lowest non-blank cell + 1, etc),
/// and sets button state accordingly.  Optionally does the increment.
/// @param commit True to make changes
void SetTapeIdsDlg::IncrementAsGroup(const bool commit)
{
	//Check whether column can be incremented by group
	int groupSize = 0;
#if USING_ULONGLONG
	wxULongLong_t number = 0; //Initialisation prevents compiler warning
	wxULongLong_t newNumber;
#else
	unsigned long number = 0; //Initialisation prevents compiler warning
	unsigned long newNumber;
#endif
	int sourceRow = 0; //Initialisation prevents compiler warning
	int col; //for convenience
	if (mGrid->GetSelectionBlockTopLeft().IsEmpty() //no blocks selected
	 && mGrid->GetSelectedCells().IsEmpty() //no individual cells selected
	 && 1 == mGrid->GetSelectedCols().GetCount()) { //exactly 1 column selected
		col = mGrid->GetSelectedCols()[0];
		for (int row = 0; row < mGrid->GetNumberRows(); row++) { //check whole column
			if (!mGrid->GetCellValue(row, col).IsEmpty()) { //not empty, or we ignore it
				if (ManipulateCell(row, col, true, false, &newNumber) && (0 == groupSize || number + 1 == newNumber)) { //can increment this cell
					sourceRow = row; //so far it's the lowest cell we can increment
					number = newNumber;
					groupSize++;
				}
				else {
					//don't want to group increment a column with non-incrementable or non-sequential cells
					groupSize = 0; //indicates no go
					break;
				}
			}
		}
	}
	//Increment as a group
	if (groupSize > 1 && commit) { //can do something useful
		for (int row = 0; row <= sourceRow; row++) {
			if (!mGrid->GetCellValue(row, col).IsEmpty()) { //not empty, or we ignore it
#if USING_ULONGLONG
				mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%d"), mGrid->GetCellValue(row,col).Len()) + wxLongLongFmtSpec + wxT("u"), ++number));
#else
				mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%du"), mGrid->GetCellValue(row,col).Len()), ++number));
#endif
				UpdateRow(row);
			}
		}
	}
	mGroupIncButton->Enable(groupSize > 1);
}

/// Checks for (numerical) incrementability or presence of content in all selected cells, and optionally increments or clears.
/// @param inc True to increment; false to clear
/// @param commit True to make changes
/// @return True if the operation would/did change anything
bool SetTapeIdsDlg::ManipulateCells(const bool inc, const bool commit)
{
	bool changing = false;
	//blocks
	for (size_t block = 0; block < mGrid->GetSelectionBlockTopLeft().GetCount(); block++) {
		for (int row = mGrid->GetSelectionBlockTopLeft()[block].GetRow(); row <= mGrid->GetSelectionBlockBottomRight()[block].GetRow(); row++) {
			for (int col = mGrid->GetSelectionBlockTopLeft()[block].GetCol(); col <= mGrid->GetSelectionBlockBottomRight()[block].GetCol(); col++) {
				if (col > 1) { //an editable cell
					changing |= ManipulateCell(row, col, inc, commit);
				}
			}
		}
	}
	//columns
	for (size_t colCounter = 0; colCounter < mGrid->GetSelectedCols().GetCount(); colCounter++) {
		for (int row = 0; row < mGrid->GetNumberRows(); row++) {
			changing |= ManipulateCell(row, mGrid->GetSelectedCols()[colCounter], inc, commit);
		}
	}
	//individual cells
	for (size_t cell = 0; cell < mGrid->GetSelectedCells().GetCount(); cell++) {
		changing |= ManipulateCell(mGrid->GetSelectedCells()[cell].GetRow(), mGrid->GetSelectedCells()[cell].GetCol(), inc, commit);
	}
	return changing;
}

/// Checks for (numerical) incrementability or presence of content, and optionally increments or clears, the given cell.
/// If changing, updates the row the cell is in.
/// @param row The row of the cell to increment or clear.
/// @param col The column of the cell to increment or clear.
/// @param inc True to increment; false to clear
/// @param commit Increment or clear if possible to do so.
/// @param retunNumber Optional cell numerical value to return
/// @return True if cell can be or has been changed.
#if USING_ULONGLONG
bool SetTapeIdsDlg::ManipulateCell(const int row, const int col, const bool inc, const bool commit, wxULongLong_t * returnNumber)
#else
bool SetTapeIdsDlg::ManipulateCell(const int row, const int col, const bool inc, const bool commit, unsigned long * returnNumber)
#endif
{
	bool changeable = false;
#if USING_ULONGLONG
	wxULongLong_t number;
	if (inc && wxNOT_FOUND == mGrid->GetCellValue(row, col).Find(wxT("-")) //not negative number
	 && mGrid->GetCellValue(row, col).ToULongLong(&number)) { //expressable as a number
#else
	unsigned long number;
	if (inc && wxNOT_FOUND == mGrid->GetCellValue(row, col).Find(wxT("-")) //not negative number
	 && mGrid->GetCellValue(row, col).ToULong(&number)) { //expressable as a number
#endif
		changeable = true;
		if (commit) {
			//set cell to incremented number, including any leading zeros
#if USING_ULONGLONG
			mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%d"), mGrid->GetCellValue(row,col).Len()) + wxLongLongFmtSpec + wxT("u"), ++number));
#else
			mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%du"), mGrid->GetCellValue(row,col).Len()), ++number));
#endif
			UpdateRow(row);
		}
		if (returnNumber) {
			*returnNumber = number;
		}
	}
	else if (!inc && !mGrid->GetCellValue(row, col).IsEmpty()) {
		changeable = true;
		if (commit) {
			mGrid->SetCellValue(row, col, wxT(""));
			UpdateRow(row);
		}
	}
	return changeable;
}

/// Responds to the "Help" button being pressed.
/// Opens a modal help dialogue.
void SetTapeIdsDlg::OnHelp(wxCommandEvent & WXUNUSED(event))
{
	wxDialog helpDlg(this, wxID_ANY, wxT("Set Tape IDs help"), wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxDEFAULT_DIALOG_STYLE);
	wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
	helpDlg.SetSizer(sizer);
	wxTextCtrl * text = new wxTextCtrl(&helpDlg, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
	wxString message = wxT("The first column in the table shows the source names of the recorders you are using.  Any that have a red background are preventing you from recording because they are enabled to record but have no corresponding tape ID.  Any that have a yellow background are also missing a tape ID but are not preventing you from recording because they are not enabled to record.\n\n"
	 "The second column shows the tape IDs, if any, corresponding to the sources.  You do not edit the cells in this column because each is created by concatenating the row of editable cells to its right.  Splitting the tape IDs into parts like this allows you to specify and increment them easily.  If there are any duplicate IDs, a warning is shown to the right of the table, but the situation is permitted.\n\n"
	 "You can use as many as, and whichever of, the editable cells you like to create a tape ID, but you must separate the ID into parts which are incremented and parts which are not, and put these into different cells.\n\n"
	 "For example, you may have a tape ID #1001A# which will become #1002A#, #1003A#, etc., when the tape is changed.  To do this, you can put #1001# into the top row of the Part 1 column, and #A# into the cell to its right.  You have now separated the incrementing part from the non-incrementing part, and the number #1001# can be incremented automatically without the #A# interfering with it.\n\n"
	 "There are two buttons to help you fill in the table quickly.  The first of these, \"Fill column with copies\", copies the contents of the highest non-empty cell to all the empty cells below it.  It is only available if you select a column (by clicking on one of the \"Part\" headings), there is at least one non-empty cell (so there is something to copy from), and there is at least one empty cell below it (so there is something to copy to).  In the example above, if you want all your tape IDs to start with #1001#, you can quickly fill in the rest of the column by clicking on the \"Part 1\" heading and pressing \"Fill column with copies\".  (As this will make the second and subsequent rows identical, you will temporarily get a duplicate tape ID warning.)  If you don't want to fill the whole column, you will need to remove unwanted cell contents manually.  You can do this quickly by selecting them and pressing \"Clear selected\".\n\n"
	 "The other button to help you fill in the table quickly is \"Fill column by incrementing\".  Again, this is only available if you select a column where there is an empty cell with empty cells below it, but also the uppermost non-empty cell must be suitable for incrementing.  This will be the case if it contains a set of digits or a single character from #a# to #z# or #A# to #Z#.  Going back to the example (#1001A#), the top cell in the Part 2 column contains #A#, so selecting this column and pressing \"Fill column by incrementing\" will put #B#, #C#, #D# etc. into the cells below.  (If the value reaches #Z# or #z#, it will wrap back round to #A# or #a# respectively.)\n\n"
	 "So now you have been able to fill the tape ID table more efficiently than by manual typing.\n\n"
	 "When tapes are changed, there are two buttons to enable you to update their IDs quickly.  The first is the \"Increment Selected\" button, which will increment any cells containing digits (not letters) that have been selected.  You can select a column as before, or you can select an area of cells by dragging the mouse across them, or you can select or deselect individual cells by control-clicking them.\n\n"
	 "The second button you can use is \"Increment as group\".  This is only available if you select a column containing a continuous sequence of increasing numbers (plus blank cells in any position).  When you press this button, the whole group will increment by the length of the sequence.  So, for instance, if the column contained #5#, #6#, #7# and #8#, pressing the button would change the cells to #9#, #10#, #11# and #12#.\n\n"
	 "A useful tip is that if IDs tend to be incremented in known groupings, rather than as a whole or randomly, it may be quicker to use different columns to specify the different groups.  This will allow you to increment some tape IDs very quickly without affecting the others, by clicking on the appropriate column heading and pressing one of the increment buttons.  For example, you could use the Part 1 column to enter the number part of some of the IDs and the Part 2 column to enter the rest, while using Part 3 for all the letters.\n\n"
	 "You cannot undo individual changes you make to the tape IDs, but you can press Cancel instead of OK to return to the situation where you started.");
	HelpDlg::StyleAndWrite(text, message);
	sizer->Add(text, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	wxButton * okButton = new wxButton(&helpDlg, wxID_OK);
	sizer->Add(okButton, 0, wxALIGN_CENTRE | wxBOTTOM, CONTROL_BORDER);
	helpDlg.ShowModal();
}

/// @return True if the user updated information while the dialogue was shown.
bool SetTapeIdsDlg::IsUpdated()
{
	return mUpdated;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(JumpToTimecodeDlg, wxDialog)
	EVT_TEXT(wxID_ANY, JumpToTimecodeDlg::OnTextChange)
	EVT_SET_FOCUS(JumpToTimecodeDlg::OnFocus)
	EVT_KILL_FOCUS(JumpToTimecodeDlg::OnFocus)
END_EVENT_TABLE()

JumpToTimecodeDlg::JumpToTimecodeDlg(wxWindow * parent, ProdAuto::MxfTimecode tc) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Jump to Timecode")), mTimecode(tc)
{
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	wxBoxSizer * timecodeSizer = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(timecodeSizer, 0, wxALL, CONTROL_BORDER);
	wxFont * font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
	mHours = new MyTextCtrl(this, HRS, wxT("00"));
	mHours->SetFont(*font);
	wxWindowDC DC(mHours);
	wxSize size = DC.GetTextExtent(wxT("00"));
	size.x += 8; //Ugh.  But it doesn't work otherwise...
	mHours->SetClientSize(size);
	mHours->SetMaxLength(2);
	timecodeSizer->Add(mHours, 0, wxFIXED_MINSIZE);
	mHours->SetFocus();
	wxStaticText * hoursSep = new wxStaticText(this, wxID_ANY, wxT(":"));
	hoursSep->SetFont(*font);
	timecodeSizer->Add(hoursSep);
	mMins = new MyTextCtrl(this, MINS, wxT("00"));
	mMins->SetFont(*font);
	mMins->SetClientSize(size);
	mMins->SetMaxLength(2);
	timecodeSizer->Add(mMins, 0, wxFIXED_MINSIZE);
	wxStaticText * minsSep = new wxStaticText(this, wxID_ANY, wxT(":"));
	minsSep->SetFont(*font);
	timecodeSizer->Add(minsSep);
	mSecs = new MyTextCtrl(this, SECS, wxT("00"));
	mSecs->SetFont(*font);
	mSecs->SetClientSize(size);
	mSecs->SetMaxLength(2);
	timecodeSizer->Add(mSecs, 0, wxFIXED_MINSIZE);
	wxStaticText * secsSep = new wxStaticText(this, wxID_ANY, wxT(":"));
	secsSep->SetFont(*font);
	timecodeSizer->Add(secsSep);
	mFrames = new MyTextCtrl(this, FRAMES, wxT("00"));
	mFrames->SetFont(*font);
	mFrames->SetClientSize(size);
	mFrames->SetMaxLength(2);
	timecodeSizer->Add(mFrames, 0, wxFIXED_MINSIZE);
	mainSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALL, CONTROL_BORDER);
	Fit();
//	wxButton * jumpButton = new wxButton(this, wxID_OK, wxT("Jump!"));
//	timecodeSizer->Add(jumpButton, 0, wxALL, CONTROL_BORDER);
//	hours->SetClientSize(size);
//	hours->Layout();
//	hours->SetInitialSize(wxSize(hours->GetCharWidth()*2 + 10, hours->GetCharHeight()));
//	SetFont(*font);
//	hours->SetInitialSize(size);
//	hours->SetClientSize(size);
//	hours->SetSize(size);

}

/// Checks a defined timecode value (for edit rate) has been supplied before allowing dialogue to be shown
int JumpToTimecodeDlg::ShowModal()
{
	if (!mTimecode.undefined) {
		return wxDialog::ShowModal();
	}
	else {
		return wxID_CANCEL; //sanity check - avoids divide by zero if timecode not defined
	}
}

/// Responds to character entry in text entry field by checking, changing focus or deleting character just entered as appropriate
void JumpToTimecodeDlg::OnTextChange(wxCommandEvent & event)
{
	MyTextCtrl * ctrl = (MyTextCtrl *) event.GetEventObject();
	wxString value = ctrl->GetValue();
	bool ok;
	switch (event.GetId()) {
		case HRS:
			ok = CheckValue(value, wxT("23"));
			break;
		case MINS: case SECS:
			ok = CheckValue(value, wxT("59"));
			break;
		default:
			ok = CheckValue(value, wxString::Format(wxT("%02d"), mTimecode.edit_rate.numerator / mTimecode.edit_rate.denominator - 1));
			break;
	}
	if (!ok) { //illegal value
		//remove the last character entered
		ctrl->SetValue(value.Left(ctrl->GetInsertionPoint()));
	}
	else if (2 == value.Len()) { //full field
		//move to the next field
//		ctrl->Navigate();
		if (FRAMES == event.GetId()) {
			FindWindow(HRS)->SetFocus();
		}
		else {
			FindWindow(event.GetId() + 1)->SetFocus();
		}
	}
}

/// Returns true if a control has an allowed value
/// @param value value expressed as a string
/// @param limit Max value (expressed as a string) that is allowed for this control
bool JumpToTimecodeDlg::CheckValue(const wxString & value, const wxString & limit)
{
	const wxString digits(wxT("0123456789"));
	int digit;
	if (!value.Len() || ((wxNOT_FOUND != (digit = digits.Find(value[0]))) && digit <= digits.Find(limit[0]))) { //either empty or first char is a digit, within the limit for that digit
		if (value.Len() < 2 || (wxNOT_FOUND != digits.Find(value[1]))) { //either <2 chars or second char is a digit, within the limit for that digit
			long total, limitVal;
			value.ToLong(&total);
			limit.ToLong(&limitVal);
			if (total <= limitVal) {
				return true;
			}
		}
	}
	return false;
}

/// Puts leading zeros into text entry fields that are losing focus
void JumpToTimecodeDlg::OnFocus(wxFocusEvent & event)
{
	if (wxEVT_KILL_FOCUS == event.GetEventType()) { //this is a control losing focus
		switch (event.GetId()) {
			case HRS: case MINS: case SECS: case FRAMES: {
				MyTextCtrl * ctrl = (MyTextCtrl *) event.GetEventObject();
				if (2 != ctrl->GetValue().Len()) { //this check prevents the value being changed unnecessarily, which causes the control focus to be changed which breaks shift-tabbing
					//put in leading zeros
					ctrl->SetValue(wxString(wxT("00") + ctrl->GetValue()).Right(2));
				}
				break;
			}
			default:
				break;
		}
	}
	event.Skip();
}

/// Returns the timecode corresponding to the values in the boxes
const ProdAuto::MxfTimecode JumpToTimecodeDlg::GetTimecode()
{
	long field;
	((wxTextCtrl *) FindWindow(HRS))->GetValue().ToLong(&field);
	mTimecode.samples = field * 60;
	((MyTextCtrl *) FindWindow(MINS))->GetValue().ToLong(&field);
	mTimecode.samples = (mTimecode.samples + field) * 60;
	((MyTextCtrl *) FindWindow(SECS))->GetValue().ToLong(&field);
	mTimecode.samples += field;
	mTimecode.samples *= mTimecode.edit_rate.numerator / mTimecode.edit_rate.denominator;
	((MyTextCtrl *) FindWindow(FRAMES))->GetValue().ToLong(&field);
	mTimecode.samples += field;
	return mTimecode;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(TestModeDlg, wxDialog)
	EVT_SPINCTRL(MIN_REC, TestModeDlg::OnChangeMinRecTime)
	EVT_SPINCTRL(MAX_REC, TestModeDlg::OnChangeMaxRecTime)
	EVT_SPINCTRL(MIN_GAP, TestModeDlg::OnChangeMinGapTime)
	EVT_SPINCTRL(MAX_GAP, TestModeDlg::OnChangeMaxGapTime)
	EVT_TOGGLEBUTTON(RUN, TestModeDlg::OnRun)
	EVT_TIMER(wxID_ANY, TestModeDlg::OnTimer)
END_EVENT_TABLE()

DEFINE_EVENT_TYPE(wxEVT_TEST_DLG_MESSAGE)
#define COUNTDOWN_FORMAT wxT("%H:%M:%S")

/// Sets up dialogue.
/// @param parent The parent window.
TestModeDlg::TestModeDlg(wxWindow * parent) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Test Mode")), mRecording(false)
{
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	wxFlexGridSizer * gridSizer = new wxFlexGridSizer(3, 5, CONTROL_BORDER, CONTROL_BORDER);
	mainSizer->Add(gridSizer, 0, wxALL, CONTROL_BORDER);
	//top row
	gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Record time minimum"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT), 0, wxALIGN_CENTRE);
#define DEFAULT_MAX_REC_TIME 1
	mMinRecTime = new wxSpinCtrl(this, MIN_REC, wxT(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, DEFAULT_MAX_REC_TIME, 1);
	gridSizer->Add(mMinRecTime, 1, wxALIGN_CENTRE);
	gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("min; maximum")), 0, wxALIGN_CENTRE);
	mMaxRecTime = new wxSpinCtrl(this, MAX_REC, wxT(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, mMinRecTime->GetValue(), TEST_MAX_REC, DEFAULT_MAX_REC_TIME);
	gridSizer->Add(mMaxRecTime, 0, wxALIGN_CENTRE);
	gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("min."), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT), 0, wxALIGN_CENTRE);
	//middle row
	gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Gap time minimum"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT), 0, wxALIGN_CENTRE);
#define DEFAULT_MAX_GAP_TIME 5
	mMinGapTime = new wxSpinCtrl(this, MIN_GAP, wxT(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, DEFAULT_MAX_GAP_TIME, 5);
	gridSizer->Add(mMinGapTime, 0, wxALIGN_CENTRE);
	gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("sec; maximum")), 0, wxALIGN_CENTRE);
	mMaxGapTime = new wxSpinCtrl(this, MAX_GAP, wxT(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, mMinGapTime->GetValue(), TEST_MAX_GAP, DEFAULT_MAX_GAP_TIME);
	gridSizer->Add(mMaxGapTime, 0, wxALIGN_CENTRE);
	gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("sec."), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT), 0, wxALIGN_CENTRE);
	//bottom row
	mRunButton = new wxToggleButton(this, RUN, wxT("Run"));
	gridSizer->Add(mRunButton);
	mRunStopMessage = new wxStaticText(this, wxID_ANY, wxT(""));
	gridSizer->Add(mRunStopMessage, 0, wxALIGN_CENTRE_VERTICAL);
	mRunStopCountdown = new wxStaticText(this, wxID_ANY, wxT(""));
	gridSizer->Add(mRunStopCountdown, 0, wxALIGN_CENTRE_VERTICAL);
	mCancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	gridSizer->Add(mCancelButton);
	Fit();
	mTimer = new wxTimer(this);
	srand(wxDateTime::Now().GetMillisecond());
	SetNextHandler(parent);
}

/// Stops timer when dialogue deleted - otherwise there's a segfault when the timer completes...
TestModeDlg::~TestModeDlg()
{
	mTimer->Stop();
}

/// Stops testing when dialogue closed but leaves any recording happening
int TestModeDlg::ShowModal()
{
	int rc = wxDialog::ShowModal();
	mTimer->Stop();
	mRunButton->SetValue(false);
	mRecording = false;
	mRunStopMessage->SetLabel(wxT("Stopped"));
	mRunStopCountdown->SetLabel(wxT(""));
	return rc;
}



/// Responds to the minimum record time being changed.
/// Adjusts the minimum value of the maximum record time to be the same as the minimum record time.
void TestModeDlg::OnChangeMinRecTime(wxSpinEvent & WXUNUSED(event))
{
	mMaxRecTime->SetRange(mMinRecTime->GetValue(), mMaxRecTime->GetMax());
}

/// Responds to the maximum record time being changed.
/// Adjusts the maximum value of the minimum record time to be the same as the maximum record time.
void TestModeDlg::OnChangeMaxRecTime(wxSpinEvent & WXUNUSED(event))
{
	mMinRecTime->SetRange(mMinRecTime->GetMin(), mMaxRecTime->GetValue());
}

/// Responds to the minimum gap time being changed.
/// Adjusts the minimum value of the maximum gap time to be the same as the minimum gap time.
void TestModeDlg::OnChangeMinGapTime(wxSpinEvent & WXUNUSED(event))
{
	mMaxGapTime->SetRange(mMinGapTime->GetValue(), mMaxGapTime->GetMax());
}

/// Responds to the maximum gap time being changed.
/// Adjusts the maximum value of the minimum gap time to be the same as the maximum gap time.
void TestModeDlg::OnChangeMaxGapTime(wxSpinEvent & WXUNUSED(event))
{
	mMinGapTime->SetRange(mMinGapTime->GetMin(), mMaxGapTime->GetValue());
}

/// Responds to the run button being pressed.
void TestModeDlg::OnRun(wxCommandEvent & WXUNUSED(event))
{
	if (mRunButton->GetValue()) {
		//run
		Record();
	}
	else {
		//stop
		Record(false);
		mTimer->Stop();
		mRunStopMessage->SetLabel(wxT("Stopped"));
		mRunStopCountdown->SetLabel(wxT(""));
	}
}

/// Responds to the timer.
void TestModeDlg::OnTimer(wxTimerEvent & WXUNUSED(event))
{
	mCountdown -= wxTimeSpan(0, 0, 1, 0); //one second
	mRunStopCountdown->SetLabel(mCountdown.Format(COUNTDOWN_FORMAT));
	if (mCountdown.IsEqualTo(wxTimeSpan())) { //empty
		Record(!mRecording);
	}
	else {
		mTimer->Start(1000, wxTIMER_ONE_SHOT); //one second
	}
}

/// Sends a record or stop command, and starts the timer for the next command.
/// @param rec True to record.
void TestModeDlg::Record(bool rec)
{
	if (mRecording != rec) {
		mRecording = rec;
		wxCommandEvent frameEvent(wxEVT_TEST_DLG_MESSAGE, rec ? RECORD : STOP);
		AddPendingEvent(frameEvent);
		int dur;
		if (mRecording) {
			mRunStopMessage->SetLabel(wxT("Recording for"));
			int range = mMaxRecTime->GetValue() - mMinRecTime->GetValue() + 1;
			while ((dur = rand()/(RAND_MAX/range)) > range) {}; //avoid occasional truncation to range
			dur += mMinRecTime->GetValue();
			mCountdown = wxTimeSpan::Minutes(dur);
		}
		else {
			mRunStopMessage->SetLabel(wxT("Stopped for"));
			int range = mMaxGapTime->GetValue() - mMinGapTime->GetValue() + 1;
			while ((dur = rand()/(RAND_MAX/range)) > range) {}; //avoid occasional truncation to range
			dur += mMinGapTime->GetValue();
			mCountdown = wxTimeSpan::Seconds(dur);
		}
		mRunStopCountdown->SetLabel(mCountdown.Format(COUNTDOWN_FORMAT));
		mTimer->Start(1000, wxTIMER_ONE_SHOT); //one second
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const struct {char colour[8]; char labelColour[8]; char label[8]; ProdAuto::LocatorColour::EnumType code;} Colours[N_CUE_POINT_COLOURS] = {
	{ "#FF0000", "#000000", "DEFAULT", ProdAuto::LocatorColour::DEFAULT_COLOUR }, //this must come first
	{ "#FFFFFF", "#000000", "White", ProdAuto::LocatorColour::WHITE },
	{ "#FF0000", "#000000", "Red", ProdAuto::LocatorColour::RED },
	{ "#FFFF00", "#000000", "Yellow", ProdAuto::LocatorColour::YELLOW },
	{ "#00FF00", "#000000", "Green", ProdAuto::LocatorColour::GREEN },
	{ "#00FFFF", "#000000", "Cyan", ProdAuto::LocatorColour::CYAN },
	{ "#0000FF", "#FFFFFF", "Blue", ProdAuto::LocatorColour::BLUE },
	{ "#FF00FF", "#000000", "Magenta", ProdAuto::LocatorColour::MAGENTA },
	{ "#000000", "#FFFFFF", "Black", ProdAuto::LocatorColour::BLACK }
};

DEFINE_EVENT_TYPE (wxEVT_SET_GRID_ROW);

BEGIN_EVENT_TABLE(CuePointsDlg, wxDialog)
	EVT_GRID_EDITOR_SHOWN(CuePointsDlg::OnEditorShown)
	EVT_GRID_EDITOR_HIDDEN(CuePointsDlg::OnEditorHidden)
	EVT_MENU(wxID_ANY, CuePointsDlg::OnMenu)
	EVT_BUTTON(wxID_OK, CuePointsDlg::OnOK)
	EVT_GRID_CELL_LEFT_CLICK(CuePointsDlg::OnCellLeftClick)
	EVT_GRID_LABEL_LEFT_CLICK(CuePointsDlg::OnLabelLeftClick)
	EVT_COMMAND(wxID_ANY, wxEVT_SET_GRID_ROW, CuePointsDlg::OnSetGridRow)
//	EVT_GRID_CELL_RIGHT_CLICK(CuePointsDlg::OnCellRightClick) //don't use right click because it ignores the mouse pointer position
END_EVENT_TABLE()

/// Sets up dialogue.
/// @param parent The parent window.
/// @param savedState The setup file
CuePointsDlg::CuePointsDlg(wxWindow * parent, wxXmlDocument & savedState) : wxDialog(parent, wxID_ANY, wxT(""),  wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mCurrentRow(0)
{
	//accelerator table
	wxAcceleratorEntry entries[11]; //NB number used below and later IDs used for colour menu
	for (int entry = 0; entry < 10; entry++) {
		entries[entry].Set(wxACCEL_NORMAL, (int) '0' + entry, wxID_HIGHEST + N_CUE_POINT_COLOURS + 2 + entry); //ID corresponds to key so we can find out which key is pressed
	}
	entries[10].Set(wxACCEL_NORMAL, WXK_F2, wxID_HIGHEST + N_CUE_POINT_COLOURS + 1);
	wxAcceleratorTable accel(11, entries); //NB number used above
	SetAcceleratorTable(accel);

	//controls
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);

	mTimecodeDisplay = new wxStaticText(this, wxID_ANY, wxT(""));
	wxFont * font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
	mTimecodeDisplay->SetFont(*font);
	mainSizer->Add(mTimecodeDisplay, 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);

	mGrid = new MyGrid(this, wxID_ANY);
	mainSizer->Add(mGrid, 0 , wxEXPAND | wxALL, CONTROL_BORDER);

	mMessage = new wxStaticText(this, wxID_ANY, wxT("Press a number key or click a label\nto choose a cue point;\npress ENTER to choose the highlighted cue point;\npress F2 to generate a default cue point;\nclick the highlighted cell to edit it."), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	mTextColour = mMessage->GetForegroundColour();
	mBackgroundColour = mMessage->GetBackgroundColour();
	mainSizer->Add(mMessage, 0 , wxEXPAND | wxALL, CONTROL_BORDER);

	wxBoxSizer * buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(buttonSizer);

	mOkButton = new wxButton(this, wxID_OK, wxT("OK"));
	buttonSizer->Add(mOkButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);

	buttonSizer->AddStretchSpacer();

	wxButton * cancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttonSizer->Add(cancelButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);

	//set up the grid
	mGrid->CreateGrid(10, 2);
	for (int row = 0; row < 10; row++) {
		mGrid->SetRowLabelValue(row, wxString::Format(wxT("%d"), (row + 1) % 10));
	}
	mGrid->SetColLabelValue(0, wxT("Description"));
	mGrid->SetColLabelValue(1, wxT("Colour"));
	wxGridCellAttr * readOnly = new wxGridCellAttr;
	readOnly->SetReadOnly();
	mGrid->SetColAttr(1, readOnly); //prevent editing of this column, which is possible if you get to it by tabbing
	mCuePointsNode = savedState.GetRoot()->GetChildren();
	while (mCuePointsNode && mCuePointsNode->GetName() != wxT("CuePoints")) {
		mCuePointsNode = mCuePointsNode->GetNext();
	}
	if (!mCuePointsNode) {
		Reset();
		mCuePointsNode = new wxXmlNode(savedState.GetRoot(), wxXML_ELEMENT_NODE, wxT("CuePoints"));
	}
	else {
		//read in existing data
		Load();
	}
}

/// Loads the grid with default values
void CuePointsDlg::Reset()
{
	mGrid->ClearGrid();
	for (int i = 0; i < 10; i++) {
		SetColour(i, 0); //default
	}
}

/// Loads the grid contents from the XML document.
void CuePointsDlg::Load()
{
	//remove current values as may not be overwriting everything
	Reset();
	//load new values
	wxXmlNode * cuePointNode = mCuePointsNode->GetChildren();
	while (cuePointNode) {
		wxString shortcut, colour;
		unsigned long shortcutVal, colourVal;
		if (wxT("CuePoint") == cuePointNode->GetName() //the right sort of node
		 && cuePointNode->GetPropVal(wxT("Shortcut"), &shortcut) //has a required attribute...
		 && shortcut.ToULong(&shortcutVal) //... which is a number...
		 && shortcutVal < 10 //...in the right range
		 && cuePointNode->GetPropVal(wxT("Colour"), &colour) //has a required attribute...
		 && colour.ToULong(&colourVal) //... which is a number...
		 && colourVal < N_CUE_POINT_COLOURS) { //...in the right range
			shortcutVal = (shortcutVal == 0) ? 9 : shortcutVal - 1; //row number
			mGrid->SetCellValue(shortcutVal, 0, cuePointNode->GetNodeContent().Trim(false).Trim(true));
			SetColour(shortcutVal, colourVal);
		}
		cuePointNode = cuePointNode->GetNext();
	}
	mGrid->AutoSizeColumns();
	Fit();
}

/// Saves the grid contents in the XML document.
void CuePointsDlg::Save()
{
	//remove old state
	wxXmlNode * cuePointNode = mCuePointsNode->GetChildren();
	while (cuePointNode) {
		wxXmlNode * deadNode = cuePointNode;
		cuePointNode = cuePointNode->GetNext();
		mCuePointsNode->RemoveChild(deadNode);
		delete deadNode;
	}
	//store new state
	for (int row = 0; row < 10; row++) {
		wxString description = mGrid->GetCellValue(row, 0).Trim(false).Trim(true);
		if (!description.IsEmpty() || mDescrColours[row]) { //something to store
			//create a new element node "CuePoint" containing a new text node with the description
			new wxXmlNode(
				new wxXmlNode(
					mCuePointsNode,
					wxXML_ELEMENT_NODE,
					wxT("CuePoint"),
					wxT(""),
					new wxXmlProperty(
						wxT("Shortcut"),
						wxString::Format(wxT("%d"), (row + 1) % 10),
						new wxXmlProperty(wxT("Colour"), wxString::Format(wxT("%d"), mDescrColours[row]))
					)
				),
				wxXML_TEXT_NODE,
				wxT(""),
				description
			);
		}
	}
}

/// If dialogue not already visible, shows the dialogue and returns when the dialogue is dismissed, saving the grid if OK is pressed.
/// If dialogue already visible, this will be a Mark Cue keystroke from an external device, so act as if the default event had been selected
/// @param timecode If present, displays this at the top of the window and goes into add cue mode - shortcut keys enabled.
int CuePointsDlg::ShowModal(const wxString timecode)
{
	int rc = wxID_CANCEL; //to stop compiler warning
	if (IsModal()) {
		//return a blank (default) locator
		mDescription.Clear();
		mColour = 0;
		EndModal(wxID_OK);
	}
	else {
		mTimecodeDisplay->SetLabel(timecode);
		mTimecodeDisplay->Show(!timecode.IsEmpty());
		mMessage->Show(!timecode.IsEmpty());
		Fit();

		if (timecode.IsEmpty()) {
			SetTitle(wxT("Edit Cue Point Descriptions"));
			mGrid->SetFocus(); //typing will immediately start editing
		}
		else {
			SetTitle(wxT("Choose/edit Cue Point"));
			mOkButton->SetFocus(); //stops shortcut keys disappearing into the grid
			mMessage->SetForegroundColour(mTextColour); //as it's shown we need to see it...
		}
		if (wxID_OK == (rc = wxDialog::ShowModal())) {
			Save();
		}
		else {
			//reload old state
			Load();
		}
	}
	return rc;
}

/// Disables shortcut keys so that they can be used for text entry
void CuePointsDlg::OnEditorShown(wxGridEvent & WXUNUSED(event))
{
	mMessage->SetForegroundColour(mBackgroundColour); //disable shortcuts
}

/// Adjusts the grid and window sizes to reflect any editing changes.
void CuePointsDlg::OnEditorHidden(wxGridEvent & WXUNUSED(event))
{
	mGrid->AutoSizeColumns();
//	mGrid->ForceRefresh(); //sometimes makes a mess if you don't
	Fit();
	if (mTimecodeDisplay->IsShown()) {
		mOkButton->SetFocus(); //stops shortcut keys disappearing into the grid
		mMessage->SetForegroundColour(mTextColour); //as it's shown we need to see it...
		//Prevent it jumping down a cell because it's likely you want to submit the just-edited value as the cue point
		wxCommandEvent event(wxEVT_SET_GRID_ROW, mCurrentRow);
		AddPendingEvent(event);
	}
	else {
		mGrid->SetFocus(); //typing will immediately start editing
		mCurrentRow = mGrid->GetCursorRow(); //this is needed if you press ENTER, and then return later to press ENTER to create a cue point, as the cursor moves down
	}
}

/// Sets the grid cursor to the row specified by the event ID - this has to be done this way because it doesn't like you doing it in a grid event handler
void CuePointsDlg::OnSetGridRow(wxCommandEvent & event)
{
	mGrid->SetGridCursor(event.GetId(), 0);
}

/// Depending on the ID, sets a new locator colour (i.e. called from the pop-up colour menu), or, if enabled to do so, closes the dialogue (saving the grid state), setting the selected locator accordingly (i.e. called with a shortcut key).
/// @param event Contains the menu ID.
void CuePointsDlg::OnMenu(wxCommandEvent & event)
{
	if (event.GetId() < wxID_HIGHEST + 1 + N_CUE_POINT_COLOURS) { //a colour menu selection
		SetColour(mCurrentRow, event.GetId() - wxID_HIGHEST - 1);
		//Move the grid cursor away from the right hand column as it can't be "edited" as such
		wxCommandEvent event(wxEVT_SET_GRID_ROW, mCurrentRow);
		AddPendingEvent(event);
	}
	else if (mTextColour == mMessage->GetForegroundColour() && mTimecodeDisplay->IsShown()) { //shortcuts enabled
		if (wxID_HIGHEST + N_CUE_POINT_COLOURS + 1 == event.GetId()) { //function key pressed
			//return a blank (default) locator
			mDescription.Clear();
			mColour = 0;
		}
		else {
			int row = event.GetId() == wxID_HIGHEST + N_CUE_POINT_COLOURS + 2 ? 9 : event.GetId() - N_CUE_POINT_COLOURS - wxID_HIGHEST - 3;
			mDescription = mGrid->GetCellValue(row, 0).Trim(false).Trim(true);
			mColour = mDescrColours[row];
		}
		Save();
		EndModal(wxID_OK);
	}
	else { //editing
		event.Skip();
	}
}

/// If dialogue is accepting locator selection shortcuts, acts as if the given locator number had been entered
/// @param shortcut The locator number (0-9) or -1 to cancel the dialogue
void CuePointsDlg::Shortcut(const int shortcut)
{
	if (mTextColour == mMessage->GetForegroundColour() && mTimecodeDisplay->IsShown()) { //shortcuts enabled
		if (-1 == shortcut) { //cancel
			Load(); //reload old state (to make it consistent with pressing ESC in the dialogue)
			EndModal(wxID_CANCEL);
		}
		else {
			int row = shortcut ? shortcut - 1 : 9;
			mDescription = mGrid->GetCellValue(row, 0).Trim(false).Trim(true);
			mColour = mDescrColours[row];
			Save();
			EndModal(wxID_OK);
		}
	}
}

/// Sets the selected locator values.
void CuePointsDlg::OnOK(wxCommandEvent & event)
{
	//Get current row
	mDescription = mGrid->GetCellValue(mCurrentRow, 0).Trim(false).Trim(true);
	mColour = mDescrColours[mCurrentRow];
	event.Skip();
}

/// Produces a pop-up colour menu if the correct column has been clicked.
/// @param event Indicates the column of the grid which was clicked.
void CuePointsDlg::OnCellLeftClick(wxGridEvent & event)
{
	mCurrentRow = event.GetRow(); //Note the row for ENTER being pressed in add cue mode, or if a colour is going to be set
	if (mTimecodeDisplay->IsShown()) {
		wxCommandEvent rowEvent(wxEVT_SET_GRID_ROW, mCurrentRow); //undo the "jump to the edited cell" from OnEditorHidden()
		AddPendingEvent(rowEvent);
		mOkButton->SetFocus(); //makes sure ENTER can still be pressed after clicking on a different event
	}
	if (1 == event.GetCol()) { //clicked in the colour column
		wxMenu colourMenu(wxT("Choose colour"));
		for (unsigned int i = 0; i < N_CUE_POINT_COLOURS; i++) {
			colourMenu.Append(wxID_HIGHEST + 1 + i, wxString(Colours[i].label, *wxConvCurrent));
		}
		PopupMenu(&colourMenu);
	}
	event.Skip();
}

/// Sets the selected event and closes the dialogue if in cue point setting mode
void CuePointsDlg::OnLabelLeftClick(wxGridEvent & event)
{
	if (mTimecodeDisplay->IsShown() && mTextColour == mMessage->GetForegroundColour() && event.GetRow() > -1) { //make sure it isn't a click in a column heading
		mDescription = mGrid->GetCellValue(event.GetRow(), 0).Trim(false).Trim(true);
		mColour = mDescrColours[event.GetRow()];
		EndModal(wxID_OK);
	}
}

/// Sets text, text colour and background colour to correspond to an event colour index
/// @param row The row to set
/// @param colour The colour index to be used
void CuePointsDlg::SetColour(const int row, const int colour)
{
	mGrid->SetCellValue(row, 1, wxString(Colours[colour].label, *wxConvCurrent));
	mGrid->SetCellBackgroundColour(row, 1, wxString(Colours[colour].colour, *wxConvCurrent));
	mGrid->SetCellTextColour(row, 1, wxString(Colours[colour].labelColour, *wxConvCurrent));
	mDescrColours[row] = colour;
}

/// Returns the currently selected locator's description
const wxString CuePointsDlg::GetDescription()
{
	return mDescription;
}

/// Returns colour index of the currently selected locator
size_t CuePointsDlg::GetColourIndex()
{
	return mColour;
}

/// Returns the locator colour code corresponding to an index
ProdAuto::LocatorColour::EnumType CuePointsDlg::GetColourCode(const size_t index)
{
	return Colours[index].code;
}

/// Returns the displayed colour corresponding to an index
const wxColour CuePointsDlg::GetColour(const size_t index)
{
	return wxColour(wxString(Colours[index].colour, *wxConvCurrent));
}

/// Returns the label colour corresponding to an index.
const wxColour CuePointsDlg::GetLabelColour(const size_t index)
{
	return wxColour(wxString(Colours[index].labelColour, *wxConvCurrent));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(ChunkingDlg, wxDialog)
	EVT_SPINCTRL(wxID_ANY, ChunkingDlg::OnChangeChunkSize)
	EVT_CHOICE(wxID_ANY, ChunkingDlg::OnChangeChunkAlignment)
	EVT_TOGGLEBUTTON(wxID_ANY, ChunkingDlg::OnEnable)
	EVT_TIMER(wxID_ANY, ChunkingDlg::OnTimer)
END_EVENT_TABLE()

#define MAX_CHUNK_SIZE 120 //minutes
#define N_ALIGNMENTS 10

static const int Alignments[N_ALIGNMENTS] = {
	0,
	1,
	2,
	5,
	10,
	15,
	20,
	30,
	40,
	60,
};

/// Sets up dialogue.
/// @param parent Parent window.
/// @param chunkButton The "chunk now" button, for manipulating the label and tooltip.
/// @param savedState The XML document for retrieving and saving settings.
/// Chunking occurs in two ways: manually, when the "chunk now" button in the main frame is pressed, or automatically, when enabled in this dialogue.
/// The "chunk now" button should only be enabled while recording.  When pressed, it calls IngexguiFrame::OnChunk(), which disables the button to prevent another chunk being asked for while the previous chunking cycle is still in progress, and calls ChunkingDlg::RunFrom() with no arguments, which stops any pending automatic chunking trigger and resets the button label.  A chunking cycle is then initiated by calling RecorderGroupCtrl::ChunkStop() with the current timecode and recording details.  See later in this description for what happens when this method is called.
/// Automatic chunking is set up at the start of recording, when ChunkingDlg::RunFrom() is called, with the start timecode, and a chunking postroll value determined by the RecorderGroup object (about half a second if all recorders can manage this).  If automatic chunking is enabled in the dialogue, the trigger timecode for the start of the next chunk is worked out.
/// This is either the start time plus the chunk length, or, if alignment is enabled in the dialogue and the function call, the next alignment point (or the one after that, if it's less than half a minute away).
/// The "chunk now" button label is set to the time difference if an alignment was calculated (otherwise it's already showing the chunk length, which is the correct value) and a repeating countdown tick timer of a second's duration is started, which decrements the countdown value on the button until it reaches zero.  (It is not used to trigger generation of the next chunk, as a timer cannot be relied upon to do this with frame accuracy.)
/// The postroll value is then subtracted from the trigger timecode, which allows recorders to be contacted sufficiently in advance of the timecode at which they are instructed to stop, to avoid recording overruns.  It also ensures that the trigger occurs in time despite it being generated asynchronously from a refresh timer.
/// Timepos::SetTrigger() is called with the next chunk start time, with a wrap flag to indicate if it is to happen tomorrow (or the trigger would occur immediately), and a window pointer to send an event to.
/// At the first Timepos::OnRefreshTimer() call after the trigger point, an event is generated which is picked up by IngexguiFrame::OnTimeposEvent(), which calls RecorderGroupCtrl::ChunkStop() with the trigger timecode, the current contents of the description field, and cue point data.  This initiates a chunking cycle by causing all the recorders to be told to stop at the trigger time, plus a postroll of the chunking postroll duration.  It also sets mode variable RecorderGroupCtrl::mChunking from (initially) RecorderGroupCtrl::NOT_CHUNKING to RecorderGroupCtrl::STOPPING.
/// Because this mode is set, the first recorder that reports that it has successfully stopped causes the mode to be updated to RecorderGroupCtrl::WAITING, and generates a RecorderGroupCtrl::SET_TRIGGER event with the timecode of the next chunk start, which is the timecode returned by the recorder (as this is the first frame not recorded).  the RecorderGroupCtrl::WAITING mode causes RecorderGroupCtrl::TRACK_STATUS events to have a flag set which prevents the recorder source tree briefly reporting an error due to a mismatch between the recorder mode (stopped) and the main frame mode (recording).
/// This event is picked up by IngexguiFrame::OnRecorderGroupEvent(), which generates another call to Timepos::SetTrigger() with the next chunk start (which can be in the past so long as it is within the recorders' preroll abilities), targeted at the RecorderGroup.  It also calls EventList::AddEvent() to append the chunk boundary entry to the recording list.
/// Every recorder that stops also causes a RecorderGroupCtrl::CHUNK_END event to be generated, which is treated by the frame similarly to a RecorderGroupCtrl::STOP event except it does not cause the frame to go into STOP mode.
/// When Timepos reaches the trigger point, ensuring that recorders are not asked to start recording in the future, the event it generates is picked up by RecorderGroupCtrl::OnTimeposEvent() which checks that the mode is still RecorderGroupCtrl::WAITING (preventing race states due to user intervention while an event is in the queue).  If so, it sets the mode to RecorderGroupCtrl::RECORDING_CHUNK and starts recording again just as if the user had pressed the record button, but at the timecode returned by the first recorder that stopped.  The mode results in each recording command (sent after requesting source details from the frame) having a preroll of zero (which is safe to do as we know the start time is in the past), rather than the normal value.
/// When recorders report that they are recording, the mode causes a RecorderGroupCtrl::CHUNK_START rather than a RecorderGroupCtrl::RECORDING event to be generated.  This prevents the main frame from calling Timepos::Record(), which would otherwise start the position display counting from zero and generate incorrect positions in the recording list, and also prevents EventList::AddEvent() from being called to append a start entry to the recording list (as the chunk boundary entry performs this function).
/// Instead, ChunkingDlg::RunFrom() is called with the timecode from the event and the align argument false, to start counting down exactly one chunk length to the next chunk point (if automatic chunking is enabled) - regardless of whether the chunk had been intiated manually or automatically.  The chunking button is also enabled in case it had been pressed by the user, which would have been disabled it.  The cycle thus begins again.
/// The mode is set back to RecorderGroupCtrl::NOT_CHUNKING when RecorderGroupCtrl::Stop() is called as a result of the user pressing the stop button. ChunkingDlg::RunFrom() with no arguments is called when the frame mode changes from recording, to stop any pending automatic chunking trigger and reset the countdown display on the "chunk now" button.
ChunkingDlg::ChunkingDlg(wxWindow * parent, wxButton * chunkButton, Timepos * timepos, wxXmlDocument & savedState) : wxDialog(parent, wxID_ANY, wxT("Chunking")), mChunkButton(chunkButton), mTimepos(timepos), mSavedState(savedState), mRecording(false)
{
	const wxChar* alignmentLabels[N_ALIGNMENTS] = {
		wxT("None"),
		wxT("minute"),
		wxT("even minutes"),
		wxT("5 minutes"),
		wxT("10 minutes"),
		wxT("15 minutes"),
		wxT("20 minutes"),
		wxT("30 minutes"),
		wxT("40 minutes"),
		wxT("60 minutes"),
	};
	//controls
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	wxStaticBoxSizer * sizeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Chunk Size"));
	mainSizer->Add(sizeBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mChunkSizeCtrl = new wxSpinCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, MAX_CHUNK_SIZE, 10);
	sizeBox->Add(mChunkSizeCtrl, 0, wxALIGN_CENTRE);
	sizeBox->Add(new wxStaticText(this, wxID_ANY, wxT(" min")), 0, wxALIGN_CENTRE);
	wxStaticBoxSizer * alignBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Chunk Alignment"));
	mainSizer->Add(alignBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mChunkAlignCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxArrayString(N_ALIGNMENTS, alignmentLabels));
	alignBox->Add(mChunkAlignCtrl, 0, wxALIGN_CENTRE);
	mEnableButton = new wxToggleButton(this, wxID_ANY, wxT("Enable"));
	mainSizer->Add(mEnableButton, 0, wxALIGN_CENTRE);
	mainSizer->Add(new wxButton(this, wxID_OK, wxT("OK")), 1, wxEXPAND | wxALL, CONTROL_BORDER);
	Fit();
	mCountdownTimer = new wxTimer(this);

	//saved state
	wxXmlNode * chunkingNode = mSavedState.GetRoot()->GetChildren();
	while (chunkingNode && chunkingNode->GetName() != wxT("Chunking")) {
		chunkingNode = chunkingNode->GetNext();
	}
	if (chunkingNode) {
		long value;
		if (chunkingNode->GetNodeContent().ToLong(&value) && 0 < value && MAX_CHUNK_SIZE >= value) {
			mChunkSizeCtrl->SetValue(value);
		}
		mEnableButton->SetValue(wxT("Yes") == chunkingNode->GetPropVal(wxT("Enabled"), wxT("No")));
		wxString str;
		mChunkAlignCtrl->SetSelection(0);
		if (chunkingNode->GetPropVal(wxT("Alignment"), &str) && str.ToLong(&value) && 0 < value && N_ALIGNMENTS > value) {
			mChunkAlignCtrl->SetSelection(value);
		}
	}

	SetNextHandler(parent);
	Reset();
}

/// Overloads Show Modal to update the XML document with chunking settings.
int ChunkingDlg::ShowModal()
{
	wxDialog::ShowModal();
	//Remove old nodes
	wxXmlNode * node = mSavedState.GetRoot()->GetChildren();
	while (node) {
		if (wxT("Chunking") == node->GetName()) {
			wxXmlNode * deadNode = node;
			node = node->GetNext();
			mSavedState.GetRoot()->RemoveChild(deadNode);
			delete deadNode;
		}
		else {
			node = node->GetNext();
		}
	}
	//Create new node (element node with two attributes, containing text node)
	node = new wxXmlNode(mSavedState.GetRoot(), wxXML_ELEMENT_NODE, wxT("Chunking"), wxT(""), new wxXmlProperty(wxT("Enabled"), mEnableButton->GetValue() ? wxT("Yes") : wxT("No")));
	node->AddProperty(new wxXmlProperty(wxT("Alignment"), wxString::Format(wxT("%d"), mChunkAlignCtrl->GetSelection())));
	new wxXmlNode(node, wxXML_TEXT_NODE, wxT(""), wxString::Format(wxT("%d"), mChunkSizeCtrl->GetValue()));
	return wxID_OK;
}

/// Responds to chunking enable button being toggled by starting/stopping chunking if recording, and updating the button label.
void ChunkingDlg::OnEnable(wxCommandEvent & WXUNUSED(event))
{
	if (mRecording) {
		if (mEnableButton->GetValue()) {
			//start chunk countdown from now
			ProdAuto::MxfTimecode currentTimecode;
			mTimepos->GetTimecode(&currentTimecode);
			RunFrom(currentTimecode);
		}
		else {
			//stop chunk countdown
			RunFrom();
			mRecording = true; //calling RunFrom() will clear mRecording
		}
	}
	SetCountdownLabel();
}

/// Responds to chunking size being changed by resetting the countdown counter and updating the label on the chunking button if not currently counting down.
void ChunkingDlg::OnChangeChunkSize(wxSpinEvent & WXUNUSED(event))
{
	if (!mCountdownTimer->IsRunning()) {
		Reset();
	}
}

/// Responds to chunking size being changed by resetting the countdown counter and updating the label on the chunking button if not currently counting down.
void ChunkingDlg::OnChangeChunkAlignment(wxCommandEvent & WXUNUSED(event))
{
	if (!mCountdownTimer->IsRunning()) {
		Reset();
	}
}

/// Starts or stops the countdown.
/// @param startTimecode Timecode from which to calculate when to trigger a chunk.  Stops the countdown if undefined flag set.
/// @param postroll Subtracted from the time to trigger a chunk.  If undefined flag set, previous stored value is used.
/// @param align True to start next chunk at the next alignment point (or the one after, if the next one is too soon), if aligning is enabled.
void ChunkingDlg::RunFrom(const ProdAuto::MxfTimecode & startTimecode, const ProdAuto::MxfDuration & postroll, const bool align)
{
	Reset();
	mRecording = !startTimecode.undefined && startTimecode.edit_rate.denominator; //latter a sanity check
	if (!postroll.undefined) {
		mPostroll = postroll;
	}
	if (mRecording) {
		if (mEnableButton->GetValue()) {
			//calculate when to chunk to frame accuracy
			ProdAuto::MxfTimecode triggerTimecode = startTimecode;
			if (align && mChunkAlignment) {
				triggerTimecode.samples /= mChunkAlignment * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator; //number of alignment points passed since midnight
				triggerTimecode.samples = ++triggerTimecode.samples * mChunkAlignment * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator; //the next alignment point
				if (triggerTimecode.samples - startTimecode.samples < 30 * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator) { //less than half a minute to the next alignment point, so producing a very small chunk
					//don't chunk until the alignment point after this
					triggerTimecode.samples += mChunkAlignment * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator;
				}
				//we now know the time to the next chunk
				mCountdown = (triggerTimecode.samples - startTimecode.samples) / (triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator);
				mCountdownTimer->Start(1000); //countdown in seconds - this is only for the button label
				SetCountdownLabel();
			}
			else {
				mCountdownTimer->Start(1000); //countdown in seconds - this is only for the button label
				triggerTimecode.samples += (int64_t) mChunkLength * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator;
			}
			triggerTimecode.samples -= mPostroll.samples; //trigger the stop command before a postroll period to make sure recorders get it in time
			bool wrap = triggerTimecode.samples > 24LL * 3600 * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator;
			triggerTimecode.samples %= 24LL * 3600 * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator;
			//set the trigger to chunk
			mTimepos->SetTrigger(&triggerTimecode, (wxEvtHandler *) GetParent(), wrap); //will inform main frame when trigger occurs
		}
	}
	else {
		//stop chunking
		mCountdownTimer->Stop();
		mTimepos->SetTrigger(&InvalidMxfTimecode, (wxEvtHandler *) GetParent());
	}
}

/// Responds to the countdown timer by updating the label on the chunking button.
void ChunkingDlg::OnTimer(wxTimerEvent & WXUNUSED(event))
{
	if (mCountdown) {
		mCountdown--;
	}
	SetCountdownLabel();
}

/// Resets the countdown to the full chunk size.
void ChunkingDlg::Reset()
{
	mChunkLength = (unsigned long) mChunkSizeCtrl->GetValue() * 60; //in seconds
	mChunkAlignment = Alignments[mChunkAlignCtrl->GetSelection()] * 60; //in seconds
	mCountdown = mChunkLength;
	if (mCountdownTimer->IsRunning()) {
		//nicer if it's accurate to the second
		mCountdownTimer->Stop();
		mCountdownTimer->Start();
	}
	SetCountdownLabel();
}

/// Updates the label on the chunking button to the current countdown value, or dashes if automatic chunking is not enabled.
void ChunkingDlg::SetCountdownLabel()
{
	if (mEnableButton->GetValue()) {
		if (mChunkAlignment && !mCountdownTimer->IsRunning()) { //don't know when the next chunk is going to be because that can only be determined when we start recording
			mChunkButton->SetLabel(wxT("Chunk ???:??"));
			mChunkButton->SetToolTip(wxT("Automatic chunking is enabled, aligned with timecode"));
		}
		else {
			mChunkButton->SetLabel(wxString::Format(wxT("Chunk %d:%02d"), mCountdown / 60, mCountdown % 60));
			mChunkButton->SetToolTip(wxT("Automatic chunking is enabled"));
		}
	}
	else {
		mChunkButton->SetLabel(wxT("Chunk ---:--"));
		mChunkButton->SetToolTip(wxT("Automatic chunking is disabled"));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SelectRecDlg, wxDialog)
	EVT_CHOICE(PROJECT, SelectRecDlg::OnSelectProject)
	EVT_CHOICE(DATE, SelectRecDlg::OnSelectDate)
	EVT_LISTBOX(RECORDING, SelectRecDlg::OnSelectRecording)
	EVT_LISTBOX_DCLICK(RECORDING, SelectRecDlg::OnDoubleClick)
	EVT_BUTTON(UP, SelectRecDlg::OnNavigateRecordings)
	EVT_BUTTON(DOWN, SelectRecDlg::OnNavigateRecordings)
	EVT_TOGGLEBUTTON(PREFER_ONLINE, SelectRecDlg::OnPreferOnline)
END_EVENT_TABLE()

/// Sets up dialogue.
SelectRecDlg::SelectRecDlg(wxWindow * parent) : wxDialog(parent, wxID_ANY, wxT("Select a recording"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	//controls
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);

	wxStaticBoxSizer * projectBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Project"));
	mainSizer->Add(projectBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mProjectSelector = new wxChoice(this, PROJECT);
	projectBox->Add(mProjectSelector, 1, wxEXPAND);

	wxStaticBoxSizer * dateBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Date"));
	mainSizer->Add(dateBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mDateSelector = new wxChoice(this, DATE);
	dateBox->Add(mDateSelector, 1, wxEXPAND);

	wxStaticBoxSizer * recordingBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Recording"));
	mainSizer->Add(recordingBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	wxBoxSizer * recSizer = new wxBoxSizer(wxHORIZONTAL);
	recordingBox->Add(recSizer, 1, wxEXPAND);
	wxSize size = wxDefaultSize;
	size.SetHeight(100);
	mRecordingSelector = new wxListBox(this, RECORDING, wxDefaultPosition, size);
	recSizer->Add(mRecordingSelector, 1, wxEXPAND);
	wxBoxSizer * arrowsSizer = new wxBoxSizer(wxVERTICAL);
	recSizer->Add(arrowsSizer, 0, wxEXPAND);
	mUpButton = new wxBitmapButton(this, UP, wxBitmap(up));
	arrowsSizer->Add(mUpButton);
	arrowsSizer->AddStretchSpacer();
	mDownButton = new wxBitmapButton(this, DOWN, wxBitmap(down));
	arrowsSizer->Add(mDownButton);

	wxStaticBoxSizer * qualityBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Quality"));
	mainSizer->Add(qualityBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mPreferOnline = new wxToggleButton(this, PREFER_ONLINE, wxT("Prefer Online"));
	qualityBox->Add(mPreferOnline, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mOnlineMessage = new wxStaticText(this, wxID_ANY, wxT("Online available"));
	qualityBox->Add(mOnlineMessage, 0, wxEXPAND | wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);

	mainSizer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0, wxALL, CONTROL_BORDER);

	//accelerator keys
	wxAcceleratorEntry accelerators[2];
	accelerators[0].Set(wxACCEL_NORMAL, WXK_UP, UP);
	accelerators[1].Set(wxACCEL_NORMAL, WXK_DOWN, DOWN);
	wxAcceleratorTable table(2, accelerators);
	SetAcceleratorTable(table);

	//window
	Fit(); //needed as size of recording selector
	SetMinSize(GetSize()); //stops window being shrunk far enough for controls to overlap
}

/// Displays dialogue.
int SelectRecDlg::ShowModal()
{
//	wxArrayString projects;
//	wxDir::GetAllFiles(ROOT_DIR, &projects, wxEmptyString, wxDIR_FILES|wxDIR_DIRS);
//	mProjectSelector->Append(projects);
	wxDir offlineDir(wxString(RECORDING_SERVER_ROOT) + wxFileName::GetPathSeparator() + OFFLINE_SUBDIR);
	if (offlineDir.IsOpened()) {
		wxString project;
		mProjectSelector->Clear(); //repopulating it in case directory structure has changed
		if (offlineDir.GetFirst(&project, wxEmptyString, wxDIR_DIRS)) { //GetAllFiles() doesn't return directory names so use this method instead
			mProjectSelector->Enable();
			do {
				mProjectSelector->Append(project);
				//maintain the selection if this was the previously selected project
				if (project == mSelectedProject) {
					mProjectSelector->SetSelection(mProjectSelector->GetCount() - 1);
					wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, PROJECT);
					AddPendingEvent(event); //trigger repopulating of date control
				}
			} while (offlineDir.GetNext(&project));
			//if only a single project, select it for convenience
			if (1 == mProjectSelector->GetCount() && wxNOT_FOUND == mProjectSelector->GetSelection()) {
				mProjectSelector->SetSelection(0);
				wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, PROJECT);
				AddPendingEvent(event); //trigger repopulating of date control
			}
		}
		else { //no projects
			mProjectSelector->Append(wxT("NO PROJECTS"));
			mProjectSelector->Select(0);
			mProjectSelector->Disable();
		}
	}
	else {
		mProjectSelector->Append(wxT("NO OFFLINE MATERIAL DIRECTORY"));
		mProjectSelector->Select(0);
		mProjectSelector->Disable();
	}
	if (!mProjectSelector->IsEnabled() || wxNOT_FOUND == mProjectSelector->GetSelection()) { //no project selected
		//can select no dates and recordings so clear
		mDateSelector->Clear();
		mDateSelector->Disable();
		mRecordingSelector->Clear();
		mRecordingSelector->Disable();
		mUpButton->Disable();
		mDownButton->Disable();
		mPreferOnline->SetBackgroundColour(wxNullColour);
		mOnlineMessage->Hide();
		FindWindow(wxID_OK)->Disable();
	}
	Fit(); //make sure all text can all be displayed
	return wxDialog::ShowModal();
}

/// Populates the date selector with available dates for this project.
/// If previously-selected date is found, or only one date is found, selects this and generates a date select event.
void SelectRecDlg::OnSelectProject(wxCommandEvent & WXUNUSED(event))
{
	mSelectedProject = mProjectSelector->GetStringSelection();
	wxDir projDir(wxString(RECORDING_SERVER_ROOT) + wxFileName::GetPathSeparator() + OFFLINE_SUBDIR + wxFileName::GetPathSeparator() + mSelectedProject); //produces rubbish if ROOT_DIR not turned into a wxString
	wxString date;
	mDateSelector->Clear(); //repopulating it in case directory structure has changed
	wxArrayString dates; //to allow sorting
	if (projDir.GetFirst(&date, wxEmptyString, wxDIR_DIRS)) { //GetAllFiles() doesn't return directory names so use this method instead
		mDateSelector->Enable();
		long value;
		do {
			if (8 == date.Len() && wxNOT_FOUND == date.Find(wxT("-")) && date.ToLong(&value)) { //it's an 8-digit date directory
				dates.Add(date);
			}
		} while (projDir.GetNext(&date));
		dates.Sort();
		wxDateTime dateTime;
		for (size_t i = 0; i < dates.GetCount(); i++) {
			date = dates[i];
			//show a formatted date as the choice
			date.Left(4).ToLong(&value);
			dateTime.SetYear((int) value);
			date.Mid(4, 2).ToLong(&value);
			dateTime.SetMonth((wxDateTime::Month) (value - 1));
			date.Right(2).ToLong(&value);
			dateTime.SetDay((wxDateTime::wxDateTime_t) value);
			mDateSelector->Append(dateTime.Format(wxT("%a %d %b %g")));
			//store the directory name for later use
			mDateSelector->SetClientObject(mDateSelector->GetCount() - 1, new StringContainer(date)); //deleted by control
			//maintain the selection if this was the previously selected date, or select for convenience if it's the only date
			if (date == mSelectedDate || (1 == mDateSelector->GetCount() && dates.GetCount() == i + 1)) {
				mDateSelector->SetSelection(mDateSelector->GetCount() - 1);
				wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, DATE);
				AddPendingEvent(event); //trigger repopulating of timecode control
			}
		}
	}
	else { //no dates for selected project
		mDateSelector->Append(wxT("NO DATES"));
		mDateSelector->Select(0);
		mDateSelector->Disable();
	}
	if (!mDateSelector->IsEnabled() || wxNOT_FOUND == mDateSelector->GetSelection()) { //no date selected
		//can select no recordings so clear
		mRecordingSelector->Clear();
		mRecordingSelector->Disable();
		mUpButton->Disable();
		mDownButton->Disable();
		mOnlineMessage->Hide();
		mPreferOnline->SetBackgroundColour(wxNullColour);
		FindWindow(wxID_OK)->Disable();
	}
}

/// Populates the recording selector with available recordings for this project and date.
/// If previously-selected recording is found, or only one recording is found, selects this and enables thmPreferOnline->SetBackgroundColour(e OK button.
void SelectRecDlg::OnSelectDate(wxCommandEvent & WXUNUSED(event))
{
	mSelectedDate = ((StringContainer *) mDateSelector->GetClientObject(mDateSelector->GetSelection()))->GetString();
	mRecordingSelector->Clear(); //repopulating it in case directory structure has changed
	wxArrayString paths; //to allow sorting
	wxDir::GetAllFiles(wxString(RECORDING_SERVER_ROOT) + wxFileName::GetPathSeparator() + OFFLINE_SUBDIR + wxFileName::GetPathSeparator() + mSelectedProject + wxFileName::GetPathSeparator() + mSelectedDate, &paths, wxEmptyString, wxDIR_FILES); //produces rubbish if ROOT_DIR not turned into a wxString
	if (paths.GetCount()) {
		paths.Sort(); //into time order
		mRecordingSelector->Enable();
		long value;
		for (size_t i = 0; i < paths.GetCount(); i++) {
			wxString file = paths[i].Mid(paths[i].Find(wxFileName::GetPathSeparator(), true) + 1); //remove path
			if (file.Left(8) == mSelectedDate //first 8 digits (date)
			 && file[8] == wxT('_') //following underscore
			 && wxNOT_FOUND == file.Mid(9, 8).Find(wxT("-")) && file.Mid(9, 8).ToLong(&value) //next 8 digits (time)
			 && file[17] == wxT('_') //following underscore
			 && 0 == file.Mid(18).Find(mSelectedProject + wxT("_")) //project name and following underscore
			 && !file.Right(4).CmpNoCase(wxT(".mxf")) //extension
			) { //it's a valid filename
				wxString recording = file.Mid(9, 2) + wxT(":") + file.Mid(11, 2) + wxT(":") + file.Mid(13, 2) + wxT(":") + file.Mid(15, 2);// + (file.Right(6)[0] == wxT('a') ? wxT(" (Audio only)") : wxEmptyString);
				if (!mRecordingSelector->GetCount() || mRecordingSelector->GetString(mRecordingSelector->GetCount() - 1).Left(11) != recording.Left(11)) { //new recording
					mRecordingSelector->Append(recording + (file.Right(6)[0] == wxT('a') ? wxT(" (Audio only)") : wxEmptyString)); //subsequently found video tracks will overwrite the "audio only" indication
				}
				else if (file.Right(6)[0] != wxT('a')) { //already added this recording, and this is a video track
					mRecordingSelector->SetString(mRecordingSelector->GetCount() - 1, recording); //it might have been marked audio only
				}
				//maintain the selection if this was the previously selected recording
				if (mSelectedRecording == recording) { //the previously selected recording still exists
					//maintain the selection
					mRecordingSelector->SetSelection(mRecordingSelector->GetCount() - 1);
					wxCommandEvent event(wxEVT_COMMAND_LISTBOX_SELECTED, RECORDING);
					AddPendingEvent(event); //trigger repopulating of timecode control
				}
			}
		}
		//if only a single recording, select it for convenience
		if (1 == mRecordingSelector->GetCount() && wxNOT_FOUND == mRecordingSelector->GetSelection()) {
			mRecordingSelector->SetSelection(0);
			wxCommandEvent event(wxEVT_COMMAND_LISTBOX_SELECTED, RECORDING);
			AddPendingEvent(event); //trigger repopulating of date control
		}
	}
	else {
		mRecordingSelector->Append(wxT("NO RECORDINGS"));
		mRecordingSelector->Select(0);
		mRecordingSelector->Disable();
	}
	if (!mRecordingSelector->IsEnabled() || wxNOT_FOUND == mRecordingSelector->GetSelection()) { //no recording selected
		//can select no recordings so clear
		mUpButton->Disable();
		mDownButton->Disable();
		mOnlineMessage->Hide();
		mPreferOnline->SetBackgroundColour(wxNullColour);
		FindWindow(wxID_OK)->Disable();
	}
}

/// Sets state of up and down buttons, OK button, background colour of PreferOnline button and visibility of online message depending on the recording selected.
void SelectRecDlg::OnSelectRecording(wxCommandEvent & WXUNUSED(event))
{
	if (wxNOT_FOUND == mRecordingSelector->GetSelection()) { //deselecting - this is fairly pointless so could be disabled but would require control to be subclassed
		FindWindow(wxID_OK)->Disable();
		mUpButton->Disable();
		mDownButton->Disable();
		mOnlineMessage->Hide();
		mPreferOnline->SetBackgroundColour(wxNullColour);
	}
	else {
		mSelectedRecording = mRecordingSelector->GetString(mRecordingSelector->GetSelection()).Left(11);
		FindWindow(wxID_OK)->Enable();
		mUpButton->Enable(mRecordingSelector->GetSelection());
		mDownButton->Enable((unsigned int) mRecordingSelector->GetSelection() != mRecordingSelector->GetCount() - 1);
		//see if online is available
		wxArrayString dummy;
		GetPaths(dummy, true);
		if (dummy.GetCount()) {
			mOnlineMessage->Show(dummy.GetCount());
			Layout(); //needed to put messge in the right place
		}
		mPreferOnline->SetBackgroundColour((mPreferOnline->GetValue() && mOnlineMessage->IsShown()) ? BUTTON_WARNING_COLOUR : wxNullColour); //highlight button if prefer online and online file(s) are present
	}
}

/// Closes the dialogue as if the OK button had been pressed.
void SelectRecDlg::OnDoubleClick(wxCommandEvent & WXUNUSED(event))
{
	EndModal(wxID_OK);
}

/// Selects previous or next recording in the list, if possible.
void SelectRecDlg::OnNavigateRecordings(wxCommandEvent & event)
{
	//check that we can move in the requested direction (as this might be called from an accelerator key, which is always enabled)
	if (mRecordingSelector->GetCount()
	 && ((UP == event.GetId() && mRecordingSelector->GetSelection() > 0) //want to go up and not at the top
	  || (DOWN  == event.GetId() && mRecordingSelector->GetSelection() != (int) mRecordingSelector->GetCount() - 1) //want to go down and not at the bottom
	)) {
		mRecordingSelector->Select(mRecordingSelector->GetSelection() + (UP == event.GetId() ? -1 : 1));
		wxCommandEvent newEvent(wxEVT_COMMAND_LISTBOX_SELECTED, RECORDING);
		AddPendingEvent(newEvent);
	}
}

/// Sets the background colour of the PreferOnline button depending on its state and if online material is available.
void SelectRecDlg::OnPreferOnline(wxCommandEvent & WXUNUSED(event))
{
	mPreferOnline->SetBackgroundColour((mPreferOnline->GetValue() && mOnlineMessage->IsShown()) ? BUTTON_WARNING_COLOUR : wxNullColour); //highlight button if prefer online and online file(s) are present
}

/// Gets the paths of the files of the selected recording.
/// @param paths Returns the full path of each file, unsorted.
/// @param selectOnline Forces online files to be returned; otherwise, online files are returned if PreferOnline button is pressed and at least one is available.
void SelectRecDlg::GetPaths(wxArrayString & paths, bool selectOnline)
{
	wxString stem = wxString(RECORDING_SERVER_ROOT) + wxFileName::GetPathSeparator() + ((selectOnline || BUTTON_WARNING_COLOUR == mPreferOnline->GetBackgroundColour()) ? ONLINE_SUBDIR : OFFLINE_SUBDIR) + wxFileName::GetPathSeparator() + mSelectedProject + wxFileName::GetPathSeparator() + mSelectedDate;
	wxDir::GetAllFiles(stem, &paths, wxEmptyString, wxDIR_FILES); //all files in the recording's directory
	stem += wxFileName::GetPathSeparator() + mSelectedDate + wxT("_") + mSelectedRecording.Left(2) + mSelectedRecording.Mid(3, 2) + mSelectedRecording(6, 2) + mSelectedRecording(9, 2) + wxT("_") + mSelectedProject + wxT("_"); //common stem for all files in the wanted recording
	for (size_t i = 0; i < paths.GetCount(); i++) {
		if (paths[i].Find(stem) || paths[i].Right(4).CmpNoCase(wxT(".mxf"))) { //doesn't match
			paths.RemoveAt(i);
			i--; //next value moved to this position
		}
	}
}
