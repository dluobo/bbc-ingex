#include <pthread.h>
#include <cassert>
#include <unistd.h>

#include "LocalIngexPlayer.h"
#include "Macros.h"


#include <multiple_sources.h>
#include <raw_file_source.h>
#include <mxf_source.h>
#include <buffered_media_source.h>
#include <x11_display_sink.h>
#include <x11_xv_display_sink.h>
#include <buffered_media_sink.h>
#include <dvs_sink.h>
#include <dual_sink.h>
#include <raw_file_sink.h>
#include <video_switch_sink.h>
#include <dv_stream_connect.h>
#include <mjpeg_stream_connect.h>
#include <audio_sink.h>
#include <audio_level_sink.h>
#include <version.h>
#include <utils.h>
#include <logging.h>


using namespace prodauto;
using namespace std;


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
                ml_log_error("pthread_rwlock_wrlock failed\n");
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
                ml_log_error("pthread_rwlock_rdlock failed\n");
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
                ml_log_error("pthread_rwlock_unlock failed\n");
            }
        }
    }
    
private:
    pthread_rwlock_t* _rwlock;
};


typedef struct
{
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



static void* player_thread(void* arg)
{
    LocalIngexPlayerState* playState = ((PlayerThreadArgs*)arg)->playState;
    
    if (!ply_start_player(playState->mediaPlayer))
    {
        ml_log_error("Media player failed to play\n");
    }
    
    pthread_exit((void*)0);
}



static void frame_displayed_event(void* data, const FrameInfo* frameInfo)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->frameDisplayedEvent(frameInfo);
    }
}

static void frame_dropped_event(void* data, const FrameInfo* lastFrameInfo)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->frameDroppedEvent(lastFrameInfo);
    }
}

static void state_change_event(void* data, const MediaPlayerStateEvent* event)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->stateChangeEvent(event);
    }
}

static void end_of_source_event(void* data, const FrameInfo* lastReadFrameInfo)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->endOfSourceEvent(lastReadFrameInfo);
    }
}

static void start_of_source_event(void* data, const FrameInfo* firstReadFrameInfo)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->startOfSourceEvent(firstReadFrameInfo);
    }
}

static void close_request(void* data)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->playerCloseRequested();
    }
}

static void player_closed(void* data)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->playerClosed();
    }
}

static void x11_key_pressed(void* data, int key)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->keyPressed(key);
    }
}

static void x11_key_released(void* data, int key)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->keyReleased(key);
    }
}

static void x11_progress_bar_position_set(void* data, float position)
{
    LocalIngexPlayer* player = (LocalIngexPlayer*)data;
    
    ReadWriteLockGuard guard(&player->_listenersRWLock, false);
        
    vector<IngexPlayerListener*>::const_iterator iter;
    for (iter = player->_listeners.begin(); iter != player->_listeners.end(); iter++)
    {
        (*iter)->progressBarPositionSet(position);
    }
}



LocalIngexPlayer::LocalIngexPlayer(PlayerOutputType outputType, bool videoSwitch, 
    int numFFMPEGThreads, bool initiallyLocked, bool useWorkerThreads, bool applyQuadSplitFilter,
    int srcBufferSize, bool disableSDIOSD, bool disableX11OSD, Rational& sourceAspectRatio, 
    Rational& pixelAspectRatio, Rational& monitorAspectRatio, float scale, bool disablePCAudio,
    int audioDevice, int numAudioLevelMonitors, float audioLineupLevel)
: _nextOutputType(outputType), _outputType(X11_OUTPUT), _actualOutputType(X11_OUTPUT), _videoSwitch(videoSwitch), _numFFMPEGThreads(numFFMPEGThreads),
_initiallyLocked(initiallyLocked), _useWorkerThreads(useWorkerThreads), 
_applyQuadSplitFilter(applyQuadSplitFilter), _srcBufferSize(srcBufferSize), 
_disableSDIOSD(disableSDIOSD), _disableX11OSD(disableX11OSD), _x11WindowName("Ingex Player"), 
_sourceAspectRatio(sourceAspectRatio), _pixelAspectRatio(pixelAspectRatio),
_monitorAspectRatio(monitorAspectRatio), _scale(scale), _prevScale(scale),
_disablePCAudio(disablePCAudio), _audioDevice(audioDevice), 
_numAudioLevelMonitors(numAudioLevelMonitors), _audioLineupLevel(audioLineupLevel) 
{
    initialise();
}

LocalIngexPlayer::LocalIngexPlayer(PlayerOutputType outputType)
: _nextOutputType(outputType), _outputType(X11_OUTPUT), _actualOutputType(X11_OUTPUT), _videoSwitch(true), _numFFMPEGThreads(4),
_initiallyLocked(false), _useWorkerThreads(true), 
_applyQuadSplitFilter(true), _srcBufferSize(0), _disableSDIOSD(false), _disableX11OSD(false), 
_x11WindowName("Ingex Player"), _scale(1.0), _prevScale(1.0), _disablePCAudio(false), _audioDevice(0), 
_numAudioLevelMonitors(2), _audioLineupLevel(-18.0)
{
    _sourceAspectRatio.num = 0;
    _sourceAspectRatio.den = 0;
    _pixelAspectRatio.num = 0;
    _pixelAspectRatio.den = 0;
    _monitorAspectRatio.num = 4;
    _monitorAspectRatio.den = 3;
    
    initialise();
}

void LocalIngexPlayer::initialise()
{
    _playState = 0;
    
    memset(&_mediaPlayerListener, 0, sizeof(MediaPlayerListener));
    
    _mediaPlayerListener.data = this;
    _mediaPlayerListener.frame_displayed_event = frame_displayed_event;
    _mediaPlayerListener.frame_dropped_event = frame_dropped_event;
    _mediaPlayerListener.state_change_event = state_change_event;
    _mediaPlayerListener.end_of_source_event = end_of_source_event;
    _mediaPlayerListener.start_of_source_event = start_of_source_event;
    _mediaPlayerListener.player_closed = player_closed;

    
    memset(&_x11WindowListener, 0, sizeof(X11WindowListener));
    _x11WindowListener.data = this;
    _x11WindowListener.close_request = close_request;
    
    memset(&_x11KeyListener, 0, sizeof(KeyboardInputListener));
    _x11KeyListener.data = this;
    _x11KeyListener.key_pressed = x11_key_pressed;
    _x11KeyListener.key_released = x11_key_released;
    
    memset(&_x11ProgressBarListener, 0, sizeof(ProgressBarInputListener));
    _x11ProgressBarListener.data = this;
    _x11ProgressBarListener.position_set = x11_progress_bar_position_set;
    
    pthread_rwlock_init(&_listenersRWLock, NULL);
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

    {    
        ReadWriteLockGuard guard(&_listenersRWLock, true);
        
        vector<IngexPlayerListener*>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            (*iter)->unsetRegistry();
        }
        _listeners.clear();
    }
    
    pthread_rwlock_destroy(&_listenersRWLock);
    pthread_rwlock_destroy(&_playStateRWLock);
    
    free_dv_decoder_resources();
    free_mjpeg_decoder_resources();
}

string LocalIngexPlayer::getVersion()
{
    return get_player_version();
}

string LocalIngexPlayer::getBuildTimestamp()
{
    return get_player_build_timestamp();
}

bool LocalIngexPlayer::dvsCardIsAvailable()
{
    return dvs_card_is_available() == 1;
}

bool LocalIngexPlayer::x11XVIsAvailable()
{
    return xvsk_check_is_available() == 1;
}

void LocalIngexPlayer::setOutputType(PlayerOutputType outputType, float scale)
{
    _nextOutputType = outputType;
    _scale = scale;
}

PlayerOutputType LocalIngexPlayer::getOutputType()
{
    return _outputType;
}

PlayerOutputType LocalIngexPlayer::getActualOutputType()
{
    return _actualOutputType;
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
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::start(vector<string> mxfFilenames, vector<bool>& opended)
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
    MXFFileSource* mxfSource = 0;
    LocalIngexPlayerState* currentPlayState = 0;
    
    try
    {
        if (mxfFilenames.size() == 0)
        {
            THROW_EXCEPTION(("No files to play\n"));
        }
        
        // effectively disable client access to the player
        {
            ReadWriteLockGuard guard(&_playStateRWLock, true);
            currentPlayState = _playState;
            _playState = 0;
        }
        
        // decide how to proceed if already playing 

        if (currentPlayState)
        {
            bool resetPlayer = true;
            if (_nextOutputType != _outputType || _scale != _prevScale)
            {
                // output type or scale has changed - stop the player
                SAFE_DELETE(&currentPlayState);
                newPlayState = auto_ptr<LocalIngexPlayerState>(new LocalIngexPlayerState());
                resetPlayer = false;
            }
            else if (_outputType == X11_AUTO_OUTPUT && _actualOutputType == X11_OUTPUT &&
                x11XVIsAvailable())
            {
                // Xv is now available
                SAFE_DELETE(&currentPlayState);
                newPlayState = auto_ptr<LocalIngexPlayerState>(new LocalIngexPlayerState());
                resetPlayer = false;
            }
            else if (_outputType == DUAL_DVS_AUTO_OUTPUT && _actualOutputType == DUAL_DVS_X11_OUTPUT &&
                x11XVIsAvailable())
            {
                // Xv is now available
                SAFE_DELETE(&currentPlayState);
                newPlayState = auto_ptr<LocalIngexPlayerState>(new LocalIngexPlayerState());
                resetPlayer = false;
            }
            
            if (resetPlayer)
            {
                // stop and reset the player
                currentPlayState->stopPlaying();
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
        }
        else
        {
            // new player
            newPlayState = auto_ptr<LocalIngexPlayerState>(new LocalIngexPlayerState());
        }
   
        
        // create multiple source source 
        
        CHK_OTHROW(mls_create(&_sourceAspectRatio, -1, &multipleSource));
        newPlayState->mediaSource = mls_get_media_source(multipleSource);
    
        
        // open the MXF media sources 
        
        bool atLeastOneInputOpened = false;
        vector<string>::const_iterator iter;
        for (iter = mxfFilenames.begin(); iter != mxfFilenames.end(); iter++)
        {
            string filename = *iter;
            MediaSource* mediaSource;
            
            if (!mxfs_open(filename.c_str(), 0, 0, 0, &mxfSource))
            {
                ml_log_warn("Failed to open MXF file source '%s'\n", filename.c_str());
                opended.push_back(false);
                newPlayState->inputsPresent.push_back(false);
            }
            else
            {
                mediaSource = mxfs_get_media_source(mxfSource);
                opended.push_back(true);
                newPlayState->inputsPresent.push_back(true);
                atLeastOneInputOpened = true;
                CHK_OTHROW(mls_assign_source(multipleSource, &mediaSource));
            }
        }
        
        // check at least one input could be opened
        if (!atLeastOneInputOpened)
        {
            if (haveReset && currentPlayState != 0)
            {
                // keep the old play state
                ReadWriteLockGuard guard(&_playStateRWLock, true);
                _playState = newPlayState.release();
            }
            return false;
        }
        
        
        // open the buffered media source 
        
        if (_srcBufferSize > 0)
        {
            CHK_OTHROW(bmsrc_create(newPlayState->mediaSource, _srcBufferSize, 1, -1.0, &bufferedSource));
            newPlayState->mediaSource = bmsrc_get_source(bufferedSource);
        }
        
        
        // open media sink 
        
        if (!haveReset)
        {
            // get the actual output type, possibly auto-detect whether X11 XV is available as output type
            
            if (_nextOutputType == X11_AUTO_OUTPUT)
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
            else if (_nextOutputType == DUAL_DVS_AUTO_OUTPUT)
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
                _actualOutputType = _nextOutputType;
            }
        

            switch (_actualOutputType)
            {
                case X11_OUTPUT:
                    CHK_OTHROW_MSG(xsk_open(20,_disableX11OSD, &_pixelAspectRatio, &_monitorAspectRatio, _scale, &x11Sink),
                        ("Failed to open X11 display sink\n"));
                    xsk_register_window_listener(x11Sink, &_x11WindowListener);
                    xsk_register_keyboard_listener(x11Sink, &_x11KeyListener);
                    xsk_register_progress_bar_listener(x11Sink, &_x11ProgressBarListener);
                    newPlayState->mediaSink = xsk_get_media_sink(x11Sink);
                    CHK_OTHROW_MSG(bms_create(&newPlayState->mediaSink, 2, 0, &bufSink),
                        ("Failed to create bufferred x11 xv display sink\n"));
                    newPlayState->mediaSink = bms_get_sink(bufSink);
                    newPlayState->x11Sink = x11Sink;
                    break;
        
                case X11_XV_OUTPUT:
                    CHK_OTHROW_MSG(xvsk_open(20, _disableX11OSD, &_pixelAspectRatio, &_monitorAspectRatio, _scale, &x11XVSink),
                        ("Failed to open X11 XV display sink\n"));
                    xvsk_register_window_listener(x11XVSink, &_x11WindowListener);
                    xvsk_register_keyboard_listener(x11XVSink, &_x11KeyListener);
                    xvsk_register_progress_bar_listener(x11XVSink, &_x11ProgressBarListener);
                    newPlayState->mediaSink = xvsk_get_media_sink(x11XVSink);
                    CHK_OTHROW_MSG(bms_create(&newPlayState->mediaSink, 2, 0, &bufSink),
                        ("Failed to create bufferred x11 xv display sink\n"));
                    newPlayState->mediaSink = bms_get_sink(bufSink);
                    newPlayState->x11XVSink = x11XVSink;
                    break;
        
                case DVS_OUTPUT:
                    CHK_OTHROW_MSG(dvs_open(VITC_AS_SDI_VITC, 0, 12, _disableSDIOSD, 1, &dvsSink), 
                        ("Failed to open DVS sink\n"));
                    newPlayState->mediaSink = dvs_get_media_sink(dvsSink);
                    break;
                    
                case DUAL_DVS_X11_OUTPUT:
                    CHK_OTHROW_MSG(dusk_open(20, VITC_AS_SDI_VITC, 0, 12, 0, _disableSDIOSD, 
                        _disableX11OSD, &_pixelAspectRatio, 
                        &_monitorAspectRatio, _scale, 1, &dualSink),
                        ("Failed to open dual DVS and X11 display sink\n"));
                    dusk_register_window_listener(dualSink, &_x11WindowListener);
                    dusk_register_keyboard_listener(dualSink, &_x11KeyListener);
                    dusk_register_progress_bar_listener(dualSink, &_x11ProgressBarListener);
                    newPlayState->mediaSink = dusk_get_media_sink(dualSink);
                    newPlayState->dualSink = dualSink;
                    break;
                    
                case DUAL_DVS_X11_XV_OUTPUT:
                    CHK_OTHROW_MSG(dusk_open(20, VITC_AS_SDI_VITC, 0, 12, 1, _disableSDIOSD, 
                        _disableX11OSD, &_pixelAspectRatio, 
                        &_monitorAspectRatio, _scale, 1, &dualSink),
                        ("Failed to open dual DVS and X11 XV display sink\n"));
                    dusk_register_window_listener(dualSink, &_x11WindowListener);
                    dusk_register_keyboard_listener(dualSink, &_x11KeyListener);
                    dusk_register_progress_bar_listener(dualSink, &_x11ProgressBarListener);
                    newPlayState->mediaSink = dusk_get_media_sink(dualSink);
                    newPlayState->dualSink = dualSink;
                    break;
                    
                default:
                    assert(false);
            }
            
#if defined(HAVE_PORTAUDIO)    
            // create audio sink
            
            if (!_disablePCAudio)
            {
                if (_actualOutputType == X11_OUTPUT ||
                    _actualOutputType == X11_XV_OUTPUT)
                    // TODO* || _actualOutputType == DUAL_OUTPUT)
                {
                    AudioSink* audioSink;
                    if (!aus_create_audio_sink(newPlayState->mediaSink, _audioDevice, &audioSink))
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
            
            if (_videoSwitch)
            {
                VideoSwitchSink* videoSwitch;
                CHK_OTHROW(qvs_create_video_switch(newPlayState->mediaSink, 1, _applyQuadSplitFilter, &videoSwitch));
                newPlayState->mediaSink = vsw_get_media_sink(videoSwitch);
            }
            
            
            // create audio level monitors
            
            if (_numAudioLevelMonitors > 0)
            {
                AudioLevelSink* audioLevelSink;
                CHK_OTHROW(als_create_audio_level_sink(newPlayState->mediaSink, _numAudioLevelMonitors, 
                    _audioLineupLevel, &audioLevelSink));
                newPlayState->mediaSink = als_get_media_sink(audioLevelSink);
            }

        }
    
        
        // create the player 
        
        CHK_OTHROW(ply_create_player(newPlayState->mediaSource, newPlayState->mediaSink, 
            _initiallyLocked, 0, _numFFMPEGThreads, _useWorkerThreads, 0, 0, 
            &g_invalidTimecode, &g_invalidTimecode, NULL, &newPlayState->mediaPlayer));
        CHK_OTHROW(ply_register_player_listener(newPlayState->mediaPlayer, &_mediaPlayerListener));
            
        if (_srcBufferSize > 0)
        {
            bmsrc_set_media_player(bufferedSource, newPlayState->mediaPlayer);
        }
        
        
        // start with player state screen
        mc_set_osd_screen(ply_get_media_control(newPlayState->mediaPlayer), OSD_PLAY_STATE_SCREEN);

        
        // start play thread 
    
        newPlayState->playThreadArgs.playState = newPlayState.get();
        CHK_OTHROW(create_joinable_thread(&newPlayState->playThreadId, player_thread, &newPlayState->playThreadArgs));


        _outputType = _nextOutputType;
        _prevScale = _scale;
        {
            ReadWriteLockGuard guard(&_playStateRWLock, true);
            _playState = newPlayState.release();
        }
        
        setX11WindowName(_x11WindowName);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::registerListener(IngexPlayerListener* listener)
{
    if (listener == 0)
    {
        return false;
    }
    
    try
    {
        ReadWriteLockGuard guard(&_listenersRWLock, true);
        
        _listeners.push_back(listener);
        if (listener->getRegistry() != 0 && listener->getRegistry() != this)
        {
            // first unregister the listener with the 'other' player 
            listener->getRegistry()->unregisterListener(listener);
        }
        
        listener->setRegistry(this);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool LocalIngexPlayer::unregisterListener(IngexPlayerListener* listener)
{
    if (listener == 0 || listener->getRegistry() != this)
    {
        return true;
    }
    
    try
    {
        ReadWriteLockGuard guard(&_listenersRWLock, true);
        
        vector<IngexPlayerListener*>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if (*iter == listener)
            {
                _listeners.erase(iter);
                listener->unsetRegistry();
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

