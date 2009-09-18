/*
 * $Id: LocalIngexPlayer.cpp,v 1.17 2009/09/18 16:13:50 philipn Exp $
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

#include <cassert>
#include <unistd.h>
#include <cstring>
#include <memory>

#include "LocalIngexPlayer.h"
#include "Macros.h"


#include <multiple_sources.h>
#include <raw_file_source.h>
#include <mxf_source.h>
#include <udp_source.h>
#include <blank_source.h>
#include <buffered_media_source.h>
#include <raw_file_source.h>
#include <shared_mem_source.h>
#include <raw_dv_source.h>
#include <ffmpeg_source.h>
#include <bouncing_ball_source.h>
#include <clapper_source.h>
#include <x11_display_sink.h>
#include <x11_xv_display_sink.h>
#include <buffered_media_sink.h>
#include <dvs_sink.h>
#include <dual_sink.h>
#include <raw_file_sink.h>
#include <video_switch_sink.h>
#include <audio_switch_sink.h>
#include <dv_stream_connect.h>
#include <mjpeg_stream_connect.h>
#include <audio_sink.h>
#include <audio_level_sink.h>
#include <utils.h>
#include <logging.h>


using namespace prodauto;
using namespace std;


static const char* g_playerVersion = "version 0.1";
static const char* g_playerBuildTimestamp = __DATE__ " " __TIME__;



class MutexGuard
{
public:
    MutexGuard(pthread_mutex_t* mutex)
    : _mutex(NULL)
    {
        if (pthread_mutex_lock(mutex) != 0)
        {
            ml_log_error("pthread_mutex_lock failed\n");

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
                ml_log_error("pthread_mutex_unlock failed\n");
            }
        }
    }

private:
    pthread_mutex_t* _mutex;
};

typedef struct
{
    bool startPaused;
    LocalIngexPlayerState* playState;
} PlayerThreadArgs;


class prodauto::LocalIngexPlayerState
{
public:
    LocalIngexPlayerState()
    : x11Sink(0), x11XVSink(0), dualSink(0), mediaSource(0), mediaSink(0), mediaPlayer(0), playThreadId(0)
    {}

    ~LocalIngexPlayerState()
    {
        stopPlaying();

        // close int this order
        msc_close(mediaSource);
        ply_close_player(&mediaPlayer);
        closeMediaSink();
    }

    bool hasStopped() { return playThreadId == 0; };

    void stopPlaying()
    {
        if (playThreadId != 0)
        {
            // stop the player; this will also cause the player thread to exit
            mc_stop(ply_get_media_control(mediaPlayer));

            // wait until player has stopped
            join_thread(&playThreadId, NULL, NULL);

            playThreadId = 0;
        }
    }

    bool reset()
    {
        inputsPresent.clear();

        msc_close(mediaSource);
        mediaSource = 0;
        ply_close_player(&mediaPlayer);

        int result = msk_reset_or_close(mediaSink);
        if (result == 0)
        {
            ml_log_warn("Failed to reset_or_close media sink - closing media sink\n");
            closeMediaSink();
            return false;
        }
        else if (result == 2) // failed to reset so it was closed
        {
            mediaSink = 0; // media sink already closed
            closeMediaSink();
            return false;
        }

        return true;
    }

    // use this method to ensure all sink references are set to 0
    void closeMediaSink()
    {
        if (mediaSink != 0)
        {
            msk_close(mediaSink);
        }
        mediaSink = 0;
        x11Sink = 0;
        x11XVSink = 0;
        dualSink = 0;
    }

    X11DisplaySink* x11Sink;
    X11XVDisplaySink* x11XVSink;
    DualSink* dualSink;

    MediaSource* mediaSource;
    MediaSink* mediaSink;
    MediaPlayer* mediaPlayer;

    vector<bool> inputsPresent;

    PlayerThreadArgs playThreadArgs;
    pthread_t playThreadId;
};


static string get_option(const map<string, string>& options, string name)
{
    map<string, string>::const_iterator result = options.find(name);
    if (result == options.end())
    {
        return "";
    }
    return (*result).second;
}

static int parse_int_option(const map<string, string>& options, string name, int defaultValue)
{
    int result = defaultValue;
    string value = get_option(options, name);
    if (value.size() != 0)
    {
        if (sscanf(value.c_str(), "%d", &result) != 1)
        {
            ml_log_warn("Failed to parse int option: '%s' = '%s'\n", name.c_str(), value.c_str());
        }
    }
    return result;
}

static Rational parse_rational_option(const map<string, string>& options, string name, Rational defaultValue)
{
    Rational result = defaultValue;
    string value = get_option(options, name);
    if (value.size() != 0)
    {
        if (sscanf(value.c_str(), "%d/%d", &result.num, &result.den) != 2)
        {
            ml_log_warn("Failed to parse int option: '%s' = '%s'\n", name.c_str(), value.c_str());
        }
    }
    return result;
}

static void parse_streaminfo_options(const map<string, string>& options, StreamInfo* streamInfo)
{
    string typeStr = get_option(options, "stream_type");
    if (strcmp(typeStr.c_str(), "picture") == 0)
    {
        streamInfo->type = PICTURE_STREAM_TYPE;
    }
    else if (strcmp(typeStr.c_str(), "sound") == 0)
    {
        streamInfo->type = SOUND_STREAM_TYPE;
    }
    else if (strcmp(typeStr.c_str(), "timecode") == 0)
    {
        streamInfo->type = TIMECODE_STREAM_TYPE;
    }
    else if (strcmp(typeStr.c_str(), "event") == 0)
    {
        streamInfo->type = EVENT_STREAM_TYPE;
    }
    else if (streamInfo->type == UNKNOWN_STREAM_TYPE)
    {
        streamInfo->type = PICTURE_STREAM_TYPE;
    }

    string formatStr = get_option(options, "stream_format");
    if (strcmp(formatStr.c_str(), "uyvy") == 0)
    {
        streamInfo->format = UYVY_FORMAT;
        streamInfo->type = PICTURE_STREAM_TYPE;
    }
    else if (strcmp(formatStr.c_str(), "yuv422") == 0)
    {
        streamInfo->format = YUV422_FORMAT;
        streamInfo->type = PICTURE_STREAM_TYPE;
    }
    else if (strcmp(formatStr.c_str(), "yuv420") == 0)
    {
        streamInfo->format = YUV420_FORMAT;
        streamInfo->type = PICTURE_STREAM_TYPE;
    }
    else if (strcmp(formatStr.c_str(), "yuv411") == 0)
    {
        streamInfo->format = YUV411_FORMAT;
        streamInfo->type = PICTURE_STREAM_TYPE;
    }
    else if (strcmp(formatStr.c_str(), "pcm") == 0)
    {
        streamInfo->format = PCM_FORMAT;
        streamInfo->type = SOUND_STREAM_TYPE;
    }
    else if (strcmp(formatStr.c_str(), "timecode") == 0)
    {
        streamInfo->format = TIMECODE_FORMAT;
        streamInfo->type = TIMECODE_STREAM_TYPE;
    }
    else if (streamInfo->format == UNKNOWN_FORMAT)
    {
        if (streamInfo->type == PICTURE_STREAM_TYPE)
        {
            streamInfo->format = UYVY_FORMAT;
        }
        else if (streamInfo->type == SOUND_STREAM_TYPE)
        {
            streamInfo->format = PCM_FORMAT;
        }
        else if (streamInfo->type == TIMECODE_STREAM_TYPE)
        {
            streamInfo->format = TIMECODE_FORMAT;
        }
    }

    streamInfo->frameRate = parse_rational_option(options, "frame_rate", (Rational){25, 1});
    if (streamInfo->type == PICTURE_STREAM_TYPE)
    {
        streamInfo->width = parse_int_option(options, "width", 720);
        streamInfo->height = parse_int_option(options, "height", 576);
        streamInfo->aspectRatio = parse_rational_option(options, "aspect_ratio", (Rational){4, 3});
    }
    else if (streamInfo->type == SOUND_STREAM_TYPE)
    {
        streamInfo->samplingRate = parse_rational_option(options, "sampling_rate", (Rational){48000, 1});
        streamInfo->numChannels = parse_int_option(options, "num_channels", 1);
        streamInfo->bitsPerSample = parse_int_option(options, "bits_per_sample", 16);
    }
}

static void init_sound_streaminfo(StreamInfo* streamInfo)
{
    memset(streamInfo, 0, sizeof(*streamInfo));

    streamInfo->type = SOUND_STREAM_TYPE;
    streamInfo->format = PCM_FORMAT;
    streamInfo->frameRate = (Rational){25, 1};
    streamInfo->samplingRate = (Rational){48000, 1};
    streamInfo->numChannels = 1;
    streamInfo->bitsPerSample = 16;
}

static void* player_thread(void* arg)
{
    PlayerThreadArgs* threadArg = (PlayerThreadArgs*)arg;

    if (!ply_start_player(threadArg->playState->mediaPlayer, threadArg->startPaused))
    {
        ml_log_error("Media player failed to play\n");
    }

    pthread_exit((void*)0);
}

static void frame_displayed_event(void* data, const FrameInfo* frameInfo)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->frameDisplayedEvent(frameInfo);
    }
}

static void frame_dropped_event(void* data, const FrameInfo* lastFrameInfo)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->frameDroppedEvent(lastFrameInfo);
    }
}

static void state_change_event(void* data, const MediaPlayerStateEvent* event)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->stateChangeEvent(event);
    }
}

static void end_of_source_event(void* data, const FrameInfo* lastReadFrameInfo)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->endOfSourceEvent(lastReadFrameInfo);
    }
}

static void start_of_source_event(void* data, const FrameInfo* firstReadFrameInfo)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->startOfSourceEvent(firstReadFrameInfo);
    }
}

static void close_request(void* data)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->playerCloseRequested();
    }
}

static void player_closed(void* data)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->playerClosed();
    }
}

static void x11_key_pressed(void* data, int key, int modifier)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->keyPressed(key, modifier);
    }
}

static void x11_key_released(void* data, int key, int modifier)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->keyReleased(key, modifier);
    }
}

static void x11_progress_bar_position_set(void* data, float position)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->progressBarPositionSet(position);
    }
}

static void x11_mouse_clicked(void* data, int imageWidth, int imageHeight, int xPos, int yPos)
{
    IngexPlayerListenerRegistry* listener_registry = (IngexPlayerListenerRegistry*)data;

    ReadWriteLockGuard guard(&listener_registry->_listenersRWLock, false);

    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = listener_registry->_listeners.begin(); iter != listener_registry->_listeners.end(); iter++)
    {
        (*iter)->mouseClicked(imageWidth, imageHeight, xPos, yPos);
    }
}


LocalIngexPlayer::LocalIngexPlayer(IngexPlayerListenerRegistry* listenerRegistry)
{
    _config.outputType = X11_AUTO_OUTPUT;
    _config.dvsCard = -1;
    _config.dvsChannel = -1;
    _config.videoSplit = QUAD_SPLIT_VIDEO_SWITCH;
    _config.numFFMPEGThreads = 4;
    _config.initiallyLocked = false;
    _config.useWorkerThreads = true;
    _config.applySplitFilter = true;
    _config.srcBufferSize = 0;
    _config.disableSDIOSD = false;
    _config.disableX11OSD = false;
    _config.sourceAspectRatio = (Rational){0, 1};
    _config.pixelAspectRatio = (Rational){0, 1};
    _config.monitorAspectRatio = (Rational){4, 3};
    _config.scale = 1.0;
    _config.disablePCAudio = false;
    _config.audioDevice = 0;
    _config.numAudioLevelMonitors = 2;
    _config.audioLineupLevel = -18.0;
    _config.enableAudioSwitch = true;
    memset(&_config.externalWindowInfo, 0, sizeof(_config.externalWindowInfo));
    
    _nextConfig = _config;
    
    _actualOutputType = X11_OUTPUT;

    memset(&_windowInfo, 0, sizeof(_windowInfo));

    _x11WindowName = "Ingex Player";
    

    _playState = 0;

    memset(&_mediaPlayerListener, 0, sizeof(MediaPlayerListener));

    _mediaPlayerListener.data = listenerRegistry;
    _mediaPlayerListener.frame_displayed_event = frame_displayed_event;
    _mediaPlayerListener.frame_dropped_event = frame_dropped_event;
    _mediaPlayerListener.state_change_event = state_change_event;
    _mediaPlayerListener.end_of_source_event = end_of_source_event;
    _mediaPlayerListener.start_of_source_event = start_of_source_event;
    _mediaPlayerListener.player_closed = player_closed;


    memset(&_x11WindowListener, 0, sizeof(X11WindowListener));
    _x11WindowListener.data = listenerRegistry;
    _x11WindowListener.close_request = close_request;

    memset(&_x11KeyListener, 0, sizeof(KeyboardInputListener));
    _x11KeyListener.data = listenerRegistry;
    _x11KeyListener.key_pressed = x11_key_pressed;
    _x11KeyListener.key_released = x11_key_released;

    memset(&_x11ProgressBarListener, 0, sizeof(ProgressBarInputListener));
    _x11ProgressBarListener.data = listenerRegistry;
    _x11ProgressBarListener.position_set = x11_progress_bar_position_set;

    memset(&_x11MouseListener, 0, sizeof(MouseInputListener));
    _x11MouseListener.data = listenerRegistry;
    _x11MouseListener.click = x11_mouse_clicked;

    memset(&_videoStreamInfo, 0, sizeof(_videoStreamInfo));

    pthread_mutex_init(&_configMutex, NULL);
    pthread_rwlock_init(&_playStateRWLock, NULL);

    init_dv_decoder_resources();
    init_mjpeg_decoder_resources();
}

LocalIngexPlayer::~LocalIngexPlayer()
{
    LocalIngexPlayerState* currentPlayState = 0;
    {
        ReadWriteLockGuard guard(&_playStateRWLock, true);
        currentPlayState = _playState;
        _playState = 0;
    }
    SAFE_DELETE(&currentPlayState);

    pthread_mutex_destroy(&_configMutex);
    pthread_rwlock_destroy(&_playStateRWLock);

    free_dv_decoder_resources();
    free_mjpeg_decoder_resources();
}

string LocalIngexPlayer::getVersion()
{
    return g_playerVersion;
}

string LocalIngexPlayer::getBuildTimestamp()
{
    return g_playerBuildTimestamp;
}

bool LocalIngexPlayer::dvsCardIsAvailable()
{
    return dvsCardIsAvailable(-1, -1);
}

bool LocalIngexPlayer::dvsCardIsAvailable(int card, int channel)
{
    return dvs_card_is_available(card, channel) == 1;
}

bool LocalIngexPlayer::x11XVIsAvailable()
{
    return xvsk_check_is_available() == 1;
}

void LocalIngexPlayer::setWindowInfo(const X11WindowInfo* windowInfo)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.externalWindowInfo = *windowInfo;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setInitiallyLocked(bool locked)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.initiallyLocked = locked;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setNumFFMPEGThreads(int num)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.numFFMPEGThreads = num;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setUseWorkerThreads(bool enable)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.useWorkerThreads = enable;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setApplySplitFilter(bool enable)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.applySplitFilter = enable;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setSourceBufferSize(int size)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.srcBufferSize = size;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setEnableX11OSD(bool enable)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.disableX11OSD = !enable;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setSourceAspectRatio(Rational *aspect)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.sourceAspectRatio = *aspect;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setPixelAspectRatio(Rational *aspect)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.pixelAspectRatio = *aspect;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setMonitorAspectRatio(Rational *aspect)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.monitorAspectRatio = *aspect;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setEnablePCAudio(bool enable)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.disablePCAudio = !enable;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setAudioDevice(int device)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.audioDevice = device;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setNumAudioLevelMonitors(int num)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.numAudioLevelMonitors = num;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setAudioLineupLevel(float level)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.audioLineupLevel = level;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setEnableAudioSwitch(bool enable)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.enableAudioSwitch = enable;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setOutputType(PlayerOutputType outputType)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.outputType = outputType;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setScale(float scale)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.scale = scale;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setDVSTarget(int card, int channel)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.dvsCard = card;
    _nextConfig.dvsChannel = channel;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setVideoSplit(VideoSwitchSplit videoSplit)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.videoSplit = videoSplit;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

void LocalIngexPlayer::setSDIOSDEnable(bool enable)
{
    PTHREAD_MUTEX_LOCK(&_configMutex);
    _nextConfig.disableSDIOSD = !enable;
    PTHREAD_MUTEX_UNLOCK(&_configMutex);
}

bool LocalIngexPlayer::reset()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            // no player - pretend the reset failed and the player was closed
            return false;
        }

        _playState->stopPlaying();
        return _playState->reset();
    }
    catch (...)
    {
        return false;
    }
}

bool LocalIngexPlayer::close()
{
    try
    {
        LocalIngexPlayerState* currentPlayState = 0;
        {
            ReadWriteLockGuard guard(&_playStateRWLock, true);
            currentPlayState = _playState;
            _playState = 0;
        }

        SAFE_DELETE(&currentPlayState);

        closeLocalX11Window();
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::start(vector<PlayerInput> inputs, vector<bool>& opened, bool startPaused, int64_t startPosition)
{
    auto_ptr<LocalIngexPlayerState> newPlayState;
    MultipleMediaSources* multipleSource = 0;
    BufferedMediaSource* bufferedSource = 0;
    DVSSink* dvsSink = 0;
    X11DisplaySink* x11Sink = 0;
    X11XVDisplaySink* x11XVSink = 0;
    BufferedMediaSink* bufSink = 0;
    DualSink* dualSink = 0;
    bool haveReset = false;
    LocalIngexPlayerState* currentPlayState = 0;
    int swScale = 1;
    vector<bool> inputsPresent;
    MediaSource* mainMediaSource = 0;
    bool forceUYVYFormat;
    Configuration nextConfig;

    try
    {
        if (inputs.size() == 0)
        {
            THROW_EXCEPTION(("No files to play\n"));
        }

        // effectively disable client access to the player
        {
            ReadWriteLockGuard guard(&_playStateRWLock, true);
            currentPlayState = _playState;
            _playState = 0;
        }
        
        // stop playing
        if (currentPlayState)
        {
            currentPlayState->stopPlaying();
        }
        
        // get the next config
        PTHREAD_MUTEX_LOCK(&_configMutex);
        nextConfig = _nextConfig;
        PTHREAD_MUTEX_UNLOCK(&_configMutex);


        // create a multiple source source

        CHK_OTHROW(mls_create(&nextConfig.sourceAspectRatio, -1, &g_palFrameRate, &multipleSource));
        mainMediaSource = mls_get_media_source(multipleSource);


        // open the media sources

        int videoStreamIndex = -1;
        bool videoChanged = false;
        bool atLeastOneInputOpened = false;
        vector<PlayerInput>::const_iterator iter;
        for (iter = inputs.begin(); iter != inputs.end(); iter++)
        {
            const PlayerInput& input = *iter;
            MediaSource* mediaSource = 0;
            StreamInfo streamInfo;
            StreamInfo soundStreamInfo;
            int numBalls = 5;
            int numFFMPEGThreads = 0;

            memset(&streamInfo, 0, sizeof(streamInfo));
            memset(&soundStreamInfo, 0, sizeof(soundStreamInfo));

            switch (input.type)
            {
                case MXF_INPUT:
                {
                    MXFFileSource* mxfSource = 0;
                    if (!mxfs_open(input.name.c_str(), 0, 0, 0, &mxfSource))
                    {
                        ml_log_warn("Failed to open MXF file source '%s'\n", input.name.c_str());
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                    mediaSource = mxfs_get_media_source(mxfSource);
                }
                break;

                case RAW_INPUT:
                {
                    parse_streaminfo_options(input.options, &streamInfo);
                    if (!rfs_open(input.name.c_str(), &streamInfo, &mediaSource))
                    {
                        ml_log_error("Failed to open raw file source '%s'\n", input.name.c_str());
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

                case DV_INPUT:
                {
                    if (!rds_open(input.name.c_str(), &mediaSource))
                    {
                        ml_log_error("Failed to open DV file source '%s'\n", input.name.c_str());
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

                case FFMPEG_INPUT:
                {
                    forceUYVYFormat = (nextConfig.outputType == DVS_OUTPUT ||
                        nextConfig.outputType == DUAL_DVS_AUTO_OUTPUT ||
                        nextConfig.outputType == DUAL_DVS_X11_OUTPUT ||
                        nextConfig.outputType == DUAL_DVS_X11_XV_OUTPUT);
                    numFFMPEGThreads = parse_int_option(input.options, "num_ffmpeg_threads", 0);
                    if (!fms_open(input.name.c_str(), numFFMPEGThreads, forceUYVYFormat, &mediaSource))
                    {
                        ml_log_warn("Failed to open FFmpeg file source '%s'\n", input.name.c_str());
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

                case SHM_INPUT:
                {
                    if (!shared_mem_open(input.name.c_str(), 0, &mediaSource))
                    {
                        ml_log_error("Failed to open shared memory source '%s'\n", input.name.c_str());
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

                case UDP_INPUT:
                {
                    if (!udp_open(input.name.c_str(), &mediaSource))
                    {
                        ml_log_warn("Failed to open UDP source '%s'\n", input.name.c_str());
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

                case BALLS_INPUT:
                {
                    streamInfo.type = PICTURE_STREAM_TYPE;
                    parse_streaminfo_options(input.options, &streamInfo);
                    numBalls = parse_int_option(input.options, "num_balls", 5);
                    if (!bbs_create(&streamInfo, 2160000, numBalls, &mediaSource))
                    {
                        ml_log_error("Failed to create bouncing balls source\n");
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

                case BLANK_INPUT:
                {
                    streamInfo.type = PICTURE_STREAM_TYPE;
                    parse_streaminfo_options(input.options, &streamInfo);
                    if (!bks_create(&streamInfo, 2160000, &mediaSource))
                    {
                        ml_log_error("Failed to create blank source\n");
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

                case CLAPPER_INPUT:
                {
                    streamInfo.type = PICTURE_STREAM_TYPE;
                    parse_streaminfo_options(input.options, &streamInfo);
                    init_sound_streaminfo(&soundStreamInfo);
                    if (!clp_create(&streamInfo, &soundStreamInfo, 2160000, &mediaSource))
                    {
                        ml_log_error("Failed to create clapper source\n");
                        opened.push_back(false);
                        inputsPresent.push_back(false);
                        continue;
                    }
                }
                break;

            }

            // set software scaling to 2 if the material dimensions exceeds 1024
            // also, get the index of the first video stream
            // also, check if the video format has changed
            if (swScale == 1)
            {
                const StreamInfo* streamInfo;
                int i;
                int numStreams = msc_get_num_streams(mediaSource);
                for (i = 0; i < numStreams; i++)
                {
                    if (msc_get_stream_info(mediaSource, i, &streamInfo))
                    {
                        if (videoStreamIndex == -1 && streamInfo->type == PICTURE_STREAM_TYPE)
                        {
                            videoStreamIndex = i;

                            if (_videoStreamInfo.type == PICTURE_STREAM_TYPE)
                            {
                                if (streamInfo->format != _videoStreamInfo.format ||
                                    streamInfo->width != _videoStreamInfo.width ||
                                    streamInfo->height != _videoStreamInfo.height ||
                                    memcmp(&streamInfo->aspectRatio, &_videoStreamInfo.aspectRatio, sizeof(_videoStreamInfo.aspectRatio)))
                                {
                                    videoChanged = true;
                                }
                            }

                            _videoStreamInfo = *streamInfo;
                        }

                        if (swScale == 1 &&
                            streamInfo->type == PICTURE_STREAM_TYPE &&
                            (streamInfo->width > 1024 || streamInfo->height > 1024))
                        {
                            swScale = 2;
                        }
                    }
                }
            }

            opened.push_back(true);
            inputsPresent.push_back(true);
            atLeastOneInputOpened = true;
            CHK_OTHROW(mls_assign_source(multipleSource, &mediaSource));
        }

        // check at least one input could be opened
        if (!atLeastOneInputOpened)
        {
            msc_close(mainMediaSource);
            {
                ReadWriteLockGuard guard(&_playStateRWLock, true);
                _playState = currentPlayState;
            }
            return false;
        }


        // add a blank video source if no video source is present

        if (videoStreamIndex == -1)
        {
            if (_videoStreamInfo.type == PICTURE_STREAM_TYPE)
            {
                if (_videoStreamInfo.format != UYVY_FORMAT)
                {
                    videoChanged = true;
                }

                StreamInfo prevStreamInfo = _videoStreamInfo;

                memset(&_videoStreamInfo, 0, sizeof(_videoStreamInfo));
                _videoStreamInfo.type = PICTURE_STREAM_TYPE;
                _videoStreamInfo.format = UYVY_FORMAT;
                _videoStreamInfo.width = prevStreamInfo.width;
                _videoStreamInfo.height = prevStreamInfo.height;
                _videoStreamInfo.frameRate.num = prevStreamInfo.frameRate.num;
                _videoStreamInfo.frameRate.den = prevStreamInfo.frameRate.den;
                _videoStreamInfo.aspectRatio.num = prevStreamInfo.aspectRatio.num;
                _videoStreamInfo.aspectRatio.den = prevStreamInfo.aspectRatio.den;
            }
            else
            {
                memset(&_videoStreamInfo, 0, sizeof(_videoStreamInfo));
                _videoStreamInfo.type = PICTURE_STREAM_TYPE;
                _videoStreamInfo.format = UYVY_FORMAT;
                _videoStreamInfo.width = 720;
                _videoStreamInfo.height = 576;
                _videoStreamInfo.frameRate.num = 25;
                _videoStreamInfo.frameRate.den = 1;
                _videoStreamInfo.aspectRatio.num = 4;
                _videoStreamInfo.aspectRatio.den = 3;
            }

            MediaSource* mediaSource;
            CHK_OTHROW(bks_create(&_videoStreamInfo, 120 * 60 * 60 * 25, &mediaSource));
            CHK_OTHROW(mls_assign_source(multipleSource, &mediaSource));

        }

        // finalise blank sources

        CHK_OTHROW(mls_finalise_blank_sources(multipleSource));


        // open the buffered media source

        if (nextConfig.srcBufferSize > 0)
        {
            CHK_OTHROW(bmsrc_create(mainMediaSource, nextConfig.srcBufferSize, 1, -1.0, &bufferedSource));
            mainMediaSource = bmsrc_get_source(bufferedSource);
        }


        // decide how to proceed if already playing

        if (currentPlayState)
        {
            bool resetPlayer = true; // if false then close and restart the player
            if (videoChanged)
            {
                // video format has changed
                resetPlayer = false;
            }
            else if (_actualOutputType != DVS_OUTPUT &&
                nextConfig.externalWindowInfo.window != 0 &&
                nextConfig.externalWindowInfo.window != _config.externalWindowInfo.window)
            {
                // external window info has changed
                resetPlayer = false;
            }
            else if (nextConfig.outputType != _config.outputType || nextConfig.scale != _config.scale)
            {
                // output type or scale has changed
                resetPlayer = false;
            }
            else if (nextConfig.outputType == X11_AUTO_OUTPUT && _actualOutputType == X11_OUTPUT &&
                x11XVIsAvailable())
            {
                // Xv is now available
                resetPlayer = false;
            }
            else if (nextConfig.outputType == DUAL_DVS_AUTO_OUTPUT && _actualOutputType == DUAL_DVS_X11_OUTPUT &&
                x11XVIsAvailable())
            {
                // Xv is now available
                resetPlayer = false;
            }
            else if ((nextConfig.outputType == DVS_OUTPUT || nextConfig.outputType == DUAL_DVS_AUTO_OUTPUT ||
                nextConfig.outputType == DUAL_DVS_X11_OUTPUT || nextConfig.outputType == DUAL_DVS_X11_XV_OUTPUT) &&
                (nextConfig.dvsCard != _config.dvsCard || nextConfig.dvsChannel != _config.dvsChannel))
            {
                // DVS card/channel selection has changed
                resetPlayer = false;
            }
            else if (nextConfig.videoSplit != _config.videoSplit ||
                nextConfig.applySplitFilter != _config.applySplitFilter)
            {
                // video split has changed
                resetPlayer = false;
            }
            else if (nextConfig.disableSDIOSD != _config.disableSDIOSD)
            {
                // sdi osd disable has changed
                resetPlayer = false;
            }
            else if (nextConfig.disableX11OSD != _config.disableX11OSD)
            {
                // x11 osd disable has changed
                resetPlayer = false;
            }
            else if (nextConfig.disablePCAudio != _config.disablePCAudio ||
                nextConfig.audioDevice != _config.audioDevice)
            {
                // pc audio support has changed
                resetPlayer = false;
            }
            else if (nextConfig.numAudioLevelMonitors != _config.numAudioLevelMonitors ||
                nextConfig.audioLineupLevel != _config.audioLineupLevel)
            {
                // audio monitoring has changed
                resetPlayer = false;
            }
            else if (nextConfig.enableAudioSwitch != _config.enableAudioSwitch)
            {
                // audio switching support has changed
                resetPlayer = false;
            }

            if (resetPlayer)
            {
                // reset the player
                haveReset = currentPlayState->reset();
                if (haveReset)
                {
                    // reset ok, newPlayState equals current play state
                    newPlayState = auto_ptr<LocalIngexPlayerState>(currentPlayState);
                }
                else
                {
                    // reset failed and therefore player was stopped
                    SAFE_DELETE(&currentPlayState);
                    newPlayState = auto_ptr<LocalIngexPlayerState>(new LocalIngexPlayerState());
                }
            }
            else
            {
                // close the player and start again
                SAFE_DELETE(&currentPlayState);
                newPlayState = auto_ptr<LocalIngexPlayerState>(new LocalIngexPlayerState());
            }
        }
        else
        {
            // new player
            newPlayState = auto_ptr<LocalIngexPlayerState>(new LocalIngexPlayerState());
        }

        newPlayState->mediaSource = mainMediaSource;
        mainMediaSource = 0;
        newPlayState->inputsPresent = inputsPresent;
        inputsPresent.clear();



        // open media sink

        if (!haveReset)
        {
            // get the actual output type, possibly auto-detect whether X11 XV is available as output type

            if (nextConfig.outputType == X11_AUTO_OUTPUT)
            {
                if (x11XVIsAvailable())
                {
                    ml_log_info("Auto detected X11 XV output available\n");
                    _actualOutputType = X11_XV_OUTPUT;
                }
                else
                {
                    ml_log_info("Auto detected X11 XV output NOT available - using non-XV X11 output\n");
                    _actualOutputType = X11_OUTPUT;
                }
            }
            else if (nextConfig.outputType == DUAL_DVS_AUTO_OUTPUT)
            {
                if (x11XVIsAvailable())
                {
                    ml_log_info("Auto detected X11 XV output available\n");
                    _actualOutputType = DUAL_DVS_X11_XV_OUTPUT;
                }
                else
                {
                    ml_log_info("Auto detected X11 XV output NOT available - using non-XV X11 output\n");
                    _actualOutputType = DUAL_DVS_X11_OUTPUT;
                }
            }
            else
            {
                _actualOutputType = nextConfig.outputType;
            }


            switch (_actualOutputType)
            {
                case X11_OUTPUT:
                    CHK_OTHROW(setOrCreateX11Window(&nextConfig.externalWindowInfo));
                    CHK_OTHROW_MSG(xsk_open(20, nextConfig.disableX11OSD, &nextConfig.pixelAspectRatio,
                        &nextConfig.monitorAspectRatio, nextConfig.scale, swScale, &_windowInfo,
                        &x11Sink), ("Failed to open X11 display sink\n"));
                    xsk_register_window_listener(x11Sink, &_x11WindowListener);
                    xsk_register_keyboard_listener(x11Sink, &_x11KeyListener);
                    xsk_register_progress_bar_listener(x11Sink, &_x11ProgressBarListener);
                    xsk_register_mouse_listener(x11Sink, &_x11MouseListener);
                    newPlayState->mediaSink = xsk_get_media_sink(x11Sink);
                    CHK_OTHROW_MSG(bms_create(&newPlayState->mediaSink, 2, 0, &bufSink),
                        ("Failed to create bufferred x11 xv display sink\n"));
                    newPlayState->mediaSink = bms_get_sink(bufSink);
                    newPlayState->x11Sink = x11Sink;
                    break;

                case X11_XV_OUTPUT:
                    CHK_OTHROW(setOrCreateX11Window(&nextConfig.externalWindowInfo));
                    CHK_OTHROW_MSG(xvsk_open(20, nextConfig.disableX11OSD, &nextConfig.pixelAspectRatio,
                        &nextConfig.monitorAspectRatio, nextConfig.scale, swScale, &_windowInfo,
                        &x11XVSink), ("Failed to open X11 XV display sink\n"));
                    xvsk_register_window_listener(x11XVSink, &_x11WindowListener);
                    xvsk_register_keyboard_listener(x11XVSink, &_x11KeyListener);
                    xvsk_register_progress_bar_listener(x11XVSink, &_x11ProgressBarListener);
                    xvsk_register_mouse_listener(x11XVSink, &_x11MouseListener);
                    newPlayState->mediaSink = xvsk_get_media_sink(x11XVSink);
                    CHK_OTHROW_MSG(bms_create(&newPlayState->mediaSink, 2, 0, &bufSink),
                        ("Failed to create bufferred x11 xv display sink\n"));
                    newPlayState->mediaSink = bms_get_sink(bufSink);
                    newPlayState->x11XVSink = x11XVSink;
                    break;

                case DVS_OUTPUT:
                    closeLocalX11Window();
                    CHK_OTHROW_MSG(dvs_open(nextConfig.dvsCard, nextConfig.dvsChannel, VITC_AS_SDI_VITC, 0, 12,
                        nextConfig.disableSDIOSD, 1, &dvsSink), ("Failed to open DVS sink\n"));
                    newPlayState->mediaSink = dvs_get_media_sink(dvsSink);
                    break;

                case DUAL_DVS_X11_OUTPUT:
                    CHK_OTHROW(setOrCreateX11Window(&nextConfig.externalWindowInfo));
                    CHK_OTHROW_MSG(dusk_open(20, nextConfig.dvsCard, nextConfig.dvsChannel, VITC_AS_SDI_VITC, 0, 12, 0,
                        nextConfig.disableSDIOSD, nextConfig.disableX11OSD, &nextConfig.pixelAspectRatio,
                        &nextConfig.monitorAspectRatio, nextConfig.scale, swScale, 1, &_windowInfo,
                        &dualSink), ("Failed to open dual DVS and X11 display sink\n"));
                    dusk_register_window_listener(dualSink, &_x11WindowListener);
                    dusk_register_keyboard_listener(dualSink, &_x11KeyListener);
                    dusk_register_progress_bar_listener(dualSink, &_x11ProgressBarListener);
                    dusk_register_mouse_listener(dualSink, &_x11MouseListener);
                    newPlayState->mediaSink = dusk_get_media_sink(dualSink);
                    newPlayState->dualSink = dualSink;
                    break;

                case DUAL_DVS_X11_XV_OUTPUT:
                    CHK_OTHROW(setOrCreateX11Window(&nextConfig.externalWindowInfo));
                    CHK_OTHROW_MSG(dusk_open(20, nextConfig.dvsCard, nextConfig.dvsChannel, VITC_AS_SDI_VITC, 0, 12, 1,
                        nextConfig.disableSDIOSD, nextConfig.disableX11OSD, &nextConfig.pixelAspectRatio,
                        &nextConfig.monitorAspectRatio, nextConfig.scale, swScale, 1, &_windowInfo,
                        &dualSink), ("Failed to open dual DVS and X11 XV display sink\n"));
                    dusk_register_window_listener(dualSink, &_x11WindowListener);
                    dusk_register_keyboard_listener(dualSink, &_x11KeyListener);
                    dusk_register_progress_bar_listener(dualSink, &_x11ProgressBarListener);
                    dusk_register_mouse_listener(dualSink, &_x11MouseListener);
                    newPlayState->mediaSink = dusk_get_media_sink(dualSink);
                    newPlayState->dualSink = dualSink;
                    break;

                default:
                    assert(false);
            }

#if defined(HAVE_PORTAUDIO)
            // create audio sink

            if (!nextConfig.disablePCAudio)
            {
                if (_actualOutputType == X11_OUTPUT ||
                    _actualOutputType == X11_XV_OUTPUT)
                    // TODO: DVS drops frames when pc audio is enabled _actualOutputType != DVS_OUTPUT)
                {
                    AudioSink* audioSink;
                    if (!aus_create_audio_sink(newPlayState->mediaSink, nextConfig.audioDevice, &audioSink))
                    {
                        ml_log_warn("Audio device is not available and will be disabled\n");
                    }
                    else
                    {
                        newPlayState->mediaSink = aus_get_media_sink(audioSink);
                    }
                }
            }
#endif

            // create video switch sink

            if (nextConfig.videoSplit == QUAD_SPLIT_VIDEO_SWITCH || nextConfig.videoSplit == NONA_SPLIT_VIDEO_SWITCH)
            {
                VideoSwitchSink* videoSwitch;
                CHK_OTHROW(qvs_create_video_switch(newPlayState->mediaSink, nextConfig.videoSplit,
                    nextConfig.applySplitFilter, 0, 0, 0, -1, -1, -1, &videoSwitch));
                newPlayState->mediaSink = vsw_get_media_sink(videoSwitch);
            }


            // create audio level monitors

            if (nextConfig.numAudioLevelMonitors > 0)
            {
                AudioLevelSink* audioLevelSink;
                CHK_OTHROW(als_create_audio_level_sink(newPlayState->mediaSink, nextConfig.numAudioLevelMonitors,
                    nextConfig.audioLineupLevel, &audioLevelSink));
                newPlayState->mediaSink = als_get_media_sink(audioLevelSink);
            }


            // create audio switch sink

            if (nextConfig.enableAudioSwitch)
            {
                AudioSwitchSink* audioSwitch;
                CHK_OTHROW(qas_create_audio_switch(newPlayState->mediaSink, &audioSwitch));
                newPlayState->mediaSink = asw_get_media_sink(audioSwitch);
            }
        }


        // create the player

        CHK_OTHROW(ply_create_player(newPlayState->mediaSource, newPlayState->mediaSink,
            nextConfig.initiallyLocked, 0, nextConfig.numFFMPEGThreads, nextConfig.useWorkerThreads, 0, 0,
            &g_invalidTimecode, &g_invalidTimecode, NULL, NULL, 0, &newPlayState->mediaPlayer));
        CHK_OTHROW(ply_register_player_listener(newPlayState->mediaPlayer, &_mediaPlayerListener));

        if (nextConfig.srcBufferSize > 0)
        {
            bmsrc_set_media_player(bufferedSource, newPlayState->mediaPlayer);
        }


        // start with player state screen
        mc_set_osd_screen(ply_get_media_control(newPlayState->mediaPlayer), OSD_PLAY_STATE_SCREEN);


        // seek to the start position
        if (startPosition > 0)
        {
            mc_seek(ply_get_media_control(newPlayState->mediaPlayer), startPosition, SEEK_SET, FRAME_PLAY_UNIT);
        }


        // start play thread

        newPlayState->playThreadArgs.startPaused = startPaused;
        newPlayState->playThreadArgs.playState = newPlayState.get();
        CHK_OTHROW(create_joinable_thread(&newPlayState->playThreadId, player_thread, &newPlayState->playThreadArgs));


        _config = nextConfig;
        {
            ReadWriteLockGuard guard(&_playStateRWLock, true);
            _playState = newPlayState.release();
        }

        setX11WindowName(_x11WindowName);
    }
    catch (...)
    {
        if (mainMediaSource != 0)
        {
            msc_close(mainMediaSource);
        }
        return false;
    }
    return true;
}

PlayerOutputType LocalIngexPlayer::getOutputType()
{
    return _config.outputType;
}

PlayerOutputType LocalIngexPlayer::getActualOutputType()
{
    return _actualOutputType;
}

bool LocalIngexPlayer::setX11WindowName(string name)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        _x11WindowName = name;

        // set name straight away if sink is open
        if (_playState != 0)
        {
            if (_playState->dualSink != 0)
            {
                dusk_set_x11_window_name(_playState->dualSink, _x11WindowName.c_str());
            }
            else if (_playState->x11Sink != 0)
            {
                xsk_set_window_name(_playState->x11Sink, _x11WindowName.c_str());
            }
            else if (_playState->x11XVSink != 0)
            {
                xvsk_set_window_name(_playState->x11XVSink, _x11WindowName.c_str());
            }
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::stop()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }

        _playState->stopPlaying();
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::toggleLock()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_toggle_lock(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::play()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_play(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::pause()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_pause(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::togglePlayPause()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_toggle_play_pause(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::seek(int64_t offset, int whence, PlayUnit unit)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_seek(ply_get_media_control(_playState->mediaPlayer), offset, whence, unit);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

/* TODO: allow non-frame units */
bool LocalIngexPlayer::playSpeed(int speed)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_play_speed(ply_get_media_control(_playState->mediaPlayer), speed, FRAME_PLAY_UNIT);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

/* TODO: allow non-frame units */
bool LocalIngexPlayer::step(bool forward)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_step(ply_get_media_control(_playState->mediaPlayer), forward, FRAME_PLAY_UNIT);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::muteAudio(int mute)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_mute_audio(ply_get_media_control(_playState->mediaPlayer), mute);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::mark(int type)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        /* TODO: legacy fix; type == 0 is no longer allowed */
        mc_mark(ply_get_media_control(_playState->mediaPlayer), type + 1, 0);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::markPosition(int64_t position, int type)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        /* TODO: legacy fix; type == 0 is no longer allowed */
        mc_mark_position(ply_get_media_control(_playState->mediaPlayer), position, type + 1, 0);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::clearMark()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_clear_mark(ply_get_media_control(_playState->mediaPlayer), ALL_MARK_TYPE);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::clearAllMarks()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_clear_all_marks(ply_get_media_control(_playState->mediaPlayer), ALL_MARK_TYPE);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::seekNextMark()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_seek_next_mark(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::seekPrevMark()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_seek_prev_mark(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::setOSDScreen(OSDScreen screen)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_set_osd_screen(ply_get_media_control(_playState->mediaPlayer), screen);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::nextOSDScreen()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_next_osd_screen(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::setOSDTimecode(int index, int type, int subType)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_set_osd_timecode(ply_get_media_control(_playState->mediaPlayer), index, type, subType);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::nextOSDTimecode()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_next_osd_timecode(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::showProgressBar(bool show)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        osd_set_progress_bar_visibility(msk_get_osd(_playState->mediaSink), show);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::switchNextVideo()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_switch_next_video(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::switchPrevVideo()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_switch_prev_video(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::switchVideo(int index)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        if (index == 0) // special case for quad split
        {
            mc_switch_video(ply_get_media_control(_playState->mediaPlayer), index);
        }
        else
        {
            // map the index of all inputs, whether present or not, to the
            // index of inputs present
            bool isPresent = false;
            int actualIndex = 1;
            int indexCount;
            vector<bool>::const_iterator iter;
            for (indexCount = 1, iter = _playState->inputsPresent.begin();
                iter != _playState->inputsPresent.end();
                indexCount++, iter++)
            {
                if (indexCount == index)
                {
                    // this is the input at index; set isPresent
                    isPresent = *iter;
                    break;
                }
                if ((*iter))
                {
                    // is present so increment the index
                    actualIndex++;
                }
            }
            if (isPresent)
            {
                mc_switch_video(ply_get_media_control(_playState->mediaPlayer), actualIndex);
            }
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::switchNextAudioGroup()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_switch_next_audio_group(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::switchPrevAudioGroup()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_switch_prev_audio_group(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::switchAudioGroup(int index)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_switch_audio_group(ply_get_media_control(_playState->mediaPlayer), index);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::snapAudioToVideo()
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_snap_audio_to_video(ply_get_media_control(_playState->mediaPlayer));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::reviewStart(int64_t duration)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_review_start(ply_get_media_control(_playState->mediaPlayer), duration);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::reviewEnd(int64_t duration)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_review_end(ply_get_media_control(_playState->mediaPlayer), duration);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::review(int64_t duration)
{
    try
    {
        ReadWriteLockGuard guard(&_playStateRWLock, false);

        if (!_playState)
        {
            return false;
        }
        if (_playState->hasStopped())
        {
            return false;
        }

        mc_review(ply_get_media_control(_playState->mediaPlayer), duration);
    }
    catch (...)
    {
        return false;
    }
    return true;
}


bool LocalIngexPlayer::setOrCreateX11Window(const X11WindowInfo* externalWindowInfo)
{
    if (externalWindowInfo->window != 0)
    {
        closeLocalX11Window();

        /* use the externally provided window */
        _windowInfo = *externalWindowInfo;
    }
    else
    {
        if (_windowInfo.window == 0 || _windowInfo.window == _config.externalWindowInfo.window)
        {
            /* create a locally provided window */
            if (x11c_open_display(&_windowInfo.display))
            {
                if (!x11c_create_window(&_windowInfo, 720, 576, _x11WindowName.c_str()))
                {
                    ml_log_error("Failed to create X11 window\n");
                    return false;
                }
            }
            else
            {
                ml_log_error("Failed to open X11 display\n");
                return false;
            }
        }
    }

    return true;
}

void LocalIngexPlayer::closeLocalX11Window()
{
    if (_windowInfo.window != 0 && _windowInfo.window != _config.externalWindowInfo.window)
    {
        x11c_close_window(&_windowInfo);
    }
}
