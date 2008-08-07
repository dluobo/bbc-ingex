/*
 * $Id: ClockTime.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to get time/date from PC clock.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "ClockTime.h"

#include <time.h>
#include <sys/timeb.h>
#include <string>

// static
const char * ClockTime::Timecode()
{
    static char buf[32];

    _timeb tb;
    _ftime(&tb);
    tm * ptm = localtime(&tb.time);
    sprintf(buf,"%02d:%02d:%02d:%02d",
        ptm->tm_hour,
        ptm->tm_min,
        ptm->tm_sec,
        tb.millitm / 40);

    return buf;
}

// static
const char * ClockTime::Date()
{
    static char buf[16];

    _timeb tb;
    _ftime(&tb);
    tm * ptm = localtime(&tb.time);
    sprintf(buf,"%02d.%02d.%04d",
        ptm->tm_mday,
        ptm->tm_mon + 1,
        ptm->tm_year + 1900
        );
    return buf;
}


