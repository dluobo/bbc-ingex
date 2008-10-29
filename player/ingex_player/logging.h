/*
 * $Id: logging.h,v 1.2 2008/10/29 17:47:42 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 * Copyright (C) 2008 BBC Research, Stuart Cunningham, <stuart_hc@users.sourceforge.net>
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

#ifndef __LOGGING_H__
#define __LOGGING_H__


#include <stdarg.h>

#ifdef __cplusplus
extern "C" 
{
#endif



typedef enum
{
    DEBUG_LOG_LEVEL = 0, 
    INFO_LOG_LEVEL, 
    WARN_LOG_LEVEL, 
    ERROR_LOG_LEVEL
} LogLevel;


typedef void (*ml_log_func) (LogLevel level, int cont, const char* format, ...);
typedef void (*ml_vlog_func) (LogLevel level, int cont, const char* format, va_list p_arg);

/* is set by default to the ml_log_default function */
extern ml_log_func ml_log;
extern ml_vlog_func ml_vlog;
extern LogLevel g_logLevel;

void ml_set_log_level(int level);

/* outputs to stderr for ERROR_LOG_LEVEL or stdout for the other levels */
void ml_log_default(LogLevel level, int cont, const char* format, ...);
void ml_vlog_default(LogLevel level, int cont, const char* format, va_list p_arg);


/* sets ml_log to log to the file 'filename' */
int ml_log_file_open(const char* filename);

void ml_log_file_flush();
void ml_log_file_close();


/* log level in function name */
void ml_log_debug(const char* format, ...);
void ml_log_info(const char* format, ...);
void ml_log_warn(const char* format, ...);
void ml_log_error(const char* format, ...);

/* continue the last log message rather than start a new one */
void ml_log_debug_cont(const char* format, ...);
void ml_log_info_cont(const char* format, ...);
void ml_log_warn_cont(const char* format, ...);
void ml_log_error_cont(const char* format, ...);




#ifdef __cplusplus
}
#endif


#endif

