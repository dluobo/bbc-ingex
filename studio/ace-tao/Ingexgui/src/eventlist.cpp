/***************************************************************************
 *   $Id: eventlist.cpp,v 1.1 2009/01/29 07:45:26 stuart_hc Exp $           *
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
#include "eventlist.h"
#include "ingexgui.h" //consts
#include "timepos.h"
#include "recordergroup.h"

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
mCurrentTakeInfo(-1), mCurrentSelectedEvent(-1)
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
/// @param type The event type: START, CUE, STOP or PROBLEM.
/// @param timecode Timecode of the event.
/// @param position Position of the event.
/// @param message A project name for START events or a description to display for other events.
/// @param frameCount The frame count for CUE events.
/// @param colour The colour of a CUE event.
/// @param labelColour The colour of text for a CUE event.
/// @param colourCode The colour code of a CUE event, to be sent to the recorder.
void EventList::AddEvent(EventType type, const ProdAuto::MxfTimecode timecode, const wxString position, const wxString message, const int64_t frameCount, const wxColour colour, const wxColour labelColour, const ProdAuto::LocatorColour::EnumType colourCode)
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
	wxFont font(EVENT_FONT_SIZE, wxFONTFAMILY_DEFAULT, wxFONTFLAG_UNDERLINED, wxFONTFLAG_UNDERLINED);
	switch (type) {
		case START :
			item.SetText(TypeLabels[START]);
			item.SetTextColour(wxColour(0xFF, 0x20, 0x00));
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
			{
				TakeInfo * info = new TakeInfo(GetItemCount(), message); //this will be the index of the current event
				mTakeInfoArray.Add(info); //deleted by array (object array)
			}
//			font.SetWeight(wxFONTWEIGHT_BOLD); breaks auto col width
//			font.SetStyle(wxFONTSTYLE_ITALIC); breaks auto col width
			break;
		case CUE :
			item.SetText(TypeLabels[CUE]);
			item.SetTextColour(labelColour);
			item.SetBackgroundColour(colour);
			break;
		case STOP :
			item.SetText(TypeLabels[STOP]);
			item.SetTextColour(wxColour(wxT("BLACK"))); //needed with cygwin for text to appear
			item.SetBackgroundColour(wxColour(wxT("WHITE")));
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
	wxString timecodeString;
	item.SetText(Timepos::FormatTimecode(timecode));
	font.SetFamily(wxFONTFAMILY_MODERN); //fixed pitch so fields don't move about
	SetItem(item);
#ifndef __WIN32__
	SetColumnWidth(1, wxLIST_AUTOSIZE);
#endif

	//position
	item.SetColumn(2);
	item.SetText(position);

	//cue points
	if (START != type && STOP != type) {
		mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).AddCuePoint(frameCount, timecode, colourCode);
	}

	SetItem(item); //additional to previous data
#ifndef __WIN32__
	SetColumnWidth(2, wxLIST_AUTOSIZE);
#endif

	//description
	if (STOP == type) {
		item.SetColumn(3);
		item.SetText(message);
		if (item.GetText().Len()) { //kludge to stop desc field being squashed if all descriptions are empty
			SetItem(item); //additional to previous data
#ifndef __WIN32__
			SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
		}
	}
	else if (CUE == type) {
		item.SetColumn(3);
		item.SetText(message);
		SetItem(item); //additional to previous data
#ifndef __WIN32__
		SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
	}
	if (-1 == GetFirstSelected()) {
		//First item in the list
		Select(0);
	}
}

/// Adds track and file list data to the latest take info.
void EventList::AddRecorderData(RecorderData * data)
{
	if (mTakeInfoArray.GetCount()) { //sanity check
		mTakeInfoArray.Item(mTakeInfoArray.GetCount() - 1).AddRecorder(data->GetTrackList(), data->GetFileList());
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
		locators.length(latestTakeInfo.GetCueColourCodes()->size());
		wxListItem item;
		item.SetColumn(3); //description column
		for (unsigned int i = 0; i < locators.length(); i++) {
			item.SetId(latestTakeInfo.GetStartIndex() + i + 1); //the 1 is to jump over the start event
			GetItem(item);
			locators[i].comment = item.GetText().mb_str(*wxConvCurrent);
			locators[i].colour = (*(latestTakeInfo.GetCueColourCodes()))[i];
			locators[i].timecode = (*(latestTakeInfo.GetCueTimecodes()))[i];
		}
	}
	return locators;
}
