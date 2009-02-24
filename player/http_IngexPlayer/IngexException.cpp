/*
 * $Id: IngexException.cpp,v 1.1 2009/02/24 08:21:16 stuart_hc Exp $
 *
 * General purpose exception class
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

#include "IngexException.h"

using namespace std;
using namespace ingex;


IngexException::IngexException()
{}

IngexException::IngexException(const char* format, ...)
{
    char message[512];

    va_list varg;
    va_start(varg, format);
    vsnprintf(message, 512, format, varg);
    va_end(varg);

    _message = message;
}


IngexException::~IngexException()
{}

string IngexException::getMessage() const
{
    return _message;
}
