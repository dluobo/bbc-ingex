/*
 * $Id: Utilities.cpp,v 1.1 2008/07/08 16:23:31 philipn Exp $
 *
 * Common utility functions
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
 
#define __STDC_FORMAT_MACROS 1

#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#include "Utilities.h"
#include "Logging.h"


using namespace std;
using namespace rec;


string rec::join_path(string path1, string path2)
{
    string result;
    
    result = path1;
    if (path1.size() > 0 && path1.at(path1.size() - 1) != '/' && 
        (path2.size() == 0 || path2.at(0) != '/'))
    {
        result.append("/");
    }
    result.append(path2);
    
    return result;
}

string rec::join_path(string path1, string path2, string path3)
{
    string result;
    
    result = join_path(path1, path2);
    result = join_path(result, path3);

    return result;
}

string rec::strip_path(string filename)
{
    size_t sepIndex;
    if ((sepIndex = filename.rfind("/")) != string::npos)
    {
        return filename.substr(sepIndex + 1);
    }
    return filename;
}

string rec::strip_name(string filename)
{
    size_t sepIndex;
    if ((sepIndex = filename.rfind("/")) != string::npos)
    {
        return filename.substr(0, sepIndex);
    }
    return filename;
}



string rec::get_host_name()
{
    char buf[256];
    
    if (gethostname(buf, 256) != 0)
    {
        return "";
    }
    buf[255] = '\0';
    if (buf[0] == '\0')
    {
        return "";
    }
    
    return buf;
}

string rec::get_user_timestamp_string(Timestamp value)
{
    char buf[128];
    buf[0] = '\0';
    
    float secFloat = (float)value.sec + value.msec / 1000.0;
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02.6f", 
        value.year, value.month, value.day,
        value.hour, value.min, secFloat);
    
    return buf;
}

string rec::int_to_string(int value)
{
    char buf[32];
    sprintf(buf, "%d", value);
    return buf;
}

string rec::long_to_string(long value)
{
    char buf[32];
    sprintf(buf, "%ld", value);
    return buf;
}

string rec::int64_to_string(int64_t value)
{
    char buf[32];
    sprintf(buf, "%"PRIi64"", value);
    return buf;
}

Date rec::get_todays_date()
{
    Date today;
    
    time_t t = time(NULL);
    struct tm gmt;
    memset(&gmt, 0, sizeof(struct tm));
#if defined(_MSC_VER)
    // TODO: need thread-safe (reentrant) version
    const struct tm* gmtPtr = gmtime(&t);
    if (gmtPtr == NULL)
    {
        Logging::error("Failed to generate timestamp now\n");
        return today;
    }
    gmt = *gmtPtr;
#else
    if (gmtime_r(&t, &gmt) == NULL)
    {
        Logging::error("Failed to generate timestamp now\n");
        return today;
    }
#endif
    
    today.year = gmt.tm_year + 1900;
    today.month = gmt.tm_mon + 1;
    today.day = gmt.tm_mday;
    
    return today;
}

Timestamp rec::get_localtime_now()
{
    Timestamp now;
    
    time_t t = time(NULL);
    struct tm lt;
    memset(&lt, 0, sizeof(struct tm));
#if defined(_MSC_VER)
    // TODO: need thread-safe (reentrant) version
    const struct tm* ltPtr = localtime(&t);
    if (ltPtr == NULL)
    {
        Logging::error("Failed to generate timestamp now\n");
        return now;
    }
    lt = *ltPtr;
#else
    if (localtime_r(&t, &lt) == NULL)
    {
        Logging::error("Failed to generate timestamp now\n");
        return now;
    }
#endif
    
    now.year = lt.tm_year + 1900;
    now.month = lt.tm_mon + 1;
    now.day = lt.tm_mday;
    now.hour = lt.tm_hour;
    now.min = lt.tm_min;
    now.sec = lt.tm_sec;
    now.msec = 0;
    
    return now;
}

void rec::clear_old_log_files(string directory, string prefix, string suffix, int daysOld)
{
    DIR* dirStream = NULL;
    struct dirent* entry;
    struct stat statBuf;
    
    if ((dirStream = opendir(directory.c_str())) == NULL)
    {
        // write to stderr because logging might not be initialised yet
        fprintf(stderr, "Error: Failed to open directory '%s' to read its contents\n", directory.c_str());
        return;
    }
    
    time_t now = time(NULL);
    while ((entry = readdir(dirStream)) != NULL)
    {
        // remove log file if it starts with 'prefix', ends with 'suffix' and is older than 'daysOld'

        if (strlen(entry->d_name) >= prefix.size() + suffix.size() &&
            (prefix.empty() || strncmp(prefix.c_str(), entry->d_name, prefix.size()) == 0) &&
            (suffix.empty() || strcmp(suffix.c_str(), &entry->d_name[strlen(entry->d_name) - suffix.size()]) == 0) &&
            stat(join_path(directory, entry->d_name).c_str(), &statBuf) == 0 &&
            !S_ISDIR(statBuf.st_mode) &&
            now - statBuf.st_mtime > (daysOld * 24 * 60 * 60))
        {
            if (remove(join_path(directory, entry->d_name).c_str()) != 0)
            {
                // write to stderr because logging might not be initialised yet
                fprintf(stderr, "Error: Failed to remove old log file '%s'\n", join_path(directory, entry->d_name).c_str());
            }
            else
            {
                // write to stdout because logging might not be initialised yet
                printf("Removed old log file '%s'\n", join_path(directory, entry->d_name).c_str());
            }
        }
    }    
    
    closedir(dirStream);
}

string rec::add_timestamp_to_filename(string prefix, string suffix)
{
    Timestamp now = get_localtime_now();

    char buf[64];
    sprintf(buf, "%04d%02d%02d%02d%02d%02d", now.year, now.month, now.day, now.hour, now.min, now.sec);
    
    return prefix + buf + suffix;
}


bool rec::directory_exists(string path)
{
    struct stat statBuf;
    if (stat(path.c_str(), &statBuf) == 0 && S_ISDIR(statBuf.st_mode))
    {
        return true;
    }
    return false;
}

bool rec::directory_path_is_absolute(string path)
{
    if (path.at(0) == '/')
    {
        return true;
    }
    return false;
}

bool rec::file_exists(string filename)
{
    struct stat statBuf;
    if (stat(filename.c_str(), &statBuf) == 0)
    {
        return true;
    }
    return false;
}

bool rec::parse_log_level_string(string levelStr, LogLevel* level)
{
    if (levelStr.compare("DEBUG") == 0)
    {
        *level = LOG_LEVEL_DEBUG;
        return true;
    }
    else if (levelStr.compare("INFO") == 0)
    {
        *level = LOG_LEVEL_INFO;
        return true;
    }
    else if (levelStr.compare("WARNING") == 0)
    {
        *level = LOG_LEVEL_WARNING;
        return true;
    }
    else if (levelStr.compare("ERROR") == 0)
    {
        *level = LOG_LEVEL_ERROR;
        return true;
    }
    return false;
}

string rec::trim_string(string val)
{
    size_t start;
    size_t len;
    
    // trim spaces from the start
    start = 0;
    while (start < val.size() && isspace(val[start]))
    {
        start++;
    }
    if (start >= val.size())
    {
        // empty string or string with spaces only
        return "";
    }
    
    // trim spaces from the end by reducing the length
    len = val.size() - start;
    while (len > 0 && isspace(val[start + len - 1]))
    {
        len--;
    }
    
    return val.substr(start, len);
}

bool rec::is_valid_barcode(string barcode)
{
    // a valid barcode consist of 3 capital letters, padded with spaces at end, followed by 6 digits

    if (barcode.size() != 9)
    {
        return false;
    }
    
    int i;
    // capital letters;
    for (i = 0; i < 3; i++)
    {
        if (barcode[i] == ' ')
        {
            break;
        }
        if (barcode[i] < 'A' || barcode[i] > 'Z')
        {
            return false;
        }
    }
    
    // spaces    
    for (;i < 3; i++)
    {
        if (barcode[i] != ' ')
        {
            return false;
        }
    }
    
    // digits
    for (;i < 9; i++)
    {
        if (barcode[i] < '0' || barcode[i] > '9')
        {
            return false;
        }
    }
    
    return true;
}

