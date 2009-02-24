/*
 * $Id: Utilities.cpp,v 1.1 2009/02/24 08:21:17 stuart_hc Exp $
 *
 * Common utility functions
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

#define __STDC_FORMAT_MACROS 1


#include "Utilities.h"


using namespace std;
using namespace ingex;


string ingex::int64_to_string(int64_t value)
{
    char buf[32];
    sprintf(buf, "%"PRIi64"", value);
    return buf;
}

string ingex::int_to_string(int value)
{
    char buf[32];
    sprintf(buf, "%d", value);
    return buf;
}

string ingex::float_to_string(float value)
{
    char buf[48];
    sprintf(buf, "%f", value);
    return buf;
}

string ingex::bool_to_string(bool value)
{
    if (value)
    {
        return "true";
    }
    return "false";
}

bool ingex::parse_int(string strValue, int* value)
{
    return sscanf(strValue.c_str(), "%d", value) == 1;
}

bool ingex::parse_int64(string strValue, int64_t* value)
{
    return sscanf(strValue.c_str(), "%"PRIi64"", value) == 1;
}

bool ingex::parse_bool(string strValue, bool* value)
{
    if (strValue.compare("true") == 0)
    {
        *value = true;
        return true;
    }
    else if (strValue.compare("false") == 0)
    {
        *value = false;
        return true;
    }

    return false;
}

bool ingex::parse_float(string strValue, float* value)
{
    return sscanf(strValue.c_str(), "%f", value) == 1;
}

char* ingex::string_print(char* buffer, size_t bufferSize, const char* format, ...)
{
    char* result;
    va_list ap;

    va_start(ap, format);
    result = string_print(buffer, bufferSize, format, ap);
    va_end(ap);

    return result;
}

char* ingex::string_print(char* buffer, size_t bufferSize, const char* format, va_list ap)
{
    if (bufferSize <= 0)
    {
        return NULL;
    }

    buffer[0] = '\0';
    vsnprintf(buffer, bufferSize, format, ap);

    return buffer;
}
