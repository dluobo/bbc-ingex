/*
 * $Id: Timing.cpp,v 1.2 2009/10/12 16:09:18 philipn Exp $
 *
 * Provides sleep functions and timer using a monotonic system clock
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
 
 
#include <errno.h>
 
#include "Timing.h"
#include "RecorderException.h"


using namespace std;
using namespace rec;


void rec::sleep_sec(int64_t sec)
{
    struct timespec rem;
    struct timespec tm = {sec, 0};
    
    int result;
    while (true)
    {
        result = clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, &rem);
        if (result != 0 && errno == EINTR)
        {
            // sleep was interrupted by a signal - sleep for the remaining time
            tm = rem;
            continue;
        }
        
        break;
    }
}

void rec::sleep_msec(int64_t msec)
{
    struct timespec rem;
    struct timespec tm = {msec / 1000, (msec % 1000) * 1000000};
    
    int result;
    while (true)
    {
        result = clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, &rem);
        if (result != 0 && errno == EINTR)
        {
            // sleep was interrupted by a signal - sleep for the remaining time
            tm = rem;
            continue;
        }
        
        break;
    }
}

void rec::sleep_usec(int64_t usec)
{
    struct timespec rem;
    struct timespec tm = {usec / 1000000, (usec % 1000000) * 1000};
    
    int result;
    while (true)
    {
        result = clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, &rem);
        if (result != 0 && errno == EINTR)
        {
            // sleep was interrupted by a signal - sleep for the remaining time
            tm = rem;
            continue;
        }
        
        break;
    }
}




Timer::Timer()
{
    reset();
}

Timer::~Timer()
{
}

void Timer::start(int64_t usec)
{
    clock_gettime(CLOCK_MONOTONIC, &_start);
    _duration = usec;
}

int64_t Timer::timeLeft()
{
    if (_duration <= 0)
    {
        // not started so no time left
        return 0;
    }
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int64_t remainder = _duration - (SEC_IN_USEC * (now.tv_sec - _start.tv_sec) + (int64_t)(now.tv_nsec - _start.tv_nsec) / 1000);
    
    return (remainder < 0) ? 0 : remainder;
}

void Timer::sleepRemainder()
{
    if (_duration <= 0)
    {
        return;
    }
    
    struct timespec target;
    target.tv_sec = _start.tv_sec + _duration / SEC_IN_USEC + 
        (_start.tv_nsec / 1000 + (_duration % SEC_IN_USEC)) / SEC_IN_USEC;
    target.tv_nsec = ((_start.tv_nsec / 1000 + (_duration % SEC_IN_USEC)) % SEC_IN_USEC) * 1000;
    
    
    int result;
    while (true)
    {
        result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &target, 0);
        if (result != 0 && errno == EINTR)
        {
            // sleep was interrupted by a signal - sleep for the remaining time
            continue;
        }
        
        break;
    }
}

void Timer::start()
{
    clock_gettime(CLOCK_MONOTONIC, &_start);
    _duration = 0;
}

int64_t Timer::elapsedTime()
{
    if (_start.tv_sec == 0 && _start.tv_nsec == 0)
    {
        // not started so no time elapsed
        return 0;
    }
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    return SEC_IN_USEC * (now.tv_sec - _start.tv_sec) + (int64_t)(now.tv_nsec - _start.tv_nsec) / 1000;
}

bool Timer::isRunning()
{
    return _start.tv_sec != 0 || _start.tv_nsec != 0;
}

void Timer::reset()
{
    _start.tv_sec = 0;
    _start.tv_nsec = 0;
    _duration = 0;
}


