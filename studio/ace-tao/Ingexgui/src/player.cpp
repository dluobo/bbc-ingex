/***************************************************************************
 *   $Id: player.cpp,v 1.16 2009/10/22 14:44:33 john_f Exp $              *
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

#include "player.h"
#include "wx/file.h"

DEFINE_EVENT_TYPE(wxEVT_PLAYER_MESSAGE)

using namespace prodauto; //IngexPlayer class is in this namespace

/***************************************************************************
 *   PLAYER                                                                *
 ***************************************************************************/


BEGIN_EVENT_TABLE( Player, wxEvtHandler )
	EVT_COMMAND( FRAME_DISPLAYED, wxEVT_PLAYER_MESSAGE, Player::OnFrameDisplayed )
	EVT_COMMAND( STATE_CHANGE, wxEVT_PLAYER_MESSAGE, Player::OnStateChange )
	EVT_COMMAND( SPEED_CHANGE, wxEVT_PLAYER_MESSAGE, Player::OnSpeedChange )
	EVT_COMMAND( PROGRESS_BAR_DRAG, wxEVT_PLAYER_MESSAGE, Player::OnProgressBarDrag )
	EVT_TIMER( wxID_ANY, Player::OnFilePollTimer )
	EVT_SOCKET( wxID_ANY, Player::OnSocketEvent )
END_EVENT_TABLE()

/// Creates the player, but does not display it.
/// Player must be deleted explicitly or a traffic control notification will be missed.
/// @param handler The handler where events will be sent.
/// @param enabled True to enable player.
/// @param outputType The output type - accelerated or unaccelerated; SDI or not.
/// @param displayType The on screen display type.
Player::Player(wxEvtHandler * handler, const bool enabled, const PlayerOutputType outputType, const OSDtype displayType) :
LocalIngexPlayer(&mListenerRegistry),
mOSDtype(displayType), mEnabled(enabled), mOK(false), mMode(PlayerMode::STOP), mSpeed(0), mMuted(false), mOpeningSocket(false),
mPrevTrafficControl(true) //so that it can be switched off
{
	setOutputType(outputType);
	setNumAudioLevelMonitors(4);
	Rational unity = {1, 1};
	setPixelAspectRatio(&unity);

	mListener = new Listener(this, &mListenerRegistry); //registers with the player
	mFilePollTimer = new wxTimer(this, wxID_ANY);
	SetNextHandler(handler);
	mDesiredTrackName = ""; //display the quad split by default
	mSocket = new wxSocketClient();
	mSocket->SetEventHandler(*this);
	mSocket->SetNotify(wxSOCKET_CONNECTION_FLAG);
	TrafficControl(false); //in case it has just been restarted after crashing and leaving traffic control on
}

Player::~Player()
{
	TrafficControl(false, true); //must do this synchronously or the socket will be opened but the data will never be sent
	delete mListener;
	delete mSocket;
}

/// Indicates whether the player has successfully loaded anything.
/// @return True if at least one file loaded.
bool Player::IsOK()
{
	return mOK;
}

/// Indicates that an SDI card with a free output is available.
/// @return True if SDI available.
bool Player::ExtOutputIsAvailable()
{
	return dvsCardIsAvailable();
}

/// Enables or disables the player.
/// Enabling opens the window if player is displaying on-screen and there are files loaded.  Disabling closes the window (and the SDI output if used).
/// @param state True to enable.
void Player::Enable(bool state)
{
//std::cerr << "Player Enable" << std::endl;
	if (state != mEnabled) {
		mEnabled = state;
		if (mEnabled) {
			if (mFileNames.size()) {
				//a clip was already registered: reload
				Load(0);
			}
		}
		else {
			mFilePollTimer->Stop(); //suspend any current attempts to load
			close(); //stops and closes window
			if (mSpeed) {
				mSpeed = 0;
				TrafficControl(false);
			}
			//clear the position display
			wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
			guiFrameEvent.SetInt(false); //invalid position
			AddPendingEvent(guiFrameEvent);
			mOK = false;
		}
	}
}

/// If player is enabled, tries to load the given filenames or re-load previously given filenames.  Starts polling if it can't open them all.
/// If player is disabled, stores the given filenames for later.
/// @param fileNames List of file paths or source names for shared memory, or if zero, use previous supplied list and ignore all other parameters.
/// @param trackNames List of track names corresponding to fileNames, for display as the player window title.
/// @param inputType The type of the files
/// The following are ignored if inputType is SHM_INPUT:
/// @param offset Frame offset to jump to.
/// @param cuePoints Frame numbers of cue points (not including start and end positions).
/// @param startIndex Event list index for entry corresponding to start of file.
/// @param cuePoint Index in cuePoints to jump to.
/// @param chunkBefore There is a chunk before this recording, which will be played next if playing backwards
/// @param chunkAfter There is a chunk after this recording, which will be played next if playing forwards
void Player::Load(std::vector<std::string> * fileNames, std::vector<std::string> * trackNames, PlayerInputType inputType, int64_t offset, std::vector<int64_t> * cuePoints, int startIndex, unsigned int cuePoint, const bool chunkBefore, const bool chunkAfter)
{
//std::cerr << "Player Load" << std::endl;
	std::vector<std::string> * fNames = fileNames ? fileNames : &mFileNames;
	if (fileNames) { //New paths
		mLastPlayingBackwards = false; //no point continuing the default play direction of the previous set of files
	}
	if (fNames->size()) { //not a router recorder recording only, for instance
		if (mEnabled) {
			mFilePollTimer->Stop(); //may be running from another take
			if (SHM_INPUT != inputType) {
				//see how many files we have before the player loads them, so we can start polling even if more appear just after loading
				mNFilesExisting = 0;
				for (size_t i = 0; i < fNames->size(); i++) {
					if (wxFile::Exists(wxString((*fNames)[i].c_str(), *wxConvCurrent))) {
						mNFilesExisting++;
					}
				}
			}
			else {
				//can't poll for shared memory sources
				mNFilesExisting = mFileNames.size();
			}
		}
		if (!mNFilesExisting || SHM_INPUT == inputType) {
			TrafficControl(false);
			mOK = reset(); //otherwise it keeps showing what was there before even if it can't open any files/shared memory
		}
		//load the player (this will merely store the parameters if the player is not enabled)
		if (!Start(fileNames, trackNames, inputType, offset, cuePoints, startIndex, cuePoint, chunkBefore, chunkAfter) && mEnabled && mFileNames.size() != mNFilesExisting) { //player isn't happy, and (probably) not all files were there when the player opened
			//start polling for files
			mFilePollTimer->Start(FILE_POLL_TIMER_INTERVAL);
		}
	}
}

/// If player is enabled, tries to load the given filenames or re-load previously given filenames, and generates a NEW_FILESET event indicating which filenames were opened and which has been selected.
/// Tries to select the file position previously selected by the user.  If this wasn't opened, selects the only file open or a quad split otherwise.
/// If player is disabled, stores the given filenames for later.
/// This method is the same as Load() except that it does not manipulate the polling timer.
/// @param fileNames List of file paths, or if zero, use previous supplied list and ignore all other parameters.
/// @param trackNames Corresponding list of track names, for display as the player window title.
/// @param inputType The type of the files
/// The following are ignored if inputType is SHM_INPUT:
/// @param offset Frame offset to jump to.  Ignored if player just requested a new chunk.
/// @param cuePoints Frame numbers of cue points (not including start and end positions).
/// @param startIndex Event list index for entry corresponding to start of file.
/// @param cuePoint Index in cuePoints to jump to.
/// @param chunkBefore There is a chunk before this recording, which will be played next if playing backwards
/// @param chunkAfter There is a chunk after this recording, which will be played next if playing forwards
/// @return True if all files were opened.
bool Player::Start(std::vector<std::string> * fileNames, std::vector<std::string> * trackNames, PlayerInputType inputType, int64_t offset, std::vector<int64_t> * cuePoints, unsigned int startIndex, unsigned int cuePoint, bool chunkBefore, bool chunkAfter)
{
//std::cerr << "Player Start" << std::endl;
	if (fileNames) {
		//a new set of files
		mFileNames.clear();
		for (size_t i = 0; i < fileNames->size(); i++) {
			mFileNames.push_back((*fileNames)[i]);
		}
		mTrackNames.clear();
		for (size_t i = 0; i < trackNames->size(); i++) {
			mTrackNames.push_back((*trackNames)[i]);
		}
		mInputType = inputType;
		mPreviousFrameDisplayed = offset;
		mCuePoints.clear();
		if (cuePoints) {
			for (size_t i = 0; i < cuePoints->size(); i++) {
				mCuePoints.push_back((*cuePoints)[i]); //so that we know where to jump to for a given cue point
			}
		}
		mStartIndex = startIndex;
		mLastCuePointNotified = startIndex;
		mLastRequestedCuePoint = cuePoint;
		mChunkBefore = chunkBefore;
		mChunkAfter = chunkAfter;
		if (SHM_INPUT == inputType) {
			mMode = PlayerMode::PLAY;
		}
		else if (STATE_CHANGE == mChunkLinking) { //no chunk linking
			mMode = PlayerMode::PAUSE;
		}
	}
	bool allFilesOpen = false;
	if (mEnabled) {
		mOpened.clear();
		mAtStart = false; //set when a frame is displayed
		mAtChunkEnd = false; //set when a frame is displayed
		std::vector<PlayerInput> inputs;
		for (size_t i = 0; i < mFileNames.size(); i++) {
			PlayerInput input;
			input.name = mFileNames[i];
			input.type = mInputType;
			inputs.push_back(input);
		}
		mOK = start(inputs, mOpened, SHM_INPUT != mInputType && LOAD_FIRST_CHUNK != mChunkLinking && (PlayerMode::PAUSE == mMode || PlayerMode::STOP == mMode), SHM_INPUT == mInputType ? 0 : mPreviousFrameDisplayed); //play forwards or paused
		int trackToSelect = (1 == mFileNames.size() ? 1 : 0); //display quad split by default unless only one file
		if (mOK) {
			if (SHM_INPUT != mInputType) {
				//(re)load stored cue points
				for (size_t i = 0; i < mCuePoints.size(); i++) {
					markPosition(mCuePoints[i], 0); //so that the pointer changes colour at each cue point
				}
				if (STATE_CHANGE != mChunkLinking && mSpeed) { //latter a sanity check
					playSpeed(mSpeed);
				}
				else if (PlayerMode::PLAY_BACKWARDS == mMode) {
					playSpeed(-1);
				}
//				SetOSD(mOSDtype); //player doesn't respond to setOSDScreen() here, so wait until a frame has been displayed
				mSetOSDType = true; //if player is fixed, revert to the above
				if (LOAD_PREV_CHUNK == mChunkLinking) {
					JumpToCue(-1); //the end
				}
				else if (!mPreviousFrameDisplayed) {
					//if mLastRequestedCuePoint isn't zero (start), player was requested to go to a cue point in this clip while no files were available
					JumpToCue(mLastRequestedCuePoint);
				}
			}
			else {
				showProgressBar(false); //irrelevant
//				setOSDTimecode(-1, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE); //timecode from shared memory
				if (mSpeed) {
					mSpeed = 0;
					TrafficControl(false); //not reading from disk
				}
				//clear the position display - no more FRAME_DISPLAYED events will be sent in this mode
				wxCommandEvent event(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
				event.SetInt(false); //invalid position
				GetNextHandler()->AddPendingEvent(event); //don't add it to this handler or it will be blocked
			}
			mChunkLinking = STATE_CHANGE;
			// work out which track to select
			unsigned int nFilesOpen = 0;
			int aWorkingTrack = 0; //initialisation prevents compiler warning
			for (size_t i = 0; i < mOpened.size(); i++) {
				if (mOpened[i]) {
					nFilesOpen++;
					aWorkingTrack = i + 1; // +1 because track 0 is quad split
				}
			}
			if (mDesiredTrackName.size()) { //want something other than quad split
				size_t i;
				for (i = 0; i < mTrackNames.size(); i++) {
					if (mTrackNames[i] == mDesiredTrackName && mOpened[i]) { //located the desired track and it's available
						trackToSelect = i + 1; // + 1 to offset for quad split
						break;
					}
				}
				if (mTrackNames.size() == i && 1 == nFilesOpen) { //can't use the desired track and only one file open
					//display the only track, full screen
					trackToSelect = aWorkingTrack;
				}
			}
			SelectTrack(trackToSelect, false);
			allFilesOpen = mOpened.size() == nFilesOpen;
			muteAudio(mMuted);
		}
		else {
			SetWindowName(wxT("Ingex Player - no files"));
		}
		//tell the track selection list the situation
		wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, NEW_FILESET);
		guiEvent.SetClientData(&mOpened);
		guiEvent.SetInt(trackToSelect);
		AddPendingEvent(guiEvent);
	}
	return allFilesOpen;
}

/// Displays the file corresponding to the given track (which is assumed to have been loaded) and titles the window appropriately.
/// @param id The track ID - 0 for quad split.
/// @param remember Save the track name (or the fact that it's a quad split) in order to try to select it when new filesets are loaded
void Player::SelectTrack(const int id, const bool remember)
{
//std::cerr << "Player Select Track" << std::endl;
	if (mOK) {
		wxString title;
		if (id) { //individual track
			if (remember) {
				mDesiredTrackName = mTrackNames[id - 1]; // -1 to offset for quad split
			}
			title = wxString(mTrackNames[id - 1].c_str(), *wxConvCurrent);
		}
		else { //quad split
			if (remember) {
				mDesiredTrackName = "";
			}
			unsigned int nTracks = 0;
			for (size_t i = 0; i < mTrackNames.size(); i++) { //only go through video files
				if (mOpened[i]) {
					title += wxString(mTrackNames[i].c_str(), *wxConvCurrent) + wxT("; ");
					if (4 == ++nTracks) {
						//Quad Split displays the first four successfully opened files
						break;
					}
				}
			}
			if (!title.IsEmpty()) { //trap for only audio files
				title.resize(title.size() - 2); //remove trailing semicolon and space
			}
		}
		if (SHM_INPUT == mInputType && !mSpeed && remember) { //player doesn't respond to switchVideo when paused and showing shared memory
			Start();
		}
		else {
			switchVideo(id);
			if (id) { //not quad split
				mCurrentFileName = mFileNames[id - 1]; //-1 to offset for quad split
			}
			else {
				mCurrentFileName.clear(); //don't handle quad split for now
			}
		}
		SetWindowName(title);
	}
}

/// Plays at an absolute speed, forwards or backwards, or pauses.
/// @param rate: 0 to pause; positive to play forwards; speed = 2 ^ (absolute value - 1)
void Player::PlayAbsolute(const int rate)
{
//std::cerr << "Player PlayAbsolute" << std::endl;
	if (mOK) {
		if (rate && SHM_INPUT == mInputType) {
			//can only play forwards at normal speed from shared memory!
			play();
		}
		else if (rate > 0 && !AtRecEnd()) { //latter check prevents brief displays of different speeds as shuttle wheel is twiddled clockwise at the end of a recording
			playSpeed(1 << (rate - 1));
		}
		else if (rate < 0 && Within()) { //latter check prevents brief displays of different speeds as shuttle wheel is twiddled anticlockwise at the beginning of a file
			playSpeed(-1 << (rate * -1 - 1));
		}
		else if (!rate) {
			pause();
		}
	}
}

/// Starts playing, or doubles in play speed (up to max. limit) if already playing.
/// If starting to play forwards at the end of the recording, jumps to the beginning.
/// @param setDirection True to play in the direction specified by the next param; false to play in the same direction as currently playing, or previously playing if paused.
/// @param backwards True to play backwards if setDirection is true.
void Player::Play(const bool setDirection, bool backwards)
{
//std::cerr << "Player Play\n";
	if (mOK) {
		if ((setDirection && !backwards && 0 >= mSpeed) //going backwards, or paused, and want to go forwards
		 || (!setDirection && 0 == mSpeed && !mLastPlayingBackwards)) //paused after playing forwards
		{
			if (AtRecEnd() && mChunkBefore) {
				//replay from first chunk
				mChunkLinking = LOAD_FIRST_CHUNK;
				wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, mChunkLinking);
				AddPendingEvent(guiFrameEvent);
			}
			else if (AtRecEnd()) {
				//replay this chunk
				JumpToCue(0);
				play();
			}
			else {
				play();
			}
			mLastPlayingBackwards = false;
		}
		else if ((setDirection && backwards && 0 <= mSpeed) //going forwards, or paused and want to go backwards
		 || (!setDirection && 0 == mSpeed && mLastPlayingBackwards)) //paused after playing backwards
		{
			//play backwards at normal speed
			playSpeed(-1);
			mLastPlayingBackwards = true;
		}
		else if (0 <= mSpeed) { //playing forwards
			//double the speed
			if (MAX_SPEED > mSpeed) {
				playSpeed(mSpeed * 2);
			}
		}
		else { //playing backwards
			//double the speed
			if (-MAX_SPEED < mSpeed) {
				playSpeed(mSpeed * 2);
			}
		}
	}
}

/// Indicates if can't go any faster forwards.
/// @return True if at max speed.
bool Player::AtMaxForwardSpeed()
{
	if (SHM_INPUT != mInputType) {
		return mSpeed == MAX_SPEED;
	}
	else {
		//can only play at normal speed
		return mSpeed;
	}
}

/// Indicates if can't go any faster backwards.
/// @return True if at max speed.
bool Player::AtMaxReverseSpeed()
{
	if (SHM_INPUT != mInputType) {
		return mSpeed == -MAX_SPEED;
	}
	else {
		//can only play forwards!
		return true;
	}
}

/// Pauses playback.
void Player::Pause()
{
//std::cerr << "Player Pause" << std::endl;
	if (mOK) {
		pause();
	}
}

/// Steps by one frame.
/// @param direction True to step forwards.
void Player::Step(bool direction)
{
//std::cerr << "Player Step" << std::endl;
	if (mOK) {
		//change chunk if necessary - this code is needed because the player generates spurious frame displayed events so chunk changes as a result of frames displayed must be validated by the player not being paused
		if (direction //forwards
		&& mAtChunkEnd //already displayed the last frame
		&& mChunkAfter) { //another chunk follows this one
			mChunkLinking = LOAD_NEXT_CHUNK;
			wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, mChunkLinking);
			AddPendingEvent(guiFrameEvent);
		}
		else if (!direction //backwards!
		&& mAtStart //already displayed the first frame
		&& mChunkBefore) { //another chunk precedes this one
			mChunkLinking = LOAD_PREV_CHUNK;
			wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, mChunkLinking);
			AddPendingEvent(guiFrameEvent);
		}
		else {
			step(direction);
		}
	}
}

/// Stops the player and displays a blank window.
/// Keeping the window stops it re-appearing in a different place.
void Player::Reset()
{
//std::cerr << "Player Reset" << std::endl;
	mFilePollTimer->Stop();
//	mFileNames.clear(); //to indicate that nothing's loaded if player is disabled
	mOK = reset();
	if (mOK) {
		SetWindowName(wxT("Ingex Player - no files"));
	}
	if (mSpeed) {
		mSpeed = 0;
		TrafficControl(false);
	}
	mChunkLinking = STATE_CHANGE; //a default value
}

/// Moves to the given cue point.
/// @param cuePoint 0 for the start; 1+ for cue points supplied in Load(); the end for negative values or anything greater than the number of cue points supplied.
void Player::JumpToCue(const int cuePoint)
{
//std::cerr << "Player JumpToCue" << std::endl;
	if (mOK) {
		if (0 > cuePoint || cuePoint > (int) mCuePoints.size()) {
			//the end
			seek(0, SEEK_END, FRAME_PLAY_UNIT);
		}
		else if (cuePoint) {
			//somewhere in the middle
			seek(mCuePoints[cuePoint - 1], SEEK_SET, FRAME_PLAY_UNIT);
		}
		else {
			//the start
			seek(0, SEEK_SET, FRAME_PLAY_UNIT);
		}
	}
	else { //so as not to interfere with creating an event on the first frame
		mPreviousFrameDisplayed = 0; //so that the cue point takes priority when the player is reloaded if it is currently disabled
	}
	mLastRequestedCuePoint = cuePoint;
}

/// Moves to the given frame.
/// @param frame The frame offset.
void Player::JumpToFrame(const int64_t frame)
{
	if (mOK) {
		seek(frame, SEEK_SET, FRAME_PLAY_UNIT);
	}
}

/// Sets the type of on screen display.
/// @param OSDtype :
///	OSD_OFF : none
///	CONTROL_TIMECODE : timecode based on file position
///	SOURCE_TIMECODE : timecode read from file
void Player::SetOSD(const OSDtype type)
{
//std::cerr << "Player SetOSD" << std::endl;
	mOSDtype = type;
	switch (mOSDtype) {
		case OSD_OFF:
			setOSDScreen(OSD_EMPTY_SCREEN);
			break;
		case CONTROL_TIMECODE:
			setOSDTimecode(-1, CONTROL_TIMECODE_TYPE, -1);
			setOSDScreen(OSD_PLAY_STATE_SCREEN);
			break;
		case SOURCE_TIMECODE:
			setOSDTimecode(-1, SOURCE_TIMECODE_TYPE, -1);
			setOSDScreen(OSD_PLAY_STATE_SCREEN);
			break;
	}
}

/// Enable/disable OSD on the SDI output
/// @param enabled True to enable.
void Player::EnableSDIOSD(bool enabled)
{
	setSDIOSDEnable(enabled);
	Load(0); //takes effect on reload
}

/// Sets how the player appears.
/// @param outputType Whether external, on-screen or both, and whether on-screen is accelerated.
void Player::SetOutputType(const PlayerOutputType outputType)
{
//std::cerr << "Player SetOutputType" << std::endl;
	setOutputType(outputType);
	if (mOK) {
		//above call has stopped the player so start it again
		Start();
	}
}

/// Responds to a frame displayed event from the listener. If not playing from shared memory:
/// - Detects reaching the start, the end, a chunk start/end while playing, or leaving the start/end of a file and generates a player event accordingly.
/// - Detects reaching a cue point and generates a player event accordingly.
/// - Pauses if the player is playing and hits a recording end stop.
/// - Notes the frame number in case of reloading.
/// - Skips the event so it passes to the parent handler for position display.
/// NB a cue point is reached if the position is between it and the next cue point in the sense of playing forwards - regardless of the playing direction.
/// @param event The command event.
void Player::OnFrameDisplayed(wxCommandEvent& event) {
	//remember the position to jump to if re-loading occurs
	if (SHM_INPUT != mInputType) {
		if (event.GetInt()) { //a valid frame value (i,e, not the zero that's sent when the player is disabled)
			if (!event.GetExtraLong() && !mAtStart && 0 > mSpeed && mChunkBefore) { //just reached the start of a chunk, playing backwards (this may disrupt the first frame playing but academic as there will be a disturbance anyway as new files are loaded)
				mChunkLinking = LOAD_PREV_CHUNK;
				wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, mChunkLinking);
				AddPendingEvent(guiFrameEvent);
			}
			else if ((bool) event.GetClientData() && !mAtChunkEnd && 0 < mSpeed && mChunkAfter) { //just reached the end of a chunk, playing forwards (this may disrupt the first frame playing but academic as there will be a disturbance anyway as new files are loaded)
				mChunkLinking = LOAD_NEXT_CHUNK;
				wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, mChunkLinking);
				AddPendingEvent(guiFrameEvent);
			}
			else if (!event.GetExtraLong() && !mChunkBefore) { //at the start of a recording
				wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, AT_START);
				AddPendingEvent(guiFrameEvent);
				if (0 > mSpeed) { //playing forwards
					Pause();
				}
			}
			else if ((bool) event.GetClientData() && !mChunkAfter) { //at the end of a recording
				wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, AT_END);
				AddPendingEvent(guiFrameEvent);
				Pause();
			}

			if (!mPreviousFrameDisplayed || mAtChunkEnd) { //have just moved into a take
				wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, WITHIN);
				AddPendingEvent(guiFrameEvent);
			}
			mAtStart = !event.GetExtraLong();
			mAtChunkEnd = (bool) event.GetClientData();
			mPreviousFrameDisplayed = event.GetExtraLong();
			//work out which cue point area we're in
			unsigned int mark = 0; //assume before first cue point/end of file
			if (mAtChunkEnd) {
				mark = mCuePoints.size() + 1;
			}
			else {
				for (mark = 0; mark < mCuePoints.size(); mark++) {
					if (mCuePoints[mark] > event.GetExtraLong()) { //not reached this mark yet
						break;
					}
				}
			}
			mark += mStartIndex;
			//Tell gui if we've moved into a new area
			if (mLastCuePointNotified != mark) {
				if (!mAtChunkEnd || !mChunkAfter) { //not at the last frame with a chunk following, where there is no stop event
//std::cerr << "cue point event" << std::endl;
					wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, CUE_POINT);
					guiEvent.SetInt(mark);
					AddPendingEvent(guiEvent);
				}
				mLastCuePointNotified = mark;
			}
			if (mSetOSDType) {
				SetOSD(mOSDtype); //player doesn't obey this call on loading, so do it here
				mSetOSDType = false;
			}
		}
		//tell the gui so it can update the position display
		event.Skip();
	}
}

/// Responds to a state change event from the listener.
/// Notes the state change to restore the state if the player is re-loaded.
/// Skips the event so it passes to the parent handler.
/// @param event The command event.
void Player::OnStateChange(wxCommandEvent& event)
{
	//remember the mode to set if re-loading occurs
//std::cerr << "State Change " << event.GetInt() << std::endl;
	if (PlayerMode::CLOSE != event.GetInt()) {
		mMode = (PlayerMode::EnumType) event.GetInt();
	}
	//tell the gui so it can update state
	event.Skip();
}

/// Responds to a speed change event from the listener.
/// Notes the speed to enable new speed to be calculated for repeated play or play backwards requests.
/// Skips the event so it passes to the parent handler.
/// @param event The command event.
void Player::OnSpeedChange(wxCommandEvent& event)
{
	//inform any copying server
	if (event.GetInt() && !mSpeed && SHM_INPUT != mInputType) { //just started playing
		//traffic control on, to ensure smooth playback
		TrafficControl(true);
	}
	else if (!event.GetInt() && mSpeed) { //just stopped playing
		//traffic control off, to resume fast copying
		TrafficControl(false);
	}
	//remember the current speed to set if re-loading occurs and if changing speed
	mSpeed = event.GetInt();
//std::cerr << "Speed: " << mSpeed << std::endl;
	//tell the gui
	event.Skip();
}

/// Responds to a progress bar drag event from the listener.
/// Seeks to the given position.
/// @param event The command event.
void Player::OnProgressBarDrag(wxCommandEvent& event)
{
	seek(event.GetInt(), SEEK_SET, PERCENTAGE_PLAY_UNIT);
}

/// Responds to the file poll timer.
/// Checks to see if any more files have appeared, and if so, restarts the player.
/// If all files have appeared, kills the timer.
/// @param event The timer event.
void Player::OnFilePollTimer(wxTimerEvent& WXUNUSED(event))
{
//std::cerr << "Player OnFilePollTimer" << std::endl;
	//Have any more files appeared?
	unsigned int nFilesExisting = 0;
	for (size_t i = 0; i < mFileNames.size(); i++) {
		if (wxFile::Exists(wxString(mFileNames[i].c_str(), *wxConvCurrent))) {
			nFilesExisting++;
		}
	}
	if (nFilesExisting > mNFilesExisting) {
		mNFilesExisting = nFilesExisting; //prevents restarting continually unless more files appear
		Start(); //existing parameters
		if (nFilesExisting == mFileNames.size()) {
			//all the files are there - even if the player is unhappy there's nothing else we can do
 			mFilePollTimer->Stop();
		}
	}	
}

/// Responds to successful socket connection.
/// Sends traffic control message and disconnects.
void Player::OnSocketEvent(wxSocketEvent& WXUNUSED(event))
{
//std::cerr << "Socket Connection made; setting traffic control " << (mTrafficControl ? "ON\n" : "OFF\n");
	//Socket is now open so best send a message even if traffic control status hasn't changed
	mSocket->Write(mTrafficControl ? "ingexgui\n0\n" : "ingexgui\n", mTrafficControl ? 11 : 9);
	mPrevTrafficControl = mTrafficControl;
	mSocket->Close();
	mOpeningSocket = false;
}

/// Indicates whether player is at the very start of the recording or not.
/// @return True if within the file.
bool Player::Within()
{
	return mOK && (!mAtStart || mChunkBefore);
}

/// Indicates whether player is at the end of the recording or not.
/// @return True if at the end.
bool Player::AtRecEnd()
{
	return mOK && mAtChunkEnd && !mChunkAfter;
}

/// Mutes or unmutes audio
/// @param state true to mute
void Player::MuteAudio(const bool state)
{
	mMuted = state;
	muteAudio(state ? 1 : 0);
	SetWindowName();
}

/// Sets audio to follow video or stick to the first audio files
/// @param state true to follow video
void Player::AudioFollowsVideo(const bool state)
{
	switchAudioGroup(state ? 0 : 1);
}

/// Returns true if the last/current playing direction is backwards.
bool Player::LastPlayingBackwards()
{
	return mLastPlayingBackwards;
}

/// Sets the name of the player window if the player is displayed
/// @param name The name to display, which will have a note appended if audio is muted.  If empty, the previous name will be used (useful for updating mute status)
void Player::SetWindowName(const wxString & name)
{
	if (!name.IsEmpty()) {
		mName = name;
	}
//	if (mOK) {
		wxString title = mName;
		if (X11_OUTPUT == getActualOutputType() || DUAL_DVS_X11_OUTPUT == getActualOutputType()) {
			title += wxT(" (unaccelerated)");
		}
		if (mMuted) {
			title += wxT(" (Muted)");
		}
		setX11WindowName((const char*) title.mb_str(*wxConvCurrent));
//	}
}

/// Sends a traffic control message to a server socket.
/// @param state True to switch traffic control on; false to switch it off.
/// @param synchronous Don't use events - block until the message has been sent (with a timeout)
void Player::TrafficControl(const bool state, const bool synchronous)
{
	mTrafficControl = state; //allows state to be changed while waiting for socket to open
	if (!mOpeningSocket //without this check, app can hang with 100% usage in gettimeofday() if it manages to open a socket
	 && mTrafficControl != mPrevTrafficControl) { //avoid unnecessary socket connections
		wxIPV4address addr;
		addr.Hostname(wxT("localhost"));
		addr.Service(TRAFFIC_CONTROL_PORT);
		mSocket->Notify(!synchronous);
		mSocket->Connect(addr, false); //mustn't block the GUI; instead generates an event on connect (don't bother handling the connection fail situation)
		if (synchronous) {
			mSocket->WaitOnConnect(3);
			wxSocketEvent dummyEvent;
			OnSocketEvent(dummyEvent);
		}
		else {
			mOpeningSocket = true;
		}
	}
}

/// Returns the filename of the currently selected track; empty string if quad split
std::string Player::GetCurrentFileName()
{
	return mCurrentFileName;
}

/// Returns the frame offset of the latest frame to be displayed
unsigned long Player::GetLatestFrameDisplayed()
{
	return mPreviousFrameDisplayed;
}

/***************************************************************************
 *   LISTENER                                                              *
 ***************************************************************************/

/// @param player The player associated with this listener.
Listener::Listener(Player * player, prodauto::IngexPlayerListenerRegistry * registry) : prodauto::IngexPlayerListener(registry), mPlayer(player)
{
}

/// Callback for each frame displayed.
/// Sends a Frame Displayed event to the player (after the Cue Point event so that the gui stops the player at the end of the file).
/// @param frameInfo Structure with information about the frame being displayed.
/// NB Called in another thread context.
void Listener::frameDisplayedEvent(const FrameInfo* frameInfo)
{
	wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
	guiFrameEvent.SetExtraLong(frameInfo->position);
	guiFrameEvent.SetInt(true); //valid position
	if (frameInfo->position == frameInfo->sourceLength - 1) { //at the end of the file
		guiFrameEvent.SetClientData((void *) 1);
	}
	mPlayer->AddPendingEvent(guiFrameEvent);
}

/// Callback for a frame being repeated from the SDI card because the player didn't supply the next one in time.
/// Currently does nothing
/// NB Called in another thread context
void Listener::frameDroppedEvent(const FrameInfo*)
{
//std::cerr << "frame dropped event" << std::endl;
}

/// Callback for the player changing state.
/// Sends a state change event to the player in the case of a change in direction or between stop, play and pause.
/// Sends a speed change event for the above and a speed change.
/// @param playerEvent Structure with information about state changes.
/// NB Called in another thread context
void Listener::stateChangeEvent(const MediaPlayerStateEvent* playerEvent)
{
//std::cerr << "state change event" << std::endl;
	if (playerEvent->stopChanged) {
		if (playerEvent->stop) {
			wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
			guiEvent.SetInt(PlayerMode::STOP);
			mPlayer->AddPendingEvent(guiEvent);
		}
	}
	if (playerEvent->playChanged) {
		//play/pause
		wxCommandEvent stateEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
		wxCommandEvent speedEvent(wxEVT_PLAYER_MESSAGE, SPEED_CHANGE);
		if (playerEvent->play) {
//std::cerr << "state change PLAY: speed: " << playerEvent->speed << std::endl;
			stateEvent.SetInt(playerEvent->speed < 0 ? PlayerMode::PLAY_BACKWARDS : PlayerMode::PLAY);
			speedEvent.SetInt(playerEvent->speed); //always need this as mSpeed will be 0 and speedChanged will be false if speed is 1
		}
		else {
//std::cerr << "state change PAUSE: speed: " << playerEvent->speed << std::endl;
			stateEvent.SetInt(PlayerMode::PAUSE);
			speedEvent.SetInt(0); //Pause reports speed as being 1
		}	
		mPlayer->AddPendingEvent(stateEvent);
		mPlayer->AddPendingEvent(speedEvent);
	}
	else if (playerEvent->speedChanged) {
		wxCommandEvent speedEvent(wxEVT_PLAYER_MESSAGE, SPEED_CHANGE);
		speedEvent.SetInt(playerEvent->speed);
		mPlayer->AddPendingEvent(speedEvent);
//std::cerr << "speed change: " << playerEvent->speed << std::endl;
	}
}

/// Callback for the player reaching the end of the file (while playing forwards).
/// Does nothing (situation detected in frameDisplayedEvent() instead)
/// NB Called in another thread context.
void Listener::endOfSourceEvent(const FrameInfo*)
{
}

/// Callback for the player reaching the start of the file (while playing backwards).
/// Does nothing (situation detected in frameDisplayedEvent() instead).
/// NB Called in another thread context.
/// @param ypos The vertical position of the click within the player window (relative to its default height)
void Listener::startOfSourceEvent(const FrameInfo*)
{
}

/// Callback for the player being closed.
/// Sends a State Change event to the player.
/// NB Called in another thread context.
void Listener::playerClosed()
{
//std::cerr << "player close event" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
	guiEvent.SetInt(PlayerMode::CLOSE);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user clicking the close box on the player window.
/// Sends a Close Request event to the player.
/// NB Called in another thread context.
void Listener::playerCloseRequested()
{
//std::cerr << "player close requested" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, CLOSE_REQ);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user pressing a key when the player window has focus.
/// Sends a Keypress event to the player.
/// @param code The X11 key code
/// NB Called in another thread context.
//NB Called in another thread context
void Listener::keyPressed(int code, int modifier)
{
//std::cerr << "key pressed" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, KEYPRESS);
	guiEvent.SetInt(code);
	guiEvent.SetExtraLong(modifier);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user releasing a key when the player window has focus.
/// Does nothing.
/// NB Called in another thread context.
void Listener::keyReleased(int, int)
{
//std::cerr << "key released" << std::endl;
}

/// Callback for the progress bar being dragged with the mouse.
/// Sends the position to the player.
/// @param position The progress bar position, from 0 to 100.
/// NB Called in another thread context.
void Listener::progressBarPositionSet(float position)
{
	wxCommandEvent event(wxEVT_PLAYER_MESSAGE, PROGRESS_BAR_DRAG);
	event.SetInt((int) (position * 1000.)); //this is the resolution the player seek command works to
	mPlayer->AddPendingEvent(event);
}

/// Callback for the mouse being clicked.
/// Decodes which quadrant has been clicked on (if any) and sends the value to the player.
/// @param imageWidth The width of the entire image in the player window.
/// @param imageHeight The height of the entire image in the player window.
/// @param xpos The horizontal position of the click within the player window (relative to its default width).
/// @param ypos The vertical position of the click within the player window (relative to its default height).
void Listener::mouseClicked(int imageWidth, int imageHeight, int xpos, int ypos)
{
	if (xpos <= imageWidth && ypos <= imageHeight) { //clicked inside the image (rather than the window)
		wxCommandEvent event(wxEVT_PLAYER_MESSAGE, QUADRANT_CLICK);
		event.SetInt((xpos < imageWidth / 2 ? 1 : 2) + (ypos < imageHeight / 2 ? 0 : 2));
		mPlayer->AddPendingEvent(event);
	}
}
