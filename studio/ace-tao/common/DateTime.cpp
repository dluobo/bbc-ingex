/*
 * $Id: DateTime.cpp,v 1.3 2008/10/08 10:16:06 john_f Exp $
 *
 * Date/time to text functions.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
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

#include "DateTime.h"

#include <ace/OS_NS_time.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/OS_NS_stdio.h>
#include <ace/Time_Value.h>
#include <ace/Log_Msg.h>


/*
Returns a pointer to a null-terminated string in format YYYYMMDD.
The caller has responsibility for deallocating the memory
by invoking delete [] on the returned pointer.
*/
std::string DateTime::DateNoSeparators()
{
    time_t my_time = ACE_OS::time();
    tm my_tm;
    ACE_OS::localtime_r(&my_time, &my_tm);

    char s[9];
    ACE_OS::sprintf(s, "%04d%02d%02d",
        my_tm.tm_year + 1900,
        my_tm.tm_mon + 1,
        my_tm.tm_mday
        );

    return std::string(s);
}

/*
Returns a string in format YYYYMMDDHHMM.
*/
std::string DateTime::DateTimeNoSeparators()
{
    time_t my_time = ACE_OS::time();
    tm my_tm;
    ACE_OS::localtime_r(&my_time, &my_tm);

    char s[13];
    ACE_OS::sprintf(s, "%04d%02d%02d%02d%02d",
        my_tm.tm_year + 1900,
        my_tm.tm_mon + 1,
        my_tm.tm_mday,
        my_tm.tm_hour,
        my_tm.tm_min
        );

    return std::string(s);
}

/*
Returns a string in format HH:MM:SS.sss
*/
std::string DateTime::Timecode()
{
    ACE_Time_Value t = ACE_OS::gettimeofday();
    time_t t_secs = t.sec();
	suseconds_t t_us = t.usec();
    tm my_tm;
    tm * ptm = ACE_OS::localtime_r(&t_secs, &my_tm);

    char s [13];
    ACE_OS::sprintf(s,"%02d:%02d:%02d.%03d",
		ptm->tm_hour,
		ptm->tm_min,
		ptm->tm_sec,
		t_us / 1000 );

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("DateTime::Timecode() returning %C\n"), s));
    return std::string(s);
}

/*
Returns a string in format HH:MM:SS.ff (25 fps)
*/
std::string DateTime::Timecode25()
{
    ACE_Time_Value t = ACE_OS::gettimeofday();
    time_t t_secs = t.sec();
	suseconds_t t_us = t.usec();
    tm my_tm;
    tm * ptm = ACE_OS::localtime_r(&t_secs, &my_tm);

    char s [13];
    ACE_OS::sprintf(s,"%02d:%02d:%02d.%02d",
		ptm->tm_hour,
		ptm->tm_min,
		ptm->tm_sec,
		t_us / 40000 );

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("DateTime::Timecode25() returning %C\n"), s));
    return std::string(s);
}

void DateTime::GetDate(int & year, int & month, int & day)
{
    time_t my_time = ACE_OS::time();
    tm my_tm;
    ACE_OS::localtime_r(&my_time, &my_tm);

    year = my_tm.tm_year + 1900;
    month = my_tm.tm_mon + 1;
    day = my_tm.tm_mday;
}


