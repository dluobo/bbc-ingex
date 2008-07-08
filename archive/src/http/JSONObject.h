/*
 * $Id: JSONObject.h,v 1.1 2008/07/08 16:24:41 philipn Exp $
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
 
#ifndef __RECORDER_JSON_OBJECT_H__
#define __RECORDER_JSON_OBJECT_H__


#include <string>
#include <vector>
#include "Types.h"


namespace rec
{

    
class JSONValue
{
public:
    virtual ~JSONValue() {}
    
    virtual std::string toString() = 0;
};


class JSONString : public JSONValue
{
public:
    JSONString(std::string value);
    virtual ~JSONString();
    
    virtual std::string toString();

private:
    std::string _value;
};

class JSONArray;

class JSONObject : public JSONValue
{
public:
    JSONObject();
    virtual ~JSONObject();
    
    virtual std::string toString();

    void set(std::string name, JSONValue* value);
    void setString(std::string name, std::string value);
    void setBool(std::string name, bool value);
    void setNumber(std::string name, int64_t value);
    void setNull(std::string name);
    JSONArray* setArray(std::string name);
    JSONObject* setObject(std::string name);
    
private:
    std::vector<std::pair<JSONString*, JSONValue*> > _value;
};

class JSONNumber : public JSONValue
{
public:
    JSONNumber(int64_t value);
    virtual ~JSONNumber();
    
    virtual std::string toString();

private:
    int64_t _value;
};

class JSONArray : public JSONValue
{
public:
    JSONArray();
    virtual ~JSONArray();
    
    virtual std::string toString();

    void append(JSONValue* value);
    JSONArray* appendArray();
    JSONObject* appendObject();
    
private:
    std::vector<JSONValue*> _value;
};

class JSONBool : public JSONValue
{
public:
    JSONBool(bool value);
    virtual ~JSONBool();
    
    virtual std::string toString();

private:
    bool _value;
};

class JSONNull : public JSONValue
{
public:
    JSONNull();
    virtual ~JSONNull();
    
    virtual std::string toString();
};






};





#endif

