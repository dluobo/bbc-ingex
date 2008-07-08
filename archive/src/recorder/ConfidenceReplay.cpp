/*
 * $Id: ConfidenceReplay.cpp,v 1.1 2008/07/08 16:25:33 philipn Exp $
 *
 * Runs and provides access to a player running in a separate process
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
 
/*
    The player is run in a separate process. Player control and getting player
    state information is done using HTTP requests.
*/
 
#define __STDC_FORMAT_MACROS 1

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <curl/curl.h>

#include "ConfidenceReplay.h"
#include "PlayerState.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"
#include "Timing.h"



using namespace std;
using namespace rec;


#define COMMANDS_BUFFER_SIZE                20

// maximum data size that will be accepted for the player state http response
#define MAX_PLAYER_STATE_RESPONSE_SIZE      4096

// poll the player state every 1/5 second
#define PLAYER_STATE_POLL_INTERVAL          (200 * MSEC_IN_USEC)


// TODO: make ConfidenceReplay a singelton because of these global variables

static pid_t g_childProcessPid = 0;
static int g_signalForChild = 0;
static bool g_signalledChild = false;

static void signal_child(int signum)
{
    g_signalledChild = true;
    g_signalForChild = signum;
    kill(g_childProcessPid, signum);
}



// Utility class to set and ensure the signals are returned to their original state

#define GUARD_SIGNAL(idx, signum, handler)      SignalGuard __signalGuard##idx(signum, handler);
#define GUARD_SIGNAL_BLOCK(idx, signum)         SignalGuard __signalGuardBlock##idx(signum);

class SignalGuard
{
public:
    SignalGuard(int signum)
    : _signum(signum), _isSet(false), _isBlocked(false)
    {
        // block the signal
        sigset_t blockedSet;
        sigemptyset(&blockedSet);
        sigaddset(&blockedSet, signum);
        if (sigprocmask(SIG_BLOCK, &blockedSet, &_originalBlockedSet) < 0)
        {
            Logging::error("Failed to block signal: %s\n", strerror(errno));
        }
        else
        {
            _isBlocked = true;
        }
    }
    
    SignalGuard(int signum, sighandler_t handler)
    : _signum(signum), _isSet(false), _isBlocked(false)
    {
        // install new handler
        if ((_originalHandler = signal(signum, handler)) == SIG_ERR)
        {
            Logging::error("Failed to install signal handler: %s\n", strerror(errno));
        }
        else
        {
            _isSet = true;
        }
    }
    
    ~SignalGuard()
    {
        if (_isSet)
        {
            if (signal(_signum, _originalHandler) == SIG_ERR)
            {
                Logging::error("Failed to restore original signal handler: %s\n", strerror(errno));
            }
        }
        else if (_isBlocked)
        {
            if (sigprocmask(SIG_BLOCK, &_originalBlockedSet, NULL) < 0)
            {
                Logging::error("Failed to un-block signal: %s\n", strerror(errno));
            }
        }
    }
    
private:
    int _signum;
    bool _isSet;
    bool _isBlocked;
    sighandler_t _originalHandler;
    sigset_t _originalBlockedSet;
};
    


class ConfidenceReplayWorker : public ThreadWorker
{
public:    
    ConfidenceReplayWorker(string filename, int httpAccessPort, int64_t startPosition, bool showSecondMarkBar)
    : _hasStopped(false), _filename(filename), _httpAccessPort(httpAccessPort), _startPosition(startPosition), 
    _showSecondMarkBar(showSecondMarkBar), _playerPid(-1)
    {
        _curl = curl_easy_init();
        if (!_curl)
        {
            REC_LOGTHROW(("Failed to initialise curl easy"));
        }
    }
        
    virtual ~ConfidenceReplayWorker()
    {
        curl_easy_cleanup(_curl);
    }
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        _playerPid = fork();
        if (_playerPid < 0)
        {
            Logging::error("Failed to fork for confidence replay: %s\n", strerror(errno));
            _playerPid = -1;
            return;
        }
        
        if (_playerPid == 0) // child
        {
            if (_showSecondMarkBar)
            {
                execl("/usr/local/bin/player", // the executable
                    "/usr/local/bin/player", // the executable name
                    Config::getBool("dbg_vga_replay") ? 
                        "--x11" : // output to the X11 display 
                        "--dvs", // output to SDI
                    "--audio-mon", "8", // show audio levels for maximum 8 audio streams
                    "--show-tc", "VITC.0", // show VITC in place of control timecode
                    "--mark-vtr-errors", // mark VTR errors on the progress bar
                    "--config-marks", "9,itemstart,red:17,vtrerror,yellow", // show item start and vtr error mark types
                    "--qc-control", // use the QC control input mapping
                    "--disable-console-mon", // disable console monitoring of the player
                    "--log-file", "/dev/null", // no logging
                    "--http-access", int_to_string(_httpAccessPort).c_str(), // port for sending control commands
                    "--start", int64_to_string(_startPosition).c_str(), // start playing from this position
                    "--pb-mark-mask", "0xfffcffff", // mask for everything but PSE and VTR marks on the progress bar
                    "--pb-mark-mask", "0x00030000", // mask for PSE and VTR marks on the second progress bar
                    "-m", _filename.c_str(), // input file
                    (char *) NULL);
            }
            else
            {
                execl("/usr/local/bin/player", // the executable
                    "/usr/local/bin/player", // the executable name
                    Config::getBool("dbg_vga_replay") ? 
                        "--x11" : // output to the X11 display 
                        "--dvs", // output to SDI
                    "--audio-mon", "8", // show audio levels for maximum 8 audio streams
                    "--show-tc", "VITC.0", // show VITC in place of control timecode
                    "--mark-vtr-errors", // mark VTR errors on the progress bar
                    "--config-marks", "9,itemstart,red:17,vtrerror,yellow", // show item start and vtr error mark types
                    "--qc-control", // use the QC control input mapping
                    "--disable-console-mon", // disable console monitoring of the player
                    "--log-file", "/dev/null", // no logging
                    "--http-access", int_to_string(_httpAccessPort).c_str(), // port for sending control commands
                    "--start", int64_to_string(_startPosition).c_str(), // start playing from this position
                    "-m", _filename.c_str(), // input file
                    (char *) NULL);
            }
            Logging::error("Failed to execute player command for confidence replay: %s\n", strerror(errno));
            exit(1);
        }
        else // parent
        {
            int childResult;
            int childStatus;
            { // scope where signals handling is modified
                
                // globals used by signal_child function and later on
                g_childProcessPid = _playerPid;
                g_signalledChild = false;
                g_signalForChild = 0;
                
                // send signals through to child and handle through child return status
                GUARD_SIGNAL(1, SIGINT, signal_child);
                GUARD_SIGNAL(2, SIGTERM, signal_child);
                GUARD_SIGNAL(3, SIGQUIT, signal_child);
                GUARD_SIGNAL(4, SIGHUP, signal_child);
                
                // block sigchild because it is handled through child return status
                GUARD_SIGNAL_BLOCK(1, SIGCHLD);
            
                // wait for child to exit
                Timer timer;
                while ((childResult = waitpid(_playerPid, &childStatus, WNOHANG)) == 0)
                {
                    string command;
                    {
                        LOCK_SECTION(_commandMutex);
                        
                        if (_commands.size() > 0)
                        {
                            command = _commands.back();
                            _commands.pop_back();
                        }
                    }
                    
                    if (command.size() > 0)
                    {
                        sendCommand(command);
                    }
                    else
                    {
                        // don't hog the CPU - sleep at least 500usec
                        sleep_usec(500);
                        timer.sleepRemainder();
                    }
                    
                    // check for commands or child exit every 5 msec
                    timer.start(5 * MSEC_IN_USEC);
                }
            }

            _playerPid = -1;
            
            if (childResult < 0) // error
            {
                Logging::error("Failed to wait for confidence replay child process: %s\n", strerror(errno));
            }
            else // child returned status
            {
                Logging::debug("Confidence replay child process returned status %d\n", childStatus);
                
                // if a signal was sent to the child through the signal handlers then send it to this process
                if (g_signalledChild)
                {
                    Logging::info("Termination signal received through confidence replay child\n");
                    kill(0, g_signalForChild);
                }
            }
        }
        
    }
    
    virtual void stop()
    {
        if (_playerPid > 0)
        {
            Logging::info("Stopping the confidence replay process\n");
            
            sendCommand("stop");
            
            // wait maximum 2 seconds for the player to stop
            int count = 200;
            while (!_hasStopped && count > 0)
            {
                sleep_msec(10);
                count--;
            }
            
            // if not stopped then terminate using a signal
            if (!_hasStopped)
            {
                Logging::warning("Failed to stop the confidence replay process - sending SIGKILL signal\n");
                
                if (kill(_playerPid, SIGKILL) < 0)
                {
                    Logging::error("Failed to kill (SIGKILL) child process running player: %s\n", strerror(errno));
                }
            }
        }
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
    
    void forwardControl(string command)
    {
        LOCK_SECTION(_commandMutex);
        
        if (_commands.size() >= COMMANDS_BUFFER_SIZE)
        {
            // ignore the request
            return;
        }
        
        _commands.push_back(command);
    }
    
    void sendCommand(string command)
    {
        // send command through to the player
        
        string url;
        url = "http://localhost:" + int_to_string(_httpAccessPort) + 
            "/player/control/" + command;
        
        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, errorBuffer);
        
        curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
        CURLcode result = curl_easy_perform(_curl);
        if (result != 0)
        {
            Logging::warning("Failed to send replay command '%s': %s\n", url.c_str(), errorBuffer);
        }
    }
    
private:
    bool _hasStopped;
    string _filename;
    int _httpAccessPort;
    int64_t _startPosition;
    bool _showSecondMarkBar;
    pid_t _playerPid;
    
    vector<string> _commands;
    Mutex _commandMutex;
    
    CURL* _curl;
};



class StateMonitorWorker : public ThreadWorker
{
public:    
    StateMonitorWorker( int httpAccessPort)
    : _stop(false), _hasStopped(false), _httpAccessPort(httpAccessPort), _duration(-1), _position(-1)
    {
        _curl = curl_easy_init();
        if (!_curl)
        {
            REC_LOGTHROW(("Failed to initialise curl easy"));
        }
    }
        
    virtual ~StateMonitorWorker()
    {
        curl_easy_cleanup(_curl);
    }
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        Timer timer;

        while (!_stop)
        {
            timer.start(PLAYER_STATE_POLL_INTERVAL);
            
            getPlayerState();
            
            timer.sleepRemainder();
        }
    }
    
    virtual void stop()
    {
        _stop = true;
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    

    int64_t getDuration()
    {
        LOCK_SECTION(_stateMutex);
        
        return _duration;
    }
    
    int64_t getPosition()
    {
        LOCK_SECTION(_stateMutex);
        
        return _position;
    }
    

private:    
    static size_t responseHandler(void* ptr, size_t size, size_t nmemb, void* stream)
    {
        StateMonitorWorker* monitorWorker = static_cast<StateMonitorWorker*>(stream);
        
        if (monitorWorker->_stateResponse.size() <= MAX_PLAYER_STATE_RESPONSE_SIZE)
        {
            monitorWorker->_stateResponse.append(static_cast<char*>(ptr), size * nmemb);
        }
        
        return size * nmemb;
    }
    
    void getPlayerState()
    {
        string url;
        url = "http://localhost:" + int_to_string(_httpAccessPort) + 
            "/player/state.txt";
        
        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, responseHandler);
        curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
        
        _stateResponse.clear();
        _stateResponse.reserve(512);
        
        CURLcode result = curl_easy_perform(_curl);
        if (result != 0)
        {
            // assume we failed to get player state because the player has not yet started or
            // has been closed
            return;
        }

        
        if (!_playerState.parse(_stateResponse))
        {
            Logging::warning("Failed to parse player state http response\n");
            return;
        }
        
        int64_t position;
        int64_t startOffset;
        int64_t length;
        
        if (!_playerState.getInt64("length", &length))
        {
            Logging::warning("Failed to parse 'length' from player state\n");
            return;
        }
        if (!_playerState.getInt64("startOffset", &startOffset))
        {
            Logging::warning("Failed to parse 'startOffset' from player state\n");
            return;
        }
        if (!_playerState.getInt64("position", &position))
        {
            Logging::warning("Failed to parse 'position' from player state\n");
            return;
        }

        
        LOCK_SECTION(_stateMutex);
        
        _duration = length;
        _position = startOffset + position;
    }

    
    bool _stop;
    bool _hasStopped;
    int _httpAccessPort;
    
    Mutex _stateMutex;
    string _stateResponse;
    PlayerState _playerState;
    int64_t _duration;
    int64_t _position;
    
    CURL* _curl;
};



ConfidenceReplay::ConfidenceReplay(std::string filename, int httpAccessPort, int64_t startPosition, bool showSecondMarkBar)
: Thread(new ConfidenceReplayWorker(filename, httpAccessPort, startPosition, showSecondMarkBar), true), _filename(filename), _stateMonitor(0)
{
    _stateMonitor = new Thread(new StateMonitorWorker(httpAccessPort), true);
    _stateMonitor->start();
}

ConfidenceReplay::~ConfidenceReplay()
{
    stop();
    
    delete _stateMonitor;
}

void ConfidenceReplay::forwardControl(string command)
{
    if (!_worker->hasStopped())
    {
        ConfidenceReplayWorker* confReplayWorker = dynamic_cast<ConfidenceReplayWorker*>(_worker);
        confReplayWorker->forwardControl(command);
    }
}
    
void ConfidenceReplay::setMark(int64_t position, int type)
{
    char buffer[128];
    sprintf(buffer, "mark-position?position=%"PRId64"&type=%d&toggle=false", position, type);
    
    forwardControl(buffer);
}

void ConfidenceReplay::clearMark(int64_t position, int type)
{
    char buffer[128];
    sprintf(buffer, "clear-mark-position?position=%"PRId64"&type=%d", position, type);
    
    forwardControl(buffer);
}

void ConfidenceReplay::seek(int64_t position)
{
    char buffer[128];
    sprintf(buffer, "seek?offset=%"PRId64"&whence=SEEK_SET&unit=FRAME_PLAY_UNIT&pause=true", position);
    
    forwardControl(buffer);
}

void ConfidenceReplay::play()
{
    forwardControl("play");
}

void ConfidenceReplay::seekPositionOrPlay(int64_t position)
{
    if (getPosition() == position)
    {
        play();
    }
    else
    {
        seek(position);
    }
}

int64_t ConfidenceReplay::getDuration()
{
    StateMonitorWorker* monitorWorker = dynamic_cast<StateMonitorWorker*>(_stateMonitor->getWorker());
    return monitorWorker->getDuration();
}

int64_t ConfidenceReplay::getPosition()
{
    StateMonitorWorker* monitorWorker = dynamic_cast<StateMonitorWorker*>(_stateMonitor->getWorker());
    return monitorWorker->getPosition();
}

string ConfidenceReplay::getFilename()
{
    return _filename;
}


