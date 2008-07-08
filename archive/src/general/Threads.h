/*
 * $Id: Threads.h,v 1.1 2008/07/08 16:23:39 philipn Exp $
 *
 * Provides classes to manage mutexes, RW locks and threads
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
 
#ifndef __RECORDER_THREADS_H__
#define __RECORDER_THREADS_H__


#include <pthread.h>


#define LOCK_SECTION(m)             rec::MutexLocker __lock(m);
#define LOCK_SECTION_2(m)           rec::MutexLocker __lock2(m);
#define LOCK_SECTION_3(m)           rec::MutexLocker __lock3(m);

#define TRY_LOCK_SECTION(m)         rec::TryMutexLocker __tryLock(m);
#define HAVE_SECTION_LOCK()         __tryLock.haveLock()

#define READ_LOCK_SECTION(rwl)      rec::ReadLocker __readLock(rwl);
#define WRITE_LOCK_SECTION(rwl)     rec::WriteLocker __writeLock(rwl);

#define GUARD_THREAD_START(hs)      rec::HasStoppedGuard __guard(hs)



namespace rec
{

class Mutex
{
public:
    Mutex();
    ~Mutex();
    
    void lock();
    bool tryLock();
    void unlock();
    
private:
    pthread_mutex_t _pthreadMutex;
};


class MutexLocker
{
public:
    MutexLocker(Mutex& mutex);
    ~MutexLocker();
    
private:
    Mutex& _mutex;
};


class TryMutexLocker
{
public:
    TryMutexLocker(Mutex& mutex);
    ~TryMutexLocker();
    
    bool haveLock();
    
private:
    Mutex& _mutex;
    bool _haveLock;
};



class ReadWriteLock
{
public:
    ReadWriteLock();
    ~ReadWriteLock();

    void readLock();
    void writeLock();
    void unlock();
    
private:
    pthread_rwlock_t _pthreadReadWriteLock;
};


class ReadLocker
{
public:
    ReadLocker(ReadWriteLock& rwLock);
    ~ReadLocker();
    
private:
    ReadWriteLock& _rwLock;
};

class WriteLocker
{
public:
    WriteLocker(ReadWriteLock& rwLock);
    ~WriteLocker();
    
private:
    ReadWriteLock& _rwLock;
};


class HasStoppedGuard
{
public:    
    HasStoppedGuard(bool& value)
    : _hasStopped(value)
    {
        _hasStopped = false;
    }
    ~HasStoppedGuard()
    {
        _hasStopped = true;
    }
    
private:
    bool& _hasStopped;
};

// Note / TODO: a thread can only be started once because there is a gap between
// initiating a thread start and it actually starting. The thread can be deleted in this
// gap and therefore setting _stop = false at the top of the start() function is wrong
//
// Need to add a reset() function to ThreadWorker to reset the _stop variable from the Thread::start() function ?
class ThreadWorker
{
public:
    virtual ~ThreadWorker() {}
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool hasStopped() const = 0;
};

class Thread
{
public:
    Thread(ThreadWorker* worker, bool joinable);
    virtual ~Thread();
    
    virtual void start();
    virtual void stop();
    virtual bool isRunning() const;
    virtual ThreadWorker* getWorker();
    
protected:
    ThreadWorker* _worker;
    bool _joinable;
    bool _started;
    pthread_t _thread;
};


};



#endif

