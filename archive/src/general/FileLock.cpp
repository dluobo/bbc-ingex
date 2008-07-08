/*
 * $Id: FileLock.cpp,v 1.1 2008/07/08 16:23:27 philipn Exp $
 *
 * Manages an advisory file lock
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
 
#include <sys/file.h>
#include <errno.h>

#include "FileLock.h"
#include "RecorderException.h"
#include "Logging.h"


using namespace std;
using namespace rec;



bool FileLock::isLocked(string filename)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return false;
    }
    
    bool locked = false;
    while (true)
    {
        int result = flock(fd, LOCK_EX | LOCK_NB);
        if (result == 0)
        {
            break;
        }
        else if (errno != EINTR)
        {
            if (errno == EWOULDBLOCK)
            {
                locked = true;
            }
            break;
        }

        // try again if interrupted by a signal
    }
    
    close(fd);
    
    return locked;
}
 

FileLock::FileLock(string filename, bool createIfNotExists)
: _fd(0), _haveLock(false)
{
    int flags = O_RDONLY;
    if (createIfNotExists)
    {
        flags += O_CREAT;
    }
    
    _fd = open(filename.c_str(), flags);
    if (_fd < 0)
    {
        REC_LOGTHROW(("Failed to open file for locking: %s", strerror(errno)));
    }
}

FileLock::~FileLock()
{
    close(_fd);
}

bool FileLock::lock(bool block)
{
    if (_haveLock)
    {
        return true;
    }

    int operation = LOCK_EX;
    if (!block)
    {
        operation |= LOCK_NB;
    }
    
    while (true)
    {
        int result = flock(_fd, operation);
        if (result == 0)
        {
            _haveLock = true;
            break;
        }
        else if (errno != EINTR)
        {
            break;
        }

        // try again if interrupted by a signal
    }
    
    return _haveLock;
}

bool FileLock::haveLock()
{
    return _haveLock;
}

void FileLock::unlock()
{
    if (!_haveLock)
    {
        return;
    }

    flock(_fd, LOCK_UN);
    
    _haveLock = false;
}


