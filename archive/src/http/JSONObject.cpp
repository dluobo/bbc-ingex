/*
 * $Id: JSONObject.cpp,v 1.1 2008/07/08 16:24:40 philipn Exp $
 *
 * Utility class for serialising data to JSON text strings
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

#include <sstream>

#include "JSONObject.h"
#include "Utilities.h"


using namespace std;
using namespace rec;


// we assume 8-bit ascii characters!
// TODO: support full UTF-8 range
string json_escape(string value)
{
    char buf[16];
    const char decToHex[] = 
        {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    size_t index = 0;
    char c;
    
    while (index < value.size())
    {
        c = value.at(index);
        switch (c)
        {
            case '\b':
                value.replace(index, 1, "\\b");
                index += 2;
                break;
            case '\f':
                value.replace(index, 1, "\\f");
                index += 2;
                break;
            case '\n':
                value.replace(index, 1, "\\n");
                index += 2;
                break;
            case '\r':
                value.replace(index, 1, "\\r");
                index += 2;
                break;
            case '\t':
                value.replace(index, 1, "\\t");
                index += 2;
                break;
            case '"':
                value.replace(index, 1, "\\\"");
                index += 2;
                break;
            case '\\':
                value.replace(index, 1, "\\\\");
                index += 2;
                break;
            case '/':
                value.replace(index, 1, "\\/");
                index += 2;
                break;
            default:
                if (c < 0x20) // control characters
                {
                    sprintf(buf, "\\u00%c%c", 
                        decToHex[(c >> 4) & 0x0f],
                        decToHex[c & 0x0f]);
                    value.replace(index, 1, buf);
                    index += 6;
                }
                else
                {
                    index++;
                }
        }
    }
    
    return value;
}



JSONObject::JSONObject()
{}

JSONObject::~JSONObject()
{
    vector<pair<JSONString*, JSONValue*> >::const_iterator iter;
    for (iter = _value.begin(); iter != _value.end(); iter++)
    {
        delete (*iter).first;
        delete (*iter).second;
    }
}

string JSONObject::toString()
{
    ostringstream out;
    
    out << "{" << endl;
        
    vector<pair<JSONString*, JSONValue*> >::const_iterator iter;
    for (iter = _value.begin(); iter != _value.end(); iter++)
    {
        out << (*iter).first->toString() << " : " << (*iter).second->toString();
        if (iter + 1 != _value.end())
        {
            out << ", " << endl;
        }
        else
        {
            out << endl;
        }
    }
    
    out << "}";
    
    return out.str();
}

void JSONObject::set(string name, JSONValue* value)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), value));
}

void JSONObject::setString(string name, string value)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), new JSONString(value)));
}

void JSONObject::setBool(string name, bool value)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), new JSONBool(value)));
}

void JSONObject::setNumber(string name, int64_t value)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), new JSONNumber(value)));
}

void JSONObject::setNull(string name)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), new JSONNull()));
}

JSONArray* JSONObject::setArray(std::string name)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), new JSONArray()));
    return dynamic_cast<JSONArray*>(_value.back().second);
}

JSONObject* JSONObject::setObject(std::string name)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), new JSONObject()));
    return dynamic_cast<JSONObject*>(_value.back().second);
}


JSONString::JSONString(string value)
: _value(value)
{}

JSONString::~JSONString()
{
}

string JSONString::toString()
{
    ostringstream out;
    out << "\"" << json_escape(_value) << "\"";
    return out.str();
}


JSONNumber::JSONNumber(int64_t value)
: _value(value)
{
}

JSONNumber::~JSONNumber()
{
}

string JSONNumber::toString()
{
    return int64_to_string(_value);
}


JSONArray::JSONArray()
{}

JSONArray::~JSONArray()
{
    vector<JSONValue*>::const_iterator iter;
    for (iter = _value.begin(); iter != _value.end(); iter++)
    {
        delete (*iter);
    }
}

string JSONArray::toString()
{
    ostringstream out;
    
    out << "[" << endl;
        
    vector<JSONValue*>::const_iterator iter;
    for (iter = _value.begin(); iter != _value.end(); iter++)
    {
        out << (*iter)->toString();
        if (iter + 1 != _value.end())
        {
            out << ", " << endl;
        }
        else
        {
            out << endl;
        }
    }
    
    out << "]";
    
    return out.str();
}

void JSONArray::append(JSONValue* value)
{
    _value.push_back(value);
}

JSONArray* JSONArray::appendArray()
{
    _value.push_back(new JSONArray());
    return dynamic_cast<JSONArray*>(_value.back());
}

JSONObject* JSONArray::appendObject()
{
    _value.push_back(new JSONObject());
    return dynamic_cast<JSONObject*>(_value.back());
}


JSONBool::JSONBool(bool value)
: _value(value)
{}

JSONBool::~JSONBool()
{}

string JSONBool::toString()
{
    return _value ? "true" : "false";
}


JSONNull::JSONNull()
{}

JSONNull::~JSONNull()
{}

string JSONNull::toString()
{
    return "null";
}

