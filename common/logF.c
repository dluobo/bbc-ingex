/*
 * $Id: logF.c,v 1.5 2010/01/12 16:03:05 john_f Exp $
 *
 * Logging and debugging utility functions.
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "logF.h"

// Utility functions
//

static FILE *pLogFile = NULL;
static char log_filename[FILENAME_MAX] = "recorder.log";
static int log_overwrite = 1;

extern int openLogFileWithDate(const char *logfile)
{
    time_t now = time(NULL);
    struct tm l;
    localtime_r(&now, &l);

    sprintf(log_filename, "%s_%02d%02d%02d_%02d%02d%02d.log",
                    logfile,
                    l.tm_year % 100, l.tm_mon + 1, l.tm_mday,
                    l.tm_hour, l.tm_min, l.tm_sec);

    // Open the log file once for lifetime of program
    if (! pLogFile)
        pLogFile = fopen(log_filename, log_overwrite ? "w" : "a");

    return pLogFile != NULL;
}

extern int reopenLogFileWithDate(const char *logfile)
{
    if (! pLogFile)
        return openLogFileWithDate(logfile);
    
    time_t now = time(NULL);
    struct tm l;
    localtime_r(&now, &l);
    char new_log_filename[FILENAME_MAX];

    sprintf(new_log_filename, "%s_%02d%02d%02d_%02d%02d%02d.log",
                    logfile,
                    l.tm_year % 100, l.tm_mon + 1, l.tm_mday,
                    l.tm_hour, l.tm_min, l.tm_sec);

    // Check the new log filename if different
    if (strcmp(log_filename, new_log_filename) == 0)
        return 1;

    // test the new file can be opened because freopen modifies original when it fails
    FILE* test_file;
    if ((test_file = fopen(logfile, log_overwrite ? "w" : "a")) == NULL)
        return 0;
    fclose(test_file);

    // Re-open the log file once for lifetime of program
    if (freopen(new_log_filename, log_overwrite ? "w" : "a", pLogFile) == NULL) {
        // try reopening the original file
        freopen(log_filename, "a", pLogFile);
        return 0;
    }
    
    strcpy(log_filename, new_log_filename);
    return 1;
}

extern int openLogFile(const char *logfile)
{
    if (logfile != NULL) {
        strcpy(log_filename, logfile);
    }

    // Backup old log file once (convenient for debugging)
    if (log_overwrite)
    {
        char *p, backup_name[FILENAME_MAX], suffix[FILENAME_MAX];
        strcpy(backup_name, log_filename);
        p = strchr(backup_name, '.');
        if (p)
        {
            strcpy(suffix, p);
            strcpy(p, "_old");
            strcat(p, suffix);
        }
        else
            strcat(backup_name, "_old");

        rename(log_filename, backup_name);
    }

    // Open the log file once for lifetime of program
    if (! pLogFile)
        pLogFile = fopen(log_filename, log_overwrite ? "w" : "a");

    return pLogFile != NULL;
}

extern int reopenLogFile(const char *logfile)
{
    if (! pLogFile)
        return openLogFile(logfile);
    
    if (logfile == NULL)
        return 0;
    
    if (strcmp(log_filename, logfile) == 0)
        return 1;
    
    // test the new file can be opened because freopen modifies original when it fails
    FILE* test_file;
    if ((test_file = fopen(logfile, log_overwrite ? "w" : "a")) == NULL)
        return 0;
    fclose(test_file);

    // Re-open the log file once for lifetime of program
    if (freopen(logfile, log_overwrite ? "w" : "a", pLogFile) == NULL) {
        // try reopening the original file
        freopen(log_filename, "a", pLogFile);
        return 0;
    }
    
    strcpy(log_filename, logfile);
    return 1;
}

extern void flushLogFile()
{
    if (pLogFile)
        fflush(pLogFile);
}

// printf style logging to stdout and logfile

extern void logF(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    int logfile_open = 1;
    if (! pLogFile)
        logfile_open = openLogFile(NULL);

    if (logfile_open) {
        va_start(ap, fmt);
        vfprintf(pLogFile, fmt, ap);
        va_end(ap);
    }
}

// same as logTF but takes a va_list instead of ...
extern void vlogTF(const char *fmt, va_list ap)
{
    struct timeval tv;
    time_t now = time(NULL);
    gettimeofday(&tv, NULL);
    struct tm l;
    localtime_r(&now, &l);

    printf("%02d %02d:%02d:%02d.%06ld ", l.tm_mday, l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);

    vprintf(fmt, ap);

    fflush(stdout);

    int logfile_open = 1;
    if (! pLogFile)
        logfile_open = openLogFile(NULL);

    if (logfile_open) {
        fprintf(pLogFile, "%02d %02d:%02d:%02d.%06ld ", l.tm_mday, l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);
        
        vfprintf(pLogFile, fmt, ap);

        //fflush(pLogFile);
    }
}

// logging with time stamp and flushing to stdout and file
extern void logTF(const char *fmt, ...)
{
    va_list ap;
    
    struct timeval tv;
    time_t now = time(NULL);
    gettimeofday(&tv, NULL);
    struct tm l;
    localtime_r(&now, &l);

    printf("%02d %02d:%02d:%02d.%06ld ", l.tm_mday, l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);
        va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);

    int logfile_open = 1;
    if (! pLogFile)
        logfile_open = openLogFile(NULL);

    if (logfile_open) {
        fprintf(pLogFile, "%02d %02d:%02d:%02d.%06ld ", l.tm_mday, l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);
        va_start(ap, fmt);
        vfprintf(pLogFile, fmt, ap);
        va_end(ap);
        //fflush(pLogFile);
    }
}

// logging with time stamp and flushing to file only
extern void logFF(const char *fmt, ...)
{
    va_list ap;

    struct timeval tv;
    time_t now = time(NULL);
    gettimeofday(&tv, NULL);
    struct tm l;
    localtime_r(&now, &l);

    int logfile_open = 1;
    if (! pLogFile)
        logfile_open = openLogFile(NULL);

    if (logfile_open) {
        fprintf(pLogFile, "%02d %02d:%02d:%02d.%06ld ", l.tm_mday, l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);
        va_start(ap, fmt);
        vfprintf(pLogFile, fmt, ap);
        va_end(ap);
        //fflush(pLogFile);
    }
}

// logging flushing to file only
extern void logFFi(const char *fmt, ...)
{
    va_list ap;

    int logfile_open = 1;
    if (! pLogFile)
        logfile_open = openLogFile(NULL);

    if (logfile_open) {
            va_start(ap, fmt);
        vfprintf(pLogFile, fmt, ap);
        va_end(ap);
        //fflush(pLogFile);
    }
}

// printf style logging to stderr and logfile
extern void logerrF(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    int logfile_open = 1;
    if (! pLogFile)
        logfile_open = openLogFile(NULL);

    if (logfile_open) {
        va_start(ap, fmt);
        vfprintf(pLogFile, fmt, ap);
        va_end(ap);
    }
}
