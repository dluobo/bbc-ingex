/*
 * $Id: HTTPIngexPlayer.cpp,v 1.2 2009/09/18 16:13:50 philipn Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Modifications: Matthew Marks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define __STDC_FORMAT_MACROS 1

#include "HTTPIngexPlayer.h"
#include "JSONObject.h"
#include "Logging.h"
#include "IngexException.h"
#include "Utilities.h"


using namespace std;
using namespace ingex;
using namespace prodauto;


static const char* g_pingURL = "/ping";

static const char* g_settingsOutputTypeURL = "/settings/outputtype";
static const char* g_settingsDVSTargetURL = "/settings/dvstarget";
static const char* g_settingsVideoSplitURL = "/settings/videosplit";
static const char* g_settingsSDIOSDEnableURL = "/settings/sdiosdenable";
static const char* g_settingsX11WindowNameURL = "/settings/x11windowname";

static const char* g_infoStateURL = "/info/state.json";
static const char* g_infoStatePushURL = "/info/statepush.json";
static const char* g_infoDVSIsAvailableURL = "/info/dvsisavailable.json";
static const char* g_infoOutputTypeURL = "/info/outputtype.json";
static const char* g_infoActualOutputTypeURL = "/info/actualoutputtype.json";

static const char* g_startURL = "/start";
static const char* g_closeURL = "/close";
static const char* g_resetURL = "/reset";

static const char* g_controlToggleLockURL = "/control/togglelock";
static const char* g_controlPlayURL = "/control/play";
static const char* g_controlPauseURL = "/control/pause";
static const char* g_controlTogglePlayPauseURL = "/control/toggleplaypause";
static const char* g_controlPlaySpeedURL = "/control/playspeed";
static const char* g_controlSeekURL = "/control/seek";
static const char* g_controlStepURL = "/control/step";
static const char* g_controlMarkURL = "/control/mark/set";
static const char* g_controlMarkPositionURL = "/control/mark/setposition";
static const char* g_controlClearMarkURL = "/control/mark/clear";
static const char* g_controlClearAllMarksURL = "/control/mark/clearall";
static const char* g_controlSeekNextMarkURL = "/control/mark/seeknext";
static const char* g_controlSeekPrevMarkURL = "/control/mark/seekprev";
static const char* g_controlSetOSDScreenURL = "/control/osd/screen";
static const char* g_controlNextOSDScreenURL = "/control/osd/nextscreen";
static const char* g_controlSetOSDTimecodeURL = "/control/osd/timecode";
static const char* g_controlNextOSDTimecodeURL = "/control/osd/nexttimecode";
static const char* g_controlSetVideoURL = "/control/video/set";
static const char* g_controlNextVideoURL = "/control/video/next";
static const char* g_controlPrevVideoURL = "/control/video/prev";
static const char* g_controlMuteAudioURL = "/control/audio/mute";
static const char* g_controlSetAudioGroupURL = "/control/audio/group";
static const char* g_controlNextAudioGroupURL = "/control/audio/nextgroup";
static const char* g_controlPrevAudioGroupURL = "/control/audio/prevgroup";
static const char* g_controlSnapAudioGroupURL = "/control/audio/snapvideo";
static const char* g_controlReviewStartURL = "/control/review/start";
static const char* g_controlReviewEndURL = "/control/review/end";
static const char* g_controlReviewURL = "/control/review/now";



class PlayerConnectionState : public HTTPConnectionState
{
public:
    PlayerConnectionState()
    {
        memset(&playerState, 0, sizeof(playerState));
        memset(&lastUpdate, 0, sizeof(lastUpdate));
    }
    virtual ~PlayerConnectionState() {}


    HTTPPlayerState playerState;
    struct timeval lastUpdate;
};




HTTPIngexPlayer::HTTPIngexPlayer(HTTPServer* server, LocalIngexPlayer* player, prodauto::IngexPlayerListenerRegistry* listenerRegistry)
: IngexPlayerListener(listenerRegistry), _player(player)
{
    HTTPServiceDescription* service;

    service = server->registerService(new HTTPServiceDescription(g_pingURL), this);
    service->setDescription("Ping the player");


    service = server->registerService(new HTTPServiceDescription(g_settingsOutputTypeURL), this);
    service->setDescription("Set the player's output type");
    service->addArgument("type", "int", true, "The output type");
    service->addArgument("scale", "float", false, "Scale factor for the output");

    service = server->registerService(new HTTPServiceDescription(g_settingsDVSTargetURL), this);
    service->setDescription("Set the target card and channel for DVS output");
    service->addArgument("card", "int", false, "The DVS card number");
    service->addArgument("channel", "int", false, "The DVS channel number");

    service = server->registerService(new HTTPServiceDescription(g_settingsVideoSplitURL), this);
    service->setDescription("Set the video split type");
    service->addArgument("type", "int", true, "The video split type");

    service = server->registerService(new HTTPServiceDescription(g_settingsSDIOSDEnableURL), this);
    service->setDescription("Enable/disable the SDI OSD");
    service->addArgument("enable", "bool", true, "Enable/disable the SDI OSD");

    service = server->registerService(new HTTPServiceDescription(g_settingsX11WindowNameURL), this);
    service->setDescription("Set the name of the X11 window");
    service->addArgument("name", "string", true, "The X11 window name");



    service = server->registerService(new HTTPServiceDescription(g_infoStateURL), this);
    service->setDescription("Returns the player's state");

    service = server->registerService(new HTTPServiceDescription(g_infoStatePushURL), this);
    service->setDescription("Keep connection open and push the player's state when it changes");

    service = server->registerService(new HTTPServiceDescription(g_infoDVSIsAvailableURL), this);
    service->setDescription("Returns true if the DVS card and channel are available");
    service->addArgument("card", "int", false, "The DVS card number to check");
    service->addArgument("channel", "int", false, "The DVS card's channel number to check");

    service = server->registerService(new HTTPServiceDescription(g_infoOutputTypeURL), this);
    service->setDescription("Returns the player's output type");

    service = server->registerService(new HTTPServiceDescription(g_infoActualOutputTypeURL), this);
    service->setDescription("Returns the actual player's output type");



    service = server->registerService(new HTTPServiceDescription(g_startURL), this);
    service->setDescription("Start the player");
    service->addArgument("inputs", "json", true, "Start options and list of inputs");

    service = server->registerService(new HTTPServiceDescription(g_closeURL), this);
    service->setDescription("Stop the player and close any X11 window");

    service = server->registerService(new HTTPServiceDescription(g_resetURL), this);
    service->setDescription("Reset the player and display blank video on the output");



    service = server->registerService(new HTTPServiceDescription(g_controlToggleLockURL), this);
    service->setDescription("Toggle lock");

    service = server->registerService(new HTTPServiceDescription(g_controlPlayURL), this);
    service->setDescription("Play");

    service = server->registerService(new HTTPServiceDescription(g_controlPauseURL), this);
    service->setDescription("Pause");

    service = server->registerService(new HTTPServiceDescription(g_controlTogglePlayPauseURL), this);
    service->setDescription("Toggle play/pause");

    service = server->registerService(new HTTPServiceDescription(g_controlPlaySpeedURL), this);
    service->setDescription("Play speed");
    service->addArgument("speed", "int", true, "The frame offset for the next output frame");

    service = server->registerService(new HTTPServiceDescription(g_controlSeekURL), this);
    service->setDescription("Seek");
    service->addArgument("offset", "int64", true, "The seek offset");
    service->addArgument("whence", "enum", true, "One of the following: SEEK_SET, SEEK_CUR or SEEK_END");
    service->addArgument("unit", "enum", true, "One of the following: FRAME_PLAY_UNIT or PERCENTAGE_PLAY_UNIT");

    service = server->registerService(new HTTPServiceDescription(g_controlStepURL), this);
    service->setDescription("Step");
    service->addArgument("forwards", "bool", true, "Step forwards or backwards");

    service = server->registerService(new HTTPServiceDescription(g_controlMarkURL), this);
    service->setDescription("Set a mark at the current position");
    service->addArgument("type", "int", true, "Mark type");

    service = server->registerService(new HTTPServiceDescription(g_controlMarkPositionURL), this);
    service->setDescription("Set a mark at the specified position");
    service->addArgument("position", "int64", true, "Position to set mark");
    service->addArgument("type", "int", true, "Mark type");

    service = server->registerService(new HTTPServiceDescription(g_controlClearMarkURL), this);
    service->setDescription("Clear the mark at the current position");

    service = server->registerService(new HTTPServiceDescription(g_controlClearAllMarksURL), this);
    service->setDescription("Clear all marks");

    service = server->registerService(new HTTPServiceDescription(g_controlSeekNextMarkURL), this);
    service->setDescription("Seek to the next mark");

    service = server->registerService(new HTTPServiceDescription(g_controlSeekPrevMarkURL), this);
    service->setDescription("Seek to the previous mark");

    service = server->registerService(new HTTPServiceDescription(g_controlSetOSDScreenURL), this);
    service->setDescription("Set the OSD screen");
    service->addArgument("screen", "enum", true, "One of the following: SOURCE_INFO, PLAY_STATE, MENU, EMPTY");

    service = server->registerService(new HTTPServiceDescription(g_controlNextOSDScreenURL), this);
    service->setDescription("Show the next OSD screen");

    service = server->registerService(new HTTPServiceDescription(g_controlSetOSDTimecodeURL), this);
    service->setDescription("Set the timecode to show in the OSD");
    service->addArgument("index", "int", false, "Timecode index");
    service->addArgument("type", "int", false, "Timecode type");
    service->addArgument("subtype", "int", false, "Timecode sub-type");

    service = server->registerService(new HTTPServiceDescription(g_controlNextOSDTimecodeURL), this);
    service->setDescription("Show the next timecode in the OSD");

    service = server->registerService(new HTTPServiceDescription(g_controlSetVideoURL), this);
    service->setDescription("Switch to the video source");
    service->addArgument("index", "int", true, "The index of the video");

    service = server->registerService(new HTTPServiceDescription(g_controlNextVideoURL), this);
    service->setDescription("Switch to the next video source");

    service = server->registerService(new HTTPServiceDescription(g_controlPrevVideoURL), this);
    service->setDescription("Switch to the previous video source");

    service = server->registerService(new HTTPServiceDescription(g_controlMuteAudioURL), this);
    service->setDescription("Enable/disable audio muting");
    service->addArgument("mute", "int", true, "toggle (-1), mute (1) or don't mute (0)");

    service = server->registerService(new HTTPServiceDescription(g_controlSetAudioGroupURL), this);
    service->setDescription("Switch to the audio group");
    service->addArgument("index", "int", true, "The index of the audio group");

    service = server->registerService(new HTTPServiceDescription(g_controlNextAudioGroupURL), this);
    service->setDescription("Switch to the next audio group");

    service = server->registerService(new HTTPServiceDescription(g_controlPrevAudioGroupURL), this);
    service->setDescription("Switch to the previous audio group");

    service = server->registerService(new HTTPServiceDescription(g_controlSnapAudioGroupURL), this);
    service->setDescription("Snap the audio group to the video");

    service = server->registerService(new HTTPServiceDescription(g_controlReviewStartURL), this);
    service->setDescription("Review the start");
    service->addArgument("duration", "int64", true, "Review duration");

    service = server->registerService(new HTTPServiceDescription(g_controlReviewEndURL), this);
    service->setDescription("Review the end");
    service->addArgument("duration", "int64", true, "Review duration");

    service = server->registerService(new HTTPServiceDescription(g_controlReviewURL), this);
    service->setDescription("Review around current position");
    service->addArgument("duration", "int64", true, "Review duration");


    memset(&_frameDisplayed, 0, sizeof(_frameDisplayed));
    memset(&_frameDropped, 0, sizeof(_frameDropped));
    memset(&_stateChange, 0, sizeof(_stateChange));
    _playerClosed = true;
    memset(&_state, 0, sizeof(_state));
}

HTTPIngexPlayer::~HTTPIngexPlayer()
{
    delete _player;
}

void HTTPIngexPlayer::frameDisplayedEvent(const FrameInfo* frameInfo)
{
    LOCK_SECTION(_playerStatusMutex);

    _frameDisplayed = *frameInfo;
    _state.displayedFrame = *frameInfo;
    _playerClosed = false;
}

void HTTPIngexPlayer::frameDroppedEvent(const FrameInfo* lastFrameInfo)
{
    LOCK_SECTION(_playerStatusMutex);

    _frameDropped = *lastFrameInfo;
    _playerClosed = false;
}

void HTTPIngexPlayer::stateChangeEvent(const MediaPlayerStateEvent* event)
{
    LOCK_SECTION(_playerStatusMutex);

    _stateChange = *event;
    _state.locked = event->locked;
    _state.play = event->play;
    _state.stop = event->stop;
    _state.speed = event->speed;
}

void HTTPIngexPlayer::endOfSourceEvent(const FrameInfo* lastReadFrameInfo)
{
    // ignoring because end of source can be determined by looking at _frameDisplayed
}

void HTTPIngexPlayer::startOfSourceEvent(const FrameInfo* firstReadFrameInfo)
{
    // ignoring because start of source can be determined by looking at _frameDisplayed
}

void HTTPIngexPlayer::playerCloseRequested()
{
    // TODO: fix resource lock issue: _player->close();
}

void HTTPIngexPlayer::playerClosed()
{
    LOCK_SECTION(_playerStatusMutex);

    _playerClosed = true;
}

void HTTPIngexPlayer::keyPressed(int key, int modifier)
{
    if (key == 'q' && modifier == 0x00)
    {
        // TODO: fix resource lock issue: _player->close();
    }

    // TODO: other key presses we want to support
}

void HTTPIngexPlayer::keyReleased(int key, int modifier)
{
    // TODO: key releases we want to support
}

void HTTPIngexPlayer::progressBarPositionSet(float position)
{
    _player->seek((int64_t)(position * 1000.0), SEEK_SET, PERCENTAGE_PLAY_UNIT);
    _player->pause();
}

void HTTPIngexPlayer::mouseClicked(int imageWidth, int imageHeight, int xPos, int yPos)
{
    // TODO: mouse clicks we want to support
}


bool HTTPIngexPlayer::processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection)
{
    if (serviceDescription->getURL().compare(g_pingURL) == 0)
    {
        ping(connection);
    }
    else if (serviceDescription->getURL().compare(g_settingsOutputTypeURL) == 0)
    {
        setOutputType(connection);
    }
    else if (serviceDescription->getURL().compare(g_settingsDVSTargetURL) == 0)
    {
        setDVSTarget(connection);
    }
    else if (serviceDescription->getURL().compare(g_settingsVideoSplitURL) == 0)
    {
        setVideoSplit(connection);
    }
    else if (serviceDescription->getURL().compare(g_settingsSDIOSDEnableURL) == 0)
    {
        setSDIOSDEnable(connection);
    }
    else if (serviceDescription->getURL().compare(g_settingsX11WindowNameURL) == 0)
    {
        setX11WindowName(connection);
    }
    else if (serviceDescription->getURL().compare(g_infoStateURL) == 0)
    {
        getState(connection);
    }
    else if (serviceDescription->getURL().compare(g_infoStatePushURL) == 0)
    {
        return getStatePush(connection);
    }
    else if (serviceDescription->getURL().compare(g_infoDVSIsAvailableURL) == 0)
    {
        dvsIsAvailable(connection);
    }
    else if (serviceDescription->getURL().compare(g_infoOutputTypeURL) == 0)
    {
        getOutputType(connection);
    }
    else if (serviceDescription->getURL().compare(g_infoActualOutputTypeURL) == 0)
    {
        getActualOutputType(connection);
    }
    else if (serviceDescription->getURL().compare(g_startURL) == 0)
    {
        start(connection);
    }
    else if (serviceDescription->getURL().compare(g_closeURL) == 0)
    {
        close(connection);
    }
    else if (serviceDescription->getURL().compare(g_resetURL) == 0)
    {
        reset(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlToggleLockURL) == 0)
    {
        toggleLock(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlPlayURL) == 0)
    {
        play(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlPauseURL) == 0)
    {
        pause(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlTogglePlayPauseURL) == 0)
    {
        togglePlayPause(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlPlaySpeedURL) == 0)
    {
        playSpeed(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSeekURL) == 0)
    {
        seek(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlStepURL) == 0)
    {
        step(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlMarkURL) == 0)
    {
        mark(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlMarkPositionURL) == 0)
    {
        markPosition(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlClearMarkURL) == 0)
    {
        clearMark(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlClearAllMarksURL) == 0)
    {
        clearAllMarks(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSeekNextMarkURL) == 0)
    {
        seekNextMark(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSeekPrevMarkURL) == 0)
    {
        seekPrevMark(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSetOSDScreenURL) == 0)
    {
        setOSDScreen(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlNextOSDScreenURL) == 0)
    {
        nextOSDScreen(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSetOSDTimecodeURL) == 0)
    {
        setOSDTimecode(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlNextOSDTimecodeURL) == 0)
    {
        nextOSDTimecode(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlNextVideoURL) == 0)
    {
        switchNextVideo(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlPrevVideoURL) == 0)
    {
        switchPrevVideo(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSetVideoURL) == 0)
    {
        switchVideo(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlMuteAudioURL) == 0)
    {
        muteAudio(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlNextAudioGroupURL) == 0)
    {
        switchNextAudioGroup(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlPrevAudioGroupURL) == 0)
    {
        switchPrevAudioGroup(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSetAudioGroupURL) == 0)
    {
        switchAudioGroup(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlSnapAudioGroupURL) == 0)
    {
        snapAudioToVideo(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlReviewStartURL) == 0)
    {
        reviewStart(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlReviewEndURL) == 0)
    {
        reviewEnd(connection);
    }
    else if (serviceDescription->getURL().compare(g_controlReviewURL) == 0)
    {
        review(connection);
    }
    else
    {
        INGEX_ASSERT(false);
    }

    return true;
}

void HTTPIngexPlayer::ping(HTTPConnection* connection)
{
    connection->sendOk();
}

void HTTPIngexPlayer::setOutputType(HTTPConnection* connection)
{
    int type;
    float scale = 1.0;

    string typeArg = connection->getQueryValue("type");
    if (typeArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "type");
        return;
    }
    if (!parse_int(typeArg, &type) || type < DVS_OUTPUT || type > DUAL_DVS_X11_XV_OUTPUT)
    {
        reportInvalidQueryArgument(connection, "type", "integer (PlayerOutputType enum)");
        return;
    }

    string scaleArg = connection->getQueryValue("scale");
    if (scaleArg.size() > 0 && !parse_float(scaleArg, &scale))
    {
        reportInvalidQueryArgument(connection, "scale", "float");
        return;
    }

    _player->setOutputType((PlayerOutputType)type);
    _player->setScale(scale);

    connection->sendOk();
}

void HTTPIngexPlayer::setDVSTarget(HTTPConnection* connection)
{
    int card = -1;
    int channel = -1;

    string cardArg = connection->getQueryValue("card");
    if (cardArg.size() > 0 && !parse_int(cardArg, &card))
    {
        reportInvalidQueryArgument(connection, "card", "int");
        return;
    }

    string channelArg = connection->getQueryValue("channel");
    if (channelArg.size() > 0 && !parse_int(channelArg, &channel))
    {
        reportInvalidQueryArgument(connection, "channel", "int");
        return;
    }

    _player->setDVSTarget(card, channel);

    connection->sendOk();
}

void HTTPIngexPlayer::setVideoSplit(HTTPConnection* connection)
{
    int type;

    string typeArg = connection->getQueryValue("type");
    if (typeArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "type");
        return;
    }
    if (!parse_int(typeArg, &type) ||
        (type != NO_SPLIT_VIDEO_SWITCH && type != QUAD_SPLIT_VIDEO_SWITCH && type != NONA_SPLIT_VIDEO_SWITCH))
    {
        reportInvalidQueryArgument(connection, "type", "integer (VideoSwitchSplit enum)");
        return;
    }

    _player->setVideoSplit((VideoSwitchSplit)type);

    connection->sendOk();
}

void HTTPIngexPlayer::setSDIOSDEnable(HTTPConnection* connection)
{
    bool enable;

    string enableArg = connection->getQueryValue("enable");
    if (enableArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "enable");
        return;
    }
    if (!parse_bool(enableArg, &enable))
    {
        reportInvalidQueryArgument(connection, "enable", "boolean");
        return;
    }

    _player->setSDIOSDEnable(enable);

    connection->sendOk();
}

void HTTPIngexPlayer::setX11WindowName(HTTPConnection* connection)
{
    string nameArg = connection->getQueryValue("name");
    if (nameArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "name");
        return;
    }

    _player->setX11WindowName(nameArg);

    connection->sendOk();
}

void HTTPIngexPlayer::getState(HTTPConnection* connection)
{
    HTTPPlayerState state;
    bool playerClosed;

    {
        LOCK_SECTION(_playerStatusMutex);
        state = _state;
        playerClosed = _playerClosed;
    }

    JSONObject* json = serialize_player_state(&state);
    if (json == 0)
    {
        connection->sendServerError("Failed to serialize player state");
        return;
    }
    json->setBool("closed", playerClosed);


    connection->sendJSON(json);
    delete json;
}

// TODO: this function isn't scalable when multiple clients are connected because the shttpd library is
// calling http_service in HTTPServer without pausing. This required the usleep call below to prevent CPU
// usage nearing it's maximum. If multiple clients  are connected to get the state then each will result
// in an an additional usleep and this will make the server less responsive. See shttpd/examples/scalable.c
// for an example of how to spawn a thread  for each connection
bool HTTPIngexPlayer::getStatePush(HTTPConnection* connection)
{
    HTTPPlayerState state;
    bool playerClosed;
    HTTPConnectionState* connectionState = 0;
    PlayerConnectionState* playerConnectionState = 0;
    bool firstCall = false;

    connectionState = connection->getState("playerstatepush");
    if (connectionState == 0)
    {
        connectionState = connection->insertState("playerstatepush", new PlayerConnectionState());
        firstCall = true;
    }
    playerConnectionState = dynamic_cast<PlayerConnectionState*>(connectionState);


    {
        LOCK_SECTION(_playerStatusMutex);
        state = _state;
        playerClosed = _playerClosed;
    }


    if (memcmp(&state, &playerConnectionState->playerState, sizeof(state)) != 0)
    {
        JSONObject* json = serialize_player_state(&state);
        if (json == 0)
        {
            connection->sendServerError("Failed to serialize player state");
            return true;
        }
        json->setBool("closed", playerClosed);


        if (firstCall)
        {
            connection->startMultipartResponse("KINGSWOODWARRENKT206NP");
        }

        connection->sendMultipartJSON(json, "KINGSWOODWARRENKT206NP");

        delete json;
    }
    else
    {
        if (firstCall)
        {
            connection->startMultipartResponse("KINGSWOODWARRENKT206NP");
        }
        else
        {
            // slow the HTTP server connection processing to avoid 100% CPU usage
            usleep(1000);
        }
    }


    playerConnectionState->playerState = state;

    connection->setResponseDataIsReady();
    connection->setResponseDataIsPartial();

    return false;
}

void HTTPIngexPlayer::dvsIsAvailable(HTTPConnection* connection)
{
    bool result;
    int card = -1;
    int channel = -1;


    string cardArg = connection->getQueryValue("card");
    if (cardArg.size() > 0 && !parse_int(cardArg, &card))
    {
        reportInvalidQueryArgument(connection, "card", "int");
        return;
    }

    string channelArg = connection->getQueryValue("channel");
    if (channelArg.size() > 0 && !parse_int(channelArg, &channel))
    {
        reportInvalidQueryArgument(connection, "channel", "int");
        return;
    }


    result = _player->dvsCardIsAvailable(card, channel);

    JSONObject json;
    json.setBool("dvsCardIsAvailable", result);

    connection->sendJSON(&json);
}

void HTTPIngexPlayer::getOutputType(HTTPConnection* connection)
{
    PlayerOutputType outputType = _player->getOutputType();

    JSONObject json;
    json.setNumber("outputType", outputType);
    json.setString("outputTypeString", getOutputTypeString(outputType));

    connection->sendJSON(&json);
}

void HTTPIngexPlayer::getActualOutputType(HTTPConnection* connection)
{
    PlayerOutputType outputType = _player->getActualOutputType();

    JSONObject json;
    json.setNumber("actualOutputType", outputType);
    json.setString("actualOutputTypeString", getOutputTypeString(outputType));

    connection->sendJSON(&json);
}

void HTTPIngexPlayer::start(HTTPConnection* connection)
{
    vector<PlayerInput> inputs;
    PlayerInput input;
    JSONObject* jsonObject;
    JSONObject* jsonObject2;
    JSONArray* jsonArray;
    JSONObject* json;
    int index = 0;
    auto_ptr<JSONObject> guard;
    int64_t startPosition = 0;
    bool startPaused = false;
    int64_t number;

    json = JSONObject::parse(connection->getPostData().c_str(), &index);
    if (json == NULL)
    {
        printf("%s\n", connection->getPostData().c_str());
        reportInvalidJSON(connection);
        return;
    }

    guard = auto_ptr<JSONObject>(json);



    // get start options

    json->getNumberValue2("startPosition", &startPosition);
    json->getBoolValue2("startPaused", &startPaused);



    // get inputs and options

    jsonArray = json->getArrayValue("inputs");
    if (jsonArray == 0)
    {
        reportMissingOrWrongTypeJSONValue(connection, "inputs", "array");
        return;
    }

    vector<JSONValue*>::const_iterator iter;
    for (iter = jsonArray->getValue().begin(); iter != jsonArray->getValue().end(); iter++)
    {
        jsonObject = dynamic_cast<JSONObject*>(*iter);
        if (jsonObject == 0)
        {
            reportMissingOrWrongTypeJSONValue(connection, "inputs element", "object");
            return;
        }

        // type
        if (!jsonObject->getNumberValue2("type", &number))
        {
            reportMissingOrWrongTypeJSONValue(connection, "inputs element::type", "number");
            return;
        }
        if ((int)number < MXF_INPUT || (int)number > CLAPPER_INPUT)
        {
            reportInvalidJSONValue(connection, "inputs::type", "enum");
            return;
        }
        input.type = (PlayerInputType)number;

        // name
        if (!jsonObject->getStringValue2("name", &input.name))
        {
            reportMissingOrWrongTypeJSONValue(connection, "inputs element::name", "string");
            return;
        }

        // options
        jsonObject2 = jsonObject->getObjectValue("options");
        if (jsonObject2 != 0)
        {
            vector<pair<JSONString*, JSONValue*> >::const_iterator iter2;
            for (iter2 = jsonObject2->getValue().begin(); iter2 != jsonObject2->getValue().end(); iter2++)
            {
                JSONValue* jsonValue = (*iter2).second;

                if (dynamic_cast<JSONString*>(jsonValue) == 0 &&
                    dynamic_cast<JSONNumber*>(jsonValue) == 0 &&
                    dynamic_cast<JSONBool*>(jsonValue) == 0)
                {
                    reportMissingOrWrongTypeJSONValue(connection, "inputs element::options element", "string/number/bool");
                    return;
                }

                if (dynamic_cast<JSONNumber*>(jsonValue) != 0 ||
                    dynamic_cast<JSONBool*>(jsonValue) != 0)
                {
                    input.options.insert(pair<string, string>((*iter2).first->getValue(), jsonValue->toString()));
                }
                else
                {
                    input.options.insert(pair<string, string>((*iter2).first->getValue(), dynamic_cast<JSONString*>(jsonValue)->getValue()));
                }
            }
        }

        inputs.push_back(input);
    }

    if (inputs.size() == 0)
    {
        connection->sendBadRequest("No inputs");
        return;
    }


    // start the player

    vector<bool> opened;
    if (!_player->start(inputs, opened, startPaused, startPosition))
    {
        Logging::warning("Failed to start player\n");
    }
    else
    {
        Logging::info("Player start called\n");
        vector<bool>::const_iterator iterOpen;
        vector<PlayerInput>::const_iterator iterInput;
        for (iterInput = inputs.begin(), iterOpen = opened.begin();
            iterInput != inputs.end() && iterOpen != opened.end();
            iterInput++, iterOpen++)
        {
            if (*iterOpen)
            {
                Logging::info("Opened '%s' (type=%d)\n", (*iterInput).name.c_str(), (*iterInput).type);
            }
            else if (*iterOpen)
            {
                Logging::info("Failed to open '%s' (type=%d)\n", (*iterInput).name.c_str(), (*iterInput).type);
            }
        }
    }

    connection->sendOk();
}

void HTTPIngexPlayer::close(HTTPConnection* connection)
{
    Logging::info("Player close called\n");

    if (!_player->close())
    {
        Logging::warning("Failed to close\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::reset(HTTPConnection* connection)
{
    if (!_player->reset())
    {
        Logging::warning("Failed to reset\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::toggleLock(HTTPConnection* connection)
{
    if (!_player->toggleLock())
    {
        Logging::warning("Failed to toggleLock\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::play(HTTPConnection* connection)
{
    if (!_player->play())
    {
        Logging::warning("Failed to play\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::pause(HTTPConnection* connection)
{
    if (!_player->pause())
    {
        Logging::warning("Failed to pause\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::togglePlayPause(HTTPConnection* connection)
{
    if (!_player->togglePlayPause())
    {
        Logging::warning("Failed to togglePlayPause\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::seek(HTTPConnection* connection)
{
    int64_t offset;
    int whence;
    int unit;

    string offsetArg = connection->getQueryValue("offset");
    if (offsetArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "offset");
        return;
    }
    if (!parse_int64(offsetArg, &offset))
    {
        reportInvalidQueryArgument(connection, "offset", "int64");
        return;
    }

    string whenceArg = connection->getQueryValue("whence");
    if (whenceArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "whence");
        return;
    }
    if (whenceArg.compare("SEEK_SET") == 0)
    {
        whence = SEEK_SET;
    }
    else if (whenceArg.compare("SEEK_CUR") == 0)
    {
        whence = SEEK_CUR;
    }
    else if (whenceArg.compare("SEEK_END") == 0)
    {
        whence = SEEK_END;
    }
    else
    {
        reportInvalidQueryArgument(connection, "whence", "enum SEEK_SET, SEEK_CUR or SEEK_END");
        return;
    }

    string unitArg = connection->getQueryValue("unit");
    if (unitArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "unit");
        return;
    }
    if (unitArg.compare("FRAME_PLAY_UNIT") == 0)
    {
        unit = FRAME_PLAY_UNIT;
    }
    else if (unitArg.compare("PERCENTAGE_PLAY_UNIT") == 0)
    {
        unit = PERCENTAGE_PLAY_UNIT;
    }
    else
    {
        reportInvalidQueryArgument(connection, "unit", "enum FRAME_PLAY_UNIT or PERCENTAGE_PLAY_UNIT");
        return;
    }


    if (!_player->seek(offset, whence, (PlayUnit)unit))
    {
        Logging::warning("Failed to seek(%"PRIi64", %d, %d)\n", offset, whence, unit);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::playSpeed(HTTPConnection* connection)
{
    int speed;

    string speedArg = connection->getQueryValue("speed");
    if (speedArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "speed");
        return;
    }
    if (!parse_int(speedArg, &speed))
    {
        reportInvalidQueryArgument(connection, "speed", "int");
        return;
    }

    if (!_player->playSpeed(speed))
    {
        Logging::warning("Failed to playSpeed(%d)\n", speed);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::step(HTTPConnection* connection)
{
    bool forwards;

    string forwardsArg = connection->getQueryValue("forwards");
    if (forwardsArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "forwards");
        return;
    }
    if (!parse_bool(forwardsArg, &forwards))
    {
        reportInvalidQueryArgument(connection, "forwards", "bool");
        return;
    }

    if (!_player->step(forwards))
    {
        Logging::warning("Failed to step(%s)\n", (forwards ? "true" : "false"));
    }

    connection->sendOk();
}

void HTTPIngexPlayer::mark(HTTPConnection* connection)
{
    int type;

    string typeArg = connection->getQueryValue("type");
    if (typeArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "type");
        return;
    }
    if (!parse_int(typeArg, &type))
    {
        reportInvalidQueryArgument(connection, "type", "int");
        return;
    }

    if (!_player->mark(type))
    {
        Logging::warning("Failed to mark(%d)\n", type);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::markPosition(HTTPConnection* connection)
{
    int64_t position;
    int type;

    string positionArg = connection->getQueryValue("position");
    if (positionArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "position");
        return;
    }
    if (!parse_int64(positionArg, &position))
    {
        reportInvalidQueryArgument(connection, "position", "int64");
        return;
    }

    string typeArg = connection->getQueryValue("type");
    if (typeArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "type");
        return;
    }
    if (!parse_int(typeArg, &type))
    {
        reportInvalidQueryArgument(connection, "type", "int");
        return;
    }

    if (!_player->markPosition(position, type))
    {
        Logging::warning("Failed to markPosition(%"PRIi64", %d)\n", position, type);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::clearMark(HTTPConnection* connection)
{
    if (!_player->clearMark())
    {
        Logging::warning("Failed to clearMark\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::clearAllMarks(HTTPConnection* connection)
{
    if (!_player->clearAllMarks())
    {
        Logging::warning("Failed to clearAllMarks\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::seekNextMark(HTTPConnection* connection)
{
    if (!_player->seekNextMark())
    {
        Logging::warning("Failed to seekNextMark\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::seekPrevMark(HTTPConnection* connection)
{
    if (!_player->seekPrevMark())
    {
        Logging::warning("Failed to seekPrevMark\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::setOSDScreen(HTTPConnection* connection)
{
    OSDScreen screen;

    string screenArg = connection->getQueryValue("screen");
    if (screenArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "screen");
        return;
    }
    if (screenArg.compare("SOURCE_INFO") == 0)
    {
        screen = OSD_SOURCE_INFO_SCREEN;
    }
    else if (screenArg.compare("PLAY_STATE") == 0)
    {
        screen = OSD_PLAY_STATE_SCREEN;
    }
    else if (screenArg.compare("MENU") == 0)
    {
        screen = OSD_MENU_SCREEN;
    }
    else if (screenArg.compare("EMPTY") == 0)
    {
        screen = OSD_EMPTY_SCREEN;
    }
    else
    {
        reportInvalidQueryArgument(connection, "screen", "enum SOURCE_INFO, PLAY_STATE, MENU, EMPTY");
        return;
    }

    if (!_player->setOSDScreen(screen))
    {
        Logging::warning("Failed to setOSDScreen(%d)\n", screen);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::nextOSDScreen(HTTPConnection* connection)
{
    if (!_player->nextOSDScreen())
    {
        Logging::warning("Failed to nextOSDScreen\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::setOSDTimecode(HTTPConnection* connection)
{
    int index;
    int type;
    int subType;

    string indexArg = connection->getQueryValue("index");
    if (indexArg.size() == 0)
    {
        index = -1;
    }
    else if (!parse_int(indexArg, &index))
    {
        reportInvalidQueryArgument(connection, "index", "int");
        return;
    }

    string typeArg = connection->getQueryValue("type");
    if (typeArg.size() == 0)
    {
        type = -1;
    }
    else if (!parse_int(typeArg, &type))
    {
        reportInvalidQueryArgument(connection, "type", "int");
        return;
    }

    string subTypeArg = connection->getQueryValue("subtype");
    if (subTypeArg.size() == 0)
    {
        subType = -1;
    }
    else if (!parse_int(subTypeArg, &subType))
    {
        reportInvalidQueryArgument(connection, "subtype", "int");
        return;
    }

    if (!_player->setOSDTimecode(index, type, subType))
    {
        Logging::warning("Failed to setOSDTimecode(%d, %d, %d)\n", index, type, subType);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::nextOSDTimecode(HTTPConnection* connection)
{
    if (!_player->nextOSDTimecode())
    {
        Logging::warning("Failed to nextOSDTimecode\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::switchNextVideo(HTTPConnection* connection)
{
    if (!_player->switchNextVideo())
    {
        Logging::warning("Failed to switchNextVideo\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::switchPrevVideo(HTTPConnection* connection)
{
    if (!_player->switchPrevVideo())
    {
        Logging::warning("Failed to switchPrevVideo\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::switchVideo(HTTPConnection* connection)
{
    int index;

    string indexArg = connection->getQueryValue("index");
    if (indexArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "index");
        return;
    }
    if (!parse_int(indexArg, &index))
    {
        reportInvalidQueryArgument(connection, "index", "int");
        return;
    }

    if (!_player->switchVideo(index))
    {
        Logging::warning("Failed to switchVideo(%d)\n", index);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::muteAudio(HTTPConnection* connection)
{
    int mute;

    string muteArg = connection->getQueryValue("mute");
    if (muteArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "mute");
        return;
    }
    if (!parse_int(muteArg, &mute))
    {
        reportInvalidQueryArgument(connection, "mute", "int");
        return;
    }

    if (!_player->muteAudio(mute))
    {
        Logging::warning("Failed to mute audio(%d)\n", mute);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::switchNextAudioGroup(HTTPConnection* connection)
{
    if (!_player->switchNextAudioGroup())
    {
        Logging::warning("Failed to switchNextAudioGroup\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::switchPrevAudioGroup(HTTPConnection* connection)
{
    if (!_player->switchPrevAudioGroup())
    {
        Logging::warning("Failed to switchPrevAudioGroup\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::switchAudioGroup(HTTPConnection* connection)
{
    int index;

    string indexArg = connection->getQueryValue("index");
    if (indexArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "index");
        return;
    }
    if (!parse_int(indexArg, &index))
    {
        reportInvalidQueryArgument(connection, "index", "int");
        return;
    }

    if (!_player->switchAudioGroup(index))
    {
        Logging::warning("Failed to switchAudioGroup(%d)\n", index);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::snapAudioToVideo(HTTPConnection* connection)
{
    if (!_player->snapAudioToVideo())
    {
        Logging::warning("Failed to snapAudioToVideo\n");
    }

    connection->sendOk();
}

void HTTPIngexPlayer::reviewStart(HTTPConnection* connection)
{
    int64_t duration;

    string durationArg = connection->getQueryValue("duration");
    if (durationArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "duration");
        return;
    }
    if (!parse_int64(durationArg, &duration))
    {
        reportInvalidQueryArgument(connection, "duration", "int64");
        return;
    }

    if (!_player->reviewStart(duration))
    {
        Logging::warning("Failed to reviewStart(%"PRIi64")\n", duration);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::reviewEnd(HTTPConnection* connection)
{
    int64_t duration;

    string durationArg = connection->getQueryValue("duration");
    if (durationArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "duration");
        return;
    }
    if (!parse_int64(durationArg, &duration))
    {
        reportInvalidQueryArgument(connection, "duration", "int64");
        return;
    }

    if (!_player->reviewEnd(duration))
    {
        Logging::warning("Failed to reviewEnd(%"PRIi64")\n", duration);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::review(HTTPConnection* connection)
{
    int64_t duration;

    string durationArg = connection->getQueryValue("duration");
    if (durationArg.size() == 0)
    {
        reportMissingQueryArgument(connection, "duration");
        return;
    }
    if (!parse_int64(durationArg, &duration))
    {
        reportInvalidQueryArgument(connection, "duration", "int64");
        return;
    }

    if (!_player->review(duration))
    {
        Logging::warning("Failed to review(%"PRIi64")\n", duration);
    }

    connection->sendOk();
}

void HTTPIngexPlayer::reportMissingQueryArgument(HTTPConnection* connection, string name)
{
    connection->sendBadRequest("Missing '%s' query argument", name.c_str());
    Logging::warning("%s, %s: Missing '%s' argument\n", connection->getRequestURI().c_str(),
        connection->getQueryString().c_str(), name.c_str());
}

void HTTPIngexPlayer::reportInvalidQueryArgument(HTTPConnection* connection, string name, string type)
{
    connection->sendBadRequest("Invalid query argument '%s' (type '%s')", name.c_str(), type.c_str());
    Logging::warning("%s, %s: Invalid argument '%s' (type '%s')\n", connection->getRequestURI().c_str(),
        connection->getQueryString().c_str(), name.c_str(), type.c_str());
}

void HTTPIngexPlayer::reportInvalidJSON(HTTPConnection* connection)
{
    connection->sendBadRequest("Invalid json data");
    Logging::warning("%s: Invalid json data\n", connection->getRequestURI().c_str());
}

void HTTPIngexPlayer::reportMissingOrWrongTypeJSONValue(HTTPConnection* connection, string name, string type)
{
    connection->sendBadRequest("Missing or wrong type for json value '%s' (type '%s')", name.c_str(), type.c_str());
    Logging::warning("%s: Missing or wrong type for json value '%s' (type '%s')\n", connection->getRequestURI().c_str(),
        name.c_str(), type.c_str());
}

void HTTPIngexPlayer::reportInvalidJSONValue(HTTPConnection* connection, std::string name, std::string type)
{
    connection->sendBadRequest("Invalid post data value for '%s' (type '%s')", type.c_str(), name.c_str());
    Logging::warning("%s: Invalid post data value for '%s' (type '%s')\n", connection->getRequestURI().c_str(),
        name.c_str(), type.c_str());
}

string HTTPIngexPlayer::getOutputTypeString(int outputType)
{
    switch (outputType)
    {
        case DVS_OUTPUT:
            return "DVS";
        case X11_AUTO_OUTPUT:
            return "X11 Auto";
        case X11_XV_OUTPUT:
            return "Xv Auto";
        case X11_OUTPUT:
            return "X11";
        case DUAL_DVS_AUTO_OUTPUT:
            return "Dual DVS Auto";
        case DUAL_DVS_X11_OUTPUT:
            return "Dual DVS X11";
        case DUAL_DVS_X11_XV_OUTPUT:
            return "Dual DVS Xv";
        default:
            return "";
    }
}
