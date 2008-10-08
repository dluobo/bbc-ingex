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

#include "player.h"
#include <wx/file.h>

DEFINE_EVENT_TYPE(wxEVT_PLAYER_MESSAGE)

using namespace prodauto; //IngexPlayer class is in this namespace

BEGIN_EVENT_TABLE( Player, wxEvtHandler )
	EVT_COMMAND( FRAME_DISPLAYED, wxEVT_PLAYER_MESSAGE, Player::OnFrameDisplayed )
	EVT_COMMAND( STATE_CHANGE, wxEVT_PLAYER_MESSAGE, Player::OnStateChange )
	EVT_COMMAND( SPEED_CHANGE, wxEVT_PLAYER_MESSAGE, Player::OnSpeedChange )
	EVT_COMMAND( PROGRESS_BAR_DRAG, wxEVT_PLAYER_MESSAGE, Player::OnProgressBarDrag )
	EVT_COMMAND( QUADRANT_CLICK, wxEVT_PLAYER_MESSAGE, Player::OnQuadrantClick )
	EVT_TIMER( wxID_ANY, Player::OnFilePollTimer )
END_EVENT_TABLE()

//static Rational Zero = {0, 0};
//static Rational Aspect = {4, 3};

/// Creates the player, but does not display it.
/// @param handler The handler where events will be sent.
/// @param enabled True to enable player.
/// @param outputType The output type - accelerated or unaccelerated; SDI or not.
/// @param displayType The on screen display type.
Player::Player(wxEvtHandler * handler, const bool enabled, const PlayerOutputType outputType, const OSDtype displayType)
/*: LocalIngexPlayer(
	outputType,
	true,
	4,
	false,
	true,
	true,
	0,
	false,
	false,
	Zero,
	Zero,
	Aspect,
	(float) 1.0,
	false,
	0),*/
: 
LocalIngexPlayer(outputType), 
 mOSDtype(displayType), mEnabled(enabled), mOK(false), mSpeed(0) //simple LocalIngexPlayer constructor with defaults
{
	mListener = new Listener(this); //registers with the player
	mFilePollTimer = new wxTimer(this, wxID_ANY);
	SetNextHandler(handler);
	mDesiredTrackName = ""; //display the quad split by default
}

Player::~Player()
{
	delete mListener;
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
/// @param fileNames List of file paths, or if zero, use previous supplied list and ignore all other parameters.
/// @param trackNames Corresponding list of track names, for display as the player window title.
/// @param cuePoints Frame numbers of cue points (not including start and end positions).
/// @param startIndex Event list index for entry corresponding to start of file.
/// @param cuePoint Index in cuePoints to jump to.
void Player::Load(std::vector<std::string> * fileNames, std::vector<std::string> * trackNames, std::vector<int64_t> * cuePoints, int startIndex, unsigned int cuePoint)
{
//std::cerr << "Player Load" << std::endl;
	std::vector<std::string> * fNames = fileNames ? fileNames : &mFileNames;
	if (!fNames->size()) { //router recorder recording only, for instance
		Reset(); //otherwise it keeps showing what was there before
	}
	else {
		if (mEnabled) {
			mFilePollTimer->Stop(); //may be running from another take
			//see how many files we have before the player loads them, so we can start polling even if more appear just after loading
			mNFilesExisting = 0;
			for (size_t i = 0; i < fNames->size(); i++) {
				if (wxFile::Exists(wxString((*fNames)[i].c_str(), *wxConvCurrent))) {
					mNFilesExisting++;
				}
			}
		}
		//load the player (this will merely store the parameters if the player is not enabled
		if (!Start(fileNames, trackNames, cuePoints, startIndex, cuePoint) && mFileNames.size() != mNFilesExisting && mEnabled) {
			//player isn't happy, and (probably) not all files were there when the player opened, so start polling for them
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
/// @param cuePoints Frame numbers of cue points (not including start and end positions).
/// @param startIndex Event list index for entry corresponding to start of file.
/// @param cuePoint Index in cuePoints to jump to.
/// @return True if all files were opened.
bool Player::Start(std::vector<std::string> * fileNames, std::vector<std::string> * trackNames, std::vector<int64_t> * cuePoints, int startIndex, unsigned int cuePoint)
{
//std::cerr << "Player Start" << std::endl;
	if (fileNames) {
		//a new set of files; assume all parameters are supplied
		mFileNames.clear();
		mTrackNames.clear();
		for (size_t i = 0; i < fileNames->size(); i++) {
			mFileNames.push_back((*fileNames)[i]);
		}
		for (size_t i = 0; i < trackNames->size(); i++) {
			mTrackNames.push_back((*trackNames)[i]);
		}
		mCuePoints.clear();
		mListener->ClearCuePoints();
		mStartIndex = startIndex;
		mListener->SetStartIndex(mStartIndex); //an offset for all the cue point event values
		for (size_t i = 0; i < cuePoints->size(); i++) {
			mCuePoints.push_back((*cuePoints)[i]); //so that we know where to jump to for a given cue point
			mListener->AddCuePoint((*cuePoints)[i]); //so we can work out which cue point has been reached
		}
		mLastFrameDisplayed = 0; //new clip, so previous position not relevant
		mLastRequestedCuePoint = cuePoint;
		mMode = PAUSE;
	}
	bool allFilesOpen = false;
	if (mEnabled) {
		mOpened.clear();
		mOK = start(mFileNames, mOpened);
		int trackToSelect = 0; //display quad split by default
		if (mOK) {
			//(re)loading stored cue points
			for (size_t i = 0; i < mCuePoints.size(); i++) {
				markPosition(mCuePoints[i], 0); //so that an event is generated at each cue point
			}
			//player starts off in play forwards mode
			if (PAUSE == mMode || STOP == mMode) {
				pause();
			}
			else if (PLAY_BACKWARDS == mMode) {
				playSpeed(-1);
			}
			SetOSD(mOSDtype);
			if (mLastFrameDisplayed) {
				//player was already some way into a clip and has been reloaded
				seek(mLastFrameDisplayed, SEEK_SET, FRAME_PLAY_UNIT); //restore position
			}
			else {
				//if mLastRequestedCuePoint isn't zero (start), player was requested to go to a cue point in this clip while no files were available
				JumpToCue(mLastRequestedCuePoint);
			}

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
			SelectTrack(trackToSelect);
			allFilesOpen = mOpened.size() == nFilesOpen;
		}
		else {
			setX11WindowName("Ingex Player - no files");
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
void Player::SelectTrack(const int id)
{
//std::cerr << "Player Select Track" << std::endl;
	if (mOK) {
		std::string title;
		switchVideo(id);
		if (id) { //individual track
			mDesiredTrackName = mTrackNames[id - 1]; // -1 to offset for quad split
			title = mTrackNames[id - 1];
		}
		else { //quad split
			mDesiredTrackName = "";
			unsigned int nTracks = 0;
			for (size_t i = 0; i < mTrackNames.size(); i++) { //only go through video files
				if (mOpened[i]) {
					title += mTrackNames[i] + "; ";
					if (4 == ++nTracks) {
						//Quad Split displays the first four successfully opened files
						break;
					}
				}
			}
			if (title.size()) { //trap for only audio files
				title.resize(title.size() - 2); //remove trailing semicolon and space
			}
		}
		//add player type
		if (X11_OUTPUT == getActualOutputType() || DUAL_DVS_X11_OUTPUT == getActualOutputType()) {
			title += " (unaccelerated)";
		}
		setX11WindowName(title.c_str());
	}
}

/// Starts playing forwards.
void Player::Play()
{
//std::cerr << "Player Play" << std::endl;
	if (mOK) {
		if (mSpeed <= 0) {
			play();
		}
		else if (mSpeed < MAX_SPEED) {
			playSpeed(mSpeed * 2);
		}
	}
}

/// Starts playing backwards.
void Player::PlayBackwards()
{
//std::cerr << "Player Play backwards" << std::endl;
	if (Within()) {
		if (mSpeed >= 0) {
			playSpeed(-1);
		}
		else if (mSpeed > -MAX_SPEED) {
			playSpeed(mSpeed * 2);
		}
	}
}

/// Indicates if playing at max forward speed.
/// @return True if at max speed.
bool Player::AtMaxForwardSpeed()
{
	return mSpeed == MAX_SPEED;
}

/// Indicates if playing at max reverse speed.
/// @return True if at max speed.
bool Player::AtMaxReverseSpeed()
{
	return mSpeed == -MAX_SPEED;
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
/// @param forwards True to step forwards.
void Player::Step(bool forwards)
{
//std::cerr << "Player Step" << std::endl;
	if (mOK) {
		step(forwards);
	}
}

/// Stops the player and displays a blank window.
/// Keeping the window stops it re-appearing in a different place.
void Player::Reset()
{
//std::cerr << "Player Reset" << std::endl;
	mFilePollTimer->Stop();
	mFileNames.clear(); //to indicate that nothing's loaded if player is disabled
	if (mOK) {
		reset();
		setX11WindowName("Ingex Player - no files");
		//clear the position display
		wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
		guiFrameEvent.SetInt(false); //invalid position
		AddPendingEvent(guiFrameEvent);
	}
}

/// Moves to the given cue point.
/// @param cuePoint 0 for the start; 1+ for cue points supplied in Load(); the end for anything greater than the number of cue points supplied.
void Player::JumpToCue(unsigned int cuePoint)
{
//std::cerr << "Player JumpToCue" << std::endl;
	if (mOK) {
		if (cuePoint > mCuePoints.size()) {
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
	mLastRequestedCuePoint = cuePoint;
	mLastFrameDisplayed = 0; //so that the cue point takes priority when the player is reloaded if it is currently disabled
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

/// Sets how the player appears.
/// @param outputType Whether external, on-screen or both, and whether on-screen is accelerated.
void Player::SetOutputType(const PlayerOutputType outputType)
{
//std::cerr << "Player SetOutputType" << std::endl;
	setOutputType(outputType, 1.0);
	if (mOK) {
		//we're already seeing something, so make changes take effect immediately
		Start();
	}
}

/// Responds to a frame displayed event from the listener.
/// Detects moving into a take and generates a player event when this happens.
/// Skips the event so it passes to the parent handler
/// @param event The command event.
void Player::OnFrameDisplayed(wxCommandEvent& event) {
	//remember the position to jump to if re-loading occurs
	if (event.GetInt()) {
		//a valid frame value (i,e, not the zero that's sent when the player is disabled)
		if (!mLastFrameDisplayed && event.GetExtraLong()) {
			//have just moved into a take: tell the player so it can update the "previous take" button
			wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, WITHIN_TAKE);
			AddPendingEvent(guiFrameEvent);
		}
		mLastFrameDisplayed = event.GetExtraLong();
	}
	//tell the gui so it can update the position display
	event.Skip();
}

/// Responds to a state change event from the listener.
/// Notes the state change to restore the state if the player is re-loaded.
/// Skips the event so it passes to the parent handler.
/// @param event The command event.
void Player::OnStateChange(wxCommandEvent& event)
{
	//remember the mode to set if re-loading occurs
//std::cerr << "State Change " << event.GetInt() << std::endl;
	if (CLOSE != event.GetInt()) {
		mMode = (prodauto::PlayerMode) event.GetInt();
		if (PAUSE == event.GetInt()) {
			mSpeed = 0;
		}
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
	//remember the current speed to set if re-loading occurs and if changing speed
	mSpeed = event.GetInt();
//std::cerr << mSpeed << std::endl;
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

/// Responds to a quadrant clicked signal from the listener.
/// Passes to the GUI
void Player::OnQuadrantClick(wxCommandEvent& event)
{
	event.Skip();
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

/// Indicates whether player is at the very start of the file or not.
/// @return True if within the file.
bool Player::Within()
{
	return mOK && mLastFrameDisplayed;
}

/// @param player The player associated with this listener.
Listener::Listener(Player * player) : IngexPlayerListener((IngexPlayerListenerRegistry *) player), mPlayer(player), mStartIndex(0)
{
}

/// Sets the offset added to cue point events returned from the player.
/// @param startIndex The offset.
void Listener::SetStartIndex(const int startIndex)
{
	wxMutexLocker lock(mMutex);
	mStartIndex = startIndex;
	mLastCuePointNotified = startIndex;
}

/// Erases all cue points previously set.
void Listener::ClearCuePoints()
{
	wxMutexLocker lock(mMutex);
	mCuePoints.clear();
}

/// Adds a cue point to detect a position within the file.
/// @param frameNo The frame number of the cue point - expected to be greater than all previous frame numbers
void Listener::AddCuePoint(const int64_t frameNo)
{
	wxMutexLocker lock(mMutex);
	mCuePoints.push_back(frameNo);
}

/// Callback for each frame displayed.
/// Sends a Cue Point event to the player if a cue point (including start or finish) has been reached.
/// Sends a Frame Displayed event to the player (after the Cue Point event so that the gui stops the player at the end of the file).
/// NB a cue point is reached if the position is between it and the next cue point in the sense of playing forwards - regardless of the playing direction
/// @param frameInfo Structure with information about the frame being displayed.
/// NB Called in another thread context.
void Listener::frameDisplayedEvent(const FrameInfo* frameInfo)
{
	//work out which cue point area we're in
	unsigned int mark = 0;
	wxMutexLocker lock(mMutex);
	if (frameInfo->position == frameInfo->sourceLength - 1) { //at the end of the file
		mark = mCuePoints.size() + 1;
	}
	else { //somewhere within the file
		for (mark = 0; mark < mCuePoints.size(); mark++) {
			if (mCuePoints[mark] > frameInfo->position) { //not reached this mark yet
				break;
			}
		}
	}
	//Tell gui if we've moved into a new area
	mark += mStartIndex;
	if (mLastCuePointNotified != mark) {
//std::cerr << "cue point event" << std::endl;
		mLastCuePointNotified = mark;
		wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, CUE_POINT);
		guiEvent.SetExtraLong(frameInfo->position);
		guiEvent.SetInt(mLastCuePointNotified);
		mPlayer->AddPendingEvent(guiEvent);
	}
	//tell gui the frame number
	wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
	guiFrameEvent.SetInt(true); //valid position
	guiFrameEvent.SetExtraLong(frameInfo->position);
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
			guiEvent.SetInt(STOP);
			mPlayer->AddPendingEvent(guiEvent);
		}
	}
	if (playerEvent->playChanged) {
		//play/pause
		wxCommandEvent stateEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
		wxCommandEvent speedEvent(wxEVT_PLAYER_MESSAGE, SPEED_CHANGE);
		if (playerEvent->play) {
//std::cerr << "state change PLAY: speed: " << playerEvent->speed << std::endl;
			stateEvent.SetInt(playerEvent->speed < 0 ? PLAY_BACKWARDS : PLAY);
			speedEvent.SetInt(playerEvent->speed); //always need this as mSpeed will be 0 and speedChanged will be false if speed is 1
		}
		else {
//std::cerr << "state change PAUSE: speed: " << playerEvent->speed << std::endl;
			stateEvent.SetInt(PAUSE);
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
	guiEvent.SetInt(CLOSE);
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
void Listener::keyPressed(int code)
{
//std::cerr << "key pressed" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, KEYPRESS);
	guiEvent.SetInt(code);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user releasing a key when the player window has focus.
/// Does nothing.
/// NB Called in another thread context.
void Listener::keyReleased(int)
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
	event.SetInt((int) (position * 1000.)); //this is the resulution the player seek command works to
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
