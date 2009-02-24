/*
 * $Id: Logging.cpp,v 1.1 2009/02/24 08:21:16 stuart_hc Exp $
 *
 * Provides methods for various levels of logging to standard streams and/or file
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/time.h>

#include "Logging.h"
#include "IngexException.h"


using namespace std;
using namespace ingex;


std::auto_ptr<Logging> Logging::_instance(new Logging(LOG_LEVEL_DEBUG));

static void fprintf_prefix(FILE* out, int level)
{
    time_t now = time(NULL);

    struct tm lt;
    localtime_r(&now, &lt);

    struct timeval nowTD;
    gettimeofday(&nowTD, NULL);

    fprintf(out, "%02d %02d:%02d:%02d.%06ld ",
        lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, nowTD.tv_usec);

    switch (level)
    {
        case LOG_LEVEL_ERROR:
            fprintf(out, "ERROR: ");
            break;
        case LOG_LEVEL_WARNING:
            fprintf(out, "Warn:  ");
            break;
        case LOG_LEVEL_INFO:
            fprintf(out, "Info:  ");
            break;
        case LOG_LEVEL_DEBUG:
            fprintf(out, "Debug: ");
            break;
        default: // console
            fprintf(out, "Cnsle: ");
            break;
    }
}

static void fprintf_prefix_space(FILE* out)
{
    // length of prefix
    fprintf(out, "                          ");
}




void Logging::initialise(Logging* logging)
{
    _instance = auto_ptr<Logging>(logging);
}

void Logging::initialise(LogLevel level)
{
    _instance = auto_ptr<Logging>(new Logging(level));
}

Logging* Logging::getInstance()
{
    return _instance.get();
}

void Logging::error(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    _instance->iverror(format, ap);
    va_end(ap);
}

void Logging::warning(const char* format, ...)
{
    if (_instance->_level < LOG_LEVEL_WARNING)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    _instance->ivwarning(format, ap);
    va_end(ap);
}

void Logging::info(const char* format, ...)
{
    if (_instance->_level < LOG_LEVEL_INFO)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    _instance->ivinfo(format, ap);
    va_end(ap);
}

void Logging::debug(const char* format, ...)
{
    if (_instance->_level < LOG_LEVEL_DEBUG)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    _instance->ivdebug(format, ap);
    va_end(ap);
}

void Logging::console(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    _instance->ivconsole(format, ap);
    va_end(ap);
}

void Logging::errorMore(bool newLine, const char* format, ...)
{
    if (_instance->_level < LOG_LEVEL_ERROR)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    _instance->iverrorMore(newLine, format, ap);
    va_end(ap);
}

void Logging::warningMore(bool newLine, const char* format, ...)
{
    if (_instance->_level < LOG_LEVEL_WARNING)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    _instance->ivwarningMore(newLine, format, ap);
    va_end(ap);
}

void Logging::infoMore(bool newLine, const char* format, ...)
{
    if (_instance->_level < LOG_LEVEL_INFO)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    _instance->ivinfoMore(newLine, format, ap);
    va_end(ap);
}

void Logging::debugMore(bool newLine, const char* format, ...)
{
    if (_instance->_level < LOG_LEVEL_DEBUG)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    _instance->ivdebugMore(newLine, format, ap);
    va_end(ap);
}

void Logging::consoleMore(bool newLine, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    _instance->ivconsoleMore(newLine, format, ap);
    va_end(ap);
}


void Logging::verror(const char* format, va_list ap)
{
    _instance->iverror(format, ap);
}

void Logging::vwarning(const char* format, va_list ap)
{
    if (_instance->_level < LOG_LEVEL_WARNING)
    {
        return;
    }

    _instance->ivwarning(format, ap);
}

void Logging::vinfo(const char* format, va_list ap)
{
    if (_instance->_level < LOG_LEVEL_INFO)
    {
        return;
    }

    _instance->ivinfo(format, ap);
}

void Logging::vdebug(const char* format, va_list ap)
{
    if (_instance->_level < LOG_LEVEL_DEBUG)
    {
        return;
    }

    _instance->ivdebug(format, ap);
}

void Logging::vconsole(const char* format, va_list ap)
{
    _instance->ivconsole(format, ap);
}

void Logging::verrorMore(bool newLine, const char* format, va_list ap)
{
    if (_instance->_level < LOG_LEVEL_ERROR)
    {
        return;
    }

    _instance->iverrorMore(newLine, format, ap);
}

void Logging::vwarningMore(bool newLine, const char* format, va_list ap)
{
    if (_instance->_level < LOG_LEVEL_WARNING)
    {
        return;
    }

    _instance->ivwarningMore(newLine, format, ap);
}

void Logging::vinfoMore(bool newLine, const char* format, va_list ap)
{
    if (_instance->_level < LOG_LEVEL_INFO)
    {
        return;
    }

    _instance->ivinfoMore(newLine, format, ap);
}

void Logging::vdebugMore(bool newLine, const char* format, va_list ap)
{
    if (_instance->_level < LOG_LEVEL_DEBUG)
    {
        return;
    }

    _instance->ivdebugMore(newLine, format, ap);
}

void Logging::vconsoleMore(bool newLine, const char* format, va_list ap)
{
    _instance->ivconsoleMore(newLine, format, ap);
}



Logging::Logging(LogLevel level)
: _level(level)
{}

Logging::~Logging()
{}

void Logging::setLogLevel(LogLevel level)
{
    _level = level;
}

LogLevel Logging::getLogLevel()
{
    return _level;
}

void Logging::iverror(const char* format, va_list ap)
{
    fprintf_prefix(stderr, LOG_LEVEL_ERROR);
    vfprintf(stderr, format, ap);
}

void Logging::ivwarning(const char* format, va_list ap)
{
    fprintf_prefix(stdout, LOG_LEVEL_WARNING);
    vfprintf(stdout, format, ap);
}

void Logging::ivinfo(const char* format, va_list ap)
{
    fprintf_prefix(stdout, LOG_LEVEL_INFO);
    vfprintf(stdout, format, ap);
}

void Logging::ivdebug(const char* format, va_list ap)
{
    fprintf_prefix(stdout, LOG_LEVEL_DEBUG);
    vfprintf(stdout, format, ap);
}

void Logging::ivconsole(const char* format, va_list ap)
{
    fprintf_prefix(stdout, -1);
    vfprintf(stdout, format, ap);
}

void Logging::iverrorMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(stderr, "\n");
        fprintf_prefix_space(stderr);
    }
    vfprintf(stderr, format, ap);
}

void Logging::ivwarningMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(stdout, "\n");
        fprintf_prefix_space(stdout);
    }
    vfprintf(stdout, format, ap);
}

void Logging::ivinfoMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(stdout, "\n");
        fprintf_prefix_space(stdout);
    }
    vfprintf(stdout, format, ap);
}

void Logging::ivdebugMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(stdout, "\n");
        fprintf_prefix_space(stdout);
    }
    vfprintf(stdout, format, ap);
}

void Logging::ivconsoleMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(stdout, "\n");
        fprintf_prefix_space(stdout);
    }
    vfprintf(stdout, format, ap);
}


FileLogging::FileLogging(string filename, LogLevel level)
: Logging(level), _logFile(0), _filename(filename)
{
    if ((_logFile = fopen(filename.c_str(), "wb")) == 0)
    {
        throw IngexException("Failed to open log file: %s\n", strerror(errno));
    }
}

FileLogging::~FileLogging()
{
    if (_logFile)
    {
        fclose(_logFile);
    }
}

void FileLogging::iverror(const char* format, va_list ap)
{
    fprintf_prefix(_logFile, LOG_LEVEL_ERROR);
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

void FileLogging::ivwarning(const char* format, va_list ap)
{
    fprintf_prefix(_logFile, LOG_LEVEL_WARNING);
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

void FileLogging::ivinfo(const char* format, va_list ap)
{
    fprintf_prefix(_logFile, LOG_LEVEL_INFO);
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

void FileLogging::ivdebug(const char* format, va_list ap)
{
    fprintf_prefix(_logFile, LOG_LEVEL_DEBUG);
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

void FileLogging::iverrorMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(_logFile, "\n");
        fprintf_prefix_space(_logFile);
    }
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

void FileLogging::ivwarningMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(_logFile, "\n");
        fprintf_prefix_space(_logFile);
    }
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

void FileLogging::ivinfoMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(_logFile, "\n");
        fprintf_prefix_space(_logFile);
    }
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

void FileLogging::ivdebugMore(bool newLine, const char* format, va_list ap)
{
    if (newLine)
    {
        fprintf(_logFile, "\n");
        fprintf_prefix_space(_logFile);
    }
    vfprintf(_logFile, format, ap);
    fflush(_logFile);
}

bool FileLogging::reopenLogFile(string newFilename)
{
    if (newFilename.compare(_filename) == 0)
    {
        // already open
        return true;
    }

    // freopen changes _logFile even if it can't open newFilename - we therefore test first
    FILE* testFile;
    if ((testFile = fopen(newFilename.c_str(), "wb")) == NULL)
    {
        fprintf(stderr, "Failed to test open log file '%s': %s\n",
            newFilename.c_str(), strerror(errno));
        return false;
    }
    fclose(testFile);

    // reopen the log file
    if (freopen(newFilename.c_str(), "wb", _logFile) == NULL)
    {
        fprintf(stderr, "ERROR: Failed to reopen log file '%s' to '%s': %s\n",
            _filename.c_str(), newFilename.c_str(), strerror(errno));

        // the file logging is now lost - try reopen the original log file
        if (freopen(_filename.c_str(), "ab", _logFile) == NULL)
        {
            fprintf(stderr, "ERROR: Failed to reopen the original log file '%s': %s\n",
                _filename.c_str(), strerror(errno));
            // now what?
        }

        return false;
    }

    _filename = newFilename;
    return true;
}


DualLogging::DualLogging(string filename, LogLevel logLevel)
: FileLogging(filename, logLevel), _consoleLog(logLevel)
{
}

DualLogging::~DualLogging()
{
}

void DualLogging::iverror(const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::iverror(format, ap);
    _consoleLog.iverror(format, apcpy);
}

void DualLogging::ivwarning(const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::ivwarning(format, ap);
    _consoleLog.ivwarning(format, apcpy);
}

void DualLogging::ivinfo(const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::ivinfo(format, ap);
    _consoleLog.ivinfo(format, apcpy);
}

void DualLogging::ivdebug(const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::ivdebug(format, ap);
    _consoleLog.ivdebug(format, apcpy);
}

void DualLogging::ivconsole(const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    _consoleLog.ivconsole(format, apcpy);
}

void DualLogging::iverrorMore(bool newLine, const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::iverrorMore(newLine, format, ap);
    _consoleLog.iverrorMore(newLine, format, apcpy);
}

void DualLogging::ivwarningMore(bool newLine, const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::ivwarningMore(newLine, format, ap);
    _consoleLog.ivwarningMore(newLine, format, apcpy);
}

void DualLogging::ivinfoMore(bool newLine, const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::ivinfoMore(newLine, format, ap);
    _consoleLog.ivinfoMore(newLine, format, apcpy);
}

void DualLogging::ivdebugMore(bool newLine, const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    FileLogging::ivdebugMore(newLine, format, ap);
    _consoleLog.ivdebugMore(newLine, format, apcpy);
}

void DualLogging::ivconsoleMore(bool newLine, const char* format, va_list ap)
{
    va_list apcpy;
    va_copy(apcpy, ap);

    _consoleLog.ivconsoleMore(newLine, format, apcpy);
}

bool DualLogging::reopenLogFile(string newFilename)
{
    return FileLogging::reopenLogFile(newFilename);
}
