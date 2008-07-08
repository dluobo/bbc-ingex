/*
 * $Id: DirectoryWatch.cpp,v 1.1 2008/07/08 16:23:26 philipn Exp $
 *
 * Reports changes to a directory and it's contents to listeners
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
 
#include <sys/inotify.h>
#include <errno.h>

#include "DirectoryWatch.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Timing.h"


using namespace std;
using namespace rec;


namespace rec
{
    
class DirectoryWatchWorker : public ThreadWorker
{
public:    
    DirectoryWatchWorker(DirectoryWatch* directoryWatch, string directory, int flags, long pollUSec)
    : _directoryWatch(directoryWatch), _directory(directory), _pollUSec(pollUSec), _hasStopped(false), _stop(false),
    _inotifyFD(-1)
    {
        _inotifyFD = inotify_init();
        if (_inotifyFD < 0)
        {
            REC_LOGTHROW(("Failed to initialise inotify for directory watch: %s", strerror(errno)));
        }
        
        int inotifyFlags = IN_DELETE_SELF;
        if (flags & DIR_WATCH_DELETION)
        {
            inotifyFlags |= IN_DELETE;
        }
        if (flags & DIR_WATCH_CREATION)
        {
            inotifyFlags |= IN_CREATE;
        }
        if (flags & DIR_WATCH_MODIFICATION)
        {
            inotifyFlags |= IN_MODIFY;
        }
        if (flags & DIR_WATCH_MOVED_FROM)
        {
            inotifyFlags |= IN_MOVED_FROM;
        }
        if (flags & DIR_WATCH_MOVED_TO)
        {
            inotifyFlags |= IN_MOVED_TO;
        }
        
        _watch = inotify_add_watch(_inotifyFD, directory.c_str(), inotifyFlags);
        if (_watch < 0)
        {
            close(_inotifyFD);
            REC_LOGTHROW(("Failed to add inotify watch to '%s': %s", directory.c_str(), strerror(errno)));
        }
        
    }
    
    ~DirectoryWatchWorker()
    {
        if (_inotifyFD >= 0)
        {
            close(_inotifyFD);
        }
    }

    void start()
    {
        GUARD_THREAD_START(_hasStopped);
        _stop = false;
    
    
        struct timeval selectTime;
        fd_set rfds;
        int selectResult = 0;
        int prevSelectResult = selectResult;
        bool deletedWatchDirectory = false;
        
        while (!_stop && !deletedWatchDirectory)
        {
            FD_ZERO(&rfds);
            FD_SET(_inotifyFD, &rfds);
            
            selectTime.tv_sec = 0;
            selectTime.tv_usec = _pollUSec;
            selectResult = select(_inotifyFD + 1, &rfds, NULL, NULL, &selectTime);
            
            if (selectResult < 0)
            {
                if (errno == EINTR) 
                {
                    // we sometimes get interrupted by signals - try again
                    continue;
                }
                
                if (selectResult != prevSelectResult)
                {
                    // log warning the first time the error occurs
                    Logging::warning("Select error in directory watch: %s\n", strerror(errno));
                }
                
                // don't hog the CPU when select errors occur
                sleep_msec(500);
            }
            else if (selectResult && FD_ISSET(_inotifyFD, &rfds))
            {
                #define EVENT_SIZE  (sizeof(struct inotify_event))
                #define BUF_LEN (1024 * (EVENT_SIZE + 16))
                char buf[BUF_LEN];
                int len, i = 0;
                len = read(_inotifyFD, buf, BUF_LEN);
                if (len < 0) 
                {
                    Logging::error("Failed to read inotify events: %s\n", strerror(errno));
                } 
                else if (len == 0)
                {
                    Logging::error("Failed to read inotify events - buffer could be too small\n");
                }
                else
                {
                    while (i < len) 
                    {
                        struct inotify_event* event;
                        event = (struct inotify_event*)&buf[i];
                        
                        if (event->wd != _watch)
                        {
                            continue;
                        }
                        
                        if (event->mask & IN_DELETE_SELF)
                        {
                            _directoryWatch->sendThisDirectoryDeletion(_directory);
                            
                            // end of the line
                            deletedWatchDirectory = true;
                        }
                        if (event->mask & IN_DELETE)
                        {
                            _directoryWatch->sendDeletion(_directory, event->name);
                        }
                        if (event->mask & IN_CREATE)
                        {
                            _directoryWatch->sendCreation(_directory, event->name);
                        }
                        if (event->mask & IN_MODIFY)
                        {
                            _directoryWatch->sendModification(_directory, event->name);
                        }
                        if (event->mask & IN_MOVED_FROM)
                        {
                            _directoryWatch->sendMovedFrom(_directory, event->name);
                        }
                        if (event->mask & IN_MOVED_TO)
                        {
                            _directoryWatch->sendMovedTo(_directory, event->name);
                        }
                        
                        
                        i += EVENT_SIZE + event->len;
                    }
                }
            }
            
            prevSelectResult = selectResult;
        }   
    }

    void stop()
    {
        _stop = true;
    }
    
    bool hasStopped() const
    {
        return _hasStopped;
    }

private:
    DirectoryWatch* _directoryWatch;
    string _directory;
    long _pollUSec;
    bool _hasStopped;
    bool _stop;
    
    int _inotifyFD;
    int _watch;
};


}; // namespace rec




DirectoryWatch::DirectoryWatch(std::string directory, int flags, long pollUSec)
{
    _directoryWatchWorker = new Thread(new DirectoryWatchWorker(this, directory, flags, pollUSec), true);
    _directoryWatchWorker->start();
}

DirectoryWatch::~DirectoryWatch()
{
    delete _directoryWatchWorker;
}

void DirectoryWatch::registerListener(DirectoryWatchListener* listener)
{
    LOCK_SECTION(_listenerMutex);
    
    // check if already present
    vector<DirectoryWatchListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if (*iter == listener)
        {
            // only add to list if it is not already present
            Logging::debug("Registering directory watch listener which is already in the list of listeners\n");
            return;
        }
    }
    
    _listeners.push_back(listener);
}

void DirectoryWatch::unregisterListener(DirectoryWatchListener* listener)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<DirectoryWatchListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if (*iter == listener)
        {
            // remove listener from list
            _listeners.erase(iter);
            return;
        }
    }
    
    Logging::debug("Attempt made to un-register a unknown directory watch listener\n");
}

bool DirectoryWatch::isReady()
{
    return _directoryWatchWorker->isRunning();
}


void DirectoryWatch::sendThisDirectoryDeletion(string directory)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<DirectoryWatchListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->thisDirectoryDeletion(directory);
    }
}

void DirectoryWatch::sendDeletion(string directory, string name)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<DirectoryWatchListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->deletion(directory, name);
    }
}

void DirectoryWatch::sendCreation(string directory, string name)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<DirectoryWatchListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->creation(directory, name);
    }
}

void DirectoryWatch::sendModification(string directory, string name)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<DirectoryWatchListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->modification(directory, name);
    }
}

void DirectoryWatch::sendMovedFrom(string directory, string name)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<DirectoryWatchListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->movedFrom(directory, name);
    }
}

void DirectoryWatch::sendMovedTo(string directory, string name)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<DirectoryWatchListener*>::const_iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->movedTo(directory, name);
    }
}


