/*
 * $Id: Logfile.h,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
 *
 * Class to support ACE logging to a file
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
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

#ifndef Logfile_h
#define Logfile_h

#include <vector>
#include <string>

/**
This class looks after logfiles for run-time debug output.
*/
class Logfile
{
public:
    static void Init();
    static void PathRoot(const char * s);
    static void AddPathComponent(const char * s);
    static void Open(const char * id, bool unique = true);
    static void DebugLevel(int debuglevel);
    static int DebugLevel();
private:
    static std::vector<std::string> mPath;
    static int mDebugLevel;
    static bool mLoggingEnabled;
};

#endif

