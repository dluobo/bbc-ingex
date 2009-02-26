/***************************************************************************
 *   $Id: eventlist.h,v 1.2 2009/02/26 19:17:09 john_f Exp $             *
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

#ifndef EVENTLIST_H_
#define EVENTLIST_H_
#include <vector>
#include "RecorderC.h"
#include <wx/xml/xml.h>

WX_DECLARE_OBJARRAY(ProdAuto::TrackList_var, ArrayOfTrackList_var);
WX_DECLARE_OBJARRAY(CORBA::StringSeq_var, ArrayOfStringSeq_var);
DEFINE_EVENT_TYPE (wxEVT_RESTORE_LIST_LABEL);

/// Class holding information about a take, for later replay.
/// Holds the position of the take within the displayed list of events, the project name, an array of lists of files
/// (one for each recorder) and an array of lists of track names (ditto).
/// Also arrays of cue point colour indeces, timecodes and frame numbers.
class TakeInfo
{
	public:
		TakeInfo(const unsigned long startIndex, const wxString & projectName, ProdAuto::MxfTimecode startTimecode) : mStartIndex(startIndex), mProjectName(projectName), mStartTimecode(startTimecode) {};
		void AddRecorder(ProdAuto::TrackList_var trackList, CORBA::StringSeq_var fileList) {mFiles.Add(fileList); mTracks.Add(trackList);}; //adds a set of tracks provided by a recorder at the end of a recording
		void AddCuePoint(const int64_t frame, size_t colourIndex) {mCuePointFrames.push_back(frame); mCueColourIndeces.push_back(colourIndex);}; //no text here because it's stored in the event list (which means it can be edited)
		bool DeleteCuePoint(const unsigned long index) {
			if (mCuePointFrames.size() > index) {
				mCuePointFrames.erase(mCuePointFrames.begin() + index); mCueColourIndeces.erase(mCueColourIndeces.begin() + index);
				return true;
			}
			else {
				return false;
			}
		};
		unsigned long GetStartIndex() {return mStartIndex;};
		ProdAuto::MxfTimecode GetStartTimecode() {return mStartTimecode;};
		ArrayOfStringSeq_var * GetFiles() {return &mFiles;};
		const wxString GetProjectName() {return mProjectName;};
		ArrayOfTrackList_var & GetTracks() {return mTracks;};
		std::vector<int64_t> & GetCuePointFrames() {return mCuePointFrames;};
		std::vector<size_t> & GetCueColourIndeces() {return mCueColourIndeces;};
	private:
		const unsigned long mStartIndex; //the index in the event list for the start of this take
		ArrayOfStringSeq_var mFiles;
		const wxString mProjectName;
		ProdAuto::MxfTimecode mStartTimecode;
		ArrayOfTrackList_var mTracks; //deletes itself
		std::vector<int64_t> mCuePointFrames;
		std::vector<size_t> mCueColourIndeces;
};

WX_DECLARE_OBJARRAY(TakeInfo, TakeInfoArray);

class RecorderData;

/// Class to display and store the list of recording events
class EventList : public wxListView, wxThread //used wxListCtrl for a while because wxListView crashed when you added an item - seems ok with 2.8
{
	public:
		EventList(wxWindow *, wxWindowID, const wxPoint & = wxDefaultPosition, const wxSize & = wxDefaultSize);
		enum EventType //NB order must match TypeLabels[] initialisation
		{
			NONE = 0,
			START,
			CUE,
			STOP,
			PROBLEM
		};
		void AddEvent(EventType, ProdAuto::MxfTimecode *, const int64_t = 0, const wxString & = wxT(""), const size_t = 0);
		void AddRecorderData(RecorderData * data);
		void DeleteCuePoint();
		void Select(const long, const bool = false);
		void SelectPrevTake(const bool);
		void SelectAdjacentEvent(const bool);
		void SelectNextTake();
		void SelectLastTake();
		void CanEdit(const bool);
		bool LatestTakeCuePointIsSelected();
		bool AtTop();
		bool AtStartOfTake();
		bool InLastTake();
		bool AtBottom();
		bool SelectedTakeHasChanged();
		bool SelectedEventHasChanged();
		bool SetCurrentProjectName(const wxString &);
		ProdAuto::LocatorSeq GetLocators();
		TakeInfo * GetCurrentTakeInfo();
		void Clear();
	private:
		ExitCode Entry();
		void OnEventBeginEdit(wxListEvent&);
		void OnEventEndEdit(wxListEvent&);
		void OnRestoreListLabel(wxCommandEvent&);
		void ClearSavedData();
		const wxString GetCdata(wxXmlNode *);
		void Load();

		TakeInfoArray mTakeInfoArray;
		bool mCanEdit;
		long mCurrentTakeInfo;
		long mCurrentSelectedEvent;
		wxXmlNode * mRecordingNode;
		wxMutex mMutex;
		wxString mProjectName;
		unsigned int mRecordingNodeCount;
		//vbls used in both contexts while thread is running
		wxCondition * mCondition;
		wxXmlNode * mRootNode;
		bool mRunThread;
		bool mSyncThread;
		wxString mFilename;
	DECLARE_EVENT_TABLE()
};

#endif
