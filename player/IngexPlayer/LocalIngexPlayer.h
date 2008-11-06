/*
 * $Id: LocalIngexPlayer.h,v 1.10 2008/11/06 19:56:56 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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

#ifndef __PRODAUTO_LOCAL_INGEX_PLAYER_H__
#define __PRODAUTO_LOCAL_INGEX_PLAYER_H__


#include <map>

#include "IngexPlayer.h"



namespace prodauto
{

    
class LocalIngexPlayerState;


typedef enum PlayerOutputType
{
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


/* IngexPlayer that runs locally on the machine */
class LocalIngexPlayer : public IngexPlayer
{
public:
    LocalIngexPlayer(PlayerOutputType outputType, VideoSwitchSplit videoSplit, int numFFMPEGThreads, 
        bool initiallyLocked, bool useWorkerThreads, bool applySplitFilter,
        int srcBufferSize, bool disableSDIOSD, bool disableX11OSD, Rational& sourceAspectRatio, 
        Rational& pixelAspectRatio, Rational& monitorAspectRatio, float scale,
        bool disablePCAudio, int audioDevice, int numAudioLevelMonitors, float audioLineupLevel,
        bool enableAudioSwitch);
    LocalIngexPlayer(PlayerOutputType outputType);
    
    virtual ~LocalIngexPlayer();
    

    std::string getVersion();
    std::string getBuildTimestamp();

    
    /* returns true if a DVS card is available for output */
    bool dvsCardIsAvailable();
    bool dvsCardIsAvailable(int card, int channel);
    
    
    /* e.g. set the window-id when used as a browser plugin */
    void setWindowInfo(const X11WindowInfo* windowInfo);
    
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
    
    
    /* will reset the player and display blank video on the output - returns false if a reset fails and
       the player will be closed */
    bool reset();
    
    /* will stop the player and close the X11 window */
    bool close();
    
    
    /* opens the files/sources and start playing. The opened parameter indicates for each file/source whether
    it was successfully opened or not */
    virtual bool start(std::vector<PlayerInput> inputs, std::vector<bool>& opened, bool startPaused, int64_t startPosition);
    
    /* functions inherited from IngexPlayerListenerRegistry */
    virtual bool registerListener(IngexPlayerListener* listener);
    virtual bool unregisterListener(IngexPlayerListener* listener);

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
    

    /* allowing access for C callback functions */
    pthread_rwlock_t _listenersRWLock;
    std::vector<IngexPlayerListener*> _listeners;
    
private:
    void initialise();

    /* returns true if a X11 XV output is available */
    bool x11XVIsAvailable();

    bool setOrCreateX11Window();
    void closeLocalX11Window();
    

    PlayerOutputType _nextOutputType;
    PlayerOutputType _outputType;
    PlayerOutputType _actualOutputType;
    int _dvsCard;
    int _dvsChannel;
    VideoSwitchSplit _nextVideoSplit;
    VideoSwitchSplit _videoSplit;
    int _numFFMPEGThreads;
    bool _initiallyLocked;
    bool _useWorkerThreads;
    bool _applySplitFilter;
    int _srcBufferSize;
    bool _disableSDIOSD;
    bool _nextDisableSDIOSD;
    bool _disableX11OSD;
    X11WindowInfo _externalWindowInfo;
    X11WindowInfo _localWindowInfo;
    X11WindowInfo _windowInfo;
    std::string _x11WindowName;
    Rational _sourceAspectRatio;
    Rational _pixelAspectRatio;
    Rational _monitorAspectRatio;
    float _scale;
    float _prevScale;
    bool _disablePCAudio;
    int _audioDevice;
    int _numAudioLevelMonitors;
    float _audioLineupLevel;
    bool _enableAudioSwitch;

    pthread_rwlock_t _playStateRWLock;
    LocalIngexPlayerState* _playState;
    
    MediaPlayerListener _mediaPlayerListener;
    X11WindowListener _x11WindowListener;
    KeyboardInputListener _x11KeyListener;
    ProgressBarInputListener _x11ProgressBarListener;
    MouseInputListener _x11MouseListener;
    
    StreamInfo _videoStreamInfo;
};


};



#endif



