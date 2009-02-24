/*
 * $Id: JSONObject.h,v 1.1 2009/02/24 08:21:16 stuart_hc Exp $
 *
 * Utility class for serialising data to JSON text strings
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#ifndef __INGEX_JSON_OBJECT_H__
#define __INGEX_JSON_OBJECT_H__


#include <string>
#include <vector>
#include "Types.h"


namespace ingex
{


class JSONValue
{
public:
    static JSONValue* parse(const char* jsonString, int* index);

public:
    virtual ~JSONValue() {}

    virtual std::string toString() = 0;
};


class JSONString : public JSONValue
{
public:
    static JSONString* parse(const char* jsonString, int* index);

public:
    JSONString(std::string value);
    virtual ~JSONString();

    virtual std::string toString();

    std::string getValue() { return _value; }

private:
    std::string _value;
};

class JSONArray;
class JSONString;
class JSONNumber;
class JSONBool;
class JSONNull;

class JSONObject : public JSONValue
{
public:
    static JSONObject* parse(const char* jsonString, int* index);

public:
    JSONObject();
    virtual ~JSONObject();

    virtual std::string toString();

    void set(std::string name, JSONValue* value);
    void setString(std::string name, std::string value);
    void setBool(std::string name, bool value);
    void setNumber(std::string name, int64_t value);
    void setNull(std::string name);
    JSONArray* setArray(std::string name, JSONArray* array);
    JSONArray* setArray(std::string name);
    JSONObject* setObject(std::string name, JSONObject* obj);
    JSONObject* setObject(std::string name);

    std::vector<std::pair<JSONString*, JSONValue*> >& getValue() { return _value; }

    JSONValue* getValue(std::string name);
    JSONString* getStringValue(std::string name);
    JSONBool* getBoolValue(std::string name);
    JSONNumber* getNumberValue(std::string name);
    JSONNull* getNullValue(std::string name);
    JSONArray* getArrayValue(std::string name);
    JSONObject* getObjectValue(std::string name);

    bool getStringValue2(std::string name, std::string* value);
    bool getBoolValue2(std::string name, bool* value);
    bool getNumberValue2(std::string name, int64_t* value);
    bool getNullValue2(std::string name);
    bool getArrayValue2(std::string name, std::vector<JSONValue*>** value);

private:
    std::vector<std::pair<JSONString*, JSONValue*> > _value;
};

class JSONNumber : public JSONValue
{
public:
    static JSONNumber* parse(const char* jsonString, int* index);

public:
    JSONNumber(int64_t value);
    virtual ~JSONNumber();

    virtual std::string toString();

    int64_t getValue() { return _value; }

private:
    int64_t _value;
};

class JSONArray : public JSONValue
{
public:
    static JSONArray* parse(const char* jsonString, int* index);

public:
    JSONArray();
    virtual ~JSONArray();

    virtual std::string toString();

    void append(JSONValue* value);
    JSONArray* appendArray(JSONArray* array);
    JSONArray* appendArray();
    JSONObject* appendObject(JSONObject* obj);
    JSONObject* appendObject();

    std::vector<JSONValue*>& getValue() { return _value; }

private:
    std::vector<JSONValue*> _value;
};

class JSONBool : public JSONValue
{
public:
    static JSONBool* parse(const char* jsonString, int* index);

public:
    JSONBool(bool value);
    virtual ~JSONBool();

    virtual std::string toString();


    bool getValue() { return _value; }

private:
    bool _value;
};

class JSONNull : public JSONValue
{
public:
    static JSONNull* parse(const char* jsonString, int* index);

public:
    JSONNull();
    virtual ~JSONNull();

    virtual std::string toString();
};



};


#endif
