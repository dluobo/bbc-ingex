/*
 * $Id: Logging.h,v 1.1 2008/07/08 16:23:34 philipn Exp $
 *
 * Provides methods for various levels of logging to standard streams and/or file
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
 
#ifndef __RECORDER_LOGGING_H__
#define __RECORDER_LOGGING_H__

#include <stdarg.h>
#include <stdio.h>
#include <memory>

#include <string>


// eg. REC_LOGTHROW(("Something failed with error %d", 1));
#define REC_LOGTHROW(params) \
    Logging::error params; \
    Logging::errorMore(false, ", near %s:%d\n", __FILE__, __LINE__); \
    throw RecorderException params;

// eg. REC_LOG(warning, ("Something might be wrong here %d", 1));
#define REC_LOG(type, params) \
    Logging::type params; \
    Logging::type ## More(false, ", near %s:%d\n", __FILE__, __LINE__);
    


namespace rec
{

// lowest message type to log, starting with error at the top and ending in debug
typedef enum LogLevel
{ 
    LOG_LEVEL_ERROR = 0, 
    LOG_LEVEL_WARNING, 
    LOG_LEVEL_INFO, 
    LOG_LEVEL_DEBUG 
};

class Logging
{
public:
    // initialise with your Logging sub-class 
    static void initialise(Logging* logging); 
    
    // use the default Logging class and set the log level
    static void initialise(LogLevel level); 

    static Logging* getInstance();

    static void error(const char* format, ...); 
    static void warning(const char* format, ...); 
    static void info(const char* format, ...); 
    static void debug(const char* format, ...); 
    static void console(const char* format, ...);
    static void errorMore(bool newLine, const char* format, ...); 
    static void warningMore(bool newLine, const char* format, ...); 
    static void infoMore(bool newLine, const char* format, ...); 
    static void debugMore(bool newLine, const char* format, ...);
    static void consoleMore(bool newLine, const char* format, ...);

    static void verror(const char* format, va_list ap); 
    static void vwarning(const char* format, va_list ap); 
    static void vinfo(const char* format, va_list ap); 
    static void vdebug(const char* format, va_list ap); 
    static void vconsole(const char* format, va_list ap); 
    static void verrorMore(bool newLine, const char* format, va_list ap); 
    static void vwarningMore(bool newLine, const char* format, va_list ap); 
    static void vinfoMore(bool newLine, const char* format, va_list ap); 
    static void vdebugMore(bool newLine, const char* format, va_list ap); 
    static void vconsoleMore(bool newLine, const char* format, va_list ap); 
    
private:
    static std::auto_ptr<Logging> _instance;
        
public:
    Logging(LogLevel level);
    virtual ~Logging();
    
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel();
    

    // implement these functions in your Logging sub-class
    
    virtual void iverror(const char* format, va_list ap); 
    virtual void ivwarning(const char* format, va_list ap); 
    virtual void ivinfo(const char* format, va_list ap); 
    virtual void ivdebug(const char* format, va_list ap); 
    virtual void ivconsole(const char* format, va_list ap); 
    virtual void iverrorMore(bool newLine, const char* format, va_list ap); 
    virtual void ivwarningMore(bool newLine, const char* format, va_list ap); 
    virtual void ivinfoMore(bool newLine, const char* format, va_list ap); 
    virtual void ivdebugMore(bool newLine, const char* format, va_list ap); 
    virtual void ivconsoleMore(bool newLine, const char* format, va_list ap); 
    
private:
    LogLevel _level;
};


class FileLogging : public Logging
{
public:
    FileLogging(std::string filename, LogLevel level);
    virtual ~FileLogging();
    
    virtual void iverror(const char* format, va_list ap); 
    virtual void ivwarning(const char* format, va_list ap); 
    virtual void ivinfo(const char* format, va_list ap); 
    virtual void ivdebug(const char* format, va_list ap);
    virtual void ivconsole(const char* format, va_list ap) {} // null impl 
    virtual void iverrorMore(bool newLine, const char* format, va_list ap);
    virtual void ivwarningMore(bool newLine, const char* format, va_list ap);
    virtual void ivinfoMore(bool newLine, const char* format, va_list ap);
    virtual void ivdebugMore(bool newLine, const char* format, va_list ap);
    virtual void ivconsoleMore(bool newLine, const char* format, va_list ap) {}  // null impl 
    
    virtual bool reopenLogFile(std::string newFilename);
    
private:
    FILE* _logFile;
    std::string _filename;
};


class DualLogging : public FileLogging
{
public:
    DualLogging(std::string filename, LogLevel logLevel);
    virtual ~DualLogging();
    
    virtual void iverror(const char* format, va_list ap); 
    virtual void ivwarning(const char* format, va_list ap); 
    virtual void ivinfo(const char* format, va_list ap); 
    virtual void ivdebug(const char* format, va_list ap);
    virtual void ivconsole(const char* format, va_list ap); 
    virtual void iverrorMore(bool newLine, const char* format, va_list ap);
    virtual void ivwarningMore(bool newLine, const char* format, va_list ap);
    virtual void ivinfoMore(bool newLine, const char* format, va_list ap);
    virtual void ivdebugMore(bool newLine, const char* format, va_list ap);
    virtual void ivconsoleMore(bool newLine, const char* format, va_list ap); 
    
    virtual bool reopenLogFile(std::string newFilename);
    
private:
    Logging _consoleLog;
};



};


#endif


