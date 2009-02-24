/*
 * $Id: HTTPPlayerClient.cpp,v 1.1 2009/02/24 08:21:17 stuart_hc Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#include <pthread.h>
#include <cassert>
#include <unistd.h>
#include <cstring>
#include <memory>

#include "HTTPPlayerClient.h"

#include <JSONObject.h>
#include <Utilities.h>


using namespace std;
using namespace ingex;


#define LOCK_SECTION(m)         MutexGuard __guard(m);


static const int g_defaultStatePollInterval = 1000; /* 1 sec */



class MutexGuard
{
public:
    MutexGuard(pthread_mutex_t* mutex)
    : _mutex(NULL)
    {
        if (pthread_mutex_lock(mutex) != 0)
        {
            fprintf(stderr, "pthread_mutex_lock failed\n");
        }
        else
        {
            _mutex = mutex;
        }
    }

    ~MutexGuard()
    {
        if (_mutex != NULL)
        {
            if (pthread_mutex_unlock(_mutex) != 0)
            {
                fprintf(stderr, "pthread_mutex_unlock failed\n");
            }
        }
    }

private:
    pthread_mutex_t* _mutex;
};


class ReadWriteLockGuard
{
public:
    ReadWriteLockGuard(pthread_rwlock_t* rwlock, bool writeLock)
    : _rwlock(NULL)
    {
        if (writeLock)
        {
            if (pthread_rwlock_wrlock(rwlock) != 0)
            {
                fprintf(stderr, "pthread_rwlock_wrlock failed\n");
            }
            else
            {
                _rwlock = rwlock;
            }
        }
        else
        {
            if (pthread_rwlock_rdlock(rwlock) != 0)
            {
                fprintf(stderr, "pthread_rwlock_rdlock failed\n");
            }
            else
            {
                _rwlock = rwlock;
            }
        }
    }

    ~ReadWriteLockGuard()
    {
        if (_rwlock != NULL)
        {
            if (pthread_rwlock_unlock(_rwlock) != 0)
            {
                fprintf(stderr, "pthread_rwlock_unlock failed\n");
            }
        }
    }

private:
    pthread_rwlock_t* _rwlock;
};



static const char* g_playerVersion = "version 0.1";
static const char* g_playerBuildTimestamp = __DATE__ " " __TIME__;


namespace ingex
{

void* player_state_poll_thread(void* arg)
{
    HTTPPlayerClient* client = (HTTPPlayerClient*)arg;

    try
    {
        client->pollThread();
    }
    catch (...)
    {
        fprintf(stderr, "Exception thrown in state poll thread. Exiting thread.\n");
    }

    pthread_exit((void*) 0);
}

};

static size_t receive_json_data(void* ptr, size_t size, size_t nmemb, void* stream)
{
    string* result = (string*)stream;

    if (result->size() >= 16384)
    {
        fprintf(stderr, "JSON data received exceeds maximum 16384 characters\n");
        return 0;
    }

    result->append((char*)ptr, size * nmemb);

    return size * nmemb;
}





HTTPPlayerClient::HTTPPlayerClient()
: _curl(0), _port(0), _statePollInterval(g_defaultStatePollInterval), _pollThreadId(0), _stopPollThread(true)
{
    _curl = curl_easy_init();
    if (!_curl)
    {
        throw "Failed to initialise curl easy";
    }

    memset(&_state, 0, sizeof(_state));

    if (pthread_mutex_init(&_connectionMutex, NULL) != 0)
    {
        throw "Failed to initialise mutex";
    }

    if (pthread_rwlock_init(&_listenersRWLock, NULL) != 0)
    {
        throw "Failed to initialise read/write lock";
    }
}

HTTPPlayerClient::~HTTPPlayerClient()
{
    stopPollThread();

    pthread_rwlock_destroy(&_listenersRWLock);
    pthread_mutex_destroy(&_connectionMutex);

    curl_easy_cleanup(_curl);
}

bool HTTPPlayerClient::connect(string hostName, int port, int statePollInterval)
{
    stopPollThread();

    {
        LOCK_SECTION(&_connectionMutex);

        _hostName = hostName;
        _port = port;
        _statePollInterval = statePollInterval;

        if (!sendSimpleCommand("ping"))
        {
            _hostName.clear();
            _port = 0;
            _statePollInterval = g_defaultStatePollInterval;
            return false;
        }
    }

    if (_statePollInterval > 0)
    {
        if (!startPollThread())
        {
            LOCK_SECTION(&_connectionMutex);

            _hostName.clear();
            _port = 0;
            _statePollInterval = g_defaultStatePollInterval;

            return false;
        }
    }

    return true;
}

bool HTTPPlayerClient::ping()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("ping"))
    {
        return false;
    }

    return true;
}

void HTTPPlayerClient::disconnect()
{
    LOCK_SECTION(&_connectionMutex);

    _hostName.clear();
    _port = 0;
}

string HTTPPlayerClient::getVersion()
{
    return g_playerVersion;
}

string HTTPPlayerClient::getBuildTimestamp()
{
    return g_playerBuildTimestamp;
}

bool HTTPPlayerClient::dvsCardIsAvailable()
{
    return dvsCardIsAvailable(-1, -1);
}

bool HTTPPlayerClient::dvsCardIsAvailable(int card, int channel)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    return false;
}

void HTTPPlayerClient::setOutputType(PlayerOutputType outputType, float scale)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return;
    }

    string cmd = "settings/outputtype";
    map<string, string> args;
    args.insert(pair<string, string>("type", int_to_string((int)outputType)));
    args.insert(pair<string, string>("scale", float_to_string(scale)));

    sendSimpleCommand(cmd, args);
}

void HTTPPlayerClient::setDVSTarget(int card, int channel)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return;
    }

    string cmd = "settings/dvstarget";
    map<string, string> args;
    args.insert(pair<string, string>("card", int_to_string(card)));
    args.insert(pair<string, string>("channel", int_to_string(channel)));

    sendSimpleCommand(cmd, args);
}

PlayerOutputType HTTPPlayerClient::getOutputType()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return UNKNOWN_OUTPUT;
    }

    return UNKNOWN_OUTPUT;
}

PlayerOutputType HTTPPlayerClient::getActualOutputType()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return UNKNOWN_OUTPUT;
    }

    return UNKNOWN_OUTPUT;
}

void HTTPPlayerClient::setVideoSplit(VideoSwitchSplit videoSplit)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return;
    }

    string cmd = "settings/videosplit";
    map<string, string> args;
    args.insert(pair<string, string>("type", int_to_string((int)videoSplit)));

    sendSimpleCommand(cmd, args);
}

void HTTPPlayerClient::setSDIOSDEnable(bool enable)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return;
    }

    string cmd = "settings/sdiosdenable";
    map<string, string> args;
    args.insert(pair<string, string>("enable", bool_to_string(enable)));

    sendSimpleCommand(cmd, args);
}

HTTPPlayerState HTTPPlayerClient::getState()
{
    LOCK_SECTION(&_connectionMutex);

    return _state;
}

bool HTTPPlayerClient::reset()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("reset"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::close()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("close"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::start(vector<PlayerInput> inputs, bool startPaused, int64_t startPosition)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    JSONObject json;
    json.setBool("startPaused", startPaused);
    json.setNumber("startPosition", startPosition);

    JSONArray* inputsArray = json.setArray("inputs");
    vector<PlayerInput>::const_iterator inputsIter;
    for (inputsIter = inputs.begin(); inputsIter != inputs.end(); inputsIter++)
    {
        const PlayerInput& input = *inputsIter;

        JSONObject* inputObject = inputsArray->appendObject();
        inputObject->setNumber("type", input.type);
        inputObject->setString("name", input.name);

        if (input.options.size() > 0)
        {
            JSONObject* optionObject = inputObject->setObject("options");

            map<string, string>::const_iterator optionsIter;
            for (optionsIter = input.options.begin(); optionsIter != input.options.end(); optionsIter++)
            {
                const pair<string, string>& option = *optionsIter;

                optionObject->setString(option.first, option.second);
            }
        }
    }

    if (!sendJSONCommand("start", json.toString()))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::registerListener(HTTPPlayerClientListener* listener)
{
    if (listener == 0)
    {
        return false;
    }

    try
    {
        ReadWriteLockGuard guard(&_listenersRWLock, true);

        vector<ListenerData>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if ((*iter).listener == listener)
            {
                _listeners.erase(iter);
                break;
            }
        }

        ListenerData data;
        data.listener = listener;
        memset(&data.state, 0, sizeof(data.state));
        data.state.displayedFrame.position = -1;

        _listeners.push_back(data);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::unregisterListener(HTTPPlayerClientListener* listener)
{
    if (listener == 0)
    {
        return true;
    }

    try
    {
        ReadWriteLockGuard guard(&_listenersRWLock, true);

        vector<ListenerData>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if ((*iter).listener == listener)
            {
                _listeners.erase(iter);
                break;
            }
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::setX11WindowName(string name)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "settings/x11windowname";
    map<string, string> args;
    args.insert(pair<string, string>("name", name));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::stop()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    return true;
}

bool HTTPPlayerClient::toggleLock()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/togglelock"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::play()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/play"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::pause()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/pause"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::togglePlayPause()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/toggleplaypause"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::seek(int64_t offset, int whence, PlayUnit unit)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/seek";
    map<string, string> args;
    args.insert(pair<string, string>("offset", int64_to_string(offset)));
    switch (whence)
    {
        case SEEK_SET:
            args.insert(pair<string, string>("whence", "SEEK_SET"));
            break;
        case SEEK_CUR:
            args.insert(pair<string, string>("whence", "SEEK_CUR"));
            break;
        case SEEK_END:
            args.insert(pair<string, string>("whence", "SEEK_END"));
            break;
        default:
            throw "Invalid argument 'whence'";
    }
    switch (unit)
    {
        case FRAME_PLAY_UNIT:
            args.insert(pair<string, string>("unit", "FRAME_PLAY_UNIT"));
            break;
        case PERCENTAGE_PLAY_UNIT:
            args.insert(pair<string, string>("unit", "PERCENTAGE_PLAY_UNIT"));
            break;
    }

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::playSpeed(int speed)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/playspeed";
    map<string, string> args;
    args.insert(pair<string, string>("speed", int_to_string(speed)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::step(bool forward)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/step";
    map<string, string> args;
    args.insert(pair<string, string>("forwards", bool_to_string(forward)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::muteAudio(int mute)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/audio/mute";
    map<string, string> args;
    args.insert(pair<string, string>("mute", int_to_string(mute)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::mark(int type)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/mark/set";
    map<string, string> args;
    args.insert(pair<string, string>("type", int_to_string(type)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::markPosition(int64_t position, int type)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/mark/setposition";
    map<string, string> args;
    args.insert(pair<string, string>("position", int64_to_string(position)));
    args.insert(pair<string, string>("type", int_to_string(type)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::clearMark()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/mark/clear"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::clearAllMarks()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/mark/clearall"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::seekNextMark()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/mark/seeknext"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::seekPrevMark()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/seekprev"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::setOSDScreen(OSDScreen screen)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/osd/screen";
    map<string, string> args;
    switch (screen)
    {
        case OSD_SOURCE_INFO_SCREEN:
            args.insert(pair<string, string>("screen", "SOURCE_INFO"));
            break;
        case OSD_PLAY_STATE_SCREEN:
            args.insert(pair<string, string>("screen", "PLAY_STATE"));
            break;
        case OSD_MENU_SCREEN:
            args.insert(pair<string, string>("screen", "MENU"));
            break;
        case OSD_EMPTY_SCREEN:
            args.insert(pair<string, string>("screen", "EMPTY"));
            break;
    }

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::nextOSDScreen()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/osd/nextscreen"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::setOSDTimecode(int index, int type, int subType)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/osd/timecode";
    map<string, string> args;
    args.insert(pair<string, string>("index", int_to_string(index)));
    args.insert(pair<string, string>("type", int_to_string(type)));
    args.insert(pair<string, string>("subtype", int_to_string(subType)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::nextOSDTimecode()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/osd/nexttimecode"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::switchNextVideo()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/video/next"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::switchPrevVideo()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/video/prev"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::switchVideo(int index)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/video/set";
    map<string, string> args;
    args.insert(pair<string, string>("index", int_to_string(index)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::switchNextAudioGroup()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/audio/nextgroup"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::switchPrevAudioGroup()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/audio/prevgroup"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::switchAudioGroup(int index)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/audio/group";
    map<string, string> args;
    args.insert(pair<string, string>("index", int_to_string(index)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::snapAudioToVideo()
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    if (!sendSimpleCommand("control/audio/snapvideo"))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::reviewStart(int64_t duration)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/review/start";
    map<string, string> args;
    args.insert(pair<string, string>("duration", int64_to_string(duration)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::reviewEnd(int64_t duration)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/review/end";
    map<string, string> args;
    args.insert(pair<string, string>("duration", int64_to_string(duration)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

bool HTTPPlayerClient::review(int64_t duration)
{
    LOCK_SECTION(&_connectionMutex);

    if (!isConnected())
    {
        fprintf(stderr, "Command failed: not connected\n");
        return false;
    }

    string cmd = "control/review/now";
    map<string, string> args;
    args.insert(pair<string, string>("duration", int64_to_string(duration)));

    if (!sendSimpleCommand(cmd, args))
    {
        return false;
    }

    return true;
}

void HTTPPlayerClient::pollThread()
{
    int pollIntervalSec;
    int pollIntervalMSec;
    string jsonString;
    JSONObject* jsonObject;
    int parseIndex;
    HTTPPlayerState state;
    bool stateUpdated;
    int i;

    while (!_stopPollThread)
    {
        // sleep and check at least every second if _stopPollThread is true
        {
            LOCK_SECTION(&_connectionMutex);
            pollIntervalSec = _statePollInterval / 1000;
            pollIntervalMSec = _statePollInterval % 1000;
        }
        for (i = 0; i < pollIntervalSec; i++)
        {
            usleep(1000 * 1000);
            if (_stopPollThread)
            {
                break;
            }
        }
        usleep(1000 * pollIntervalMSec);


        stateUpdated = false;
        {
            LOCK_SECTION(&_connectionMutex);

            jsonString.clear();
            if (sendJSONInfoCommand("info/state.json", &jsonString))
            {
                parseIndex = 0;
                jsonObject = JSONObject::parse(jsonString.c_str(), &parseIndex);
                if (jsonObject == 0)
                {
                    fprintf(stderr, "Failed to parse string to JSON object\n");
                }
                else
                {
                    if (!parse_player_state(jsonObject, &state))
                    {
                        fprintf(stderr, "Failed to parse JSON to player state\n");
                    }
                    else
                    {
                        _state = state;
                        stateUpdated = true;
                    }

                    delete jsonObject;
                }
            }
        }


        if (stateUpdated)
        {
            ReadWriteLockGuard guard(&_listenersRWLock, false);

            vector<ListenerData>::iterator iter;
            for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
            {
                ListenerData& data = *iter;

                if (memcmp(&state, &data.state, sizeof(state)) != 0)
                {
                    data.listener->stateUpdate(this, state);
                    data.state = state;
                }
            }
        }
    }
}

bool HTTPPlayerClient::startPollThread()
{
    pthread_t newThread;
    pthread_attr_t attr;
    int result;

    _stopPollThread = false;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    result = pthread_create(&newThread, &attr, player_state_poll_thread, (void*)this);
    pthread_attr_destroy(&attr);

    if (result == 0)
    {
        _pollThreadId = newThread;
    }
    else
    {
        fprintf(stderr, "Failed to create joinable thread: %s", strerror(result));
        return false;
    }

    return true;
}

void HTTPPlayerClient::stopPollThread()
{
    int result;
    void* status;

    if (_pollThreadId != 0)
    {
        _stopPollThread = true;

        if ((result = pthread_join(_pollThreadId, (void **)&status)) != 0)
        {
            fprintf(stderr, "Failed to join thread: %s", strerror(result));
        }
    }

    _pollThreadId = 0;
}

bool HTTPPlayerClient::isConnected()
{
    return _hostName.size() > 0;
}

bool HTTPPlayerClient::sendSimpleCommand(string command)
{
    return sendSimpleCommand(command, map<string, string>());
}

bool HTTPPlayerClient::sendSimpleCommand(string command, map<string, string> args)
{
    curl_easy_reset(_curl);

    snprintf(_urlBuffer, sizeof(_urlBuffer), "http://%s:%d/%s", _hostName.c_str(), _port, command.c_str());

    if (args.size() > 0)
    {
        strncat(_urlBuffer, "?", sizeof(_urlBuffer));

        char* nameString;
        char* valueString;
        map<string, string>::const_iterator iter;
        for (iter = args.begin(); iter != args.end(); iter++)
        {
            const pair<string, string>& arg = *iter;

            if (iter != args.begin())
            {
                strncat(_urlBuffer, "&", sizeof(_urlBuffer));
            }

            nameString = curl_easy_escape(_curl, arg.first.c_str(), arg.first.length());
            valueString = curl_easy_escape(_curl, arg.second.c_str(), arg.second.length());

            strncat(_urlBuffer, nameString, sizeof(_urlBuffer));
            strncat(_urlBuffer, "=", sizeof(_urlBuffer));
            strncat(_urlBuffer, valueString, sizeof(_urlBuffer));

            curl_free(nameString);
            curl_free(valueString);
        }
    }

    char errorBuffer[CURL_ERROR_SIZE];
    curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(_curl, CURLOPT_URL, _urlBuffer);

    CURLcode result = curl_easy_perform(_curl);
    if (result != 0)
    {
        fprintf(stderr, "Failed to send command '%s': %s\n", _urlBuffer, errorBuffer);
        return false;
    }

    return true;
}

bool HTTPPlayerClient::sendJSONCommand(string command, string jsonString)
{
    curl_easy_reset(_curl);

    snprintf(_urlBuffer, sizeof(_urlBuffer), "http://%s:%d/%s", _hostName.c_str(), _port, command.c_str());

    struct curl_slist* slist = NULL;
    slist = curl_slist_append(slist, "Content-Type: application/json");

    char errorBuffer[CURL_ERROR_SIZE];
    curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(_curl, CURLOPT_URL, _urlBuffer);
    curl_easy_setopt(_curl, CURLOPT_POST, 1);
    curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, jsonString.c_str());


    CURLcode result = curl_easy_perform(_curl);
    curl_slist_free_all(slist);

    if (result != 0)
    {
        fprintf(stderr, "Failed to send json command '%s': %s\n", _urlBuffer, errorBuffer);
        return false;
    }

    return true;
}

bool HTTPPlayerClient::sendJSONInfoCommand(std::string command, std::string* jsonString)
{
    curl_easy_reset(_curl);

    snprintf(_urlBuffer, sizeof(_urlBuffer), "http://%s:%d/%s", _hostName.c_str(), _port, command.c_str());

    string stringResult;
    stringResult.reserve(512);

    char errorBuffer[CURL_ERROR_SIZE];
    curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(_curl, CURLOPT_URL, _urlBuffer);
    curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, receive_json_data);
    curl_easy_setopt(_curl, CURLOPT_WRITEDATA, (void*)&stringResult);


    CURLcode result = curl_easy_perform(_curl);

    if (result != 0)
    {
        fprintf(stderr, "Failed to send json info command '%s': %s\n", _urlBuffer, errorBuffer);
        return false;
    }

    *jsonString = stringResult;
    return true;
}
