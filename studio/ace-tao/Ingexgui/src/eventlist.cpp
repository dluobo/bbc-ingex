/***************************************************************************
 *   $Id: eventlist.cpp,v 1.3 2009/02/27 12:19:16 john_f Exp $           *
 *                                                                         *
 *   Copyright (C) 2009 British Broadcasting Corporation                   *
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
#include <wx/filename.h>
#include <wx/file.h>
#include "eventlist.h"
#include "ingexgui.h" //consts
#include "timepos.h"
#include "recordergroup.h"
#include "dialogues.h"

#define ROOT_NODE_NAME wxT("IngexguiEvents")

BEGIN_EVENT_TABLE( EventList, wxListView )
//	EVT_LIST_ITEM_SELECTED( wxID_ANY, EventList::OnEventSelection )
//	EVT_LIST_ITEM_ACTIVATED( wxID_ANY, EventList::OnEventActivated )
	EVT_LIST_BEGIN_LABEL_EDIT( wxID_ANY, EventList::OnEventBeginEdit )
	EVT_LIST_END_LABEL_EDIT( wxID_ANY, EventList::OnEventEndEdit )
	EVT_COMMAND( wxID_ANY, wxEVT_RESTORE_LIST_LABEL, EventList::OnRestoreListLabel )
END_EVENT_TABLE()

const wxString TypeLabels[] = {wxT(""), wxT("Start"), wxT("Cue"), wxT("Stop"), wxT("PROBLEM")}; //must match order of EventType enum

EventList::EventList(wxWindow * parent, wxWindowID id, const wxPoint & pos, const wxSize & size) :
wxListView(parent, id, pos, size, wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER|wxLC_EDIT_LABELS/*|wxALWAYS_SHOW_SB*/), //ALWAYS_SHOW_SB results in a disabled scrollbar on GTK (wx 2.8))
mCurrentTakeInfo(-1), mCurrentSelectedEvent(-1), mRecordingNodeCount(0), mRunThread(false), mSyncThread(false)
{
	//set up the columns
	wxListItem itemCol;
	itemCol.SetText(wxT("Event"));
	itemCol.SetWidth(EVENT_COL_WIDTH);
	InsertColumn(0, itemCol);
#ifdef __WIN32__
	SetColumnWidth(0, 140);
#endif
	itemCol.SetText(wxT("Timecode"));
	InsertColumn(1, itemCol); //will set width of this automatically later
#ifdef __WIN32__
	SetColumnWidth(1, 125);
#endif
	itemCol.SetText(wxT("Position"));
	InsertColumn(2, itemCol); //will set width of this automatically later
#ifdef __WIN32__
	SetColumnWidth(2, 125);
#endif
	itemCol.SetText(wxT("Description"));
	InsertColumn(3, itemCol); //will set width of this automatically later
	mRootNode = new wxXmlNode(wxXML_ELEMENT_NODE, ROOT_NODE_NAME);
	//start the thread (not checking for errors as not crucial to save; not waiting until thread starts as not crucial either)
	mCondition = new wxCondition(mMutex);
	mMutex.Lock(); //that's how the thread expects it
	wxThread::Create();
	wxThread::Run();
}

/// Returns the take info of the currently selected take, if any
TakeInfo * EventList::GetCurrentTakeInfo()
{
	TakeInfo * currentTake = 0;
	long selected = GetFirstSelected();
	if (-1 < selected) { //something's selected
		wxListItem item;
		item.SetId(selected);
		GetItem(item); //the take info index is in this item's data
		currentTake = &mTakeInfoArray.Item(item.GetData());
	}
	return currentTake;
}

/// Responds to an attempt to edit in the event list.
/// Allows editing only on cue points on a recording currently in progress.
/// Sets the selected label to the text of the description, as you can only edit the labels (left hand column) of a list control.
/// Dispatches an event to restore the label text in case the user presses ENTER without making changes.
/// @param event The list control event.
void EventList::OnEventBeginEdit(wxListEvent& event)
{
	if (mCanEdit) {
		//want to edit the description text (as opposed to the column 0 label text), so transfer it to the label
		wxListItem item;
		item.SetId(event.GetIndex());
		item.SetColumn(3);
		GetItem(item);
		item.SetColumn(0);
		item.SetMask(wxLIST_MASK_TEXT); //so it will update the text property
		SetItem(item);
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
void EventList::OnEventEndEdit(wxListEvent& event)
{
	if (!event.IsEditCancelled()) { //user hasn't pressed ESCAPE - otherwise the event text is the label text
		//put the edited text in the right column
		wxListItem item;
		item.SetId(event.GetIndex());
		item.SetColumn(3);
		item.SetText(event.GetText()); //the newly edited string
		SetItem(item);
		//prevent the label being overwritten
		event.Veto();
#ifndef __WIN32__
		SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
	}
}

/// Responds to commencement of editing by restoring the default label.
/// @param event The command event.
void EventList::OnRestoreListLabel(wxCommandEvent& event)
{
	wxListItem item;
	item.SetId(event.GetExtraLong());
	item.SetColumn(0);
	item.SetText(TypeLabels[CUE]);
	SetItem(item);
}

/// Clears the list and the take info.
void EventList::Clear()
{
	DeleteAllItems();
	mTakeInfoArray.Empty();
	mCurrentTakeInfo = -1;
	mCurrentSelectedEvent = -1;
	ClearSavedData();
}

/// Returns true if the selected take has changed since the last time this function was called.
bool EventList::SelectedTakeHasChanged()
{
	bool hasChanged = true; //err on the safe side
	if (mTakeInfoArray.GetCount()) { //sanity check
		wxListItem item;
		item.SetId(GetFirstSelected());
		GetItem(item); //the take info index is in this item's data
		hasChanged = (long) item.GetData() != mCurrentTakeInfo; //GetData() is supposed to return a long anyway...
		mCurrentTakeInfo = item.GetData();
	}
	return hasChanged;
}

/// Returns true if the selected event has changed since the last time this function was called.
bool EventList::SelectedEventHasChanged()
{
	bool hasChanged = GetFirstSelected() != mCurrentSelectedEvent;
	mCurrentSelectedEvent = GetFirstSelected();
	return hasChanged;
}

/// Selects the given item
/// @param item The item to select.
/// @param doNotFlag Do not flag the change for SelectedEventHasChanged() if not already changed
void EventList::Select(const long item, const bool doNotFlag)
{
	if (doNotFlag && GetFirstSelected() == mCurrentSelectedEvent) {
		mCurrentSelectedEvent = item;
	}
	wxListView::Select(item);
}

/// Moves to the start event of the current take or the start of the previous take.
/// Generates a select event if new event selected.
/// @param withinTake True to move to the start of the current take rather than the previous take.
void EventList::SelectPrevTake(const bool withinTake) {
	long selected = GetFirstSelected();
	if (0 < selected) { //something's selected and it's not the first event
		wxListItem item;
		item.SetId(selected);
		GetItem(item); //the take info index is in this item's data
		if (withinTake || (long) mTakeInfoArray.Item(item.GetData()).GetStartIndex() != selected || 0 == item.GetData()) { //within a take or the first take
			//move to the start of this take (if not already there)
			Select(mTakeInfoArray.Item(item.GetData()).GetStartIndex());
		}
		else { //at the start of a take which is not the first
			//move to the start of the previous take
			Select(mTakeInfoArray.Item(item.GetData() - 1).GetStartIndex());
		}
		EnsureVisible(GetFirstSelected());
	}
}

/// Moves up or down the event list by one item, if possible.
/// Generates a select event if new event selected.
/// @param down True to move downwards; false to move upwards.
void EventList::SelectAdjacentEvent(const bool down) {
	long selected = GetFirstSelected();
	if (-1 == selected) {
		//nothing selected
		Select(down ? GetItemCount() - 1 : 0); //select first item
	}
	else if (!down && 0 != selected) {
		//not the first item
		Select(--selected); //select next item up
	}
	else if (down && GetItemCount() - 1 != selected) {
		//not the last item
		Select(++selected); //select next item down
	}
	EnsureVisible(selected); //scroll if necessary
}

/// Moves to the start of the next take, or the end of the last take, if possible.
/// Generates a select event if new event selected.
void EventList::SelectNextTake()
{
	long selected = GetFirstSelected();
	if (mTakeInfoArray.GetCount() && -1 < selected) { //sanity check
		wxListItem item;
		item.SetId(selected);
		GetItem(item); //the take info index is in this item's data
		if (mTakeInfoArray.GetCount() - 1 == item.GetData()) { //last take is selected
			//move to end of last take
			Select(GetItemCount() - 1);
		}
		else {
			//move to start of next take
			Select(mTakeInfoArray.Item(item.GetData() + 1).GetStartIndex());
		}
	}
}

/// Moves to the start of the last take, if possible.
/// Generates a select event if new event selected.
void EventList::SelectLastTake() {
	if (mTakeInfoArray.GetCount()) { //sanity check
		Select(mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartIndex());
	}
}

/// Sets whether cue points can be edited or not.
void EventList::CanEdit(const bool canEdit)
{
	mCanEdit = canEdit;
}

/// Returns true if an event is selected and if it's a cue point in the latest take.
bool EventList::LatestTakeCuePointIsSelected() {
	bool isCuePoint = false;
	if (-1 < GetFirstSelected()) {
		wxListItem item;
		item.SetId(GetFirstSelected());
		GetItem(item); //the take info index is in this item's data
		isCuePoint = item.GetData() == mTakeInfoArray.GetCount() - 1 && item.GetText() == TypeLabels[CUE];
	}
	return isCuePoint;
}

/// Returns true if the first or no events are selected
bool EventList::AtTop()
{
	return (1 > GetFirstSelected());
}

/// Returns true if an event is selected event and is the start of a take
bool EventList::AtStartOfTake()
{
	bool atStartOfTake = false;
	if (-1 < GetFirstSelected()) {
		wxListItem item;
		item.SetId(GetFirstSelected());
		GetItem(item);
		atStartOfTake = item.GetText() == TypeLabels[START];
	}
	return atStartOfTake;
}

/// Returns true if an event is selected and if it's in the last take.
bool EventList::InLastTake()
{
	bool inLastTake = false;
	if (-1 < GetFirstSelected()) {
		wxListItem item;
		item.SetId(GetFirstSelected());
		GetItem(item); //the take info index is in this item's data
		inLastTake = item.GetData() == mTakeInfoArray.GetCount() - 1;
	}
	return inLastTake;
}

/// Returns true if the last or no events are selected
bool EventList::AtBottom()
{
	return GetFirstSelected() == GetItemCount() - 1 || -1 == GetFirstSelected();
}

/// Adds a stop/start/cue point event to the event list.
/// Start and stop events are ignored if already started/stopped
/// For start events, creates a new TakeInfo object.
/// Updates the other controls and the player.
/// @param type The event type: START, CUE, STOP or [PROBLEM]-not fully implemented.
/// @param timecode Timecode of the event, for START and optionally STOP events (for STOP events, assumed to be frame-accurate, unlike frameCount).
/// @param frameCount The position in frames, for CUE and STOP events. (For STOP events, if timecode supplied, used to work out the number of days; otherwise, used as the frame-accurate length unless zero, which indicates unknown).
/// @param description A description to display for events other than START (which uses the project name).
/// @param colourIndex The colour of a CUE event.
void EventList::AddEvent(EventType type, ProdAuto::MxfTimecode * timecode, const int64_t frameCount, const wxString & description, const size_t colourIndex)
{
	wxListItem item;
	if (NONE == type) { //sanity check
		return;
	}
	//Reject multiple stop or start events
	else if (GetItemCount()) {
		item.SetId(GetItemCount() - 1);
		GetItem(item);
		if ((START == type && TypeLabels[STOP] != item.GetText()) || (STOP == type && TypeLabels[STOP] == item.GetText())) {
			return;
		}
	}
	//Reject adding non-start event as first event (sanity check) - prevents there being list entries with no associated take info object
	else if (START != type) {
		return;
	}
	//Try to add edit rate - can't guarantee that the first start event will have a valid timecode
	wxString dummy;
	mMutex.Lock();
	if (!mRootNode->GetPropVal(wxT("EditRateNumerator"), &dummy) && timecode && !timecode->undefined) {
		mRootNode->AddProperty(wxT("EditRateNumerator"), wxString::Format(wxT("%d"), timecode->edit_rate.numerator));
		mRootNode->AddProperty(wxT("EditRateDenominator"), wxString::Format(wxT("%d"), timecode->edit_rate.denominator));
	}
	int64_t position = frameCount;
	mMutex.Unlock();
	wxFont font(EVENT_FONT_SIZE, wxFONTFAMILY_DEFAULT, wxFONTFLAG_UNDERLINED, wxFONTFLAG_UNDERLINED);
	switch (type) {
		case START :
			mMutex.Lock();
			mRecordingNode = new wxXmlNode(mRootNode, wxXML_ELEMENT_NODE, wxT("Recording"), wxT(""), new wxXmlProperty(wxT("Index"), wxString::Format(wxT("%d"), mRecordingNodeCount++)));
			mMutex.Unlock();
			if (!timecode->undefined) {
				mRecordingNode->AddProperty(wxT("StartTime"), wxString::Format(wxT("%d"), timecode->samples));
			}
			item.SetText(TypeLabels[START]);
			item.SetTextColour(wxColour(0xFF, 0x20, 0x00));
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
			{
				TakeInfo * info = new TakeInfo(GetItemCount(), mProjectName, *timecode); //this will be the index of the current event
				mTakeInfoArray.Add(info); //deleted by array (object array)
			}
//			font.SetWeight(wxFONTWEIGHT_BOLD); breaks auto col width
//			font.SetStyle(wxFONTSTYLE_ITALIC); breaks auto col width
			break;
		case CUE :
			item.SetText(TypeLabels[CUE]);
			item.SetTextColour(CuePointsDlg::GetLabelColour(colourIndex));
			item.SetBackgroundColour(CuePointsDlg::GetColour(colourIndex));
			mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).AddCuePoint(frameCount, colourIndex);
			break;
		case STOP :
			item.SetText(TypeLabels[STOP]);
			item.SetTextColour(wxColour(wxT("BLACK"))); //needed with cygwin for text to appear
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
			if (!description.IsEmpty()) {
				new wxXmlNode(new wxXmlNode(mRecordingNode, wxXML_ELEMENT_NODE, wxT("Description")), wxXML_CDATA_SECTION_NODE, wxT(""), description);
			}
			if (timecode && !timecode->undefined) {
				position = timecode->samples - mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartTimecode().samples; //this is the correct number of samples, but mod one day as timecodes wrap at 24 hours
				if (position < 0) { //rolled over midnight
					position += 24LL * 3600 * timecode->edit_rate.numerator / timecode->edit_rate.denominator;
				}
				int64_t totalMinutes = frameCount / timecode->edit_rate.numerator * timecode->edit_rate.denominator / 60; //this is an approx number of minutes in total
				if (position < 3600 * 12 * timecode->edit_rate.numerator / timecode->edit_rate.denominator) { //fractional day is less than half
					totalMinutes += 1; //nudge upwards to ensure all the days are added if frameCount is a bit low
				}
				else {
					totalMinutes -= 1; //nudge downwards to ensure an extra day is not added if frameCount is a bit high
				}
				position += (totalMinutes / 60 / 24) * 3600 * 24 * timecode->edit_rate.numerator / timecode->edit_rate.denominator; //add whole days to the fractional day
				mRecordingNode->AddProperty(wxT("StopSamples"), wxString::Format(wxT("%d"), position));
			}
			else if (frameCount) {
				mRecordingNode->AddProperty(wxT("StopSamples"), wxString::Format(wxT("%d"), frameCount));
			}
			//add cue points to XML doc now, because they could have been edited or deleted up to this point
			{
				TakeInfo latestTakeInfo = mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1);
				wxListItem item;
				item.SetColumn(3); //description column
				for (unsigned int i = 0; i < latestTakeInfo.GetCueColourIndeces().size(); i++) {
					item.SetId(latestTakeInfo.GetStartIndex() + i + 1); //the 1 is to jump over the start event
					GetItem(item);
					wxXmlNode * cuePointNode = new wxXmlNode(mRecordingNode, wxXML_ELEMENT_NODE, wxT("CuePoint"), wxT(""), new wxXmlProperty(wxT("Index"), wxString::Format(wxT("%d"), i)));
					cuePointNode->AddProperty(wxT("Samples"), wxString::Format(wxT("%d"), latestTakeInfo.GetCuePointFrames()[i]));
					cuePointNode->AddProperty(wxT("Colour"), wxString::Format(wxT("%d"), CuePointsDlg::GetColourCode(latestTakeInfo.GetCueColourIndeces()[i])));
					if (item.GetText().length()) {
						new wxXmlNode(cuePointNode, wxXML_CDATA_SECTION_NODE, wxT(""), item.GetText());
					}
				}
			}
			break;
		default : //FIXME: putting these in will break the creation of the list of locators in OnStop()
			item.SetText(TypeLabels[PROBLEM]);
			item.SetTextColour(wxColour(wxT("RED")));
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
			break;
	}

	item.SetColumn(0);
	item.SetId(GetItemCount()); //will insert at end
	item.SetData(mTakeInfoArray.GetCount() - 1); //the index of the take info for this take
	InsertItem(item); //insert (at end)
	EnsureVisible(item.GetId()); //scroll down if necessary
	if (START == type) {
		Select(item.GetId());
	}
#ifndef __WIN32__
	SetColumnWidth(0, wxLIST_AUTOSIZE);
#endif
	item.SetFont(font); //point size ignored on GTK and row height doesn't adjust on Win32

	//timecode
	item.SetColumn(1);
	if (timecode) {
		item.SetText(Timepos::FormatTimecode(*timecode));
	}
	else {
		ProdAuto::MxfTimecode tc = mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartTimecode();
		tc.samples += frameCount; //will wrap automatically on display
		tc.undefined |= 0 == frameCount;
		item.SetText(Timepos::FormatTimecode(tc));
	}
	font.SetFamily(wxFONTFAMILY_MODERN); //fixed pitch so fields don't move about
	SetItem(item);
#ifndef __WIN32__
	SetColumnWidth(1, wxLIST_AUTOSIZE);
#endif

	//position
	ProdAuto::MxfDuration duration;
	duration.edit_rate.numerator = mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartTimecode().edit_rate.numerator;
	duration.edit_rate.denominator = mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartTimecode().edit_rate.denominator;
	duration.undefined = mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).GetStartTimecode().undefined;
	duration.samples = position;
	item.SetColumn(2);
	item.SetText(Timepos::FormatPosition(duration));
	SetItem(item); //additional to previous data
#ifndef __WIN32__
	SetColumnWidth(2, wxLIST_AUTOSIZE);
#endif

	//description
	item.SetColumn(3);
	if (START == type) {
		item.SetText(mProjectName);
	}
	else if (STOP == type || CUE == type) {
		item.SetText(description);
	}
	if (item.GetText().Len()) { //kludge to stop desc field being squashed if all descriptions are empty
		SetItem(item); //additional to previous data
#ifndef __WIN32__
		SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
	}

	if (-1 == GetFirstSelected()) {
		//First item to be put in the list
		Select(0);
	}
}

/// Adds track and file list data to the latest take info.
void EventList::AddRecorderData(RecorderData * data)
{
	if (mTakeInfoArray.GetCount()) { //sanity check
		mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).AddRecorder(data->GetTrackList(), data->GetFileList());
		unsigned int index = 0;
		for (size_t i = 0; i < data->GetTrackList()->length(); i++) {
			if (data->GetTrackList()[i].has_source && strlen(data->GetFileList()[i].in())) { //tracks not enabled for record have blank filenames
				wxXmlNode * fileNode = new wxXmlNode(mRecordingNode, wxXML_ELEMENT_NODE, wxT("File"), wxT(""), new wxXmlProperty(wxT("Type"), ProdAuto::VIDEO == (data->GetTrackList())[i].type ? wxT("Video") : wxT("Audio")));
				fileNode->AddProperty(wxT("Index"), wxString::Format(wxT("%d"), index++));
				new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Label")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetTrackList())[i].src.package_name, *wxConvCurrent));
				new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Path")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetFileList())[i].in(), *wxConvCurrent));
			}
		}
		mMutex.Lock();
		mRunThread = true; //in case thread is busy so misses the signal
		mCondition->Signal();
		mMutex.Unlock();
	}
}

/// Deletes the cue point corresponding to the currently selected event list item, if valid.
/// Does not check if recording is still in progress.
void EventList::DeleteCuePoint()
{
	if (mTakeInfoArray.GetCount()) { //sanity check
		//remove cue point from take info
		long selected = GetFirstSelected();
		if (mTakeInfoArray[mTakeInfoArray.GetCount() - 1].DeleteCuePoint(selected - mTakeInfoArray[mTakeInfoArray.GetCount() - 1].GetStartIndex() - 1)) { //sanity check; the last 1 is to jump over the start event
			//remove displayed cue point
			wxListItem item;
			item.SetId(selected);
			DeleteItem(item);
			//Selected row has gone so select something sensible (the next row, or the last row if the last row was deleted)
			Select(GetItemCount() == selected ? selected - 1 : selected); //Generates an event which will update the delete cue button state
		}
	}
}

/// Returns information about the locators (cue points) in the latest take.
ProdAuto::LocatorSeq EventList::GetLocators()
{
	ProdAuto::LocatorSeq locators;
	if (mTakeInfoArray.GetCount()) { //sanity check
		TakeInfo latestTakeInfo = mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1);
		locators.length(latestTakeInfo.GetCueColourIndeces().size());
		wxListItem item;
		item.SetColumn(3); //description column
		for (unsigned int i = 0; i < locators.length(); i++) {
			item.SetId(latestTakeInfo.GetStartIndex() + i + 1); //the 1 is to jump over the start event
			GetItem(item);
			locators[i].comment = item.GetText().mb_str(*wxConvCurrent);
			locators[i].colour = CuePointsDlg::GetColourCode(latestTakeInfo.GetCueColourIndeces()[i]);
			locators[i].timecode = latestTakeInfo.GetStartTimecode();
			locators[i].timecode.samples = latestTakeInfo.GetCuePointFrames()[i] % (24 * 3600 * latestTakeInfo.GetStartTimecode().edit_rate.numerator / latestTakeInfo.GetStartTimecode().edit_rate.denominator);
		}
	}
	return locators;
}

/// Sets the project name, for use in start event description and the filename of the saved events.
/// This will generate a new XML file when new recordings are made.
/// Assumes that the current XML file is up to date.
/// @return True if there are events in the list.
bool EventList::SetCurrentProjectName(const wxString & name)
{
	mProjectName = name;
	wxString filename = name;
	for (size_t i = 0; i < wxFileName::GetForbiddenChars().length(); i++) {
		filename.Replace(wxString(wxFileName::GetForbiddenChars()[i]), wxT("_"));
	}
	filename.Replace(wxString(wxFileName::GetPathSeparator()), wxT("_"));
	filename += wxDateTime::Now().Format(wxT("-%y%m%d"));
	ClearSavedData();
	mFilename = filename; //safe to do this after calling ClearSavedData()
	Load();
	return GetItemCount();
}

/// Populates the XML tree from a file, which will then be used to save to when new events are added.
/// Ignores all data if the project name doesn't match that in the file.
/// Calls AddEvent() and AddRecorderData() in the same way a live recording would.
/// Does not clear existing events from the event list.
/// Assumes ClearSavedData() has been called since the last save.
void EventList::Load()
{
	wxXmlDocument doc;
	bool haveProjectNameNode = false;
	wxXmlNode * loadedRootNode = 0; //initialisation prevents compiler warning
	if (wxFile::Exists(mFilename) && doc.Load(mFilename) && (loadedRootNode = doc.GetRoot())) {
 		if (ROOT_NODE_NAME == loadedRootNode->GetName()) {
			wxString str;
			long long1, long2;
			ProdAuto::MxfTimecode timecode;
			//get edit rate (if present; if not, no timecodes will be valid)
			if (loadedRootNode->GetPropVal(wxT("EditRateNumerator"), &str)
			 && str.ToLong(&long1)
			 && 0 < long1
			 && loadedRootNode->GetPropVal(wxT("EditRateDenominator"), &str)
			 && str.ToLong(&long2)
			 && 0 < long2
			) {
				timecode.undefined = false;
				timecode.edit_rate.numerator = long1;
				timecode.edit_rate.denominator = long2;
			}
			else {
				timecode.undefined = true;
			}
			bool haveEditRate = !timecode.undefined;
			//make a list of the recording nodes in order, and find the project name node
			std::vector<wxXmlNode *> recordingNodes;
			long index;
			wxXmlNode * node = loadedRootNode->GetChildren();
			while (node) {
				if (wxT("Recording") == node->GetName()
				 && node->GetPropVal(wxT("Index"), &str)
				 && str.ToLong(&index)
				 && 0 <= index
				) {
					//extend vector if necessary
					if (recordingNodes.size() < (unsigned long) index + 1) {
						size_t oldSize = recordingNodes.size();
						recordingNodes.resize(index + 1);
						for (size_t i = oldSize; i <= (unsigned long) index; i++) {
							recordingNodes[i] = 0; //to catch missing index values
						}
					}
					recordingNodes[index] = node;
				}
				else if (wxT("ProjectName") == node->GetName()) {
					if (GetCdata(node) == mProjectName) {
						haveProjectNameNode = true;
					}
					else {
						delete loadedRootNode;
						break;
					}
				}
				node = node->GetNext();
			}
			if (loadedRootNode->GetChildren()) {
				//iterate over the recording nodes
				for (size_t i = 0; i < recordingNodes.size(); i++) {
					wxString description;
					if (recordingNodes[i]) {
						//make a list of the cue points and files in this recording, in order
						std::vector<wxXmlNode *> fileNodes;
						std::vector<wxXmlNode *> cuePointNodes;
						node = recordingNodes[i]->GetChildren();
						while (node) {
							if (wxT("CuePoint") == node->GetName()
							 && node->GetPropVal(wxT("Index"), &str)
							 && str.ToLong(&index)
							 && 0 <= index
							) {
								//extend vector if necessary
								if (cuePointNodes.size() < (unsigned long) index + 1) {
									size_t oldSize = cuePointNodes.size();
									cuePointNodes.resize(index + 1);
									for (size_t i = oldSize; i <= (unsigned long) index; i++) {
										cuePointNodes[i] = 0; //to catch missing index values
									}
								}
								cuePointNodes[index] = node;
							}
							else if (wxT("File") == node->GetName() &&
							node->GetPropVal(wxT("Index"), &str)
							 && str.ToLong(&index)
							 && 0 <= index
							) {
								//extend vector if necessary
								if (fileNodes.size() < (unsigned long) index + 1) {
									size_t oldSize = fileNodes.size();
									fileNodes.resize(index + 1);
									for (size_t i = oldSize; i <= (unsigned long) index; i++) {
										fileNodes[i] = 0; //to catch missing index values
									}
								}
								fileNodes[index] = node;
							}
							else if (wxT("Description") == node->GetName()) {
								description = GetCdata(node);
							}
							node = node->GetNext();
						}
						//add recording start to the event list
						if (haveEditRate) {
							if (recordingNodes[i]->GetPropVal(wxT("StartTime"), &str)
							&& str.ToLong(&long1)
							&& 0 <= long1
							) {
								timecode.undefined = false;
								timecode.samples = long1;
							}
							else {
								timecode.undefined = true;
							}
						}
						AddEvent(START, &timecode);
						//add cue points to the event list
						for (size_t j = 0; j < cuePointNodes.size(); j++) {
							if (cuePointNodes[j]
							 && cuePointNodes[j]->GetPropVal(wxT("Samples"), &str)
							 && str.ToLong(&long1)
							 && 0 <= long1
							) {
								if (!cuePointNodes[j]->GetPropVal(wxT("Colour"), &str)
								|| !str.ToLong(&long2)
								|| 0 > long1
								|| N_CUE_POINT_COLOURS <= long2
								) {
									long2 = 0; //default colour
								}
								AddEvent(CUE, 0, long1, GetCdata(cuePointNodes[j]), (size_t) long2);
							}
						}
						//add recording stop to the event list
						long1 = 0; //indicates unknown
						if (!recordingNodes[i]->GetPropVal(wxT("StopSamples"), &str)
						 || !str.ToLong(&long1)
						 || 0 > long1
						) {
							long1 = 0;
						}
						AddEvent(STOP, 0, long1, description);
						//add tracks/files to the take info
						ProdAuto::TrackList_var trackList = new ProdAuto::TrackList;
						CORBA::StringSeq_var fileList = new CORBA::StringSeq;
						trackList->length(fileNodes.size());
						fileList->length(fileNodes.size());
						for (size_t j = 0; j < fileNodes.size(); j++) {
							trackList[j].type = ProdAuto::VIDEO;
							trackList[j].name = "";
							trackList[j].has_source = false;
							fileList[j] = "";
							if (fileNodes[j]) {
								if (fileNodes[j]->GetPropVal(wxT("Type"), &str) && wxT("Audio") == str) {
									trackList[j].type = ProdAuto::AUDIO;
								}
								node = fileNodes[j]->GetChildren();
								while (node) {
									if (wxT("Path") == node->GetName()) {
										fileList[j] = GetCdata(node).mb_str(*wxConvCurrent);
										trackList[j].has_source = true;
									}
									else if (wxT("Label") == node->GetName()) {
										trackList[j].src.package_name = GetCdata(node).mb_str(*wxConvCurrent);
									}
									node = node->GetNext();
								}
							}
						}
						RecorderData data(trackList, fileList);
						AddRecorderData(&data);
					} //recording node present
				} //recording node loop
			} //root node has children
		} //root node name correct
	} //file is OK
}

/// If the given node has a CDATA node as a child, returns the value of this; empty string otherwise
const wxString EventList::GetCdata(wxXmlNode * node)
{
	wxString content;
	if (node->GetChildren() && wxXML_CDATA_SECTION_NODE == node->GetChildren()->GetType()) {
		content = node->GetChildren()->GetContent().Trim(); //seems to have appended newline and space!
	}
	return content;
}

/// Clears the XML tree which is used to generate the saved data file.
/// Puts a single node in the tree, containing the project name.
/// Returns when the thread is in a waiting state, so shared variables can be manipulated without locking the mutex after calling this function.
void EventList::ClearSavedData()
{
	mMutex.Lock();
	mRunThread = true; //in case thread is busy so misses the signal
	mSyncThread = true; //tells thread to signal when it's not saving
//	mSavedStateFilename = wxStandardPaths::Get().GetUserConfigDir() + wxFileName::GetPathSeparator() + SAVED_STATE_FILENAME;
	mCondition->Signal();
	mCondition->Wait(); //until not saving
	mMutex.Unlock();
	delete mRootNode; //remove all stored data
	mRootNode = new wxXmlNode(wxXML_ELEMENT_NODE, ROOT_NODE_NAME);
	mRecordingNodeCount = 0;
	new wxXmlNode(new wxXmlNode(mRootNode, wxXML_ELEMENT_NODE, wxT("ProjectName")), wxXML_CDATA_SECTION_NODE, wxT(""), mProjectName);
}

/// Thread entry point, called by wxWidgets.
/// Saves the XML tree in the background, when signalled to do so
/// @return Always 0.
wxThread::ExitCode EventList::Entry()
{
	while (1) {
		if (!mRunThread) { //not been told to save/release during previous interation
			mCondition->Wait(); //unlock mutex and wait for a signal, then relock
		}
		mRunThread = false;
		if (!mSyncThread) { //save asynchronously
			//isolate the existing data from the main context
			if (mRootNode->GetChildren()) { //there's something to save
				wxXmlNode * oldRootNode = mRootNode;
				mRootNode = new wxXmlNode(wxXML_ELEMENT_NODE, ROOT_NODE_NAME); //so that things can be added while saving
				//save in the background
				wxString fileName = mFilename;
				mMutex.Unlock();
				wxXmlDocument doc;
				doc.SetRoot(oldRootNode);
				doc.Save(fileName); //zzz...
				doc.DetachRoot();
				mMutex.Lock();
				//combine saved data with any new data that might have appeared while saving
				//the wxXmlNode documentation is a little unclear about the best way of doing this, but making a list of all the nodes to be transferred rather than transferring them while iterating over the child nodes seems sensible and reliable
				std::vector<wxXmlNode *> childNodes;
				wxXmlNode * childNode = mRootNode->GetChildren();
				while (childNode) {
					childNodes.push_back(childNode);
					childNode = childNode->GetNext();
				}
				while (childNodes.size()) {
					mRootNode->RemoveChild(childNodes.back());
					oldRootNode->InsertChild(childNodes.back(), NULL); //oldRootNode will have at least one child already so this is OK
					childNodes.pop_back();
				}
				delete mRootNode;
				mRootNode = oldRootNode;
			}
		}
		else { //tell foreground we're not saving
			mSyncThread = false;
			mCondition->Signal(); //the foreground is waiting
		}
	}
	return 0;
}
