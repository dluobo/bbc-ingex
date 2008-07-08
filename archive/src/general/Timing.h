/*
 * $Id: Timing.h,v 1.1 2008/07/08 16:23:44 philipn Exp $
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
 
#ifndef __RECORDER_TIMING_H__
#define __RECORDER_TIMING_H__



#include <time.h>
#include "Types.h"


#define SEC_IN_USEC     ((int64_t)(1000 * 1000))
#define MSEC_IN_USEC    ((int64_t)(1000))


namespace rec
{


void sleep_sec(int64_t sec);
void sleep_msec(int64_t msec);
void sleep_usec(int64_t usec);

    
class Timer
{
public:
    Timer();
    ~Timer();

    void start(int64_t usec);
    int64_t timeLeft();
    void sleepRemainder();

    void start();
    int64_t elapsedTime();
    bool isRunning();

    void reset();
    
private:
    struct timespec _start;
    int64_t _duration;
};




};







#endif


