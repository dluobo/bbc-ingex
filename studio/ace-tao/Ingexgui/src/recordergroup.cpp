/***************************************************************************
 *   $Id: recordergroup.cpp,v 1.6 2009/01/29 07:36:58 stuart_hc Exp $       *
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

#include "recordergroup.h"
#include "controller.h"
#include "comms.h"
#include "ingexgui.h" //for consts
#include <wx/xml/xml.h>

DEFINE_EVENT_TYPE(wxEVT_RECORDERGROUP_MESSAGE)

BEGIN_EVENT_TABLE( RecorderGroupCtrl, wxListBox )
	EVT_LEFT_DOWN(RecorderGroupCtrl::OnLMouseDown)
	EVT_MIDDLE_DOWN(RecorderGroupCtrl::OnUnwantedMouseDown)
	EVT_RIGHT_DOWN(RecorderGroupCtrl::OnRightMouseDown)
	EVT_COMMAND(ENABLE_REFRESH, wxEVT_RECORDERGROUP_MESSAGE, RecorderGroupCtrl::OnListRefreshed)
	EVT_CONTROLLER_THREAD(RecorderGroupCtrl::OnControllerEvent)
END_EVENT_TABLE()

/// Initialises list with no recorders and creates Comms object for communication with the name server.
/// @param parent The parent window.
/// @param id The control's window ID.
/// @param pos The control's position.
/// @param size The control's size.
/// @param argc Argument count, for ORB initialisation.
/// @param argv Argument vector, for ORB initialisation.
/// @param doc The saved state.
RecorderGroupCtrl::RecorderGroupCtrl(wxWindow * parent, wxWindowID id, const wxPoint & pos, const wxSize & size, int argc, wxChar** argv, const wxXmlDocument * doc) : wxListBox(parent, id, pos, size, 0, 0, wxLB_MULTIPLE), mEnabledForInput(true), mPreroll(InvalidMxfDuration), mDoc(doc)
{
	SetNextHandler(parent);
	Insert(wxT(""), 0); //make sure something's in the control...
	SetMinSize(GetEffectiveMinSize()); //...so that it won't shrink if Comms doesn't start up or there are no recorders initially
	mComms = new Comms(this, argc, argv);
	wxString dummy;
	if (mComms->GetStatus(dummy)) { //started properly
		StartGettingRecorders();
	}
	else {
		EnableForInput(false);
	}
}

RecorderGroupCtrl::~RecorderGroupCtrl()
{
	delete mComms;
}

/// Starts the process of obtaining a list of recorders from the name server, in another thread.
/// Stops clicks on the control to prevent recorders being selected which might disappear.
void RecorderGroupCtrl::StartGettingRecorders()
{
	wxString dummy;
	if (IsEmpty()) {
		//show that something's happening
		Insert(wxT("Getting list..."), 0); //this will be removed when the list has been obtained
	}
	EnableForInput(false);
	mComms->StartGettingRecorders(wxEVT_RECORDERGROUP_MESSAGE, ENABLE_REFRESH); //threaded, so won't hang app; sends the given event when it's finished
}

/// Enables or disables the control for input, by sending an event to the parent
/// @param enable The requested state
void RecorderGroupCtrl::EnableForInput(const bool enable)
{
	if (mEnabledForInput != enable) {
		mEnabledForInput = enable;
		wxCommandEvent event(wxEVT_RECORDERGROUP_MESSAGE, enable ? ENABLE_REFRESH : DISABLE_REFRESH);
		GetNextHandler()->AddPendingEvent(event);
	}
}

/// Responds to Comms finishing obtaining a list of recorders.
/// Updates the list, removing recorders which weren't found and which aren't connected, and adding new recorders.
/// @param event The command event.
void RecorderGroupCtrl::OnListRefreshed(wxCommandEvent & WXUNUSED(event))
{
	EnableForInput();
	//Remove all the recorders which aren't connected/connecting (and the holding message plus maybe initial padding entry if present)
	for (unsigned int i = 0; i < GetCount(); i++) {
		if (!GetController(i)) {
			Delete(i--);
		}
	}
	wxString errMsg;
	if (mComms->GetStatus(errMsg)) { //OK
		wxArrayString names;
		mComms->GetRecorderList(names);
		//Put in any recorders not listed, in alphabetical order
		for (size_t j = 0; j < names.GetCount(); j++) {
			unsigned int k = 0;
			do {
				if (k == GetCount() || GetName(k).Cmp(names[j]) > 0) { //reached end of list, or gone past this recorder, so recorder not listed
					//add to list
					Insert(names[j], k);
					break;
				}
				if (GetName(k) == names[j]) { //recorder already listed
					break;
				}
				k++;
			} while (true);
		}
	}
	else { //CORBA prob
		wxMessageDialog dlg(this, wxT("Failed to get list of recorders:\n") + errMsg, wxT("Comms problem"), wxICON_ERROR | wxOK); //NB not using wxMessageBox because (in GTK) it doesn't stop the parent window from being selected, so it can end up hidden, making the app appear to have hanged
		dlg.ShowModal();
	}
}

/// Inserts an item in the list.
/// Creates a corresponding ControllerContainer (but not a controller).
/// @param name The recorder name.
/// @param pos The position to insert the recorder.
void RecorderGroupCtrl::Insert(const wxString & name, unsigned int pos)
{
	//At least one item must have client data or GetClientData will fail, so give the item client data even if there is no controller (disconnected)
	ControllerContainer * container = new ControllerContainer(name); //deleted when list item deleted
	wxListBox::Insert(name, pos, container);
}

/// Gets the name of the recorder at the given position.
/// @param index The position of the recorder name to return.
/// @return The recorder name.
const wxString RecorderGroupCtrl::GetName(unsigned int index)
{
	return ((ControllerContainer *) GetClientObject(index))->GetName();
}

/// Starts the process of connecting to a recorder, in another thread, creating a controller.
/// @param index The position in the list of the recorder to connect to.
void RecorderGroupCtrl::Connect(unsigned int index)
{
	((ControllerContainer *) GetClientObject(index))->Start(mComms, this);
}

/// Deselects the given recorder and disconnects from it.
/// Informs the frame of the disconnection.
/// Re-calculates preroll and postroll limits.
/// Looks for another recorder to get timecode from if this was the one being used.
/// @param index The position in the list of the recorder to disconnect from.
void RecorderGroupCtrl::Deselect(unsigned int index)
{
	wxListBox::Deselect(index);
	Disconnect(index);
	//depopulate the source tree
	wxCommandEvent event(wxEVT_RECORDERGROUP_MESSAGE, REMOVE_RECORDER);
	event.SetString(GetString(index));
	AddPendingEvent(event);
	//preroll and postroll limits might have been relaxed
	unsigned int i;
	for (i = 0; i < GetCount(); i++) {
		if (GetController(i) && GetController(i)->IsOK()) {
			mMaxPreroll = GetController(i)->GetMaxPreroll();
			mMaxPostroll = GetController(i)->GetMaxPostroll();
			break;
		}
	}
	for (; i < GetCount(); i++) {
		if (GetController(i) && GetController(i)->IsOK()) {
			mMaxPreroll.samples = GetController(i)->GetMaxPreroll().samples < mMaxPreroll.samples ? GetController(i)->GetMaxPreroll().samples : mMaxPreroll.samples;
			mMaxPostroll.samples = GetController(i)->GetMaxPostroll().samples < mMaxPostroll.samples ? GetController(i)->GetMaxPostroll().samples : mMaxPostroll.samples;
		}
	}
}

/// Disconnects from a recorder, by deleting its controller object.
/// Looks for another recorder to get timecode from if this was the one being used.
/// @param index The position in the list of the recorder to disconnect from.
void RecorderGroupCtrl::Disconnect(unsigned int index)
{
	if (GetString(index) == mTimecodeRecorder) { //removing the recorder we're using to display timecode
		//start looking for a new one
		SetTimecodeRecorder();
	}
	((ControllerContainer *) GetClientObject(index))->Stop();
}

/// Gets the controller of the given recorder - 0 if it doesn't exist.
/// @param index The position in the list of the wanted recorder.
Controller * RecorderGroupCtrl::GetController(unsigned int index)
{
	return ((ControllerContainer *) GetClientObject(index))->GetController();
}

/// Responds to a left mouse click on the control to select or deselect a recorder.
/// Have to intercept the mouse click event in order to allow an unmodified click to deselect the recorder.
/// If selecting, start the connection process in another thread and put a holding message in the list.
/// If deselecting, send an event to the parent and adjust maximum preroll and postroll values to the constraints imposed by the remaining recorders.
/// @param event The mouse event.
void RecorderGroupCtrl::OnLMouseDown(wxMouseEvent & event)
{
	int clickedItem;
	if (mEnabledForInput && wxNOT_FOUND != (clickedItem = HitTest(event.GetPosition()))) {
		if (IsSelected(clickedItem)) { //disconnect from a recorder
			wxMessageDialog dlg(this, wxT("Are you sure you want to disconnect from ") + GetString(clickedItem) + wxT("?"), wxT("Confirmation of Disconnect"), wxYES_NO | wxICON_QUESTION);
			if (wxID_YES == dlg.ShowModal()) {
				Deselect(clickedItem);
			}
		}
		else if (!GetController(clickedItem)) { //connect to a recorder
			SetString(clickedItem, wxT("Connecting..."));
			Connect(clickedItem);
		}
	}
}

/// Responds to unwanted mouse clicks on the control by preventing them doing anything
/// @param event The mouse event.
void RecorderGroupCtrl::OnUnwantedMouseDown(wxMouseEvent & WXUNUSED(event))
{
}

/// Responds to right mouse click on the control by deselecting the item without confirmation
/// @param event The mouse event.
void RecorderGroupCtrl::OnRightMouseDown(wxMouseEvent & event)
{
	int clickedItem;
	if (mEnabledForInput && wxNOT_FOUND != (clickedItem = HitTest(event.GetPosition())) && IsSelected(clickedItem)) {
		Deselect(clickedItem);
	}
}

/// Responds to an event from one of the recorder controllers.
/// If a comms failure, send an event to that effect.
/// If just connected to a recorder, do a sanity check on the recorder parameters, send an event to the parent and adjust max preroll and postroll to accommodate this recorder's values.
/// If recording or stopping, send an event to the parent.
/// If status information, send events if timecode status has changed, and try to use this recorder to supply displayed timecode if there currently isn't one.
/// If this recorder is supplying displayed timecode, send an event with the timecode.
/// Send a track status event.
/// @param event The controller thread event.
void RecorderGroupCtrl::OnControllerEvent(ControllerThreadEvent & event)
{
	if (Controller::DIE == event.GetCommand()) {
		//find the entry in the list
		unsigned int pos;
		for (pos = 0; pos < GetCount(); pos++) {
			if (event.GetName() == GetName(pos)) {
				break;
			}
		}
		if (pos < GetCount() && GetController(pos)) { //sanity checks
			((ControllerContainer *) GetClientObject(pos))->Del();
			SetString(pos, event.GetName());
		}
	}
	else if (Controller::CONNECT == event.GetCommand()) {
		//complete connection process if successful, or reset and report failure

		//find the entry in the list
		unsigned int pos;
		for (pos = 0; pos < GetCount(); pos++) {
			if (event.GetName() == GetName(pos)) {
				break;
			}
		}
		if (pos < GetCount() && GetController(pos)) { //sanity checks
			if (Controller::SUCCESS == event.GetResult()) {
				//check edit rate compatibility
				wxArrayInt selectedItems;
				if (!GetSelections(selectedItems) || (GetController(pos)->GetMaxPreroll().edit_rate.numerator == mMaxPreroll.edit_rate.numerator && GetController(pos)->GetMaxPreroll().edit_rate.denominator == mMaxPreroll.edit_rate.denominator)) { //the only recorder, or compatible edit rate (assume MaxPostroll has same edit rate)
					//everything about the recorder is now checked
					Select(pos);
					//populate the source tree
					wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, NEW_RECORDER);
					frameEvent.SetString(event.GetName());
					frameEvent.SetInt(pos); //to allow quick disconnection
					RecorderData * recorderData = new RecorderData(event.GetTrackStatusList(), event.GetTrackList()); //must be deleted by event handler
					frameEvent.SetClientData(recorderData);
					frameEvent.SetInt(GetController(pos)->IsRouterRecorder());
					AddPendingEvent(frameEvent);
					//preroll and postroll
					if (!selectedItems.GetCount()) { //the only recorder
						mMaxPreroll = GetController(pos)->GetMaxPreroll();
						mPreroll = mMaxPreroll; //for the edit rate values
						wxXmlNode * node = mDoc->GetRoot()->GetChildren();
						while (node && node->GetName() != wxT("Preroll")) {
							node = node->GetNext();
						}
						unsigned long samples;
						if (node && node->GetChildren() && node->GetChildren()->GetContent().Len() && node->GetChildren()->GetContent().ToULong(&samples)) {
							mPreroll.samples = samples;
						}
						else {
							mPreroll.samples = DEFAULT_PREROLL;
						}
						mMaxPostroll = GetController(pos)->GetMaxPostroll();
						mPostroll = mMaxPostroll; //for the edit rate values
						node = mDoc->GetRoot()->GetChildren();
						while (node && node->GetName() != wxT("Postroll")) {
							node = node->GetNext();
						}
						if (node && node->GetChildren() && node->GetChildren()->GetContent().Len() && node->GetChildren()->GetContent().ToULong(&samples)) {
							mPostroll.samples = samples;
						}
						else {
							mPostroll.samples = DEFAULT_POSTROLL;
						}
					}
					if (GetController(pos)->GetMaxPreroll().samples < mMaxPreroll.samples) {
						//limit mMaxPreroll (and possibly mPreroll) to new maximum
						mMaxPreroll.samples = GetController(pos)->GetMaxPreroll().samples;
						mPreroll.samples = mPreroll.samples > GetController(pos)->GetMaxPreroll().samples ? GetController(pos)->GetMaxPreroll().samples : mPreroll.samples;
					}
					if (GetController(pos)->GetMaxPostroll().samples < mMaxPostroll.samples) {
						//limit mMaxPostroll (and possibly mPostroll) to new maximum
						mMaxPostroll.samples = GetController(pos)->GetMaxPostroll().samples;
						mPostroll.samples = mPostroll.samples > GetController(pos)->GetMaxPostroll().samples ? GetController(pos)->GetMaxPostroll().samples : mPostroll.samples;
					}
					//synchronise project names
					CORBA::StringSeq_var strings = event.GetStrings();
					wxSortedArrayString namesToSend = mProjectNames;
					for (size_t i = 0; i < strings->length(); i++) {
						wxString projectName = wxString((*strings)[i], *wxConvCurrent);
						int index = namesToSend.Index(projectName);
						if (wxNOT_FOUND == index) { //new name for the controller
							mProjectNames.Add(projectName);
						}
						else { //not a new name for the recorder
							namesToSend.RemoveAt(index);
						}
					}
					if (namesToSend.GetCount()) {
						CORBA::StringSeq CorbaNames;
						CorbaNames.length(namesToSend.GetCount());
						// Assignment to the CORBA::StringSeq element must be from a const char *
						// and should use ISO Latin-1 character set.
						for (size_t i = 0; i < namesToSend.GetCount(); i++) {
							CorbaNames[i] = (const char *) namesToSend[i].mb_str(wxConvISO8859_1);
						}
						GetController(pos)->AddProjectNames(CorbaNames);
					}
				}
				else { //edit rate incompatibility
					wxMessageDialog dlg(this, wxT("Recorder \"") + event.GetName() + wxString::Format(wxT("\" has an edit rate incompatible with the existing recorder%s.  Deselecting "), selectedItems.GetCount() == 1 ? wxT("") : wxT("s")) + event.GetName() + wxString::Format(wxT(".\n\nEdit rate numerator: %d ("), GetController(pos)->GetMaxPreroll().edit_rate.numerator) + event.GetName() + wxString::Format(wxT("); %d (existing)\nEdit rate denominator: %d ("), mMaxPreroll.edit_rate.numerator, GetController(pos)->GetMaxPreroll().edit_rate.denominator) + event.GetName() + wxString::Format(wxT("); %d (existing)"), mMaxPreroll.edit_rate.denominator), wxT("Edit rate incompatibility"), wxICON_EXCLAMATION | wxOK);
					dlg.ShowModal();
					Disconnect(pos);
				}
			}
			else { //failure or comm failure
				Disconnect(pos);
				wxMessageDialog dlg(this, wxT("Couldn't connect to recorder \"") + event.GetName() + wxT("\": ") + event.GetMessage(), wxT("Initialisation failure"), wxICON_EXCLAMATION | wxOK);
				dlg.ShowModal();
			}
			SetString(pos, event.GetName());
		}
	}
	if (GetController(FindString(event.GetName(), true))) { //not been disconnected earlier in this function due to a reconnect failure, or has been destroyed (which can still result in an event being sent)
		if (Controller::COMM_FAILURE == event.GetResult()) {
			wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, COMM_FAILURE);
			frameEvent.SetString(event.GetName());
			AddPendingEvent(frameEvent);
		}
		else {
			switch (event.GetCommand()) {
				case Controller::RECONNECT :
					if (Controller::FAILURE == event.GetResult()) {
						Deselect(FindString(event.GetName(), true)); //informs frame
						wxMessageDialog dlg(this, wxT("Cannot reconnect automatically to ") + event.GetName() + wxT(" because ") + event.GetMessage() + wxT(".  Other aspects of this recorder may have changed also.  You must connect to it again manually and be aware that it has changed."), wxT("Cannot reconnect"), wxICON_ERROR | wxOK);
						dlg.ShowModal();
					}
					break;
				case Controller::RECORD : {
					wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, RECORDING);
					frameEvent.SetString(event.GetName());
					frameEvent.SetInt (Controller::SUCCESS == event.GetResult());
					RecorderData * recorderData = new RecorderData(event.GetTimecode()); //must be deleted by event handler
					frameEvent.SetClientData(recorderData);
					AddPendingEvent(frameEvent);
					break;
				}
				case Controller::STOP : {
					wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, STOPPED);
					frameEvent.SetString(event.GetName());
					frameEvent.SetInt (Controller::SUCCESS == event.GetResult());
					RecorderData * recorderData = new RecorderData(event.GetTrackList(), event.GetStrings(), event.GetTimecode()); //must be deleted by event handler
					frameEvent.SetClientData(recorderData);
					AddPendingEvent(frameEvent);
					break;
				}
				default : // NONE (never appears), CONNECT (handled above), STATUS (handled below), DIE (never appears)
					break;
			}
		}
		if (event.GetNTracks()) { //have status
			//timecode
			if (event.TimecodeStateHasChanged()) {
				wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE);
				frameEvent.SetString(event.GetName());
				switch (event.GetTimecodeState()) {
					case Controller::RUNNING : case Controller::UNCONFIRMED :
						frameEvent.SetId(TIMECODE_RUNNING);
						break;
					case Controller::STUCK : {
						frameEvent.SetId(TIMECODE_STUCK);
						RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
						frameEvent.SetClientData(recorderData);
						break;
					}
					case Controller::ABSENT :
						frameEvent.SetId(TIMECODE_MISSING);
						break;
				}
				AddPendingEvent(frameEvent);
			}
			//timecode display
			if (Controller::ABSENT != event.GetTimecodeState() //we have some timecode
			 &&
			  (mTimecodeRecorder.IsEmpty() //not getting timecode from anywhere at the moment
			   || !GetController(FindString(mTimecodeRecorder, true)) //sanity check, and recorder we're getting timecode from doesn't exist
			   || (GetController(FindString(mTimecodeRecorder, true))->IsRouterRecorder() && !GetController(FindString(event.GetName(), true))->IsRouterRecorder()))) { //getting timecode from a router recorder at the moment, and this recorder is not a router recorder
				//set the display to this recorder
				SetTimecodeRecorder(event.GetName());
				//set the timecode value, running or stuck
				mTimecodeRecorderStuck = Controller::STUCK == event.GetTimecodeState();
				wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, mTimecodeRecorderStuck ? DISPLAY_TIMECODE_STUCK : DISPLAY_TIMECODE);
				RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
				frameEvent.SetClientData(recorderData);
				AddPendingEvent(frameEvent);
			}
			else if (event.GetName() != mTimecodeRecorder && mTimecodeRecorderStuck && Controller::RUNNING == event.GetTimecodeState()) { //different recorder but timecode's running and current recorder is stuck
				//set the display to this recorder
				SetTimecodeRecorder(event.GetName());
				//set the timecode value, running
				mTimecodeRecorderStuck = false;
				wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE);
				RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
				frameEvent.SetClientData(recorderData);
				AddPendingEvent(frameEvent);
			}
			else if (event.GetName() == mTimecodeRecorder && event.TimecodeStateHasChanged()) { //change to status of current display recorder
				if (Controller::ABSENT == event.GetTimecodeState()) {
					//display no source
					SetTimecodeRecorder();
				}
				else {
					//set the timecode value, running or stuck
					mTimecodeRecorderStuck = Controller::STUCK == event.GetTimecodeState();
					wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, mTimecodeRecorderStuck ? DISPLAY_TIMECODE_STUCK : DISPLAY_TIMECODE);
					RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
					frameEvent.SetClientData(recorderData);
					AddPendingEvent(frameEvent);
				}
			}
			//track status
			wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, TRACK_STATUS);
			frameEvent.SetString(event.GetName());
			RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()); //must be deleted by event handler
			frameEvent.SetClientData(recorderData);
			AddPendingEvent(frameEvent);
		}
		else { //no status
			if (event.GetName() == mTimecodeRecorder) { //just lost timecode from the recorder we're using
				SetTimecodeRecorder();
				wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE_MISSING);
				AddPendingEvent(frameEvent);
			}
		}
	}
}

/// @param value Preroll for all recorders (expected to be <= GetMaxPreroll()).
void RecorderGroupCtrl::SetPreroll(const ProdAuto::MxfDuration value)
{
	mPreroll = value;
}

/// @param value Postroll for all recorders (expected to be <= GetMaxPostroll()).
void RecorderGroupCtrl::SetPostroll(const ProdAuto::MxfDuration value)
{
	mPostroll = value;
}

/// @return Current value of preroll for all recorders.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetPreroll()
{
	return mPreroll;
}

/// @return Current value of postroll for all recorders.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetPostroll()
{
	return mPostroll;
}

/// @return Value limited by least capable recorder.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetMaxPreroll()
{
	return mMaxPreroll;
}

/// @return Value limited by least capable recorder.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetMaxPostroll()
{
	return mMaxPostroll;
}

/// Issue a Set Tape IDs command to the given recorder.
/// @param recorderName The recorder in question.
/// @param packageNames Packages (with or without tape IDs).
/// @param tapeIds Tape IDs corresponding to packageNames.
void RecorderGroupCtrl::SetTapeIds(const wxString & recorderName, const CORBA::StringSeq & packageNames, const CORBA::StringSeq & tapeIds)
{
	if (GetController(FindString(recorderName, true))) { //sanity check
		GetController(FindString(recorderName, true))->SetTapeIds(packageNames, tapeIds);
	}
}

/// Initiate a recording by sending an event for each recorder asking for its enable states and tape IDs.
/// @param startTimecode First frame in recording after preroll period.
void RecorderGroupCtrl::RecordAll(const ProdAuto::MxfTimecode startTimecode)
{
	EnableForInput(false); //don't want recorders being removed/added while recording
	mStartTimecode = startTimecode;
	//Ask for all the enable states
	for (unsigned int i = 0; i < GetCount(); i++) {
		if (GetController(i) && GetController(i)->IsOK()) {
			wxCommandEvent event(wxEVT_RECORDERGROUP_MESSAGE, REQUEST_RECORD);
			event.SetString(GetName(i));
			AddPendingEvent(event);
		}
	}
}

/// Issue a record command to the given recorder.
/// @param recorderName The recorder in question.
/// @param enableList Record enable states.
void RecorderGroupCtrl::Record(const wxString & recorderName, const CORBA::BooleanSeq & enableList)
{
	if (GetController(FindString(recorderName, true))) { //sanity check
		GetController(FindString(recorderName, true))->Record(mStartTimecode, mPreroll, mCurrentProject, enableList);
	}
}

/// Issue a stop command to all recorders.
/// @param stopTimecode First frame in recording of postroll period.
/// @param description Recording description (which is saved).
/// @param locators Locator information.
void RecorderGroupCtrl::Stop(const ProdAuto::MxfTimecode & stopTimecode, const wxString & description, const ProdAuto::LocatorSeq & locators)
{
	//(it doesn't matter if a non-recording recorder gets a stop command)
	for (unsigned int i = 0; i < GetCount(); i++) {
		if (GetController(i) && GetController(i)->IsOK()) {
			GetController(i)->Stop(stopTimecode, mPostroll, description, locators);
		}
	}
	mCurrentDescription = description;
	EnableForInput(); //do it here for safety (in case we get no response)

}

/// Set the recorder being used for displayed timecode (may be nothing) and issue an event to this effect.
/// @param name The recorder to use.
void RecorderGroupCtrl::SetTimecodeRecorder(wxString name)
{
	mTimecodeRecorder = name;
	wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE_SOURCE);
	frameEvent.SetString(mTimecodeRecorder);
	AddPendingEvent(frameEvent);
}

/// Replace the project names list, sending any new names to the recorders.
/// @param names The project names.
void RecorderGroupCtrl::SetProjectNames(const wxSortedArrayString & names)
{
	CORBA::StringSeq CorbaNames;
	for (size_t i = 0; i < names.GetCount(); i++) {
		if (wxNOT_FOUND == mProjectNames.Index(names[i])) { //a new name
			CorbaNames.length(CorbaNames.length() + 1);
			// Assignment to the CORBA::StringSeq element must be from a const char *
			// and should use ISO Latin-1 character set.
			CorbaNames[CorbaNames.length() - 1] = (const char *) names[i].mb_str(wxConvISO8859_1);
		}
	}

	//tell all the recorders we're connected to
	for (unsigned int i = 0; i < GetCount(); i++) { //the recorders in the list
		if (GetController(i) && GetController(i)->IsOK()) { //connected to this one
			GetController(i)->AddProjectNames(CorbaNames);
		}
	}

	//update the list
	mProjectNames = names;
}

/// Get the array of project names.
/// @return The project names (sorted).
const wxSortedArrayString & RecorderGroupCtrl::GetProjectNames()
{
	return mProjectNames;
}

/// Set the project name sent at each recording.  If it is not in the list of project names, add it
void RecorderGroupCtrl::SetCurrentProjectName(const wxString & name)
{
	mCurrentProject = name;
	if (wxNOT_FOUND == mProjectNames.Index(mCurrentProject)) {
		mProjectNames.Add(mCurrentProject);
	}
}

/// Get the project name sent at each recording.
const wxString & RecorderGroupCtrl::GetCurrentProjectName()
{
	return mCurrentProject;
}

/// Get the description sent at the end of each recording.
const wxString & RecorderGroupCtrl::GetCurrentDescription()
{
	return mCurrentDescription;
}
