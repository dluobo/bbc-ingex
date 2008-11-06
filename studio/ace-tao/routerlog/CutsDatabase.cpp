/*
 * $Id: CutsDatabase.cpp,v 1.2 2008/11/06 11:08:37 john_f Exp $
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
 
#include <ace/OS_NS_stdio.h>
#include <ace/Log_Msg.h>

#include "DateTime.h"
#include "CutsDatabase.h"


#define VIDEO_SWITCH_DB_ENTRY_SIZE      64

#define SOURCE_INDEX_OFFSET             0
#define SOURCE_ID_OFFSET                2
#define DATE_OFFSET                     35
#define TIMECODE_OFFSET                 46

#define SOURCE_INDEX_LEN                1
#define SOURCE_ID_LEN                   32
#define DATE_LEN                        10
#define TIMECODE_LEN                    12





CutsDatabase::CutsDatabase()
{
}

CutsDatabase::~CutsDatabase()
{
    mFile.close();
}

void CutsDatabase::OpenAppend()
{
    if (mFile.is_open())
    {
        mFile.close();
    }
    if (! mFilename.empty())
    {
        mFile.open(mFilename.c_str(), std::ios_base::app);
    }
    if (! mFile.is_open())
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to open cuts database file \"%C\"\n"),
            mFilename.c_str()));
    }
}

void CutsDatabase::Close()
{
    if (mFile.is_open())
    {
        mFile.close();
    }
}

void CutsDatabase::AppendEntry(const std::string & sourceId, const std::string & tc)
{
    if (mFile.is_open())
    {
        char buf[VIDEO_SWITCH_DB_ENTRY_SIZE];
        memset(buf, 0, VIDEO_SWITCH_DB_ENTRY_SIZE);
        
        // source index - 1 byte - not currently used
        buf[SOURCE_INDEX_OFFSET] = (unsigned char)(0);
        
        // source id - string plus null terminator
        strncpy(&buf[SOURCE_ID_OFFSET], sourceId.c_str(), SOURCE_ID_LEN - 1);
        
        // date
        int year, month, day;
        DateTime::GetDate(year, month, day);
        ACE_OS::snprintf(&buf[DATE_OFFSET], DATE_LEN + 1, "%04d-%02d-%02d", 
            (year > 9999) ? 0 : year,
            (month > 12) ? 0 : month,
            (day > 31) ? 0 : day);
        
        // timecode - text timecode plus drop frame indicator
        ACE_OS::snprintf(&buf[TIMECODE_OFFSET], TIMECODE_LEN + 1, "%s%c", tc.c_str(), 'N');

        // Write to file
        mFile.write(buf, VIDEO_SWITCH_DB_ENTRY_SIZE);
    }
}

bool CutsDatabase::IsOpen()
{
    return mFile.is_open();
}


