/*
 * $Id: Utilities.h,v 1.1 2009/02/24 08:21:17 stuart_hc Exp $
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

#ifndef __INGEX_UTILITIES_H__
#define __INGEX_UTILITIES_H__


#include <sys/time.h>
#include <string>
#include <vector>
#include <stdarg.h>

#include "Types.h"
#include "Logging.h"


#define SAFE_DELETE(var) \
    delete var; \
    var = 0;

#define SAFE_ARRAY_DELETE(var) \
    delete [] var; \
    var = 0;



namespace ingex
{

std::string int64_to_string(int64_t value);
std::string int_to_string(int value);
std::string float_to_string(float value);
std::string bool_to_string(bool value);

bool parse_int(std::string strValue, int* value);
bool parse_int64(std::string strValue, int64_t* value);
bool parse_bool(std::string strValue, bool* value);
bool parse_float(std::string strValue, float* value);

char* string_print(char* buffer, size_t bufferSize, const char* format, ...);
char* string_print(char* buffer, size_t bufferSize, const char* format, va_list ap);

};


#endif
