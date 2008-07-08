/*
 * $Id: PlayerState.cpp,v 1.1 2008/07/08 16:25:36 philipn Exp $
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
 
#define __STDC_FORMAT_MACROS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "PlayerState.h"
#include "Utilities.h"
#include "Logging.h"


using namespace std;
using namespace rec;



PlayerState::PlayerState()
{}

PlayerState::~PlayerState()
{}

bool PlayerState::parse(string stateString)
{
    clear();
    
    size_t start = 0;
    size_t len = 0;
    string name;
    string value;
    bool haveNonSpace;
    while (start + len < stateString.size())
    {
        // parse name
        haveNonSpace = false;
        while (start + len < stateString.size())
        {
            if (stateString[start + len] == '=')
            {
                break;
            }

            haveNonSpace = haveNonSpace || !isspace(stateString[start + len]);
            len++;
        }
        if (start + len >= stateString.size())
        {
            if (haveNonSpace)
            {
                Logging::warning("Invalid state line '%s': missing '='\n", stateString.c_str());
                return false;
            }
            
            // done
            return true;
        }
        name = trim_string(stateString.substr(start, len));
        if (name.size() == 0)
        {
            Logging::warning("Invalid state string: zero length name\n");
            return false;
        }
        start += len + 1;
        len = 0;

        // parse value        
        while (start + len < stateString.size())
        {
            if (stateString[start + len] == '\n' || stateString[start + len] == '\r')
            {
                break;
            }
            len++;
        }
        value = trim_string(stateString.substr(start, len));
        start += len + 1;
        len = 0;

        // remove quotes
        if (value.size() >= 2 &&
            value[0] == value[value.size() - 1] &&
            (value[0] == '\'' || value[0] == '"'))
        {
            value = value.substr(1, value.size() - 2);
        }
        
        // set name/value        
        setValue(name, value);
    }
    
    return true;
}


bool PlayerState::haveValue(string name)
{
    return ihaveValue(name);
}

bool PlayerState::getInt(string name, int* value)
{
    string result;
    if (getValue(name, &result))
    {
        if (sscanf(result.c_str(), "%d", value))
        {
            return true;
        }
        Logging::warning("PlayerState value '%s' is not a valid int\n", result.c_str());
    }
    
    return false;
}

bool PlayerState::getInt64(string name, int64_t* value)
{
    string result;
    if (getValue(name, &result))
    {
        if (sscanf(result.c_str(), "%"PRId64, value))
        {
            return true;
        }
        Logging::warning("PlayerState value '%s' is not a valid int64\n", result.c_str());
    }
    
    return false;
}

bool PlayerState::getBool(string name, bool* value)
{
    string result;
    if (getValue(name, &result))
    {
        if (result.compare("true") == 0)
        {
            *value = true;
            return true;
        }
        else if (result.compare("false") == 0)
        {
            *value = false;
            return true;
        }
        Logging::warning("PlayerState value '%s' is not a valid boolean ('true' or 'false')\n", result.c_str());
    }
    
    return false;
}

bool PlayerState::getString(string name, string* value)
{
    return getValue(name, value);
}

bool PlayerState::ihaveValue(string name)
{
    return _nameAndValues.find(name) != _nameAndValues.end();
}

void PlayerState::setValue(string name, string value)
{
    pair<map<string, string>::iterator, bool> result = _nameAndValues.insert(make_pair(name, value));
    if (!result.second)
    {
        // replace the existing value
        _nameAndValues.erase(result.first);
        _nameAndValues.insert(make_pair(name, value));
    }
}

bool PlayerState::getValue(string name, string* value)
{
    map<string, string>::iterator result;
    if ((result = _nameAndValues.find(name)) != _nameAndValues.end())
    {
        *value = (*result).second;
        return true;
    }
    
    return false;
}

void PlayerState::clear()
{
    _nameAndValues.clear();
}

