/*
 * $Id: logging.c,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Author: Stuart Cunningham
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
#include <assert.h>

#include <time.h>


#include <logging.h>


ml_log_func ml_log = ml_log_default;
ml_vlog_func ml_vlog = ml_vlog_default;
LogLevel g_logLevel = DEBUG_LOG_LEVEL;


static FILE* g_fileLog = NULL;

static void logmsg(FILE* file, LogLevel level, int cont, const char* format, va_list p_arg)
{
    if (!cont)
    {
        switch (level)
        {
            case DEBUG_LOG_LEVEL:
                fprintf(file, "Debug: ");
                break;
            case INFO_LOG_LEVEL:
                fprintf(file, "Info: ");
                break;
            case WARN_LOG_LEVEL:
                fprintf(file, "Warning: ");
                break;
            case ERROR_LOG_LEVEL:
                fprintf(file, "ERROR: ");
                break;
        };
    }

    vfprintf(file, format, p_arg);
}

static void vlog_to_file(LogLevel level, int cont, const char* format, va_list p_arg)
{
    char timeStr[128];
    const time_t t = time(NULL);
    const struct tm* gmt = gmtime(&t);

    if (level < g_logLevel)
    {
        return;
    }

    assert(gmt != NULL);
    assert(g_fileLog != NULL);
    if (g_fileLog == NULL)
    {
        return;
    }

    if (!cont)
    {
        strftime(timeStr, 128, "%Y-%m-%d %H:%M:%S", gmt);
        fprintf(g_fileLog, "(%s) ", timeStr);
    }

    logmsg(g_fileLog, level, cont, format, p_arg);
}

static void log_to_file(LogLevel level, int cont, const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    vlog_to_file(level, cont, format, p_arg);
    va_end(p_arg);
}


void ml_set_log_level(int level)
{
    if (level >= DEBUG_LOG_LEVEL && level <= ERROR_LOG_LEVEL)
    {
        g_logLevel = (LogLevel)level;
    }
    else if (level < DEBUG_LOG_LEVEL)
    {
        g_logLevel = DEBUG_LOG_LEVEL;
    }
    else
    {
        g_logLevel = ERROR_LOG_LEVEL;
    }
}

void ml_vlog_default(LogLevel level, int cont, const char* format, va_list p_arg)
{
    if (level < g_logLevel)
    {
        return;
    }

    if (level == ERROR_LOG_LEVEL)
    {
        logmsg(stderr, level, cont, format, p_arg);
    }
    else
    {
        logmsg(stdout, level, cont, format, p_arg);
    }
}

void ml_log_default(LogLevel level, int cont, const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog_default(level, cont, format, p_arg);
    va_end(p_arg);
}

int ml_log_file_open(const char* filename)
{
    if ((g_fileLog = fopen(filename, "wb")) == NULL)
    {
        return 0;
    }

    ml_log = log_to_file;
    ml_vlog = vlog_to_file;
    return 1;
}

void ml_log_file_flush()
{
    if (g_fileLog != NULL)
    {
        fflush(g_fileLog);
    }
}

void ml_log_file_close()
{
    if (g_fileLog != NULL)
    {
        fclose(g_fileLog);
        g_fileLog = NULL;
    }
}

void ml_log_debug(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(DEBUG_LOG_LEVEL, 0, format, p_arg);
    va_end(p_arg);
}

void ml_log_info(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(INFO_LOG_LEVEL, 0, format, p_arg);
    va_end(p_arg);
}

void ml_log_warn(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(WARN_LOG_LEVEL, 0, format, p_arg);
    va_end(p_arg);
}

void ml_log_error(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(ERROR_LOG_LEVEL, 0, format, p_arg);
    va_end(p_arg);
}

void ml_log_debug_cont(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(DEBUG_LOG_LEVEL, 1, format, p_arg);
    va_end(p_arg);
}

void ml_log_info_cont(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(INFO_LOG_LEVEL, 1, format, p_arg);
    va_end(p_arg);
}

void ml_log_warn_cont(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(WARN_LOG_LEVEL, 1, format, p_arg);
    va_end(p_arg);
}

void ml_log_error_cont(const char* format, ...)
{
    va_list p_arg;

    va_start(p_arg, format);
    ml_vlog(ERROR_LOG_LEVEL, 1, format, p_arg);
    va_end(p_arg);
}


