/*
 * $Id: PlayerState.h,v 1.1 2008/07/08 16:25:46 philipn Exp $
 *
 * Parses a player state text string and returns the name/value pairs
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
 
#ifndef __RECORDER_PLAYER_STATE_H__
#define __RECORDER_PLAYER_STATE_H__


#include <string>
#include <map>

#include "Threads.h"


namespace rec
{


class PlayerState
{
public:
    PlayerState();
    ~PlayerState();
    
    bool parse(std::string stateString);
    
    bool haveValue(std::string name);
    
    bool getInt(std::string name, int* value);
    bool getInt64(std::string name, int64_t* value);
    bool getBool(std::string name, bool* value);
    bool getString(std::string name, std::string* value);
    
    void clear();
    
private:
    bool ihaveValue(std::string name);
    void setValue(std::string name, std::string value);
    bool getValue(std::string name, std::string* value);
    
    std::map<std::string, std::string> _nameAndValues;
};


};




#endif

