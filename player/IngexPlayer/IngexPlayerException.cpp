/*
 * $Id: IngexPlayerException.cpp,v 1.2 2008/10/29 17:49:55 john_f Exp $
 *
 *
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>


#include "IngexPlayerException.h"

using namespace std;
using namespace prodauto;


IngexPlayerException::IngexPlayerException()
{}

IngexPlayerException::IngexPlayerException(const char* format, ...)
{
    char message[512];
    
    va_list varg;
    va_start(varg, format);
#if defined(_MSC_VER)
    _vsnprintf(message, 512, format, varg);
#else
    vsnprintf(message, 512, format, varg);
#endif
    va_end(varg);
    
    _message = message;
}


IngexPlayerException::~IngexPlayerException()
{}

string IngexPlayerException::getMessage() const
{
    return _message;
}


