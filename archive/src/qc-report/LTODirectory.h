/*
 * $Id: LTODirectory.h,v 1.1 2008/07/08 16:25:20 philipn Exp $
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

#ifndef __LTO_DIRECTORY_H__
#define __LTO_DIRECTORY_H__

#include <sys/types.h>

#include <string>
#include <vector>


class LTOItem
{
public:
    std::string mxfFilename;
    std::string sessionFilename;
    
    // for internal use
    time_t sessionMTime;
};


class LTODirectory
{
public:
    LTODirectory(std::string directory, std::string sessionName);
    ~LTODirectory();
    
    void backup(std::string backupDirectory);
    
    const std::vector<LTOItem>& getItems() { return _items; }
    
private:
    std::string _directory;
    std::vector<LTOItem> _items;
};




#endif



