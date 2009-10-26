/*
 * $Id: Threads.cpp,v 1.2 2009/10/26 09:33:20 john_f Exp $
 *
 * Provides classes to manage mutexes, RW locks and threads
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#include "Threads.h"
#include "IngexException.h"
#include "Logging.h"

#include <cstring>

using namespace std;
using namespace ingex;



Mutex::Mutex()
{
    if (pthread_mutex_init(&_pthreadMutex, NULL) != 0)
    {
        INGEX_LOGTHROW(("Failed to initialise mutex"));
    }
}

Mutex::~Mutex()
{
    if (pthread_mutex_destroy(&_pthreadMutex) != 0)
    {
        INGEX_LOG(error, ("Failed to destroy mutex in Mutex destructor\n"));
    }
}

void Mutex::lock()
{
    if (pthread_mutex_lock(&_pthreadMutex) != 0)
    {
        INGEX_LOGTHROW(("Failed to get a mutex lock"));
    }
}

bool Mutex::tryLock()
{
    return pthread_mutex_trylock(&_pthreadMutex) == 0;
}

void Mutex::unlock()
{
    if (pthread_mutex_unlock(&_pthreadMutex) != 0)
    {
        INGEX_LOGTHROW(("Failed to unlock mutex"));
    }
}

MutexLocker::MutexLocker(Mutex& mutex)
: _mutex(mutex)
{
    mutex.lock();
}

MutexLocker::~MutexLocker()
{
    _mutex.unlock();
}


TryMutexLocker::TryMutexLocker(Mutex& mutex)
: _mutex(mutex), _haveLock(false)
{
    _haveLock = mutex.tryLock();
}

TryMutexLocker::~TryMutexLocker()
{
    if (_haveLock)
    {
        _mutex.unlock();
    }
}

bool TryMutexLocker::haveLock()
{
    return _haveLock;
}


ReadWriteLock::ReadWriteLock()
{
    if (pthread_rwlock_init(&_pthreadReadWriteLock, NULL) != 0)
    {
        INGEX_LOGTHROW(("Failed to initialise read-write lock"));
    }
}

ReadWriteLock::~ReadWriteLock()
{
    if (pthread_rwlock_destroy(&_pthreadReadWriteLock) != 0)
    {
        INGEX_LOG(error, ("Failed to destroy mutex in Mutex destructor\n"));
    }
}

void ReadWriteLock::readLock()
{
    if (pthread_rwlock_rdlock(&_pthreadReadWriteLock) != 0)
    {
        INGEX_LOGTHROW(("Failed to get a read lock"));
    }
}

void ReadWriteLock::writeLock()
{
    if (pthread_rwlock_wrlock(&_pthreadReadWriteLock) != 0)
    {
        INGEX_LOGTHROW(("Failed to get a write lock"));
    }
}

void ReadWriteLock::unlock()
{
    if (pthread_rwlock_unlock(&_pthreadReadWriteLock) != 0)
    {
        INGEX_LOGTHROW(("Failed to unlock read-write lock"));
    }
}

ReadLocker::ReadLocker(ReadWriteLock& rwLock)
: _rwLock(rwLock)
{
    rwLock.readLock();
}

ReadLocker::~ReadLocker()
{
    _rwLock.unlock();
}

WriteLocker::WriteLocker(ReadWriteLock& rwLock)
: _rwLock(rwLock)
{
    rwLock.writeLock();
}

WriteLocker::~WriteLocker()
{
    _rwLock.unlock();
}


typedef struct
{
    ThreadWorker* worker;
} ThreadArg;

void* thread_start_func(void* arg)
{
    ThreadWorker* worker = (ThreadWorker*)arg;

    worker->start();

    pthread_exit((void*) 0);
}

Thread::Thread(ThreadWorker* worker, bool joinable)
: _worker(worker), _joinable(joinable), _started(false), _thread(0)
{
}

Thread::~Thread()
{
    stop();
    delete _worker;
}

void Thread::start()
{
    if (!_started)
    {
        int result;
        pthread_t newThread;

        if (_joinable)
        {
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

            result = pthread_create(&newThread, &attr, thread_start_func, (void*)_worker);
            if (result == 0)
            {
                _thread = newThread;
            }
            else
            {
                INGEX_LOGTHROW(("Failed to create joinable thread: %s", strerror(result)));
            }

            pthread_attr_destroy(&attr);
        }
        else
        {
            result = pthread_create(&newThread, NULL, thread_start_func, (void*)_worker);
            if (result == 0)
            {
                _thread = newThread;
            }
            else
            {
                INGEX_LOGTHROW(("Failed to create thread: %s", strerror(result)));
            }
        }

        _started = true;
    }
}

// TODO: cancel non-joinable threads and timeout joins
void Thread::stop()
{
    if (_started)
    {
        if (_thread != 0)
        {
            _worker->stop();

            if (_joinable)
            {
                int result;
                void* status;
                if ((result = pthread_join(_thread, (void **)&status)) != 0)
                {
                    INGEX_LOG(warning, ("Failed to join thread: %s", strerror(result)));
                }
            }

            _thread = 0;
        }

        _started = false;
    }
}

bool Thread::isRunning() const
{
    return _started && !_worker->hasStopped();
}

ThreadWorker* Thread::getWorker()
{
    return _worker;
}
