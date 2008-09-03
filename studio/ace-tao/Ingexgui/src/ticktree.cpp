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
#include "ticktree.h"
#include "small_disabled.xpm"
#include "small_enabled.xpm"
#include "small_partial_enabled.xpm"
#include "small_rec.xpm"
#include "small_partial_rec.xpm"
#include "small_exclamation.xpm"
#include "small_question.xpm"
#include <wx/imaglist.h>
#include "wx/xml/xml.h"
#include "dialogues.h" //for the XML tape ID functions

WX_DECLARE_STRING_HASH_MAP(wxTreeItemId, TreeItemHash);

DEFINE_EVENT_TYPE(wxEVT_TREE_MESSAGE)

BEGIN_EVENT_TABLE(TickTreeCtrl, wxTreeCtrl)
	EVT_LEFT_DOWN(TickTreeCtrl::OnLMouseDown)
END_EVENT_TABLE()

/// Displays empty tree, disabled.
/// @param parent The parent window.
/// @param id The control's window ID.
/// @param pos The control's position.
/// @param size The control's size.
/// @param name The displayed name of the root node.
TickTreeCtrl::TickTreeCtrl(wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxString & name) :
	wxTreeCtrl(parent, id, pos, size), mEnableChanges(false), mName(name)
{
	SetNextHandler(parent);
	wxImageList * mImages = new wxImageList();
	mImages->Create(16, 16);
	//image loading order must correspond to enum order
	mImages->Add((const wxIcon &) small_disabled); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_partial_enabled); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_enabled); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_rec); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_partial_rec); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_exclamation); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_question); //cast needed for cygwin

	AssignImageList(mImages);

	SetItemData(AddRoot(mName, DISABLED), new ItemData(DISABLED, mName));
	mDefaultBackgroundColour = GetItemBackgroundColour(GetRootItem());
	Disable();
}

/// Adds a recorder to the tree.
/// Adds only tracks which have sources, but counts the number of sourceless video tracks between used video tracks, for arranging tape ID data correctly when requested.
/// Arranges tracks by package name, and displays tape IDs for packages names if found.
/// Sets all tracks to be enabled for recording.
/// Updates overall tree state and sends a notification event.
/// @param name The recorder name.
/// @param trackList Information about the tracks, sources and package names.
/// @param trackStatusList Whether each track is recording or not.
/// @param routerRecorder True if this recorder is a router recorder - all tracks (should be only one anyway) will be regarded as router tracks and tape IDs will not be used.
/// @param doc XML document object for tape ID information.
/// @return True if there are any packages names anywhere which can have tape IDs (i.e. not router recorder package names).
bool TickTreeCtrl::AddRecorder(const wxString & name, ProdAuto::TrackList_var trackList, ProdAuto::TrackStatusList_var trackStatusList, bool routerRecorder, wxXmlDocument & doc)
{
	Enable();
	//make recorder root node
	wxTreeItemId recorderRoot = AppendItem(GetRootItem(), name, DISABLED);
	//expand the list to show the recorder
	EnsureVisible(recorderRoot); //Not sure Expand() works but this is nicer anyway
	//See if any tracks are recording - if so, we assume that the recorder is OK (and recording) and therefore any non-recording tracks are not enabled to record
	bool someRecording = false;
	for (unsigned int i = 0; i < trackStatusList->length(); i++) {
		if (trackStatusList[i].rec) {
			someRecording = true;
			break;
		}
	}
	//make recorder package (branch) and track (terminal) nodes
	wxXmlNode * tapeIdsNode = SetTapeIdsDlg::GetTapeIdsNode(doc);
	TreeItemHash packageNameTreeNodes;
	for (unsigned int i = 0; i < trackList->length(); i++) {
		if (trackList[i].has_source) { //something's plugged into this input
			//group tracks by package name
			wxString packageName = wxString(trackList[i].src.package_name, *wxConvCurrent);
			if (packageNameTreeNodes.end() == packageNameTreeNodes.find(packageName)) {
				//haven't come across this package name: does it have a tape ID?
				wxString tapeId = SetTapeIdsDlg::GetTapeId(tapeIdsNode, packageName);
				//create a package node and remember its ID
				packageNameTreeNodes[packageName] = AppendItem(recorderRoot, packageName, DISABLED); //state will be updated below, from track nodes
				SetItemData(packageNameTreeNodes[packageName], new ItemData(DISABLED, packageName, routerRecorder, !tapeId.IsEmpty() || routerRecorder)); //remember package name for when messing about with tape IDs
				if (!tapeId.IsEmpty() && !routerRecorder) {
					AddMessage(packageNameTreeNodes[packageName], tapeId);
				}
			}
			//create a node for the track on the appropriate branch
			wxTreeItemId id = AppendItem(packageNameTreeNodes[packageName], wxString(trackList[i].src.track_name, *wxConvCurrent) + wxString(wxT(" via ")) + wxString(trackList[i].name, *wxConvCurrent), trackStatusList[i].rec ? RECORDING : (someRecording ? DISABLED : ENABLED));
			SetItemData(id, new ItemData(((TickTreeCtrl::state) GetItemImage(id)), GetItemText(id), i, trackStatusList[i].rec || !someRecording)); //enable to record unless some tracks are recording but this one isn't
			if (routerRecorder) {
				SetItemTextColour(id, ROUTER_TRACK_COLOUR);
			}
			else {
				SetItemTextColour(id, trackList[i].type == ProdAuto::VIDEO ? VIDEO_TRACK_COLOUR : AUDIO_TRACK_COLOUR);
			}
		}
	}
	//store common recorder data
	SetItemData(recorderRoot, new ItemData(DISABLED, name, trackList->length()));
	//sort recorder nodes
	SortChildren(GetRootItem());
	//report all track state up the tree
	SetTrackStatus(name, someRecording, trackStatusList);
	//tape IDs
	if (!routerRecorder && packageNameTreeNodes.size()) {
		//tell the frame to tell the recorder the tape IDs
		wxCommandEvent guiEvent(wxEVT_TREE_MESSAGE, wxID_ANY);
		guiEvent.SetString(name);
		AddPendingEvent(guiEvent);
		return true; //obviously using some tape IDs
	}
	else {
		return UsingTapeIds();
	}
}

/// Removes a recorder from the tree.
/// Updates overall tree state and sends a notification event.
/// @param recorderName The name of the recorder to remove.
/// @return True if there are any packages names anywhere which can have tape IDs (i.e. not router recorder package names).
bool TickTreeCtrl::RemoveRecorder(const wxString & recorderName)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	if (recorder.IsOk()) {
		Delete(recorder);
		if (GetCount() < 2) {
			//No recorders
			Clear();
		}
		else {
			wxTreeItemIdValue dummyCookie;
			ReportState(GetFirstChild(GetRootItem(), dummyCookie)); //any recorder-level ID will do
		}
	}
	return UsingTapeIds();
}

/// Finds the root node of a recorder by recorder name.
/// @param recorderName The name to search for.
/// @return The root node of the recorder (may not be valid).
wxTreeItemId TickTreeCtrl::FindRecorder(const wxString & recorderName)
{
	wxTreeItemIdValue cookie;
	wxTreeItemId recorder = GetFirstChild(GetRootItem(), cookie);
	while (recorder.IsOk() && ((ItemData *) GetItemData(recorder))->GetString() != recorderName) {
		recorder = GetNextChild(GetRootItem(), cookie);
	}
	return recorder;
}

/// Returns the tree to the initial state.
void TickTreeCtrl::Clear()
{
	CollapseAndReset(GetRootItem());
	SetNodeState(GetRootItem(), DISABLED);
	SetItemBackgroundColour(GetRootItem(), mDefaultBackgroundColour); //cancel no signal status
	RemoveMessage(GetRootItem());
	Disable();
	ReportState(GetRootItem());
}

/// Responds to a left mouse down event by toggling the record state of the clicked node and all nodes below, if enabled.
/// Updates overall tree state and sends a notification event.
/// @param event The mouse event.
void TickTreeCtrl::OnLMouseDown(wxMouseEvent & event)
{
	int flags;
	wxTreeItemId id = HitTest(event.GetPosition(), flags);
	if (mEnableChanges && (flags & wxTREE_HITTEST_ONITEMICON)) {
		//apply selection state to this and all children recursively
		if (DISABLED == GetItemImage(id) || PARTIALLY_ENABLED == GetItemImage(id)) {
			//record enable
			SelectRecursively(id);
		}
		else if (ENABLED == GetItemImage(id)) {
			//record disable
			SelectRecursively(id, false);
		}
		//report selection state to all ancestors recursively
		ReportState(id);
	}
	event.Skip(); //to allow event to be used for other things, i.e. item selection
}

/// Indicates whether or not any tracks are enabled for recording.
/// @return True if any tracks enabled.
bool TickTreeCtrl::SomeEnabled()
{
	return DISABLED != ((ItemData *) GetItemData(GetRootItem()))->GetUnderlyingState();
}

/// Indicates whether or not any tracks are recording.
/// @return True if any tracks recording.
bool TickTreeCtrl::IsRecording()
{
	return RECORDING == ((ItemData *) GetItemData(GetRootItem()))->GetUnderlyingState() || PARTIALLY_RECORDING == ((ItemData *) GetItemData(GetRootItem()))->GetUnderlyingState();
}

/// Indicates whether all enabled tracks' packages have tape IDs.
/// @return True if all tape IDs present.
bool TickTreeCtrl::TapeIdsOK()
{
	return ((ItemData *) GetItemData(GetRootItem()))->GetBool();
}

/// Indicates whether overall state is unknown.
/// @return True if unknown.
bool TickTreeCtrl::IsUnknown()
{
	return UNKNOWN == GetItemImage(GetRootItem());
}

/// Indicates whether overall problem (absent signals not included).
/// @return True if problem.
bool TickTreeCtrl::HasProblem()
{
	return PROBLEM == GetItemImage(GetRootItem());
}

/// Indicates whether all signals are present on enabled sources.
/// @return True if all signals present.
bool TickTreeCtrl::HasAllSignals()
{
	return NO_SIGNAL_COLOUR != GetItemBackgroundColour(GetRootItem());
}

/// Indicates whether any recorders are present.
/// @return True if any recorders.
bool TickTreeCtrl::HasRecorders()
{
	return GetCount() > 1;	
}

/// Indicates whether at least one track is recording and there are no problems.
/// @return True if recording with no problems.
bool TickTreeCtrl::RecordingSuccessfully()
{
	return RECORDING == GetItemImage(GetRootItem()) || PARTIALLY_RECORDING == GetItemImage(GetRootItem());
}

/// Indicates if all recorders that have tracks selected for recording are recording
/// @return True if all recording
bool TickTreeCtrl::AllRecording() {
	bool allRecording = true;
	wxTreeItemIdValue cookie;
	wxTreeItemId recorder = GetFirstChild(GetRootItem(), cookie);
	while (recorder.IsOk()) {
		if (ENABLED == ((ItemData *) GetItemData(recorder))->GetUnderlyingState() || PARTIALLY_ENABLED == ((ItemData *) GetItemData(recorder))->GetUnderlyingState()) { //some tracks enabled but not recording
			allRecording = false;
		}
		recorder = GetNextChild(GetRootItem(), cookie);
	}
	return allRecording;
}

/// Travels from the given point in the tree away from the root, changing the state of all items to the state given and determining the aggregate tape ID and signal present status.
/// Generates an event if a recorder signal present status changes.
/// @param id Starting node.
/// @param enabled Enabled for recording if true.
/// @param unknown Unknown state if true.  Overrides enabled parameter.
/// @param problem Problem if true.  Overrides unknown and enabled parameters.
/// @return flags: TAPE_IDS_OK, ALL_SIGNALS, TRACK_NODE.
int TickTreeCtrl::SelectRecursively(wxTreeItemId id, bool enabled, bool unknown, bool problem)
{
	//apply visible/underlying selection states to this node
	if (problem) {
		SetNodeState(id, PROBLEM, true);
	}
	else if (unknown) {
		SetNodeState(id, UNKNOWN, true);
	}
	else if (enabled) {
		SetNodeState(id, ENABLED, true);
	}
	else {
		SetNodeState(id, DISABLED, true);
	}
	//apply selection state to all children recursively
	wxTreeItemId child;
	wxTreeItemIdValue cookie;
	int rc = TAPE_IDS_OK | ALL_SIGNALS | TRACK_NODE; //all the flags cos will only be clearing them
	child = GetFirstChild(id, cookie);
	while (child.IsOk()) {
		//set child's state and aggregate tape ID and signal present status
		rc &= SelectRecursively(child, enabled, unknown, problem);
		child = GetNextChild(id, cookie);
	}
	//update state of this node
	if (!GetFirstChild(id, cookie).IsOk()) { //no children so this is a track node
		if (!unknown && !problem) { //enabled value is valid
			//set record enable state
			((ItemData *) GetItemData(id))->SetBool(enabled);
		}
		if (((ItemData *) GetItemData(id))->GetBool() && NO_SIGNAL_COLOUR == GetItemBackgroundColour(id)) { //no signal on a track enabled for recording
			rc &= ~ALL_SIGNALS;
		}
		//nothing about tape IDs here - it's at a higher level
	}
	else {
		if (TRACK_NODE & rc) { //no grandchildren so this is a package node
			if (DISABLED != ((ItemData *) GetItemData(id))->GetUnderlyingState() && !((ItemData *) GetItemData(id))->GetBool()) { //no tape ID on a package with track(s) enabled for recording
				rc &= ~TAPE_IDS_OK;
			}
		}
		else { //this is a higher level node
			((ItemData *) GetItemData(id))->SetBool(TAPE_IDS_OK & rc); //reflect tape ID status from offspring
		}
		SetSignalPresentStatus(id, rc & ALL_SIGNALS);
		rc &= ~TRACK_NODE; //not a track node
	}
	return rc;
}

/// Sets the background colour of the given node depending on the signal present state.
/// If the node is a recorder, and the state is changing, sends an event to the frame.
/// @param node The node to set.
/// @param allSignals True if all record-enabled children have signals present
void TickTreeCtrl::SetSignalPresentStatus(const wxTreeItemId node, const bool allSignals)
{
	if ((GetItemBackgroundColour(node) == mDefaultBackgroundColour) != allSignals) { //signal present status has changed
		if (GetItemParent(node) == GetRootItem()) { //a recorder node
			//tell the frame
			wxCommandEvent guiEvent(wxEVT_TREE_MESSAGE, wxID_ANY);
			guiEvent.SetString(((ItemData *) GetItemData(node))->GetString()); //recorder name
			guiEvent.SetExtraLong(1); //indicates the type of event
			guiEvent.SetInt(allSignals);
			AddPendingEvent(guiEvent);
		}
		//set parent attribute
		SetItemBackgroundColour(node, allSignals ? mDefaultBackgroundColour : NO_SIGNAL_COLOUR);
	}
}

/// Changes the state of the supplied node's parent (if not locked) to reflect the state of supplied node and its siblings.
/// If the parent is the root node, sends a status notification event
/// If the parent is a recorder node, sends a signal state change event if changing the signal present state of the recorder node
/// @param id Starting node.
/// @param recurse True to recurse to the root (and always send a notification event).
void TickTreeCtrl::ReportState(wxTreeItemId id, bool recurse) {
	wxTreeItemId parent = GetItemParent(id);
	if (parent.IsOk()) { //below root level
		bool allSelected = true;
		bool allUnselected = true;
		bool problem = false;
		bool unknown = false;
		bool recording = false;
		bool tapeIdsOK = true;
		bool allSignals = true;
		wxTreeItemIdValue cookie;
		wxTreeItemId sibling = GetFirstChild(parent, cookie);
		//determine aggregate state from this node and all siblings
		while (sibling.IsOk()) {
			if (PROBLEM == GetItemImage(sibling)) {
				problem = true; //takes priority over unknown
			}
			else if (UNKNOWN == GetItemImage(sibling)) {
				unknown = true;
			}
			if (DISABLED != ((ItemData *) GetItemData(sibling))->GetUnderlyingState()) {
				allUnselected = false;
				//if this is a package name node, we need a tape ID if it's enabled for recording and it hasn't got one; if it's a higher level node this will just pass the value on
				tapeIdsOK &= ((ItemData *) GetItemData(sibling))->GetBool();
			}
			if (ENABLED != ((ItemData *) GetItemData(sibling))->GetUnderlyingState() && RECORDING != ((ItemData *) GetItemData(sibling))->GetUnderlyingState()) {
				allSelected = false;
			}
			if (RECORDING == ((ItemData *) GetItemData(sibling))->GetUnderlyingState() || PARTIALLY_RECORDING == ((ItemData *) GetItemData(sibling))->GetUnderlyingState()) {
				recording = true; //takes priority over non-recording images
			}
			if (NO_SIGNAL_COLOUR == GetItemBackgroundColour(sibling) && DISABLED != ((ItemData *) GetItemData(sibling))->GetUnderlyingState()) { //only report no signal upwards if enabled to record
				allSignals = false;
			}
			sibling = GetNextChild(parent, cookie);
		}
		//update parent's state
		TickTreeCtrl::state newState;
		if (allUnselected) {
			newState = DISABLED;
		}
		else if (allSelected) {
			if (recording) {
				newState = RECORDING;
			}
			else {
				newState = ENABLED;
			}
		}
		else {
			if (recording) {
				newState = PARTIALLY_RECORDING;
			}
			else {
				newState = PARTIALLY_ENABLED;
			}
		}
		((ItemData *) GetItemData(parent))->SetUnderlyingState(newState); //underlying state does not include PROBLEM or UNKNOWN
		if (problem) {
			newState = PROBLEM;
		}
		else if (unknown) {
			newState = UNKNOWN;
		}
		if (!((ItemData *) GetItemData(parent))->IsLocked()) { //a changeable image
			SetItemImage(parent, newState);
		}
		//update other item attributes
		if (GetFirstChild(id, cookie).IsOk()) { //this node has children so is not a track node
			//set tape ID status for parent
			((ItemData *) GetItemData(parent))->SetBool(tapeIdsOK);
		}
		SetSignalPresentStatus(parent, allSignals);
		//recurse
		if (recurse) {
			ReportState(parent, recurse);
		}
	}
	else { //the root item
		//signals message
		if (NO_SIGNAL_COLOUR == GetItemBackgroundColour(id) && PROBLEM != GetItemImage(id)) { //don't put a message in if there are other problems - that will cause confusion
			AddMessage(id, wxT("Signals missing"));
		}
		else {
			RemoveMessage(id);
		}
		//tell the frame
		wxCommandEvent guiEvent(wxEVT_TREE_MESSAGE, wxID_ANY);
		AddPendingEvent(guiEvent);
	}
}

/// Allow or disallow record enables to be changed with the mouse.
/// @param enable True to allow changes.
void TickTreeCtrl::EnableChanges(bool enable)
{
	mEnableChanges = enable;
}

/// Provides record enable data for a recorder.
/// @param recorderName The recorder.
/// @param enableList Returns enable/disable state for each track, in recorder track order.
/// @return True if any tracks are enabled but not recording.
bool TickTreeCtrl::GetRecordEnables(const wxString & recorderName, CORBA::BooleanSeq & enableList)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	bool someEnabled = false;
	if (recorder.IsOk()) { //sanity check
		//size the vectors
		enableList.length(((ItemData *) GetItemData(recorder))->GetInt()); //total tracks
		//default enable values in case there are unused tracks
		for (unsigned int i = 0; i < enableList.length(); i++) {
			enableList[i] = false;
		}
		//look at all the tracks in the tree, 
		wxTreeItemIdValue packageCookie, trackCookie;
		wxTreeItemId package = GetFirstChild(recorder, packageCookie);
		while (package.IsOk()) {
			wxTreeItemId track = GetFirstChild(package, trackCookie);
			while (track.IsOk()) {
				ItemData * itemData = (ItemData*) GetItemData(track);
				enableList[itemData->GetInt()] = itemData->GetBool();
				someEnabled |= (itemData->GetBool() && RECORDING != GetItemImage(track));
				track = GetNextChild(package, trackCookie);
			}
			package = GetNextChild(recorder, packageCookie);
		}
	}
	return someEnabled;
}

/// Gets the package names and tape IDs for the given recorder
/// @param recorderName The recorder.
/// @param packageNames Returns the list of package names.
/// @param tapeIDs Returns the tape IDs corresponding to packageNames.
void TickTreeCtrl::GetRecorderTapeIds(const wxString & recorderName, CORBA::StringSeq & packageNames, CORBA::StringSeq & tapeIds)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	if (recorder.IsOk()) { //sanity check
		wxTreeItemIdValue packageCookie;
		wxTreeItemId package = GetFirstChild(recorder, packageCookie);
		while (package.IsOk()) {
			// Assignment to the CORBA::StringSeq element must be from a const char *
			// and should use ISO Latin-1 character set.
			packageNames.length(packageNames.length() + 1);
			packageNames[packageNames.length() - 1] = (const char *) ((ItemData*) GetItemData(package))->GetString().mb_str(wxConvISO8859_1);
			tapeIds.length(tapeIds.length() + 1);
			tapeIds[tapeIds.length() - 1] = (const char *) RetrieveMessage(package).mb_str(wxConvISO8859_1);
			package = GetNextChild(recorder, packageCookie);
		}
	}
}

/// Displays recorder level and up the tree as being OK, removing any message at recorder level
/// @param recorderName The recorder.
/// Reports to the root, and generates a notification event.
void TickTreeCtrl::SetRecorderStateOK(const wxString & recorderName)
{
	wxTreeItemId recorderRoot = FindRecorder(recorderName);
	if (recorderRoot.IsOk()) { //sanity check
		RemoveMessage(recorderRoot);
		SetItemImage(recorderRoot, ((ItemData*) GetItemData(recorderRoot))->GetUnderlyingState());
		((ItemData *) GetItemData(recorderRoot))->Unlock();
		ReportState(recorderRoot);
	}
}

/// Displays recorder level and up the tree as having an unknown state, with the given message at recorder level
/// @param recorderName The recorder.
/// @param message Message to display.
/// Reports to the root, and generates a notification event.
void TickTreeCtrl::SetRecorderStateUnknown(const wxString & recorderName, const wxString & message)
{
	SetRecorderState(recorderName, false, message);
}

/// Displays recorder level and up the tree as having a problem, with the given message at recorder level
/// @param recorderName The recorder.
/// @param message Message to display.
/// Reports to the root, and generates a notification event.
void TickTreeCtrl::SetRecorderStateProblem(const wxString & recorderName, const wxString & message)
{
	SetRecorderState(recorderName, true, message);
}

/// Sets given recorder's state to problem or unknown.
/// Reports to the root, and generates a notification event.
/// @param recorderName The recorder.
/// @param problem True for problem; false for unknown.
/// @param message Message to display.
void TickTreeCtrl::SetRecorderState(const wxString & recorderName, bool problem, const wxString & message)
{
	wxTreeItemId recorderRoot = FindRecorder(recorderName);
	if (recorderRoot.IsOk()) { //sanity check
		AddMessage(recorderRoot, message);
		SetItemImage(recorderRoot, problem ? PROBLEM : UNKNOWN);
		((ItemData *) GetItemData(recorderRoot))->Lock();
		ReportState(recorderRoot);
	}
}

/// Sets state of tracks for given recorder.
/// Reports to the root, and generates a notification event.
/// @param recorderName The recorder.
/// @param recording True if the recorder is recording.
/// @param trackStatus enabled/recording or disabled for each of the recorder's tracks.
void TickTreeCtrl::SetTrackStatus(const wxString & recorderName, bool recording, ProdAuto::TrackStatusList_var trackStatus)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	wxTreeItemIdValue packageCookie, trackCookie;
	wxTreeItemId package;
	if (recorder.IsOk()) { //sanity check
		for (unsigned int index = 0; index < trackStatus->length(); index++) {
			//find the track
			package = GetFirstChild(recorder, packageCookie);
			bool found = false;
			while (!found && package.IsOk()) {
				wxTreeItemId track = GetFirstChild(package, trackCookie);
				while (track.IsOk() && !(found = ((ItemData *) GetItemData(track))->GetInt() == (int) index)) {
					track = GetNextChild(package, trackCookie);
				}
				if (found) {
					//set the state
					if (!recording && !trackStatus[index].rec && ((ItemData *) GetItemData(track))->GetBool()) {
						//not recording and not supposed to be, but enabled
						SetNodeState(track, ENABLED);
					}
					else if (!trackStatus[index].rec && !((ItemData *) GetItemData(track))->GetBool()) {
						//not recording and not supposed to be (via being disabled - overall record state irrelavant)
						SetNodeState(track, DISABLED);
					}
					else if (recording && trackStatus[index].rec && ((ItemData *) GetItemData(track))->GetBool()) {
						//recording and supposed to be
						SetNodeState(track, RECORDING);
					}
					else if (recording && !trackStatus[index].rec) {
						SetNodeState(track, PROBLEM, false, wxT("Not recording"));
					}
					else if (recording) {
						SetNodeState(track, PROBLEM, false, wxT("Recording but disabled"));
					}
					else {
						//recording when not supposed to be
						SetNodeState(track, PROBLEM, false, wxT("Recording"));
					}
					if (VIDEO_TRACK_COLOUR == GetItemTextColour(track)) {
						if (trackStatus[index].signal_present) {
							SetItemBackgroundColour(track, mDefaultBackgroundColour);
						}
						else {
							SetItemBackgroundColour(track, NO_SIGNAL_COLOUR);
							if (PROBLEM != GetItemImage(track)) { //nothing more important showing
								AddMessage(track, wxT("No signal"));
							}
						}
					}
				}
				else {
					package = GetNextChild(recorder, packageCookie);
				}
			}
		}
		//determine state of each package of tracks
		package = GetFirstChild(recorder, packageCookie);
		while (package.IsOk()) {
			ReportState(GetFirstChild(package, trackCookie), false); //don't recurse because it just adds overhead at this stage
			package = GetNextChild(recorder, packageCookie);
		}
		//report all track packages to root
		package = GetFirstChild(recorder, packageCookie);
		ReportState(package);
	}
}

/// Works out if there are any package names which do not belong to router recorders, i.e. that use tape IDs.
/// @return true if using tape IDs.
bool TickTreeCtrl::UsingTapeIds()
{
	wxArrayString names;
	std::vector<bool> enabled;
	ScanPackageNames(&names, &enabled);
	return names.GetCount();
}

/// @return true if recorder name supplied is a router recorder
bool TickTreeCtrl::IsRouterRecorder(const wxString & recorderName)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	if (recorder.IsOk()) { //sanity check
		wxTreeItemIdValue cookie;
		wxTreeItemId package = GetFirstChild(recorder, cookie);
		if (package.IsOk()) { //sanity check
			return ((ItemData *) GetItemData(package))->GetInt();
		}
	}
	return false;
}

/// Obtains all (non router-recorder) package names and corresponding enable status.
/// @param names Returns package names.
/// @param enabled Returns true for each corresponding package in names that's enabled or partially enabled; false otherwise.
void TickTreeCtrl::GetPackageNames(wxArrayString & names, std::vector<bool> & enabled)
{
	ScanPackageNames(&names, &enabled);
}

/// Sets all (non router-recorder) package names' corresponding tape IDs (which may not exist).
/// Reports to the root, and generates a status update notification event and a recorder notification event for each recorder whose tape IDs have changed.
/// @param doc XML document object for tape ID information
void TickTreeCtrl::UpdateTapeIds(wxXmlDocument & doc)
{
	wxXmlNode * tapeIdsNode = SetTapeIdsDlg::GetTapeIdsNode(doc);
	ScanPackageNames(0, 0, tapeIdsNode);
}

/// Either returns all package names, with corresponding enabled status, or updates all tape IDs.
/// Skips package names that belong to router recorders.
/// If updating, reports to the root, and generates a status update notification event and a recorder notification event for each recorder whose tape IDs have changed.
/// @param names If 0, updates; if not, returns package names.
/// @param enabled If returning package names, returns true for each corresponding package in names that's enabled or partially enabled; false otherwise.  Ignored if not returning package names.
/// @param tapeIDsNode If updating, this XML node is searched for tape IDs.
void TickTreeCtrl::ScanPackageNames(wxArrayString * names, std::vector<bool> * enabled, wxXmlNode * tapeIdsNode)
{
	wxString packageName;
	wxTreeItemIdValue recordersCookie;
	wxTreeItemId recorder = GetFirstChild(GetRootItem(), recordersCookie);
	while (recorder.IsOk()) {
		bool tapeIdChanges = false;
		wxTreeItemIdValue packagesCookie;
		wxTreeItemId package = GetFirstChild(recorder, packagesCookie);
		while (package.IsOk()) {
			packageName = ((ItemData *) GetItemData(package))->GetString();
			if (((ItemData *) GetItemData(package))->GetInt() || packageName.IsEmpty()) { //a router recorder or fails a sanity check
				//doesn't have a tape ID so ignore
			}
			else if (names) { //scanning package names
				names->Add(packageName);
				enabled->push_back(ENABLED == GetItemImage(package) || PARTIALLY_ENABLED == GetItemImage(package));
			}
			else { //updating
				wxString tapeId = SetTapeIdsDlg::GetTapeId(tapeIdsNode, packageName);
				if (RetrieveMessage(package) != tapeId) {
					tapeIdChanges = true;
					if (tapeId.IsEmpty()) { //no tape ID
						RemoveMessage(package);
						((ItemData *) GetItemData(package))->SetBool(false);
					}
					else { //tape ID is present
						AddMessage(package, tapeId);
						((ItemData *) GetItemData(package))->SetBool(true);
					}
				}
			}
			package = GetNextChild(recorder, packagesCookie);
		}
		if (!names) { //updating
			//report tape ID state upwards for this recorder
			ReportState(GetFirstChild(recorder, packagesCookie), false); //don't recurse because it just adds overhead at this stage
			if (tapeIdChanges) {
				//tell frame to tell the recorder the tape IDs
				wxCommandEvent guiEvent(wxEVT_TREE_MESSAGE, wxID_ANY);
				guiEvent.SetString(((ItemData *) GetItemData(recorder))->GetString());
				AddPendingEvent(guiEvent);
			}
		}
		recorder = GetNextChild(GetRootItem(), recordersCookie);
	}
	if (!names) { //updating
		//report all recorder states upwards
		ReportState(GetFirstChild(GetRootItem(), recordersCookie));
	}
}

/// Adds a message to a tree item's displayed text.
/// @param item The tree item.
/// @param message The Message.
void TickTreeCtrl::AddMessage(const wxTreeItemId item, const wxString & message)
{
	SetItemText(item, ((ItemData *) GetItemData(item))->GetString() + wxT(" (") + message + wxT(")")); //NB format must correspond to RetrieveMessage()
}


/// Retrieves a message from a tree item's displayed text.
/// @param item The tree node.
/// @return The message.
const wxString TickTreeCtrl::RetrieveMessage(const wxTreeItemId item)
{
	//NB format must correspond to AddMessage()
	wxString message = GetItemText(item).Mid(((ItemData*) GetItemData(item))->GetString().Len() + 2); //remove characters up to start of message
	message.Truncate(message.Len() - 1); //remove trailing bracket
	return message;
}

/// Removes any message from a tree item's displayed text.
/// @param item The tree item.
void TickTreeCtrl::RemoveMessage(const wxTreeItemId item)
{
	SetItemText(item, ((ItemData *) GetItemData(item))->GetString());
}

/// Sets both the displayed image and the stored underlying state of the given node, optionally adding or removing a message to/from the item text
/// @param id The item to set.
/// @param state The state to set it to.
/// @param retain True to retain any message added to the item text
/// @param message Message to add - ignored if "retain" is true
void TickTreeCtrl::SetNodeState(const wxTreeItemId id, const TickTreeCtrl::state state, const bool retain, const wxString & message)
{
	SetItemImage(id, state);
	if (PROBLEM != state && UNKNOWN != state) {
		((ItemData *) GetItemData(id))->SetUnderlyingState(state);
	}
	if (!retain) {
		if (message.Len()) {
			AddMessage(id, message);
		}
		else {
			RemoveMessage(id);
		}
	}
}
