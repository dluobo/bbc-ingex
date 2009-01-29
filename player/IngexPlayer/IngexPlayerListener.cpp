/*
 * $Id: IngexPlayerListener.cpp,v 1.1 2009/01/29 07:17:08 stuart_hc Exp $
 *
 * Copyright (C) 2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Matthew Marks
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

#include "IngexPlayerListener.h"
#include <logging.h>

namespace prodauto
{

ReadWriteLockGuard::ReadWriteLockGuard(pthread_rwlock_t* rwlock, bool writeLock)
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

ReadWriteLockGuard::~ReadWriteLockGuard()
{
    if (_rwlock != NULL)
    {
        if (pthread_rwlock_unlock(_rwlock) != 0)
        {
            ml_log_error("pthread_rwlock_unlock failed\n");
        }
    }
}

IngexPlayerListenerRegistry::IngexPlayerListenerRegistry()
{
    pthread_rwlock_init(&_listenersRWLock, NULL);
}

IngexPlayerListenerRegistry::~IngexPlayerListenerRegistry()
{
    ReadWriteLockGuard guard(&_listenersRWLock, true);

    std::vector<IngexPlayerListener*>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        (*iter)->unsetRegistry();
    }
    _listeners.clear();
    pthread_rwlock_destroy(&_listenersRWLock);
}

bool IngexPlayerListenerRegistry::registerListener(IngexPlayerListener* listener)
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

bool IngexPlayerListenerRegistry::unregisterListener(IngexPlayerListener* listener)
{
    if (listener == 0 || listener->getRegistry() != this)
    {
        return true;
    }
try
    {
        ReadWriteLockGuard guard(&_listenersRWLock, true);
        std::vector<IngexPlayerListener*>::iterator iter;
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

IngexPlayerListener::IngexPlayerListener(IngexPlayerListenerRegistry* registry)
    : _registry(0)
{
    registry->registerListener(this);
}

IngexPlayerListener::~IngexPlayerListener() 
{
    if (_registry != 0)
    {
        _registry->unregisterListener(this);
    }
}

IngexPlayerListenerRegistry* IngexPlayerListener::getRegistry()
{
    return _registry;
}

void IngexPlayerListener::setRegistry(IngexPlayerListenerRegistry* registry)
{
    _registry = registry;
}

void IngexPlayerListener::unsetRegistry()
{
    _registry = 0;
}

}
