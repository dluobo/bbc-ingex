/*
 * $Id: DirectoryWatch.h,v 1.1 2008/07/08 16:23:33 philipn Exp $
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
 
#ifndef __RECORDER_DIRECTORY_WATCH_H__
#define __RECORDER_DIRECTORY_WATCH_H__


#include <string>
#include <vector>

#include "Threads.h"

// flags to pass to DirectoryWatch() for registering for particular events

#define DIR_WATCH_DELETION          0x0001
#define DIR_WATCH_CREATION          0x0002
#define DIR_WATCH_MODIFICATION      0x0004
#define DIR_WATCH_MOVED_FROM        0x0008
#define DIR_WATCH_MOVED_TO          0x0010
#define DIR_WATCH_EVERYTHING        0xffff




namespace rec
{


class DirectoryWatchListener
{
public:    
    virtual ~DirectoryWatchListener() {}
    
    virtual void thisDirectoryDeletion(std::string directory) {};
    
    virtual void deletion(std::string directory, std::string name) {};
    virtual void creation(std::string directory, std::string name) {};
    virtual void modification(std::string directory, std::string name) {};
    virtual void movedFrom(std::string directory, std::string name) {};
    virtual void movedTo(std::string directory, std::string name) {};
};



class DirectoryWatchWorker;

class DirectoryWatch
{
public:
    friend class DirectoryWatchWorker;
    
public:
    DirectoryWatch(std::string directory, int flags, long pollUSec);
    ~DirectoryWatch();
    
    void registerListener(DirectoryWatchListener* listener);
    void unregisterListener(DirectoryWatchListener* listener);
    
    bool isReady();

private:
    void sendThisDirectoryDeletion(std::string directory);
    void sendDeletion(std::string directory, std::string name);
    void sendCreation(std::string directory, std::string name);
    void sendModification(std::string directory, std::string name);
    void sendMovedFrom(std::string directory, std::string name);
    void sendMovedTo(std::string directory, std::string name);
    
    
    Thread* _directoryWatchWorker;
    
    std::vector<DirectoryWatchListener*> _listeners;
    Mutex _listenerMutex;
};




};



#endif


