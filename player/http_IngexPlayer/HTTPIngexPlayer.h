/*
 * $Id: HTTPIngexPlayer.h,v 1.1 2009/02/24 08:21:16 stuart_hc Exp $
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

#ifndef __INGEX_HTTP_INGEX_PLAYER_H__
#define __INGEX_HTTP_INGEX_PLAYER_H__


#include <LocalIngexPlayer.h>
#include "HTTPServer.h"
#include "HTTPPlayerState.h"
#include "Threads.h"


namespace ingex
{


class HTTPIngexPlayer : public HTTPConnectionHandler, public prodauto::IngexPlayerListener
{
public:
    HTTPIngexPlayer(HTTPServer* server, prodauto::LocalIngexPlayer* player, prodauto::IngexPlayerListenerRegistry* listenerRegistry);
    virtual ~HTTPIngexPlayer();

    // from HTTPConnectionHandler
    virtual bool processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection);

    // from IngexPlayerListener
    virtual void frameDisplayedEvent(const FrameInfo* frameInfo);
    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo);
    virtual void stateChangeEvent(const MediaPlayerStateEvent* event);
    virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo);
    virtual void startOfSourceEvent(const FrameInfo* firstReadFrameInfo);
    virtual void playerCloseRequested();
    virtual void playerClosed();
    virtual void keyPressed(int key, int modifier);
    virtual void keyReleased(int key, int modifier);
    virtual void progressBarPositionSet(float position);
    virtual void mouseClicked(int imageWidth, int imageHeight, int xPos, int yPos);


    void ping(HTTPConnection* connection);

    void setOutputType(HTTPConnection* connection);
    void setDVSTarget(HTTPConnection* connection);
    void setVideoSplit(HTTPConnection* connection);
    void setSDIOSDEnable(HTTPConnection* connection);
    void setX11WindowName(HTTPConnection* connection);

    void getState(HTTPConnection* connection);
    bool getStatePush(HTTPConnection* connection);
    void dvsIsAvailable(HTTPConnection* connection);
    void getOutputType(HTTPConnection* connection);
    void getActualOutputType(HTTPConnection* connection);

    void start(HTTPConnection* connection);
    void close(HTTPConnection* connection);
    void reset(HTTPConnection* connection);

    void toggleLock(HTTPConnection* connection);
    void play(HTTPConnection* connection);
    void pause(HTTPConnection* connection);
    void togglePlayPause(HTTPConnection* connection);
    void seek(HTTPConnection* connection);
    void playSpeed(HTTPConnection* connection);
    void step(HTTPConnection* connection);
    void mark(HTTPConnection* connection);
    void markPosition(HTTPConnection* connection);
    void clearMark(HTTPConnection* connection);
    void clearAllMarks(HTTPConnection* connection);
    void seekNextMark(HTTPConnection* connection);
    void seekPrevMark(HTTPConnection* connection);
    void setOSDScreen(HTTPConnection* connection);
    void nextOSDScreen(HTTPConnection* connection);
    void setOSDTimecode(HTTPConnection* connection);
    void nextOSDTimecode(HTTPConnection* connection);
    void switchNextVideo(HTTPConnection* connection);
    void switchPrevVideo(HTTPConnection* connection);
    void switchVideo(HTTPConnection* connection);
    void muteAudio(HTTPConnection* connection);
    void switchNextAudioGroup(HTTPConnection* connection);
    void switchPrevAudioGroup(HTTPConnection* connection);
    void switchAudioGroup(HTTPConnection* connection);
    void snapAudioToVideo(HTTPConnection* connection);
    void reviewStart(HTTPConnection* connection);
    void reviewEnd(HTTPConnection* connection);
    void review(HTTPConnection* connection);

private:
    void reportMissingQueryArgument(HTTPConnection* connection, std::string name);
    void reportInvalidQueryArgument(HTTPConnection* connection, std::string name, std::string type);
    void reportInvalidJSON(HTTPConnection* connection);
    void reportMissingOrWrongTypeJSONValue(HTTPConnection* connection, std::string name, std::string type);
    void reportInvalidJSONValue(HTTPConnection* connection, std::string name, std::string type);

    std::string getOutputTypeString(int outputType);

    prodauto::LocalIngexPlayer* _player;

    Mutex _playerStatusMutex;
    FrameInfo _frameDisplayed;
    FrameInfo _frameDropped;
    MediaPlayerStateEvent _stateChange;
    bool _playerClosed;
    HTTPPlayerState _state;
};


};


#endif
