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

#include "dialogues.h"
#include "help.h" //for StyleAndWrite()
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>
#include <wx/colordlg.h>

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
	mProjectList = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL); //ListBox is nicer but can't scroll it to show a new project name
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
			wxMessageBox(wxT("There is already a project named \"") + mProjectList->GetString(mProjectList->FindString( name)) + wxT("\"."), wxT("Name Clash"), wxICON_HAND); //prints existing project name (may have case differences)
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
	EVT_TEXT_ENTER(wxID_ANY, JumpToTimecodeDlg::OnEnter)
	EVT_SET_FOCUS(JumpToTimecodeDlg::OnFocus)
	EVT_KILL_FOCUS(JumpToTimecodeDlg::OnFocus)
END_EVENT_TABLE()

JumpToTimecodeDlg::JumpToTimecodeDlg(wxWindow * parent) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Jump to Timecode"))
{
	wxBoxSizer * timecodeSizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(timecodeSizer);
	wxFont * font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
	mHours = new MyTextCtrl(this, HRS, wxT("00"), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
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
	mMins = new MyTextCtrl(this, MINS, wxT("00"), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	mMins->SetFont(*font);
	mMins->SetClientSize(size);
	mMins->SetMaxLength(2);
	timecodeSizer->Add(mMins, 0, wxFIXED_MINSIZE);
	wxStaticText * minsSep = new wxStaticText(this, wxID_ANY, wxT(":"));
	minsSep->SetFont(*font);
	timecodeSizer->Add(minsSep);
	mSecs = new MyTextCtrl(this, SECS, wxT("00"), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	mSecs->SetFont(*font);
	mSecs->SetClientSize(size);
	mSecs->SetMaxLength(2);
	timecodeSizer->Add(mSecs, 0, wxFIXED_MINSIZE);
	wxStaticText * secsSep = new wxStaticText(this, wxID_ANY, wxT(":"));
	secsSep->SetFont(*font);
	timecodeSizer->Add(secsSep);
	mFrames = new MyTextCtrl(this, FRAMES, wxT("00"), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	mFrames->SetFont(*font);
	mFrames->SetClientSize(size);
	mFrames->SetMaxLength(2);
	timecodeSizer->Add(mFrames, 0, wxFIXED_MINSIZE);
//	hours->SetClientSize(size);
//	hours->Layout();
//	hours->SetInitialSize(wxSize(hours->GetCharWidth()*2 + 10, hours->GetCharHeight()));
//	SetFont(*font);
//	hours->SetInitialSize(size);
//	hours->SetClientSize(size);
//	hours->SetSize(size);

	ShowModal();
}

void JumpToTimecodeDlg::OnTextChange(wxCommandEvent & event)
{
	unsigned int limit;
	switch (event.GetId()) {
		case HRS:
			limit = 23;
			break;
		case MINS: case SECS:
			limit = 59;
			break;
		default:
			limit = 24; //FIXME
			break;
	}
	int value = CalcValue(event, limit);
	MyTextCtrl * ctrl = (MyTextCtrl *) event.GetEventObject();
	if (-1 == value) { //illegal value
		//remove the last character entered
		ctrl->SetValue(ctrl->GetValue().Left(ctrl->GetInsertionPoint()));
	}
	else if (2 == ctrl->GetValue().Len() && FRAMES != event.GetId()) { //full field
		//move to the next field
		ctrl->Navigate();
	}
}

int JumpToTimecodeDlg::CalcValue(wxCommandEvent & event, unsigned int limit)
{
	const wxString digits(wxT("0123456789"));
	const wxString text = ((MyTextCtrl *) event.GetEventObject())->GetValue();
	int digit1, digit2;
	digit1 = digit2 = 0;
	if (!text.Len() || wxNOT_FOUND != (digit1 = digits.Find(text[0]))) { //either empty or first char is a digit
		if (text.Len() < 2 || wxNOT_FOUND != (digit2 = digits.Find(text[1]))) { //either <2 chars or second char is a digit
			unsigned int total = digit1 * (text.Len() == 2 ? 10 : 1) + digit2;
			if (total <= limit) {
//std :: cerr << digit1 * (text.Len() == 2 ? 10 : 1) + digit2 << std::endl;
				return total;
			}
		}
	}
	return -1;
}

void JumpToTimecodeDlg::OnFocus(wxFocusEvent & event)
{
return;
	if (wxEVT_KILL_FOCUS == event.GetEventType()) {
		switch (event.GetId()) {
			case HRS: case MINS: case SECS: case FRAMES:
				((MyTextCtrl *) event.GetEventObject())->SetValue(wxString(wxT("00") + ((MyTextCtrl *) event.GetEventObject())->GetValue().Left(2)));
				break;
			default:
				break;
		}
	}
}

void JumpToTimecodeDlg::OnEnter(wxCommandEvent & WXUNUSED(event))
{
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
	gridSizer->AddStretchSpacer();
	mRunButton = new wxToggleButton(this, RUN, wxT("Run"));
	gridSizer->Add(mRunButton);
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
	}
}

/// Responds to the timer.
void TestModeDlg::OnTimer(wxTimerEvent & WXUNUSED(event))
{
	Record(!mRecording);
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
			int range = mMaxRecTime->GetValue() - mMinRecTime->GetValue() + 1;
			while ((dur = rand()/(RAND_MAX/range)) > range); //avoid occasional truncation to range
			dur += mMinRecTime->GetValue();
			mTimer->Start(dur * 60 * 1000, wxTIMER_ONE_SHOT);
		}
		else {
			int range = mMaxGapTime->GetValue() - mMinGapTime->GetValue() + 1;
			while ((dur = rand()/(RAND_MAX/range)) > range); //avoid occasional truncation to range
			dur += mMinGapTime->GetValue();
			mTimer->Start(dur * 1000, wxTIMER_ONE_SHOT);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define N_COLOURS 9 //including default - make sure this matches number of entries in array below
const struct {char colour[8]; char labelColour[8]; char label[8]; ProdAuto::LocatorColour::EnumType code;} colours[9] = {
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
		entries[entry].Set(wxACCEL_NORMAL, (int) '0' + entry, wxID_HIGHEST + N_COLOURS + 2 + entry); //ID corresponds to key so we can find out which key is pressed
	}
	entries[10].Set(wxACCEL_NORMAL, WXK_F2, wxID_HIGHEST + N_COLOURS + 1);
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
		 && colourVal < N_COLOURS) { //...in the right range
			shortcutVal = (shortcutVal == 0) ? 9 : shortcutVal - 1; //row number
			mGrid->SetCellValue(shortcutVal, 0, cuePointNode->GetNodeContent().Trim(false).Trim(true));
			SetColour(shortcutVal, colourVal);
		}
		cuePointNode = cuePointNode->GetNext();
	}
	mGrid->AutoSizeColumns();
	Fit();
}

/// Shows the dialogue and saves the grid if OK is pressed.
/// @param timecode If present, displays this at the top of the window and goes into add cue mode - shortcut keys enabled.
int CuePointsDlg::ShowModal(const wxString timecode)
{
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
	int rc;
	if (wxID_OK == (rc = wxDialog::ShowModal())) {
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
	else {
		//reload old state
		Load();
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

/// Depending on the ID, sets a new locator colour (i.e. called from the pop-up colour menu), or, if enabled to do so, closes the dialogue, setting the selected locator accordingly (i.e. called with a shortcut key)
/// @param event Contains the menu ID.
void CuePointsDlg::OnMenu(wxCommandEvent & event)
{
	if (event.GetId() < wxID_HIGHEST + 1 + N_COLOURS) { //a colour menu selection
		SetColour(mCurrentRow, event.GetId() - wxID_HIGHEST - 1);
		//Move the grid cursor away from the right hand column as it can't be "edited" as such
		wxCommandEvent event(wxEVT_SET_GRID_ROW, mCurrentRow);
		AddPendingEvent(event);
	}
	else if (mTextColour == mMessage->GetForegroundColour() && mTimecodeDisplay->IsShown()) { //shortcuts enabled
		if (wxID_HIGHEST + N_COLOURS + 1 == event.GetId()) { //function key pressed
			//return a blank event
			mDescription.Clear();
			mColour = 0;
		}
		else {
			int row = event.GetId() == wxID_HIGHEST + N_COLOURS + 2 ? 9 : event.GetId() - N_COLOURS - wxID_HIGHEST - 3;
			mDescription = mGrid->GetCellValue(row, 0).Trim(false).Trim(true);
			mColour = mDescrColours[row];
		}
		EndModal(wxID_OK);
	}
	else { //editing
		event.Skip();
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
		for (unsigned int i = 0; i < N_COLOURS; i++) {
			colourMenu.Append(wxID_HIGHEST + 1 + i, wxString(colours[i].label, *wxConvCurrent));
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
	mGrid->SetCellValue(row, 1, wxString(colours[colour].label, *wxConvCurrent));
	mGrid->SetCellBackgroundColour(row, 1, wxString(colours[colour].colour, *wxConvCurrent));
	mGrid->SetCellTextColour(row, 1, wxString(colours[colour].labelColour, *wxConvCurrent));
	mDescrColours[row] = colour;
}

/// Returns the currently selected locator's description
const wxString CuePointsDlg::GetDescription()
{
	return mDescription;
}

/// Returns the currently selected locator's colour code
const ProdAuto::LocatorColour::EnumType CuePointsDlg::GetColourCode()
{
	return colours[mColour].code;
}

/// Returns the currently selected locator's displayed colour
const wxColour CuePointsDlg::GetColour()
{
	return wxColour(wxString(colours[mColour].colour, *wxConvCurrent));
}

/// Returns the currently selected locator's displayed label text colour
const wxColour CuePointsDlg::GetLabelColour()
{
	return wxColour(wxString(colours[mColour].labelColour, *wxConvCurrent));
}
