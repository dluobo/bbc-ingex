/***************************************************************************
 *   $Id: player.h,v 1.10 2009/05/01 13:41:34 john_f Exp $                *
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

#ifndef PLAYER_H_
#define PLAYER_H_

#include <vector>
#include <string>
#include <map>
#include "LocalIngexPlayer.h"
#include <wx/wx.h>
#include <wx/socket.h>

//File poll timer checks for files appearing after completion of a recording
#define FILE_POLL_TIMER_INTERVAL 250 //ms
#define MAX_SPEED 64 //times normal; must be power of 2
#define TRAFFIC_CONTROL_PORT 2000

DECLARE_EVENT_TYPE(wxEVT_PLAYER_MESSAGE, -1)

enum PlayerEventType {
	STATE_CHANGE = 0,
	CUE_POINT,
	FRAME_DISPLAYED,
	NEW_FILESET,
	AT_START,
	WITHIN,
	AT_END,
	CLOSE_REQ,
	KEYPRESS,
	PROGRESS_BAR_DRAG,
	SPEED_CHANGE,
	QUADRANT_CLICK,
	LOAD_NEXT_CHUNK,
	LOAD_PREV_CHUNK,
	LOAD_FIRST_CHUNK
};

namespace PlayerMode { //this avoids clashes with different enums with the same member names
	enum EnumType {
		PLAY,
		PLAY_BACKWARDS,
		PAUSE,
		STOP,
		CLOSE,
	};
}

enum OSDtype {
	OSD_OFF,
	CONTROL_TIMECODE,
	SOURCE_TIMECODE
};

class Player;

/// Class with methods that are called by the player.
class Listener : public prodauto::IngexPlayerListener
{
	public:
		Listener(Player *, prodauto::IngexPlayerListenerRegistry *);
		virtual void frameDisplayedEvent(const FrameInfo* frameInfo);
		virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo);
		virtual void stateChangeEvent(const MediaPlayerStateEvent* event);
		virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo);
		virtual void startOfSourceEvent(const FrameInfo* lastReadFrameInfo);
		virtual void playerClosed();
		virtual void playerCloseRequested();
		virtual void keyPressed(int, int);
		virtual void keyReleased(int, int);
		virtual void progressBarPositionSet(float);
		virtual void mouseClicked(int, int, int, int);
	private:
		Player * mPlayer; //never changed so doesn't need protecting by mutex
};

/// Class representing the X11/SDI video/audio player.
class Player : public wxEvtHandler, prodauto::LocalIngexPlayer
{
	public:
		Player(wxEvtHandler *, const bool, const prodauto::PlayerOutputType, const OSDtype);
		~Player();
		bool IsOK();
		void Enable(bool);
		void Load(std::vector<std::string> * = 0, std::vector<std::string> * = 0, prodauto::PlayerInputType = prodauto::MXF_INPUT, int64_t = 0, std::vector<int64_t> * = 0, int = 0, unsigned int = 0, bool = false, bool = false);
		void SelectTrack(const int, const bool);
		void SetOSD(const OSDtype);
		void EnableSDIOSD(bool = true);
		void SetOutputType(const prodauto::PlayerOutputType);
		void Play(const bool = false, const bool = false);
		void PlayAbsolute(const int);
		void Pause();
		void Step(bool);
		void Reset();
		void JumpToCue(int cuePoint);
		bool Within();
		bool AtRecEnd();
		bool LastPlayingBackwards();
		bool ExtOutputIsAvailable();
		bool AtMaxForwardSpeed();
		bool AtMaxReverseSpeed();
		void MuteAudio(const bool);
		void AudioFollowsVideo(const bool);
	private:
		bool Start(std::vector<std::string> * = 0, std::vector<std::string> * = 0, prodauto::PlayerInputType = prodauto::MXF_INPUT, int64_t = 0, std::vector<int64_t> * = 0, unsigned int = 0, unsigned int = 0, bool = false, bool = false);
		void SetWindowName(const wxString & name = wxT(""));
		void OnFrameDisplayed(wxCommandEvent&);
		void OnFilePollTimer(wxTimerEvent&);
		void OnStateChange(wxCommandEvent&);
		void OnSpeedChange(wxCommandEvent&);
		void OnProgressBarDrag(wxCommandEvent&);
		void OnSocketEvent(wxSocketEvent&);
		void TrafficControl(const bool, const bool = false);
		Listener * mListener;
		prodauto::IngexPlayerListenerRegistry mListenerRegistry;
		OSDtype mOSDtype;
		bool mEnabled;
		bool mOK;
		PlayerMode::EnumType mMode;
		std::string mDesiredTrackName;
		unsigned int mNFilesExisting;
		std::vector<std::string> mFileNames;
		std::vector<std::string> mTrackNames;
		prodauto::PlayerInputType mInputType;
		std::vector<bool> mOpened;
		std::vector<int64_t> mCuePoints;
		unsigned int mStartIndex;
		unsigned int mLastCuePointNotified;
		long mPreviousFrameDisplayed;
		bool mAtStart;
		bool mAtChunkEnd;
		wxTimer * mFilePollTimer;
		unsigned int mLastRequestedCuePoint;
		int mSpeed;
		wxString mName;
		bool mMuted;
		bool mLastPlayingBackwards;
		wxSocketClient * mSocket;
		bool mTrafficControl;
		bool mOpeningSocket;
		bool mChunkBefore;
		bool mChunkAfter;
		PlayerEventType mChunkLinking;
		DECLARE_EVENT_TABLE()
};

#endif
