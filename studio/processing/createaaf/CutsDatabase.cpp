/*
 * $Id: CutsDatabase.cpp,v 1.2 2009/01/29 07:36:59 stuart_hc Exp $
 *
 * 
 *
 * Copyright (C) 2007  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
 
#include "CutsDatabase.h"
#include <ProdAutoException.h>
#include <Logging.h>


using namespace std;
using namespace prodauto;

#define VIDEO_SWITCH_DB_ENTRY_SIZE      64

#define SOURCE_INDEX_OFFSET             0
#define SOURCE_ID_OFFSET                2
#define DATE_OFFSET                     35
#define TIMECODE_OFFSET                 46

#define SOURCE_INDEX_LEN                1
#define SOURCE_ID_LEN                   32
#define DATE_LEN                        10
#define TIMECODE_LEN                    12



static int get_num_entries(FILE* dbFile)
{
    struct stat st;
    
    if (fstat(fileno(dbFile), &st) != 0)
    {
        return 0;
    }
    
    return st.st_size / VIDEO_SWITCH_DB_ENTRY_SIZE;
}

static bool load(FILE* dbFile, int startEntry, unsigned char* buffer, int bufferSize, int* numEntries)
{
    int totalEntries = get_num_entries(dbFile);
    if (totalEntries == 0)
    {
        *numEntries = 0;
        return true;
    }
    
    int readEntries = totalEntries - startEntry;
    readEntries = (bufferSize / VIDEO_SWITCH_DB_ENTRY_SIZE < readEntries) ? 
        bufferSize / VIDEO_SWITCH_DB_ENTRY_SIZE : readEntries;
    if (readEntries <= 0)
    {
        *numEntries = 0;
        return true;
    }
    
    if (fseeko(dbFile, startEntry * VIDEO_SWITCH_DB_ENTRY_SIZE, SEEK_SET) != 0)
    {
        Logging::error("Failed to seek to position in cut database file: %s\n", strerror(errno));
        return false;
    }
    
    *numEntries = fread(buffer, VIDEO_SWITCH_DB_ENTRY_SIZE, readEntries, dbFile);
    return true;
}

bool parse_entry(const unsigned char* entryBuffer, int entryBufferSize, CutsDatabaseEntry* entry)
{
    int hour, min, sec, frame;
    char isDropFrame;
    int year, month, day;

    if (entryBufferSize < VIDEO_SWITCH_DB_ENTRY_SIZE)
    {
        Logging::error("Entry buffer size %d is smaller than the entry size %d\n", entryBufferSize, VIDEO_SWITCH_DB_ENTRY_SIZE);
        return false;
    }
    
    if (sscanf((const char*)&entryBuffer[DATE_OFFSET], "%04d-%02d-%02d", &year, &month, &day) != 3)
    {
        Logging::error("Failed to parse date in video switch database entry\n");
        return false;
    }
    
    if (sscanf((const char*)&entryBuffer[TIMECODE_OFFSET], "%02d:%02d:%02d:%02d%c", &hour, &min, &sec, &frame, &isDropFrame) != 5)
    {
        Logging::error("Failed to parse timecode in video switch database entry\n");
        return false;
    }
    
    entry->sourceId = (const char*)(&entryBuffer[SOURCE_ID_OFFSET]);
    entry->date.year = year;
    entry->date.month = month;
    entry->date.day = day;
    entry->timecode = hour * 60 * 60 * 25 + min * 60 * 25 + sec * 25 + frame; // TODO: need frame rate info
    
    return true;
}




CutsDatabase::CutsDatabase(string filename)
{
    loadEntries(filename);
}

CutsDatabase::~CutsDatabase()
{
}

// TODO: cuts beyond 12.00 to next day 
vector<CutInfo> CutsDatabase::getCuts(Date creationDate, int64_t startTimecode, set<string> sources, string defaultSource)
{
    vector<CutInfo> result;

    CutsDatabaseEntry previousKnownSourceEntry;
    previousKnownSourceEntry.timecode = -1;
    int64_t previousTimecode = -1;
    vector<CutsDatabaseEntry>::const_iterator iter;
    for (iter = _entries.begin(); iter != _entries.end(); iter++)
    {
        const CutsDatabaseEntry& entry = *iter;
        
        if (entry.date == creationDate && 
            entry.timecode >= startTimecode &&
            sources.find(entry.sourceId) != sources.end())
        {

            // check timecodes are monotonic
            if (entry.timecode < previousTimecode)
            {
                Logging::warning("Non-monotonic source timecodes in cut database\n");
                break;
            }

            // set the start source if the first cut is > start timecode
            if (result.empty() && entry.timecode > startTimecode)
            {
                if (previousKnownSourceEntry.timecode != -1)
                {
                    // start source is taken from the previous cut
                    CutInfo cutInfo;
                    cutInfo.position = 0;
                    cutInfo.source = previousKnownSourceEntry.sourceId;
                    
                    result.push_back(cutInfo);
                }
                else
                {
                    // don't know which source was initially active
                    Logging::warning("Unknown starting source in cut sequence - using the default source '%s'\n", defaultSource.c_str());
                    
                    CutInfo cutInfo;
                    cutInfo.position = 0;
                    cutInfo.source = defaultSource;
                    
                    result.push_back(cutInfo);
                }
            }
            
            CutInfo cutInfo;
            cutInfo.position = entry.timecode - startTimecode;
            cutInfo.source = entry.sourceId;
            
            result.push_back(cutInfo);
            
            previousTimecode = entry.timecode;
        }
        
        if (entry.date == creationDate &&
            sources.find(entry.sourceId) != sources.end())
        {
            previousKnownSourceEntry = entry;
        }
    }
    
    return result;
}


void CutsDatabase::loadEntries(string filename)
{
    FILE* dbFile = fopen(filename.c_str(), "rb");
    if (dbFile == 0)
    {
        // no entries
        Logging::warning("Cuts database '%s' does not exist\n", filename.c_str());
        return;
    }
    
    unsigned char buf[VIDEO_SWITCH_DB_ENTRY_SIZE * 50];
    int numEntries = 0;
    int startEntry = 0;
    do
    {
        if (!load(dbFile, startEntry, buf, VIDEO_SWITCH_DB_ENTRY_SIZE * 50, &numEntries))
        {
            PA_LOGTHROW(ProdAutoException, ("Failed to load cut database entries\n"));
        }
        
        int i;
        for (i = 0; i < numEntries; i++)
        {
            CutsDatabaseEntry entry;
            if (!parse_entry(&buf[VIDEO_SWITCH_DB_ENTRY_SIZE * i], VIDEO_SWITCH_DB_ENTRY_SIZE * (50 - i), &entry))
            {
                PA_LOGTHROW(ProdAutoException, ("Failed to parse cut database entry %d\n", i));
            }
            
            _entries.push_back(entry);
        }
        
        startEntry += numEntries;
        
    } while (numEntries > 0);
    
    
    fclose(dbFile);
}


