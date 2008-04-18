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

#ifndef _TICKTREE_H_
#define _TICKTREE_H_

#include <wx/treectrl.h>
#include "RecorderC.h"
#include <vector>

DECLARE_EVENT_TYPE(wxEVT_TREE_MESSAGE, -1)

class wxXmlDocument;
class wxXmlNode;

#define VIDEO_TRACK_COLOUR wxColour(wxT("ORANGE"))
#define AUDIO_TRACK_COLOUR wxColour(wxT("YELLOW"))
#define ROUTER_TRACK_COLOUR wxColour(wxT("GREEN"))

/// A class derived from a tree control, allowing record enables to be set and status to be displayed for multiple recorders.
class TickTreeCtrl : public wxTreeCtrl
{
	public:
		TickTreeCtrl(wxWindow *, wxWindowID, const wxPoint& = wxDefaultPosition, const wxSize& = wxDefaultSize, const wxString & = wxT(""));
		bool AddRecorder(const wxString &, ProdAuto::TrackList_var, ProdAuto::TrackStatusList_var, bool, wxXmlDocument &);
		bool RemoveRecorder(const wxString &);

		void Clear();
		void EnableChanges(bool = true);
		bool GetRecorderData(const wxString &, CORBA::BooleanSeq &);
		void SetTrackStatus(const wxString &, bool, ProdAuto::TrackStatusList_var);
		bool SomeEnabled();
		bool IsRecording();
		bool AllRecording();
		bool TapeIdsOK();
		bool HasRecorders();
		bool IsUnknown();
		bool IsRouterRecorder(const wxString &);
		bool HasProblem();
		bool RecordingSuccessfully();
		void SetRecorderStateUnknown(const wxString &, const wxString &);
		void SetRecorderStateProblem(const wxString &, const wxString &);
		void SetRecorderStateOK(const wxString &);
		void GetPackageNames(wxArrayString &, std::vector<bool> &);
		void GetRecorderTapeIds(const wxString &, CORBA::StringSeq &, CORBA::StringSeq &);
		void UpdateTapeIds(wxXmlDocument &);
		enum state { //must correspond to order images are loaded
			DISABLED,
			PARTIALLY_ENABLED,
			ENABLED,
			RECORDING,
			PARTIALLY_RECORDING,
			PROBLEM,
			UNKNOWN,
		};
	private:
		wxTreeItemId FindRecorder(const wxString &);
		void OnLMouseDown(wxMouseEvent &);
		bool SelectRecursively(wxTreeItemId, bool = true, bool = false, bool = false);
		void SetRecorderState(const wxString &, bool, const wxString &);
		void ReportState(wxTreeItemId, bool = true);
		void ScanPackageNames(wxArrayString *, std::vector<bool> *, wxXmlNode * = 0);
		void AddMessage(const wxTreeItemId item, const wxString &);
		const wxString RetrieveMessage(const wxTreeItemId item);
		void RemoveMessage(const wxTreeItemId item);
		void SetItemState(const wxTreeItemId, const TickTreeCtrl::state, const bool = false);
		void SetItemState(const wxTreeItemId, const TickTreeCtrl::state, const wxString &);
		bool UsingTapeIds();

//		DECLARE_DYNAMIC_CLASS(TickTreeCtrl)
		bool mEnableChanges;
		wxXmlNode * mTapeIdsNode;
		wxString mName;
    		DECLARE_EVENT_TABLE()
};

/// Information about each node in the tree.  mUnderlyingState indicates the state of the node ignoring unknown and problem conditions.  mLocked indicates that the node state shouldn't be changed.  Other members are used as follows:
/// root node:
///	int: -
///	bool: enabled track(s) that aren't router recorders all have tape ID(s)
///	string: -
/// recorder node:
///	int: total number of tracks (including those without sources)
///	bool: enabled tracks(s) that aren't router recorders all have tape ID(s)
///	string: recorder name (without messages appended)
/// package name node:
///	int: non-zero if a router recorder
///	bool: true if tape ID is present or if a router recorder
///	string: package name (without tape ID appended)
/// track node:
///	int: track index
///	bool: record enable state
///	string: track name (without messages appended)
/// NB ints and strings are constant
class ItemData : public wxTreeItemData
{
	public:
		ItemData(TickTreeCtrl::state state, int intValue = 0, bool boolValue = false) : mInt(intValue), mBool(boolValue), mLocked(false), mUnderlyingState(state) {}
		ItemData(TickTreeCtrl::state state, const wxString & stringValue, int intValue = 0, bool boolValue = false) : mString(stringValue), mInt(intValue),  mBool(boolValue), mLocked(false), mUnderlyingState(state) {}
		const int GetInt() { return mInt; }
		void SetBool(bool state) { mBool = state; }
		bool GetBool() { return mBool; }
		const wxString & GetString() { return mString; }
		void SetUnderlyingState(const TickTreeCtrl::state state) { mUnderlyingState = state; }
		const TickTreeCtrl::state GetUnderlyingState() { return mUnderlyingState; }
		void Lock() { mLocked = true; };
		void Unlock() { mLocked = false; };
		bool IsLocked() { return mLocked; };
	private:
		const wxString mString;
		const int mInt;
		bool mBool;
		bool mLocked;
		TickTreeCtrl::state mUnderlyingState;
};

#endif //_TICKTREE_H_
