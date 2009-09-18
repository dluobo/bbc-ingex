/*
 * $Id: DatabaseEnums.cpp,v 1.2 2009/09/18 16:50:11 philipn Exp $
 *
 * Defines enumerated data values matching those in the database
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
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

#include "DatabaseEnums.h"
#include "Database.h"
#include "DBException.h"
#include "Logging.h"

// static member
DatabaseEnums * DatabaseEnums::mInstance = 0;

// constructor
DatabaseEnums::DatabaseEnums()
{
    // Load names from database.
    // We are assuming that database has already been initialised.
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
        if (db)
        {
            db->loadResolutionNames(mResolutionNames);
            db->loadFileFormatNames(mFileFormatNames);
            db->loadTimecodeNames(mTimecodeNames);
        }
    }
    catch (const prodauto::DBException & dbe)
    {
        PA_LOG(error, ("Database Exception: %C\n", dbe.getMessage().c_str()));
    }
}

// video resolutions
std::string DatabaseEnums::ResolutionName(int resolution)
{
    std::map<int, std::string>::const_iterator it = mResolutionNames.find(resolution);
    if (it != mResolutionNames.end())
    {
        return it->second;
    }
    else
    {
        return("");
    }
}

// file formats
std::string DatabaseEnums::FileFormatName(int file_format)
{
    std::map<int, std::string>::const_iterator it = mFileFormatNames.find(file_format);
    if (it != mFileFormatNames.end())
    {
        return it->second;
    }
    else
    {
        return("");
    }
}

// data def
std::string DatabaseEnums::DataDefName(int data_def)
{
    const char * name;
    switch (data_def)
    {
    case PICTURE_DATA_DEFINITION:
        name = PICTURE_DATA_DEFINITION_NAME;
        break;
    case SOUND_DATA_DEFINITION:
        name = SOUND_DATA_DEFINITION_NAME;
        break;
    default:
        name = "Unknown";
        break;
    }
    return name;
}

// timecode
std::string DatabaseEnums::TimecodeName(int tc)
{
    std::map<int, std::string>::const_iterator it = mTimecodeNames.find(tc);
    if (it != mTimecodeNames.end())
    {
        return it->second;
    }
    else
    {
        return("");
    }
}

