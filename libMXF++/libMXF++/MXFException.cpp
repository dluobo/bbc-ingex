/*
 * $Id: MXFException.cpp,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

#include <libMXF++/MXF.h>


using namespace std;
using namespace mxfpp;


MXFException::MXFException()
{}

MXFException::MXFException(const char* format, ...)
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


MXFException::~MXFException()
{}

string MXFException::getMessage() const
{
    return _message;
}

