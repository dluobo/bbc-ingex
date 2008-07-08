/*
 * $Id: LTODirectory.cpp,v 1.1 2008/07/08 16:25:11 philipn Exp $
 *
 * Provides a list of MXF and session file items in a LTO directory and can 
 * create a backup of the session files and index file
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>


#include "LTODirectory.h"
#include "RecorderException.h"
#include "Logging.h"


using namespace std;
using namespace rec;


static bool directory_exists(string directory)
{
    struct stat statBuf;
    
    if (stat(directory.c_str(), &statBuf) == 0 && S_ISDIR(statBuf.st_mode))
    {
        return true;
    }
    
    return false;
}

static bool file_exists(string filename)
{
    struct stat statBuf;
    if (stat(filename.c_str(), &statBuf) == 0)
    {
        return true;
    }
    
    return false;
}

static string join_path(string path1, string path2)
{
    string result;
    
    result = path1;
    if (path1.size() > 0 && path1.at(path1.size() - 1) != '/' && 
        (path2.size() == 0 || path2.at(0) != '/'))
    {
        result.append("/");
    }
    result.append(path2);
    
    return result;
}

static string last_path_component(string filepath)
{
    size_t sepIndex;
    size_t sepIndex2;
    if ((sepIndex = filepath.rfind("/")) != string::npos)
    {
        if (sepIndex + 1 == filepath.size())
        {
            if (sepIndex >= 1 &&
                (sepIndex2 = filepath.rfind("/", sepIndex - 1)) != string::npos)
            {
                return filepath.substr(sepIndex2 + 1, sepIndex - sepIndex2 - 1);
            }
            return filepath.substr(0, sepIndex);
        }
        return filepath.substr(sepIndex + 1);
    }
    return filepath;
}

static void add_mxf_file(string filename, vector<LTOItem>& items)
{
    vector<LTOItem>::iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        LTOItem& item = *iter;
        if (!item.sessionFilename.empty() &&
            item.sessionFilename.compare(0, filename.size(), filename) == 0)
        {
            item.mxfFilename = filename;
            return;
        }
    }
    
    LTOItem newItem;
    newItem.mxfFilename = filename;
    items.push_back(newItem);
}

static void add_session_file(string filename, time_t modTime, vector<LTOItem>& items)
{
    string mxfFilename = filename.substr(0, strstr(filename.c_str(), ".mxf") - filename.c_str() + strlen(".mxf"));
    
    vector<LTOItem>::iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        LTOItem& item = *iter;
        if (mxfFilename.compare(item.mxfFilename) == 0)
        {
            if (item.sessionFilename.empty() ||
                item.sessionMTime < modTime)
            {
                item.mxfFilename = mxfFilename;
                item.sessionFilename = filename;
                item.sessionMTime = modTime;
            }
            return;
        }
    }
    
    LTOItem newItem;
    newItem.mxfFilename = mxfFilename;
    newItem.sessionFilename = filename;
    newItem.sessionMTime = modTime;
    items.push_back(newItem);
}




LTODirectory::LTODirectory(string directory, string sessionName)
: _directory(directory)
{
    REC_CHECK(directory_exists(directory));

    if (sessionName.empty())
    {
        // include all items
    
        DIR* ltoDirStream = opendir(directory.c_str());
        if (ltoDirStream == NULL)
        {
            throw RecorderException("Failed to open the lto directory '%s' for reading: %s", directory.c_str(), strerror(errno));
        }
        
        struct dirent* ltoDirent;
        size_t len;
        char* mxfSuffix;
        struct stat statBuf;
        while ((ltoDirent = readdir(ltoDirStream)) != NULL)
        {
            if (strcmp(ltoDirent->d_name, ".") == 0 ||
                strcmp(ltoDirent->d_name, "..") == 0)
            {
                continue;
            }
            
            len = strlen(ltoDirent->d_name);
            mxfSuffix = strstr(ltoDirent->d_name, ".mxf");
            
            if (mxfSuffix != 0)
            {
                if (*(mxfSuffix + strlen(".mxf")) != '\0')
                {
                    if (stat(join_path(directory, ltoDirent->d_name).c_str(), &statBuf) != 0)
                    {
                        throw RecorderException("Failed to stat session file '%s': %s", ltoDirent->d_name, strerror(errno));
                    }
                    
                    add_session_file(ltoDirent->d_name, statBuf.st_mtime, _items);
                }
                else
                {
                    add_mxf_file(ltoDirent->d_name, _items);
                }
            }
        }
        
        closedir(ltoDirStream);
    }
    else
    {
        // only include the given session

        if (!file_exists(join_path(_directory, sessionName)))
        {
            throw RecorderException("Session file '%s' does not exist in the lto directory '%s'", 
                sessionName.c_str(), _directory.c_str());
        }
        
        string mxfFilename = sessionName.substr(0, strstr(sessionName.c_str(), ".mxf") - sessionName.c_str() + strlen(".mxf"));
        if (!file_exists(join_path(_directory, mxfFilename)))
        {
            throw RecorderException("Missing mxf file '%s' for session file '%s'", mxfFilename.c_str(), sessionName.c_str());
        }

        LTOItem newItem;
        newItem.mxfFilename = mxfFilename;
        newItem.sessionFilename = sessionName;
        // not set: newItem.sessionMTime
        _items.push_back(newItem);
    }
    
}

LTODirectory::~LTODirectory()
{
}

void LTODirectory::backup(string backupDirectory)
{
    REC_CHECK(directory_exists(backupDirectory));
    
    // copy the session files
    string copyCommand;
    vector<LTOItem>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        LTOItem& item = *iter;
        
        if (!item.sessionFilename.empty())
        {
            copyCommand = "cp";
            copyCommand += " \"" + join_path(_directory, item.sessionFilename) + "\"";
            copyCommand += " \"" + backupDirectory + "\"";
            
            if (system(copyCommand.c_str()) != 0)
            {
                throw RecorderException("Failed to copy session file ('%s') for backup: %s", 
                    copyCommand.c_str(), strerror(errno));
            }
        }
    }
    
    // copy the index file
    string indexFilename = last_path_component(_directory) + "00.txt";
    if (!file_exists(join_path(_directory, indexFilename)))
    {
        Logging::warning("Missing index file '%s'\n", indexFilename.c_str());
    }
    else
    {
        copyCommand = "cp";
        copyCommand += " \"" + join_path(_directory, indexFilename) + "\"";
        copyCommand += " \"" + backupDirectory + "\"";
        
        if (system(copyCommand.c_str()) != 0)
        {
            throw RecorderException("Failed to copy index file ('%s') for backup: %s", 
                copyCommand.c_str(), strerror(errno));
        }
    }
    
}

