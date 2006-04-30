/*
 * $Id: utils.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
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

#ifndef UTILS_H
#define UTILS_H

// Utilities
extern bool openLogFileWithDate(const char *logfile);
extern bool openLogFile(const char *logfile);
extern void logF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void logTF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void logerrF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void logFF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

extern char *framesToStr(int tc, char *s);

#endif /* UTILS_H */
