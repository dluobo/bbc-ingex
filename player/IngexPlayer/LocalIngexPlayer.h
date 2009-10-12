/*
 * $Id: LocalIngexPlayer.h,v 1.15 2009/10/12 16:06:27 philipn Exp $
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

#ifndef __PRODAUTO_LOCAL_INGEX_PLAYER_H__
#define __PRODAUTO_LOCAL_INGEX_PLAYER_H__


#include <pthread.h>

#include <map>

#include "IngexPlayer.h"



namespace prodauto
{


class LocalIngexPlayerState;


typedef enum
{
    /* playout to SDI via DVS card */
    DVS_OUTPUT = 1,
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
} PlayerOutputType;

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
    LocalIngexPlayer(IngexPlayerListenerRegistry* listenerRegistry);
    virtual ~LocalIngexPlayer();

    std::string getVersion();
    std::string getBuildTimestamp();


    /* returns true if a DVS card is available for output */
    bool dvsCardIsAvailable();
    bool dvsCardIsAvailable(int card, int channel);

    /* returns true if an X11 XV output is available */
    bool x11XVIsAvailable();
    

    /* configuration - call these methods before calling start() */
    void setWindowInfo(const X11WindowInfo* windowInfo); /* default: not set */
    void setInitiallyLocked(bool locked); /* default: false */
    void setNumFFMPEGThreads(int num); /* default: 4 */
    void setUseWorkerThreads(bool enable); /* default: true */
    void setApplySplitFilter(bool enable); /* default: true */
    void setSourceBufferSize(int size); /* default: 0 */
    void setEnableX11OSD(bool enable); /* default: true */
    void setSourceAspectRatio(Rational *aspect); /* default: 0 */
    void setPixelAspectRatio(Rational *aspect); /* default: 1/1 */
    void setMonitorAspectRatio(Rational *aspect); /* default: 4/3 */
    void setEnablePCAudio(bool enable); /* default: true */
    void setAudioDevice(int device); /* default: 0 */
    void setNumAudioLevelMonitors(int num); /* default: 2 */
    void setAudioLineupLevel(float level); /* default: -18.0 */
    void setEnableAudioSwitch(bool enable); /* default: true */
    void setOutputType(PlayerOutputType outputType); /* default: X11_AUTO_OUTPUT */
    void setScale(float scale); /* default: 1.0 */
    void setDVSTarget(int card, int channel); /* default: -1, -1 */
    void setVideoSplit(VideoSwitchSplit videoSplit); /* default: QUAD_SPLIT_VIDEO_SWITCH */
    void setSDIOSDEnable(bool enable); /* default: true */


    /* will reset the player and display blank video on the output - returns false if a reset fails and
       the player will be closed */
    bool reset();

    /* will stop the player and close the X11 window */
    bool close();


    /* opens the files/sources and start playing. The opened parameter indicates for each file/source whether
    it was successfully opened or not */
    virtual bool start(std::vector<PlayerInput> inputs, std::vector<bool>& opened, bool startPaused, int64_t startPosition);

    /* returns the output type used */
    PlayerOutputType getOutputType();
    /* same as getOutputType(), except it returns the actual output type if an auto type is used */
    /* eg. if X11_AUTO_OUTPUT is set then returns either X11_XV_OUTPUT or X11_OUTPUT */
    PlayerOutputType getActualOutputType();
    
    
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
    virtual bool setOSDTimecodeDefaultSHM();
    virtual bool showProgressBar(bool show);
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
    typedef struct
    {
        X11WindowInfo externalWindowInfo;
        PlayerOutputType outputType;
        int dvsCard;
        int dvsChannel;
        VideoSwitchSplit videoSplit;
        int numFFMPEGThreads;
        bool initiallyLocked;
        bool useWorkerThreads;
        bool applySplitFilter;
        int srcBufferSize;
        bool disableSDIOSD;
        bool disableX11OSD;
        Rational sourceAspectRatio;
        Rational pixelAspectRatio;
        Rational monitorAspectRatio;
        float scale;
        bool disablePCAudio;
        int audioDevice;
        int numAudioLevelMonitors;
        float audioLineupLevel;
        bool enableAudioSwitch;
    } Configuration;
    
private:
    bool setOrCreateX11Window(const X11WindowInfo* externalWindowInfo);
    void closeLocalX11Window();

private:
    pthread_mutex_t _configMutex;
    Configuration _config;
    Configuration _nextConfig;

    PlayerOutputType _actualOutputType;
    
    TimecodeType _shmDefaultTCType;
    TimecodeSubType _shmDefaultTCSubType;

    X11WindowInfo _windowInfo;

    std::string _x11WindowName;

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
