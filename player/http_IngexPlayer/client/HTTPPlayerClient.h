/*
 * $Id: HTTPPlayerClient.h,v 1.1 2009/02/24 08:21:17 stuart_hc Exp $
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

#ifndef __INGEX_HTTP_PLAYER_CLIENT_H__
#define __INGEX_HTTP_PLAYER_CLIENT_H__


#include <string>
#include <map>

#include <curl/curl.h>

#include <video_switch_sink.h>

#include <HTTPPlayerState.h>

#include "IngexPlayer.h"

namespace ingex
{


typedef enum PlayerOutputType
{
    UNKNOWN_OUTPUT = 0,
    /* playout to SDI via DVS card */
    DVS_OUTPUT,
    /* auto-detect whether X11 X video extension is available, if not use plain X11 */
    X11_AUTO_OUTPUT,
    /* playout to X11 window using X video extension*/
    X11_XV_OUTPUT,
    /* playout to X11 window */
    X11_OUTPUT,
    /* playout to both SDI and X11 window (auto-detect availability of Xv) */
    DUAL_DVS_AUTO_OUTPUT,
    /* playout to both SDI and X11 window */
    DUAL_DVS_X11_OUTPUT,
    /* playout to both SDI and X11 X video extension window */
    DUAL_DVS_X11_XV_OUTPUT
};

typedef enum
{
    MXF_INPUT = 1,
    RAW_INPUT,
    DV_INPUT,
    FFMPEG_INPUT,
    SHM_INPUT,
    UDP_INPUT,
    BALLS_INPUT,
    BLANK_INPUT,
    CLAPPER_INPUT
} PlayerInputType;

typedef struct
{
    PlayerInputType type;
    std::string name;
    std::map<std::string, std::string> options;
} PlayerInput;

/* options for each PlayerInputType are as follows:

    MXF_INPUT:
        none

    RAW_INPUT:
        "stream_type": "picture", "sound", "timecode" or "event"  (default "picture")
        "stream_format": "uyvy", "yuv422", "yuv420", "yuv411", "pcm", "timecode"  (default "uyvy")
        "frame_rate": "<numerator>/<denominator>"  (default 25/1)
      video only:
        "width": <image width> (default 720)
        "height": <image height> (default 576)
        "aspect_ratio": "<numerator>/<denominator>" (default 4/3)
      audio only:
        "sampling rate": "<numerator>/<denominator>" (default 48000/1)
        "num_channels": <num>  (default 1)
        "bits_per_sample": <bps>  (default 16)

    DV_INPUT:
        none

    FFMPEG_INPUT:
        "num_ffmpeg_threads" <num> (default 0)

    SHM_INPUT:
        none

    UDP_INPUT:
        none

    BBALLS_INPUT:
        same as RAW_INPUT
        "num_balls": <num> (default 5)

    BLANK_INPUT:
        same as RAW_INPUT

    CLAPPER_INPUT:
        same as RAW_INPUT, but video only
*/


class HTTPPlayerClient;

class HTTPPlayerClientListener
{
public:
    virtual ~HTTPPlayerClientListener() {}

    virtual void stateUpdate(HTTPPlayerClient* client, HTTPPlayerState state) {};
};



class HTTPPlayerClient : public prodauto::IngexPlayer
{
public:
    friend void* player_state_poll_thread(void* arg);

public:
    HTTPPlayerClient();
    virtual ~HTTPPlayerClient();


    bool connect(std::string hostName, int port, int statePollInterval /* msec */);
    bool ping();
    void disconnect();


    std::string getVersion();
    std::string getBuildTimestamp();


    /* returns true if a DVS card is available for output */
    bool dvsCardIsAvailable();
    bool dvsCardIsAvailable(int card, int channel);


    /* setting the output type will cause the the player to be stop()ped and restarted when start() is called again */
    void setOutputType(PlayerOutputType outputType, float scale);
    void setDVSTarget(int card, int channel);

    /* returns the output type used */
    PlayerOutputType getOutputType();
    /* same as getOutputType(), except it returns the actual output type if an auto type is used */
    /* eg. if X11_AUTO_OUTPUT is set then returns either X11_XV_OUTPUT or X11_OUTPUT */
    PlayerOutputType getActualOutputType();

    /* sets the video split type (see ingex_player/video_switch_sink.h for enum values) when start() is called again */
    void setVideoSplit(VideoSwitchSplit videoSplit);

    /* disables/enables the on screen display in the SDI output */
    void setSDIOSDEnable(bool enable);


    HTTPPlayerState getState();


    /* will reset the player and display blank video on the output - returns false if a reset fails and
       the player will be closed */
    bool reset();

    /* will stop the player and close the X11 window */
    bool close();


    /* opens the files/sources and start playing */
    bool start(std::vector<PlayerInput> inputs, bool startPaused, int64_t startPosition);

    /* functions inherited from IngexPlayerListenerRegistry */
    bool registerListener(HTTPPlayerClientListener* listener);
    bool unregisterListener(HTTPPlayerClientListener* listener);

    /* functions inherited from IngexPlayer */
    virtual bool setX11WindowName(std::string name);
    virtual bool stop();
    virtual bool toggleLock();
    virtual bool play();
    virtual bool pause();
    virtual bool togglePlayPause();
    virtual bool seek(int64_t offset, int whence, PlayUnit unit);
    virtual bool playSpeed(int speed);
    virtual bool step(bool forward);
    virtual bool muteAudio(int mute);
    virtual bool mark(int type);
    virtual bool markPosition(int64_t position, int type);
    virtual bool clearMark();
    virtual bool clearAllMarks();
    virtual bool seekNextMark();
    virtual bool seekPrevMark();
    virtual bool setOSDScreen(OSDScreen screen);
    virtual bool nextOSDScreen();
    virtual bool setOSDTimecode(int index, int type, int subType);
    virtual bool nextOSDTimecode();
    virtual bool switchNextVideo();
    virtual bool switchPrevVideo();
    virtual bool switchVideo(int index);
    virtual bool switchNextAudioGroup();
    virtual bool switchPrevAudioGroup();
    virtual bool switchAudioGroup(int index); /* index == 0 is equiv. to snapAudioToVideo */
    virtual bool snapAudioToVideo();
    virtual bool reviewStart(int64_t duration);
    virtual bool reviewEnd(int64_t duration);
    virtual bool review(int64_t duration);


private:
    void pollThread();
    bool startPollThread();
    void stopPollThread();

    bool isConnected();
    bool sendSimpleCommand(std::string command);
    bool sendSimpleCommand(std::string command, std::map<std::string, std::string> args);
    bool sendJSONCommand(std::string command, std::string jsonString);
    bool sendJSONInfoCommand(std::string command, std::string* jsonString);

    pthread_mutex_t _connectionMutex;
    CURL* _curl;
    std::string _hostName;
    int _port;
    int _statePollInterval;
    pthread_t _pollThreadId;
    bool _stopPollThread;
    HTTPPlayerState _state;
    char _urlBuffer[4096];

    typedef struct
    {
        HTTPPlayerClientListener* listener;
        HTTPPlayerState state;
    } ListenerData;

    pthread_rwlock_t _listenersRWLock;
    std::vector<ListenerData> _listeners;
};


};



#endif
