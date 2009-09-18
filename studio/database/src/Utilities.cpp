/*
 * $Id: Utilities.cpp,v 1.2 2009/09/18 16:50:11 philipn Exp $
 *
 * General utilities
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <cstdlib>
#include <cassert>
#include <ctime>

#if defined(_WIN32)

#include <time.h>
#include <windows.h>

#else

#include <uuid/uuid.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>

#endif


#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;


// MobID generation code following the same algorithm as implemented in the AAF SDK
UMID prodauto::generateUMID()
{
    UMID umid;
    uint32_t major, minor;
    static uint32_t last_part2 = 0;
    static Mutex m;

    major = (uint32_t)time(NULL);
#if defined(_WIN32)
    minor = (uint32_t)GetTickCount();
#else
    struct tms tms_buf;
    minor = (uint32_t)times(&tms_buf);
    assert(minor != 0 && minor != (uint32_t)-1);
#endif

    {
        // "time" function returns the time in seconds since the epoch and "times" function returns the 
        // number of clock ticks since some arbitrary time instant. 
        // A clock tick was found to be 1/100 th of a second. UMIDs are generated at a greater rate by 
        // multiple threads and therefore a mutex is required areound the counter that ensures that the 
        // UMID minor value is unique
        LOCK_SECTION(m);
        
        if (last_part2 >= minor)
        {
            minor = last_part2 + 1;
        }
            
        last_part2 = minor;
    }

    umid.octet0 = 0x06;
    umid.octet1 = 0x0a;
    umid.octet2 = 0x2b;
    umid.octet3 = 0x34;
    umid.octet4 = 0x01;
    umid.octet5 = 0x01;
    umid.octet6 = 0x01;
    umid.octet7 = 0x01;
    umid.octet8 = 0x01;
    umid.octet9 = 0x01;
    umid.octet10 = 0x0f; // material type not identified
    umid.octet11 = 0x00; // no method specified for material and instance number generation
    umid.octet12 = 0x13;
    umid.octet13 = 0x00;
    umid.octet14 = 0x00;
    umid.octet15 = 0x00;

    umid.octet24 = 0x06;
    umid.octet25 = 0x0e;
    umid.octet26 = 0x2b;
    umid.octet27 = 0x34;
    umid.octet28 = 0x7f;
    umid.octet29 = 0x7f;
    umid.octet30 = (uint8_t)(42 & 0x7f); // company specific prefix = 42
    umid.octet31 = (uint8_t)((42 >> 7L) | 0x80);  //* company specific prefix = 42
    
    umid.octet16 = (uint8_t)((major >> 24) & 0xff);
    umid.octet17 = (uint8_t)((major >> 16) & 0xff);
    umid.octet18 = (uint8_t)((major >> 8) & 0xff);
    umid.octet19 = (uint8_t)(major & 0xff);
    
    umid.octet20 = (uint8_t)(((uint16_t)(minor & 0xFFFF) >> 8) & 0xff);
    umid.octet21 = (uint8_t)((uint16_t)(minor & 0xFFFF) & 0xff);
    
    umid.octet22 = (uint8_t)(((uint16_t)((minor >> 16L) & 0xFFFF) >> 8) & 0xff);
    umid.octet23 = (uint8_t)((uint16_t)((minor >> 16L) & 0xFFFF) & 0xff);

    return umid;
}

// Generates UMIDs that are supported by older Avid products, eg. Protools v5.3.1 on a Mac with OS9
UMID prodauto::generateLegacyUMID()
{
    UMID umid;
    uint32_t major, minor;
    static uint32_t last_part2 = 0;
    static Mutex m;
    
    major = (uint32_t)time(NULL);
#if defined(_WIN32)
    minor = (uint32_t)GetTickCount();
#else
    struct tms tms_buf;
    minor = (uint32_t)times(&tms_buf);
    assert(minor != 0 && minor != (uint32_t)-1);
#endif

    {
        // "time" function returns the time in seconds since the epoch and "times" function returns the 
        // number of clock ticks since some arbitrary time instant. 
        // A clock tick was found to be 1/100 th of a second. UMIDs are generated at a greater rate by 
        // multiple threads and therefore a mutex is required areound the counter that ensures that the 
        // UMID minor value is unique
        LOCK_SECTION(m);
        
        if (last_part2 >= minor)
        {
            minor = last_part2 + 1;
        }
            
        last_part2 = minor;
    }

    umid.octet0 = 0x06;
    umid.octet1 = 0x0c;
    umid.octet2 = 0x2b;
    umid.octet3 = 0x34;
    umid.octet4 = 0x02;         
    umid.octet5 = 0x05;     
    umid.octet6 = 0x11; 
    umid.octet7 = 0x01;
    umid.octet8 = 0x01;
    umid.octet9 = 0x04;  // UMIDType
    umid.octet10 = 0x10;
    umid.octet11 = 0x00; // no method specified for material and instance number generation
    umid.octet12 = 0x13;
    umid.octet13 = 0x00;
    umid.octet14 = 0x00;
    umid.octet15 = 0x00;

    umid.octet24 = 0x06;
    umid.octet25 = 0x0e;
    umid.octet26 = 0x2b;
    umid.octet27 = 0x34;
    umid.octet28 = 0x7f;
    umid.octet29 = 0x7f;
    umid.octet30 = (uint8_t)(42 & 0x7f); // company specific prefix = 42
    umid.octet31 = (uint8_t)((42 >> 7L) | 0x80);  // company specific prefix = 42
    
    umid.octet16 = (uint8_t)((major >> 24) & 0xff);
    umid.octet17 = (uint8_t)((major >> 16) & 0xff);
    umid.octet18 = (uint8_t)((major >> 8) & 0xff);
    umid.octet19 = (uint8_t)(major & 0xff);
    
    umid.octet20 = (uint8_t)(((uint16_t)(minor & 0xFFFF) >> 8) & 0xff);
    umid.octet21 = (uint8_t)((uint16_t)(minor & 0xFFFF) & 0xff);
    
    umid.octet22 = (uint8_t)(((uint16_t)((minor >> 16L) & 0xFFFF) >> 8) & 0xff);
    umid.octet23 = (uint8_t)((uint16_t)((minor >> 16L) & 0xFFFF) & 0xff);

    return umid;
}

UMID prodauto::getUMID(string umidStr)
{
    int bytes[32];
    int result;
    UMID umid;
    
    result = sscanf(umidStr.c_str(), 
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5], &bytes[6], &bytes[7], 
        &bytes[8], &bytes[9], &bytes[10], &bytes[11], &bytes[12], &bytes[13], &bytes[14], &bytes[15], 
        &bytes[16], &bytes[17], &bytes[18], &bytes[19], &bytes[20], &bytes[21], &bytes[22], &bytes[23], 
        &bytes[24], &bytes[25], &bytes[26], &bytes[27], &bytes[28], &bytes[29], &bytes[30], &bytes[31]);
        
    if (result != 32)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to parse UMID string from database: %s\n", umidStr.c_str()));
    }
    
    umid.octet0 = bytes[0] & 0xff;
    umid.octet1 = bytes[1] & 0xff;
    umid.octet2 = bytes[2] & 0xff;
    umid.octet3 = bytes[3] & 0xff;
    umid.octet4 = bytes[4] & 0xff;
    umid.octet5 = bytes[5] & 0xff;
    umid.octet6 = bytes[6] & 0xff;
    umid.octet7 = bytes[7] & 0xff;
    umid.octet8 = bytes[8] & 0xff;
    umid.octet9 = bytes[9] & 0xff;
    umid.octet10 = bytes[10] & 0xff;
    umid.octet11 = bytes[11] & 0xff;
    umid.octet12 = bytes[12] & 0xff;
    umid.octet13 = bytes[13] & 0xff;
    umid.octet14 = bytes[14] & 0xff;
    umid.octet15 = bytes[15] & 0xff;
    umid.octet16 = bytes[16] & 0xff;
    umid.octet17 = bytes[17] & 0xff;
    umid.octet18 = bytes[18] & 0xff;
    umid.octet19 = bytes[19] & 0xff;
    umid.octet20 = bytes[20] & 0xff;
    umid.octet21 = bytes[21] & 0xff;
    umid.octet22 = bytes[22] & 0xff;
    umid.octet23 = bytes[23] & 0xff;
    umid.octet24 = bytes[24] & 0xff;
    umid.octet25 = bytes[25] & 0xff;
    umid.octet26 = bytes[26] & 0xff;
    umid.octet27 = bytes[27] & 0xff;
    umid.octet28 = bytes[28] & 0xff;
    umid.octet29 = bytes[29] & 0xff;
    umid.octet30 = bytes[30] & 0xff;
    umid.octet31 = bytes[31] & 0xff;
    
    return umid;
}

string prodauto::getUMIDString(UMID umid)
{
    char umidStr[65];
    
    sprintf(umidStr,
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x"
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        umid.octet0, umid.octet1, umid.octet2, umid.octet3,
        umid.octet4, umid.octet5, umid.octet6, umid.octet7,
        umid.octet8, umid.octet9, umid.octet10, umid.octet11,
        umid.octet12, umid.octet13, umid.octet14, umid.octet15,
        umid.octet16, umid.octet17, umid.octet18, umid.octet19,
        umid.octet20, umid.octet21, umid.octet22, umid.octet23,
        umid.octet24, umid.octet25, umid.octet26, umid.octet27,
        umid.octet28, umid.octet29, umid.octet30, umid.octet31);
        
   return umidStr;
}


Timestamp prodauto::generateTimestampNow()
{
    Timestamp now;
    time_t t = time(NULL);
    struct tm gmt;
    memset(&gmt, 0, sizeof(struct tm));
#if defined(_MSC_VER)
    // TODO: need thread-safe (reentrant) version
    const struct tm* gmtPtr = gmtime(&t);
    if (gmtPtr == NULL)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to generate timestamp now\n"));
    }
    gmt = *gmtPtr;
#else
    if (gmtime_r(&t, &gmt) == NULL)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to generate timestamp now\n"));
    }
#endif
    
    now.year = gmt.tm_year + 1900;
    now.month = gmt.tm_mon + 1;
    now.day = gmt.tm_mday;
    now.hour = gmt.tm_hour;
    now.min = gmt.tm_min;
    now.sec = gmt.tm_sec;
    now.qmsec = 0;
    
    return now;
}

Timestamp prodauto::generateTimestampStartToday()
{
    Timestamp startToday = generateTimestampNow();
    
    startToday.hour = 0;
    startToday.min = 0;
    startToday.sec = 0;
    startToday.qmsec = 0;
    
    return startToday;
}

Timestamp prodauto::generateTimestampStartTomorrow()
{
    Timestamp startTomorrow;
    time_t t = time(NULL);
    t += 60 * 60 * 24; // add 24 hours to time
    
    struct tm gmt;
    memset(&gmt, 0, sizeof(struct tm));
#if defined(_MSC_VER)
    // TODO: need thread-safe (reentrant) version
    const struct tm* gmtPtr = gmtime(&t);
    if (gmtPtr == NULL)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to generate timestamp for start of tommorrow\n"));
    }
    gmt = *gmtPtr;
#else
    if (gmtime_r(&t, &gmt) == NULL)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to generate timestamp for start of tommorrow\n"));
    }
#endif
    
    startTomorrow.year = gmt.tm_year + 1900;
    startTomorrow.month = gmt.tm_mon + 1;
    startTomorrow.day = gmt.tm_mday;
    startTomorrow.hour = 0;
    startTomorrow.min = 0;
    startTomorrow.sec = 0;
    startTomorrow.qmsec = 0;
    
    return startTomorrow;
}

// Note: assumption is that ODBC Timestamp processing is bypassed and the value
// is loaded as a string type
Timestamp prodauto::getTimestampFromODBC(string timestampFromODBC)
{
    Timestamp timestamp;
    int data[5];
    float dataFloat;
    
    int result;
    if ((result = sscanf(timestampFromODBC.c_str(), "%d-%d-%d %d:%d:%f",
        &data[0], &data[1], &data[2], &data[3], &data[4], &dataFloat)) < 6)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to parse database timestamp\n"));
    }
    
    timestamp.year = data[0];
    timestamp.month = data[1];
    timestamp.day = data[2];
    timestamp.hour = data[3];
    timestamp.min = data[4];
    timestamp.sec = (uint8_t)dataFloat;
    timestamp.qmsec = (uint8_t)(((long)(dataFloat * 1000.0) - (long)(dataFloat) * 1000) / 4.0 + 0.5);

    return timestamp;
}

string prodauto::getTimestampString(Timestamp timestamp)
{
    char timestampStr[32];
    
#if defined(_MSC_VER)    
    _snprintf(
#else
    snprintf(
#endif
        timestampStr, 32, "%04d-%02u-%02u %02u:%02u:%02u.%03u",
        timestamp.year, timestamp.month, timestamp.day, 
        timestamp.hour, timestamp.min, timestamp.sec, timestamp.qmsec * 4);
        
    return timestampStr;
}



Date prodauto::generateDateNow()
{
    Timestamp tNow = generateTimestampNow();
    Date dNow;
    dNow.year = tNow.year;
    dNow.month = tNow.month;
    dNow.day = tNow.day;

    return dNow;
}

string prodauto::getDateString(Date date)
{
    char dateStr[15];
    
#if defined(_MSC_VER)    
    _snprintf(
#else
    snprintf(
#endif
        dateStr, 15, "%04d-%02u-%02u", date.year, date.month, date.day);
        
    return dateStr;
}

string prodauto::getIntervalString(Interval interval)
{
    string interval_str;
    char valStr[7];
    
    if (interval.year > 0)
    {
        sprintf(valStr, " %d", interval.year);
        interval_str.append(valStr).append(" year");
    }
    if (interval.month > 0)
    {
        sprintf(valStr, " %u", interval.month);
        interval_str.append(valStr).append(" month");
    }
    if (interval.day > 0)
    {
        sprintf(valStr, " %u", interval.day);
        interval_str.append(valStr).append(" day");
    }
    if (interval.hour > 0)
    {
        sprintf(valStr, " %u", interval.hour);
        interval_str.append(valStr).append(" hour");
    }
    if (interval.min > 0)
    {
        sprintf(valStr, " %u", interval.min);
        interval_str.append(valStr).append(" minute");
    }
    if (interval.sec > 0)
    {
        sprintf(valStr, " %u", interval.sec);
        interval_str.append(valStr).append(" second");
    }
    
    if (interval_str.empty())
        interval_str.append(" 0 second");
    
    return interval_str;
}

vector<string> prodauto::getScriptReferences(string refsString)
{
    vector<string> refs;
    int state = 0;
    const char* strPtr = refsString.c_str();
    bool done = false;
    bool quoted = false;
    string ref;
    bool escape = false;
    
    
    // format is {ref1,"ref 2",ref3}
    // asssumed that when " and } characters are used the reference string is quoted
    
    while (!done && *strPtr != '\0')
    {
        switch (state)
        {
            // start, expecting '{'
            case 0:
                if (*strPtr != '{')
                {
                    PA_LOGTHROW(ProdAutoException, ("Invalid script references string %s", refsString.c_str()));
                }
                state = 1;
                strPtr++;
                break;
                
            // start of string, expecting '"', first character of string or '}'
            case 1:
                ref = "";
                if (*strPtr == '"')
                {
                    quoted = true;
                }
                else if (*strPtr == '}')
                {
                    done = true;
                }
                else
                {
                    ref.append(1, *strPtr);
                    quoted = false;
                }                    
                state = 2;
                strPtr++;
                break;
                
            // in string, where end is '"' if quoted or ',' or '}'
            // escape symbol is '\\'
            case 2:
                if (escape)
                {
                    ref.append(1, *strPtr);
                    escape = false;
                }
                else
                {
                    if (quoted)
                    {
                        if (*strPtr == '\\')
                        {
                            escape = true;
                        }
                        else if (*strPtr == '"')
                        {
                            refs.push_back(ref);
                            state = 3;
                        }
                        else
                        {
                            ref.append(1, *strPtr);
                        }
                    }
                    else
                    {
                        if (*strPtr == '\\')
                        {
                            escape = true;
                        }
                        else if (*strPtr == '}')
                        {
                            refs.push_back(ref);
                            done = true;
                        }
                        else if (*strPtr == ',')
                        {
                            refs.push_back(ref);
                            state = 1;
                        }
                        else
                        {
                            ref.append(1, *strPtr);
                        }
                    }
                }
                strPtr++;
                break;

            // next string ',' or end of array '}'                
            case 3:
                if (*strPtr == '}')
                {
                    done = true;
                }
                else if (*strPtr == ',')
                {
                    state = 1;
                }
                else
                {
                    PA_LOGTHROW(ProdAutoException, ("Invalid script references string %s", refsString.c_str()));
                }
                strPtr++;
                break;
                
        }
    }
    
    if (!done)
    {
        PA_LOGTHROW(ProdAutoException, ("Invalid script references string %s", refsString.c_str()));
    }
    
    return refs;
}

string prodauto::getScriptReferencesString(vector<string> refs)
{
    string refString = "{";
    
    vector<string>::const_iterator iter;
    for (iter = refs.begin(); iter != refs.end(); iter++)
    {
        string ref = *iter;
        
        if (iter != refs.begin())
        {
            refString.append(",");
        }
        refString.append("\"");
        
        // replaced occurrences of "\"" with "\""
        size_t index = 0;
        while ((index = ref.find("\"", index)) != string::npos)
        {
            ref.insert(index, "\\");
            index += 2;
        }
        // replaced occurrences of "}" with "\}"
        index = 0;
        while ((index = ref.find("}", index)) != string::npos)
        {
            ref.insert(index, "\\");
            index += 2;
        }
        
        refString.append(ref).append("\"");
    }
    
    refString.append("}");
    
    return refString;
}


string prodauto::getFilename(string filePath)
{
    size_t sepIndex;
    if ((sepIndex = filePath.rfind('/')) != string::npos)
    {
        return filePath.substr(sepIndex + 1);
    }
    
    return filePath;
}

