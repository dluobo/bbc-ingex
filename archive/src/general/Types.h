/*
 * $Id: Types.h,v 1.1 2008/07/08 16:23:45 philipn Exp $
 *
 * Common types
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
 
#ifndef __RECORDER_TYPES_H__
#define __RECORDER_TYPES_H__


#include <inttypes.h>

#include <string>


namespace rec
{

class Date
{
public:
    Date(int year_, int month_, int day_) : year(year_), month(month_), day(day_) {}
    Date() : year(0), month(0), day(0) {}
    ~Date() {}


    bool operator == (const Date& r) const
    {
        return year == r.year &&
            month == r.month &&
            day == r.day;
    }
    
    bool operator != (const Date& r) const
    {
        return year != r.year ||
            month != r.month ||
            day != r.day;
    }
    
    bool operator < (const Date& r) const
    {
        return year < r.year ||
            (year == r.year && month < r.month) ||
            (year == r.year && month == r.month && day < r.day);
    }
    
    bool operator > (const Date& r) const
    {
        return year > r.year ||
            (year == r.year && month > r.month) ||
            (year == r.year && month == r.month && day > r.day);
    }
    
    std::string toString() const
    {
        char buf[48];
        
        sprintf(buf, "%04d-%02d-%02d", year, month, day);
        
        return buf;
    }

    
    int year;
    int month;
    int day;
};

static const Date g_nullDate = Date();


class Timestamp
{
public:
    Timestamp(int year_, int month_, int day_, int hour_, int min_, int sec_, int msec_) 
        : year(year_), month(month_), day(day_), hour(hour_), min(min_), sec(sec_), msec(msec_) {}
    Timestamp() : year(0), month(0), day(0), hour(0), min(0), sec(0), msec(0) {}
    ~Timestamp() {}
    
    
    bool operator == (const Timestamp& r) const
    {
        return year == r.year &&
            month == r.month &&
            day == r.day &&
            hour == r.hour &&
            min == r.min &&
            sec == r.sec &&
            msec == r.msec;
    }
    
    bool operator != (const Timestamp& r) const
    {
        return year != r.year ||
            month != r.month ||
            day != r.day ||
            hour != r.hour ||
            min != r.min ||
            sec != r.sec ||
            msec != r.msec;
    }
    
    bool operator < (const Timestamp& r) const
    {
        return year < r.year ||
            (year == r.year && month < r.month) ||
            (year == r.year && month == r.month && day < r.day) ||
            (year == r.year && month == r.month && day == r.day && 
                hour < r.hour) ||
            (year == r.year && month == r.month && day == r.day && 
                hour == r.hour && min < r.min) ||
            (year == r.year && month == r.month && day == r.day && 
                hour == r.hour && min == r.min && sec < r.sec) ||
            (year == r.year && month == r.month && day == r.day && 
                hour == r.hour && min == r.min && sec == r.sec && msec < r.msec);
    }

    bool operator > (const Timestamp& r) const
    {
        return year > r.year ||
            (year == r.year && month > r.month) ||
            (year == r.year && month == r.month && day > r.day) ||
            (year == r.year && month == r.month && day == r.day && 
                hour > r.hour) ||
            (year == r.year && month == r.month && day == r.day && 
                hour == r.hour && min > r.min) ||
            (year == r.year && month == r.month && day == r.day && 
                hour == r.hour && min == r.min && sec > r.sec) ||
            (year == r.year && month == r.month && day == r.day && 
                hour == r.hour && min == r.min && sec == r.sec && msec > r.msec);
    }

    std::string toString() const
    {
        char buf[128];
        
        sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d.%03d", 
            year, month, day, hour, min, sec, msec);
        
        return buf;
    }
    
    
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int msec;
};

static const Timestamp g_nullTimestamp = Timestamp();


class Timecode
{
public:
    Timecode(int hour_, int min_, int sec_, int frame_) : hour(hour_), min(min_), sec(sec_), frame(frame_) {}
    Timecode() : hour(0), min(0), sec(0), frame(0) {}
    ~Timecode() {}


    bool operator == (const Timecode& r) const
    {
        return hour == r.hour &&
            min == r.min &&
            sec == r.sec &&
            frame == r.frame;
    }
    
    bool operator != (const Timecode& r) const
    {
        return hour != r.hour ||
            min != r.min ||
            sec != r.sec ||
            frame != r.frame;
    }
    
    bool operator < (const Timecode& r) const
    {
        return hour < r.hour ||
            (hour == r.hour && min < r.min) ||
            (hour == r.hour && min == r.min && sec < r.sec) ||
            (hour == r.hour && min == r.min && sec == r.sec && frame < r.frame);
    }
    
    bool operator > (const Timecode& r) const
    {
        return hour > r.hour ||
            (hour == r.hour && min > r.min) ||
            (hour == r.hour && min == r.min && sec > r.sec) ||
            (hour == r.hour && min == r.min && sec == r.sec && frame > r.frame);
    }
    
    std::string toString() const
    {
        char buf[64];
        
        sprintf(buf, "%02d:%02d:%02d:%02d", hour, min, sec, frame);
        
        return buf;
    }

    
    int hour;
    int min;
    int sec;
    int frame;
};

static const Timecode g_nullTimecode = Timecode();


class UMID
{
public:
    UMID()
    {
        memset(bytes, 0, sizeof(bytes));
    }
    UMID(std::string umidStr)
    {
        fromString(umidStr);
    }
    UMID(const UMID& r)
    {
        memcpy(bytes, r.bytes, sizeof(bytes));
    }
    ~UMID() {}

    
    bool operator == (const UMID& r) const
    {
        return (memcmp(bytes, r.bytes, sizeof(bytes)) == 0);
    }
    
    bool operator != (const UMID& r) const
    {
        return (memcmp(bytes, r.bytes, sizeof(bytes)) != 0);
    }
    
    bool operator < (const UMID& r) const
    {
        return (memcmp(bytes, r.bytes, sizeof(bytes)) < 0);
    }
    
    bool operator > (const UMID& r) const
    {
        return (memcmp(bytes, r.bytes, sizeof(bytes)) > 0);
    }
    
    std::string toString() const
    {
        static const char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
            
        char buf[65];
        int i;
        for (i = 0; i < 32; i++)
        {
            buf[i * 2] = hex[(bytes[i] >> 4) & 0x0f];
            buf[i * 2 + 1] = hex[bytes[i] & 0x0f];
        }
        buf[64] = '\0';
        
        return buf;
    }
    
    void fromString(std::string umidStr)
    {
        if (umidStr.size() != 64)
        {
            // invalid umid string
            memset(bytes, 0, sizeof(bytes));
            return;
        }

        int i;
        for (i = 0; i < 32; i++)
        {
            if (umidStr[i * 2] >= '0' && umidStr[i * 2] <= '9')
            {
                bytes[i] = (umidStr[i * 2] - '0') << 8;
            }
            else if (umidStr[i * 2] >= 'a' && umidStr[i * 2] <= 'f')
            {
                bytes[i] = (10 + umidStr[i * 2] - 'a') << 8;
            }
            else if (umidStr[i * 2] >= 'A' && umidStr[i * 2] <= 'F')
            {
                bytes[i] = (10 + umidStr[i * 2] - 'A') << 8;
            }
            else
            {
                // invalid umid string
                memset(bytes, 0, sizeof(bytes));
                return;
            }

            if (umidStr[i * 2 + 1] >= '0' && umidStr[i * 2 + 1] <= '9')
            {
                bytes[i] |= (umidStr[i * 2 + 1] - '0');
            }
            else if (umidStr[i * 2 + 1] >= 'a' && umidStr[i * 2 + 1] <= 'f')
            {
                bytes[i] |= (10 + umidStr[i * 2 + 1] - 'a');
            }
            else if (umidStr[i * 2 + 1] >= 'A' && umidStr[i * 2 + 1] <= 'F')
            {
                bytes[i] |= (10 + umidStr[i * 2 + 1] - 'A');
            }
            else
            {
                // invalid umid string
                memset(bytes, 0, sizeof(bytes));
                return;
            }
        }
    }
    
    bool isValidUMIDString(std::string umidStr)
    {
        if (umidStr.size() != 64)
        {
            return false;
        }

        int i;
        for (i = 0; i < 32; i++)
        {
            if (!((umidStr[i * 2] >= '0' && umidStr[i * 2] <= '9') ||
                (umidStr[i * 2] >= 'a' && umidStr[i * 2] <= 'f') ||
                (umidStr[i * 2] >= 'A' && umidStr[i * 2] <= 'F')))
            {
                return false;
            }
            if (!((umidStr[i * 2 + 1] >= '0' && umidStr[i * 2 + 1] <= '9') ||
                (umidStr[i * 2 + 1] >= 'a' && umidStr[i * 2 + 1] <= 'f') ||
                (umidStr[i * 2 + 1] >= 'A' && umidStr[i * 2 + 1] <= 'F')))
            {
                return false;
            }
        }
        
        return true;
    }

    
    unsigned char bytes[32];
};

static const UMID g_nullUMID = UMID();


};



#endif

