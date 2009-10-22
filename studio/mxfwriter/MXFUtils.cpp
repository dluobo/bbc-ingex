/*
 * $Id: MXFUtils.cpp,v 1.2 2009/10/22 13:55:36 john_f Exp $
 *
 * General MXF utilities
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
 
#include <cstdarg>
#include <cassert>

#include "MXFUtils.h"
#include <Logging.h>

extern "C"
{
    #include <mxf/mxf_logging.h>
}

using namespace std;
using namespace prodauto;


static void mxfVLogging(MXFLogLevel level, const char* format, va_list ap)
{
    if (level < g_mxfLogLevel)
        return;
    
    switch (level)
    {
        case MXF_ELOG:
            Logging::verror("libMXF: ", format, ap);
            break;
        case MXF_WLOG:
            Logging::vwarning("libMXF: ", format, ap);
            break;
        case MXF_ILOG:
            Logging::vinfo("libMXF: ", format, ap);
            break;
        default: // MXF_DLOG
            Logging::vdebug("libMXF: ", format, ap);
    }
}

static void mxfLogging(MXFLogLevel level, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    mxfVLogging(level, format, ap);
    va_end(ap);
}



void prodauto::connectLibMXFLogging()
{
    g_mxfLogLevel = MXF_DLOG;
    mxf_vlog = mxfVLogging;
    mxf_log = mxfLogging;
}
