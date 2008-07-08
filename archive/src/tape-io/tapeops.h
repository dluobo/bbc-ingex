/*
 * $Id: tapeops.h,v 1.1 2008/07/08 16:26:46 philipn Exp $
 *
 * Provides tape I/O functions
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

#ifndef Tapeops_h
#define Tapeops_h

#include <string>
using std::string;

enum TapeDetailedState { Failed, Notape, Busyload, Busyloadmom, Busyeject, Busywriting, Writeprotect, Badtape, Online };

bool rewind_tape(const char *device);
bool eject_tape(const char *device);
bool set_tape_params(const char *device);
bool tape_filemark_seek(const char *device, bool forwards, int num_marks);
bool tape_contains_tar_file(const char *device, string *p);

TapeDetailedState probe_tape_status(const char *device);

#endif // Tapeops_h
