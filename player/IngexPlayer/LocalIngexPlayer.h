#ifndef __PRODAUTO_LOCAL_INGEX_PLAYER_H__
#define __PRODAUTO_LOCAL_INGEX_PLAYER_H__


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


/* IngexPlayer that runs locally on the machine */
class LocalIngexPlayer : public IngexPlayer
{
public:
    LocalIngexPlayer(PlayerOutputType outputType, VideoSwitchSplit videoSplit, int numFFMPEGThreads, 
        bool initiallyLocked, bool useWorkerThreads, bool applySplitFilter,
        int srcBufferSize, bool disableSDIOSD, bool disableX11OSD, Rational& sourceAspectRatio, 
        Rational& pixelAspectRatio, Rational& monitorAspectRatio, float scale,
        bool disablePCAudio, int audioDevice, int numAudioLevelMonitors, float audioLineupLevel);
    LocalIngexPlayer(PlayerOutputType outputType);
    
    virtual ~LocalIngexPlayer();
    

    std::string getVersion();
    std::string getBuildTimestamp();

    
    /* returns true if a DVS card is available for output */
    bool dvsCardIsAvailable();
    
    
    /* set the window-id when used as a browser plugin */
    void setPluginInfo(X11PluginWindowInfo *pluginInfo);
    
    
    /* setting the output type will cause the the player to be stop()ped and restarted when start() is called again */
    void setOutputType(PlayerOutputType outputType, float scale);
    
    /* returns the output type used */
    PlayerOutputType getOutputType();
    /* same as getOutputType(), except it returns the actual output type if an auto type is used */
    /* eg. if X11_AUTO_OUTPUT is set then returns either X11_XV_OUTPUT or X11_OUTPUT */
    PlayerOutputType getActualOutputType();
    
    
    /* sets the video split type (see ingex_player/video_switch_sink.h for enum values) when start() is called again */
    void setVideoSplit(VideoSwitchSplit videoSplit);
    
    
    /* will reset the player and display blank video on the output - returns false if a reset fails and
       the player will be closed */
    bool reset();
    
    /* will stop the player and close the X11 window */
    bool close();
    
    
    /* opens the files and start playing. The opened parameter indicates for each file whether
    it was successfully opened or not */
    virtual bool start(std::vector<std::string> mxfFilenames, std::vector<bool>& opened);
    
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
    

    PlayerOutputType _nextOutputType;
    PlayerOutputType _outputType;
    PlayerOutputType _actualOutputType;
    VideoSwitchSplit _nextVideoSplit;
    VideoSwitchSplit _videoSplit;
    int _numFFMPEGThreads;
    bool _initiallyLocked;
    bool _useWorkerThreads;
    bool _applySplitFilter;
    int _srcBufferSize;
    bool _disableSDIOSD;
    bool _disableX11OSD;
    X11PluginWindowInfo *_pluginInfo;
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



