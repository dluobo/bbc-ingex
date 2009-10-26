/*
 * $Id: JSONObject.cpp,v 1.2 2009/10/26 09:33:20 john_f Exp $
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

#define __STDC_FORMAT_MACROS 1

#include <sstream>
#include <cstring>

#include "JSONObject.h"
#include "Utilities.h"


using namespace std;
using namespace ingex;


//#define DEBUG_JSON 1

#if defined(DEBUG_JSON)

#define CHECK_PARSE(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "Failed to parse %d\n", __LINE__); \
        return 0; \
    }
    else \
    { \
        printf("Command = %s\n", #cmd); \
    }

#define CHECK_PARSE_BOOL(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "Failed to parse %d\n", __LINE__); \
        return false; \
    }
    else \
    { \
        printf("Command = %s\n", #cmd); \
    }

#else

#define CHECK_PARSE(cmd) \
    if (!(cmd)) \
    { \
        return 0; \
    }

#define CHECK_PARSE_BOOL(cmd) \
    if (!(cmd)) \
    { \
        return false; \
    }

#endif


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

static bool parse_hex_value(const char* digits, int numDigits, int* value)
{
    int count = 0;

    *value = 0;

    while (count < numDigits && digits[count] != '\0')
    {
        *value <<= 4;
        if (digits[count] >= '0' && digits[count] <= '9')
        {
            *value |= digits[count] - '0';
        }
        else if (digits[count] >= 'a' && digits[count] <= 'f')
        {
            *value |= digits[count] - 'a';
        }
        else if (digits[count] >= 'A' && digits[count] <= 'F')
        {
            *value |= digits[count] - 'A';
        }
        else
        {
            return false;
        }

        count++;
    }
    if (count != numDigits)
    {
        return false;
    }

    return true;
}

static bool json_unescape(string value, string* result)
{
    size_t index = 0;
    size_t size = value.size();
    int hexValue;

    *result = "";

    while (index < size)
    {
        if (value[index] < 0x20)
        {
            return false;
        }
        else if (value[index] == '\\')
        {
            switch (value[index + 1])
            {
                case 'b':
                    result->append("\b");
                    index += 2;
                    break;
                case 'f':
                    result->append("\f");
                    index += 2;
                    break;
                case 'n':
                    result->append("\n");
                    index += 2;
                    break;
                case 'r':
                    result->append("\r");
                    index += 2;
                    break;
                case 't':
                    result->append("\t");
                    index += 2;
                    break;
                case '"':
                    result->append("\"");
                    index += 2;
                    break;
                case '\\':
                    result->append("\\");
                    index += 2;
                    break;
                case '/':
                    result->append("/");
                    index += 2;
                    break;
                case 'u':
                    CHECK_PARSE(parse_hex_value(&value[index], 4, &hexValue));
                    result->append(1, (char)((hexValue >> 24) && 0xff));
                    result->append(1, (char)((hexValue >> 16) && 0xff));
                    result->append(1, (char)((hexValue >> 8) && 0xff));
                    result->append(1, (char)(hexValue && 0xff));
                    index += 6;
                    break;
                default:
                    return false;
            }
        }
        else
        {
            result->append(1, value[index]);
            index++;
        }
    }

    return true;
}

static bool is_json_space(const char c)
{
    return c == ' ' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t';
}

static void skip_space(const char* jsonString, int* index)
{
    while (jsonString[*index] != '\0' && is_json_space(jsonString[*index]))
    {
        (*index)++;
    }
}

static bool parse_string(const char* jsonString, int* index, std::string* result)
{
    int newIndex = *index;
    string escapedString;
    int endIndex;
    bool escape;

    // opening quote
    skip_space(jsonString, &newIndex);
    CHECK_PARSE_BOOL(jsonString[newIndex] == '"');
    newIndex++;

    // string
    endIndex = newIndex;
    escape = false;
    while (jsonString[endIndex] != '\0' &&
        (jsonString[endIndex] != '"' || escape))
    {
        if (escape)
        {
            escape = false;
        }
        else if (jsonString[endIndex] == '\\')
        {
            escape = true;
        }

        endIndex++;
    }

    // closing quote
    CHECK_PARSE_BOOL(jsonString[endIndex] == '"');
    endIndex++;

    if (endIndex >= newIndex + 1)
    {
        escapedString.append(jsonString, newIndex, endIndex - newIndex - 1);
        CHECK_PARSE_BOOL(json_unescape(escapedString, result));
    }

    *index = endIndex;
    return true;
}



JSONValue* JSONValue::parse(const char* jsonString, int* index)
{
    int newIndex = *index;
    JSONValue* result;

    skip_space(jsonString, &newIndex);

    if (jsonString[newIndex] == '"')
    {
        result = JSONString::parse(jsonString, &newIndex);
    }
    else if (jsonString[newIndex] == '-' ||
        (jsonString[newIndex] >= '0' &&  jsonString[newIndex] <= '9'))
    {
        result = JSONNumber::parse(jsonString, &newIndex);
    }
    else if (jsonString[newIndex] == '{')
    {
        result = JSONObject::parse(jsonString, &newIndex);
    }
    else if (jsonString[newIndex] == '[')
    {
        result = JSONArray::parse(jsonString, &newIndex);
    }
    else
    {
        result = JSONBool::parse(jsonString, &newIndex);
        if (result == 0)
        {
            result = JSONNull::parse(jsonString, &newIndex);
        }
    }

    if (result == 0)
    {
        return 0;
    }

    *index = newIndex;
    return result;
}



JSONObject* JSONObject::parse(const char* jsonString, int* index)
{
    int newIndex = *index;
    string name;
    JSONValue* value;
    auto_ptr<JSONObject> result(new JSONObject());

    // opening brace
    skip_space(jsonString, &newIndex);
    CHECK_PARSE(jsonString[newIndex] == '{');
    newIndex++;

    // members
    skip_space(jsonString, &newIndex);
    if (jsonString[newIndex] != '}')
    {
        while (jsonString[newIndex] != '\0')
        {
            // name
            CHECK_PARSE(parse_string(jsonString, &newIndex, &name));
            skip_space(jsonString, &newIndex);

            // : separator
            CHECK_PARSE(jsonString[newIndex] == ':');
            newIndex++;

            // value
            skip_space(jsonString, &newIndex);
            CHECK_PARSE((value = JSONValue::parse(jsonString, &newIndex)) != 0);

            result->set(name, value);

            // , separator
            skip_space(jsonString, &newIndex);
            if (jsonString[newIndex] != ',')
            {
                break;
            }
            newIndex++;
            skip_space(jsonString, &newIndex);
        }
    }

    // closing brace
    if (jsonString[newIndex] != '}')
    {
        printf("%s\n", &jsonString[newIndex]);
    }
    CHECK_PARSE(jsonString[newIndex] == '}');
    newIndex++;

    *index = newIndex;
    return result.release();
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

JSONArray* JSONObject::setArray(std::string name, JSONArray* array)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), array));
    return dynamic_cast<JSONArray*>(_value.back().second);
}

JSONArray* JSONObject::setArray(std::string name)
{
    return setArray(name, new JSONArray());
}

JSONObject* JSONObject::setObject(std::string name, JSONObject* obj)
{
    _value.push_back(pair<JSONString*, JSONValue*>(new JSONString(name), obj));
    return dynamic_cast<JSONObject*>(_value.back().second);
}

JSONObject* JSONObject::setObject(std::string name)
{
    return setObject(name, new JSONObject());
}

JSONValue* JSONObject::getValue(string name)
{
    vector<std::pair<JSONString*, JSONValue*> >::const_iterator iter;
    for (iter = _value.begin(); iter != _value.end(); iter++)
    {
        if ((*iter).first->getValue().compare(name) == 0)
        {
            return (*iter).second;
        }
    }

    return 0;
}

JSONString* JSONObject::getStringValue(string name)
{
    JSONValue* value = getValue(name);
    if (value == 0)
    {
        return 0;
    }
    return dynamic_cast<JSONString*>(value);
}

JSONBool* JSONObject::getBoolValue(string name)
{
    JSONValue* value = getValue(name);
    if (value == 0)
    {
        return 0;
    }
    return dynamic_cast<JSONBool*>(value);
}

JSONNumber* JSONObject::getNumberValue(string name)
{
    JSONValue* value = getValue(name);
    if (value == 0)
    {
        return 0;
    }
    return dynamic_cast<JSONNumber*>(value);
}

JSONNull* JSONObject::getNullValue(string name)
{
    JSONValue* value = getValue(name);
    if (value == 0)
    {
        return 0;
    }
    return dynamic_cast<JSONNull*>(value);
}

JSONArray* JSONObject::getArrayValue(string name)
{
    JSONValue* value = getValue(name);
    if (value == 0)
    {
        return 0;
    }
    return dynamic_cast<JSONArray*>(value);
}

JSONObject* JSONObject::getObjectValue(string name)
{
    JSONValue* value = getValue(name);
    if (value == 0)
    {
        return 0;
    }
    return dynamic_cast<JSONObject*>(value);
}

bool JSONObject::getStringValue2(string name, string* value)
{
    JSONString* jsonValue = getStringValue(name);
    if (jsonValue == 0)
    {
        return false;
    }

    *value = jsonValue->getValue();
    return true;
}

bool JSONObject::getBoolValue2(string name, bool* value)
{
    JSONBool* jsonValue = getBoolValue(name);
    if (jsonValue == 0)
    {
        return false;
    }

    *value = jsonValue->getValue();
    return true;
}

bool JSONObject::getNumberValue2(string name, int64_t* value)
{
    JSONNumber* jsonValue = getNumberValue(name);
    if (jsonValue == 0)
    {
        return false;
    }

    *value = jsonValue->getValue();
    return true;
}

bool JSONObject::getNullValue2(string name)
{
    JSONNull* jsonValue = getNullValue(name);
    if (jsonValue == 0)
    {
        return false;
    }

    return true;
}

bool JSONObject::getArrayValue2(string name, std::vector<JSONValue*>** value)
{
    JSONArray* jsonValue = getArrayValue(name);
    if (jsonValue == 0)
    {
        return false;
    }

    *value = &jsonValue->getValue();
    return true;
}


JSONString* JSONString::parse(const char* jsonString, int* index)
{
    int newIndex = *index;
    string result;

    skip_space(jsonString, &newIndex);

    CHECK_PARSE(parse_string(jsonString, &newIndex, &result));

    *index = newIndex;
    return new JSONString(result);
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


// TODO: support the whole number type range
JSONNumber* JSONNumber::parse(const char* jsonString, int* index)
{
    int newIndex = *index;
    int64_t value;

    skip_space(jsonString, &newIndex);

    if (sscanf(&jsonString[newIndex], "%"PRId64"", &value) != 1)
    {
        return 0;
    }

    // skip number characters
    while (jsonString[newIndex] == '-' ||
        (jsonString[newIndex] >= '0' &&  jsonString[newIndex] <= '9'))
    {
        newIndex++;
    }

    *index = newIndex;
    return new JSONNumber(value);
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


JSONArray* JSONArray::parse(const char* jsonString, int* index)
{
    int newIndex = *index;
    string name;
    JSONValue* value;
    auto_ptr<JSONArray> result(new JSONArray());

    // opening brace
    skip_space(jsonString, &newIndex);
    CHECK_PARSE(jsonString[newIndex] == '[');
    newIndex++;

    // elements
    skip_space(jsonString, &newIndex);
    if (jsonString[newIndex] != ']')
    {
        while (jsonString[newIndex] != '\0')
        {
            CHECK_PARSE((value = JSONValue::parse(jsonString, &newIndex)) != 0);

            result->append(value);

            // , separator
            skip_space(jsonString, &newIndex);
            if (jsonString[newIndex] != ',')
            {
                break;
            }
            newIndex++;
            skip_space(jsonString, &newIndex);
        }
    }

    // closing brace
    CHECK_PARSE(jsonString[newIndex] == ']');
    newIndex++;

    *index = newIndex;
    return result.release();
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

JSONArray* JSONArray::appendArray(JSONArray* array)
{
    _value.push_back(array);
    return dynamic_cast<JSONArray*>(_value.back());
}

JSONArray* JSONArray::appendArray()
{
    return appendArray(new JSONArray());
}

JSONObject* JSONArray::appendObject(JSONObject* obj)
{
    _value.push_back(obj);
    return dynamic_cast<JSONObject*>(_value.back());
}

JSONObject* JSONArray::appendObject()
{
    return appendObject(new JSONObject());
}


JSONBool* JSONBool::parse(const char* jsonString, int* index)
{
    int newIndex = *index;
    bool result;

    skip_space(jsonString, &newIndex);

    if (strncmp(&jsonString[newIndex], "true", strlen("true")) == 0)
    {
        result = true;
        newIndex += strlen("true");
    }
    else if (strncmp(&jsonString[newIndex], "false", strlen("false")) == 0)
    {
        result = false;
        newIndex += strlen("false");
    }
    else
    {
        return 0;
    }

    *index = newIndex;
    return new JSONBool(result);
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


JSONNull* JSONNull::parse(const char* jsonString, int* index)
{
    int newIndex = *index;

    skip_space(jsonString, &newIndex);

    if (strncmp(&jsonString[newIndex], "null", strlen("null")) != 0)
    {
        return 0;
    }

    newIndex += strlen("null");

    *index = newIndex;
    return new JSONNull();
}

JSONNull::JSONNull()
{}

JSONNull::~JSONNull()
{}

string JSONNull::toString()
{
    return "null";
}
