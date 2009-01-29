/*
 * $Id: CutsDatabase.h,v 1.4 2009/01/29 07:36:59 stuart_hc Exp $
 *
 * Simple file database for video switch events.
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

#ifndef CutsDatabase_h
#define CutsDatabase_h


#include <string>
#include <vector>
#include <set>
#include <fstream>

#include "Timecode.h"



class CutsDatabase
{
public:
    CutsDatabase();
    ~CutsDatabase();
    void Filename(const std::string & filename) {mFilename = filename;}
    void OpenAppend();
    void Close();
    void AppendEntry(const std::string & sourceId, const std::string & tc, int year, int month, int day);
    bool IsOpen();

private:
    std::string mFilename;
    std::ofstream mFile;  
};


#endif // #ifndef CutsDatabase_h


