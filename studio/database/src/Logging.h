/*
 * $Id: Logging.h,v 1.4 2009/09/18 16:50:11 philipn Exp $
 *
 * Logging facility
 *
 * Copyright (C) 2006-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier <philipn@users.sourceforge.net>
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

#ifndef __PRODAUTO_LOGGING_H__
#define __PRODAUTO_LOGGING_H__


#include <memory>
#include <cstdarg>

#include "Threads.h"


// eg. PA_LOGTHROW(ProdAutoException, ("Something failed with error %d", 1));
#define PA_LOGTHROW(extype, params) \
{ \
    prodauto::Logging::error params; \
    prodauto::Logging::error(", near %s:%d\n", __FILE__, __LINE__); \
    throw extype params; \
}

// eg. PA_LOG(warning, ("Something might be wrong here %d", 1));
#define PA_LOG(type, params) \
{ \
    prodauto::Logging::type params; \
    prodauto::Logging::type(", near %s:%d\n", __FILE__, __LINE__); \
}



namespace prodauto
{

// lowest message type to log, starting with error at the top and ending in debug
enum LogLevel
{
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
};

class Logging
{
public:
    // initialise with your Logging sub-class, eg. a multi-threaded implementation
    static void initialise(Logging* logging);

    // use the default Logging class and set the log level
    static void initialise(LogLevel level);

    static void error(const char* format, ...);
    static void warning(const char* format, ...);
    static void info(const char* format, ...);
    static void debug(const char* format, ...);

    static void verror(const char* format, va_list ap);
    static void vwarning(const char* format, va_list ap);
    static void vinfo(const char* format, va_list ap);
    static void vdebug(const char* format, va_list ap);


    virtual ~Logging();

protected:
    Logging(LogLevel level);

    static Logging* getInstance();

    virtual void setLogLevel(LogLevel level);

    virtual void iverror(const char* format, va_list ap);
    virtual void ivwarning(const char* format, va_list ap);
    virtual void ivinfo(const char* format, va_list ap);
    virtual void ivdebug(const char* format, va_list ap);

private:
    static std::auto_ptr<Logging> _instance;
    static Mutex _loggingMutex;

    LogLevel _level;
};




};







#endif


