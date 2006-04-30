/*
 * $Id: utils.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
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
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "utils.h"

// Utility functions
//

static FILE *pLogFile = NULL;
static char log_filename[FILENAME_MAX] = "recorder.log";
static bool log_overwrite = true;

extern bool openLogFileWithDate(const char *logfile)
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
extern bool openLogFile(const char *logfile)
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

// printf style logging to stdout and logfile

extern void logF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	vprintf(fmt, ap);
	if (! pLogFile)
		openLogFile(NULL);
	vfprintf(pLogFile, fmt, ap);

	va_end(ap);
}

// logging with time stamp and flushing
extern void logTF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	time_t now = time(NULL);
	struct tm l;
	localtime_r(&now, &l);

	printf("%02d:%02d:%02d ", l.tm_hour, l.tm_min, l.tm_sec);
	fflush(stdout);
	vprintf(fmt, ap);

	if (! pLogFile)
		openLogFile(NULL);
	fprintf(pLogFile, "%02d:%02d:%02d ", l.tm_hour, l.tm_min, l.tm_sec);
	vfprintf(pLogFile, fmt, ap);
	fflush(pLogFile);

	va_end(ap);
}

// printf style logging with stream flush
extern void logFF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	vprintf(fmt, ap);
	fflush(stdout);

	if (! pLogFile)
		openLogFile(NULL);
	vfprintf(pLogFile, fmt, ap);
	fflush(pLogFile);

	va_end(ap);
}

// printf style logging to stderr and logfile
extern void logerrF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	if (! pLogFile)
		openLogFile(NULL);

	vfprintf(stderr, fmt, ap);
	vfprintf(pLogFile, fmt, ap);

	va_end(ap);
}

extern char *framesToStr(int tc, char *s)
{
	int frames = tc % 25;
	int hours = (int)(tc / (60 * 60 * 25));
	int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
	int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

	sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}
