/*
 * $Id: Config.cpp,v 1.1 2008/07/08 16:23:25 philipn Exp $
 *
 * Read, write and store an application's configuration options
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

/*
    An application would typically use setSpec() to set a configuration spec, 
    load a configuration using readFromFile() and then call validate().
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "Config.h"
#include "Utilities.h"
#include "RecorderException.h"


using namespace std;
using namespace rec;


static string read_next_line(FILE* file)
{
    int c;
    string line;

    // move file pointer past the newline characters
    while ((c = fgetc(file)) != EOF && (c == '\r' || c == '\n'))
    {}
    
    while (c != EOF && (c != '\r' && c != '\n'))
    {
        line += c;
        c = fgetc(file);
    }
    
    return line;
}



Config Config::_instance;
Mutex Config::_instanceMutex;


Config::Config()
{}

Config::~Config()
{}

bool Config::setSpec(const ConfigSpec* specs, int numConfigs)
{
    LOCK_SECTION(_instanceMutex);

    int i;
    for (i = 0; i < numConfigs; i++)
    {
        if (specs[i].name == 0 || strlen(specs[i].name) == 0)
        {
            return false;
        }
        
        // note: initialising as "ValueSpec valueSpec; valueSpec.type = ..."
        // causes non-null padding bytes (why?) resulting in a false failure of 
        // the memcmp below
        ValueSpec valueSpec = {specs[i].type, specs[i].isRequired};
        
        pair<map<string, ValueSpec>::iterator, bool> result = _instance._spec.insert(make_pair(specs[i].name, valueSpec));
        if (!result.second)
        {
            if (memcmp(&(*result.first).second, &valueSpec, sizeof(valueSpec)) != 0)
            {
                // replace the existing spec
                _instance._spec.erase(result.first);
                _instance._spec.insert(make_pair(specs[i].name, valueSpec));
            }
        }
        
        if (specs[i].defaultValue != 0 && !_instance.ihaveValue(specs[i].name))
        {
            _instance.setValue(specs[i].name, specs[i].defaultValue);
        }
    }
    
    return true;
}

bool Config::validate(string* error)
{
    LOCK_SECTION(_instanceMutex);

    string value;
    map<string, ValueSpec>::const_iterator iter;
    for (iter = _instance._spec.begin(); iter != _instance._spec.end(); iter++)
    {
        const string& name = (*iter).first;
        const ValueSpec& valueSpec = (*iter).second;
        
        if (_instance.ihaveValue(name))
        {
            switch (valueSpec.type)
            {
                case STRING_CONFIG_TYPE:
                    // anything goes
                    break;
                case NON_EMPTY_STRING_CONFIG_TYPE:
                    if (_instance.iisEmptyValue(name))
                    {
                        *error = name;
                        *error += ": String is empty";
                        return false;
                    }
                    break;
                case INT_CONFIG_TYPE:
                    if (!_instance.iisValidInt(name))
                    {
                        *error = name;
                        *error += ": Invalid integer value";
                        return false;
                    }
                    break;
                case BOOL_CONFIG_TYPE:
                    if (!_instance.iisValidBool(name))
                    {
                        *error = name;
                        *error += ": Invalid boolean value";
                        return false;
                    }
                    break;
                case INT_ARRAY_CONFIG_TYPE:
                    if (!_instance.iisValidIntArray(name))
                    {
                        *error = name;
                        *error += ": Invalid integer array value";
                        return false;
                    }
                    break;
                case STRING_ARRAY_CONFIG_TYPE:
                    if (!_instance.iisValidStringArray(name))
                    {
                        *error = name;
                        *error += ": Invalid string array value";
                        return false;
                    }
                    break;
                case NON_EMPTY_STRING_ARRAY_CONFIG_TYPE:
                    if (!_instance.iisValidNonEmptyStringArray(name))
                    {
                        *error = name;
                        *error += ": Invalid non-empty string array value";
                        return false;
                    }
                    break;
            }
        }
        else
        {
            if (valueSpec.isRequired)
            {
                *error = name;
                *error += ": Missing required value";
                return false;
            }
        }
    }
    
    return true;
}

// TODO/Note: if this fails then a partial set of name/value pairs could have been entered
bool Config::readFromFile(string filename)
{
    LOCK_SECTION(_instanceMutex);

    FILE* file;
    if ((file = fopen(filename.c_str(), "rb")) == NULL)
    {
        fprintf(stderr, "Failed to open log file '%s' for reading: %s\n", filename.c_str(), strerror(errno));
        return false;
    }
    
    size_t start;
    size_t len;
    string name;
    string value;
    string line;
    bool haveNonSpace;
    
    while (true)
    {
        line = read_next_line(file);
        if (line.size() == 0)
        {
            // done
            break;
        }
        
        // parse name
        start = 0;
        len = 0;   
        haveNonSpace = false;
        while (start + len < line.size())
        {
            if (line[start + len] == '=')
            {
                break;
            }
            else if (line[start + len] == '#')
            {
                // comment line
                len = line.size() - start; // force same as empty line
                break;
            }
            
            haveNonSpace = haveNonSpace || !isspace(line[start + len]);
            len++;
        }
        if (start + len >= line.size())
        {
            if (haveNonSpace)
            {
                fprintf(stderr, "Invalid config line '%s': missing '='\n", line.c_str());
                fclose(file);
                return false;
            }
            
            // empty or comment line
            continue;
        }
        name = trim_string(line.substr(start, len));
        if (name.size() == 0)
        {
            fprintf(stderr, "Invalid config line '%s': zero length name\n", line.c_str());
            fclose(file);
            return false;
        }
        start += len + 1;
        len = 0;

        // parse value        
        while (start + len < line.size())
        {
            if (line[start + len] == '#')
            {
                break;
            }
            len++;
        }
        value = trim_string(line.substr(start, len));

        // remove quotes
        if (value.size() >= 2 &&
            value[0] == value[value.size() - 1] &&
            (value[0] == '\'' || value[0] == '"'))
        {
            value = value.substr(1, value.size() - 2);
        }

        // set name/value        
        _instance.setValue(name, value);
    }
    
    fclose(file);
    return true;
}

// TODO/Note: if this fails then a partial set of name/value pairs could have been entered
bool Config::readFromString(string configString)
{
    LOCK_SECTION(_instanceMutex);

    size_t start = 0;
    size_t len = 0;
    string name;
    string value;
    bool haveNonSpace;
    while (start + len < configString.size())
    {
        // parse name
        haveNonSpace = false;
        while (start + len < configString.size())
        {
            if (configString[start + len] == '=')
            {
                break;
            }
            else if (configString[start + len] == ';')
            {
                fprintf(stderr, "Invalid config string: end of name/value before '=' found\n");
                return false;
            }

            haveNonSpace = haveNonSpace || !isspace(configString[start + len]);
            len++;
        }
        if (start + len >= configString.size())
        {
            if (haveNonSpace)
            {
                fprintf(stderr, "Invalid config line '%s': missing '='\n", configString.c_str());
                return false;
            }
            
            // done
            return true;
        }
        name = trim_string(configString.substr(start, len));
        if (name.size() == 0)
        {
            fprintf(stderr, "Invalid config string: zero length name\n");
            return false;
        }
        start += len + 1;
        len = 0;

        // parse value        
        while (start + len < configString.size())
        {
            if (configString[start + len] == ';')
            {
                break;
            }
            len++;
        }
        value = trim_string(configString.substr(start, len));
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
        _instance.setValue(name, value);
    }
    
    return true;
}


bool Config::writeToFile(string filename)
{
    LOCK_SECTION(_instanceMutex);
    
    FILE* file;
    if ((file = fopen(filename.c_str(), "wb")) == NULL)
    {
        fprintf(stderr, "Failed to open config file '%s' for writing: %s\n", filename.c_str(), strerror(errno));
        return false;
    }

    map<string, string>::const_iterator iter;
    for (iter = _instance._nameAndValues.begin(); iter != _instance._nameAndValues.end(); iter++)
    {
        if (fprintf(file, "%s = '%s'\n", (*iter).first.c_str(), (*iter).second.c_str()) < 0)
        {
            fprintf(stderr, "Failed to write to config file '%s': %s\n", filename.c_str(), strerror(errno));
            fclose(file);
            return false;
        }
    }    
    return true;
}

string Config::writeToString()
{
    LOCK_SECTION(_instanceMutex);
    
    string result;
    map<string, string>::const_iterator iter;
    for (iter = _instance._nameAndValues.begin(); iter != _instance._nameAndValues.end(); iter++)
    {
        result += (*iter).first + "='" + (*iter).second + "';";
    }
    
    return result;
}

bool Config::haveValue(string name)
{
    LOCK_SECTION(_instanceMutex);
    
    return _instance.ihaveValue(name);
}

bool Config::isValidInt(string name)
{
    LOCK_SECTION(_instanceMutex);
    
    return _instance.iisValidInt(name);
}

bool Config::isValidBool(string name)
{
    LOCK_SECTION(_instanceMutex);
    
    return _instance.iisValidBool(name);
}

bool Config::isValidIntArray(string name)
{
    LOCK_SECTION(_instanceMutex);
    
    return _instance.iisValidIntArray(name);
}

bool Config::isValidStringArray(string name)
{
    LOCK_SECTION(_instanceMutex);
    
    return _instance.iisValidStringArray(name);
}

void Config::setInt(string name, int value)
{
    LOCK_SECTION(_instanceMutex);
    
    char buf[16];
    sprintf(buf, "%d", value);
    
    _instance.setValue(name, buf);
}

void Config::setBool(string name, bool value)
{
    LOCK_SECTION(_instanceMutex);

    if (value)
    {
        _instance.setValue(name, "true");
    }
    else
    {
        _instance.setValue(name, "false");
    }
}

void Config::setString(string name, string value)
{
    LOCK_SECTION(_instanceMutex);

    _instance.setValue(name, value);
}

void Config::setIntArray(string name, vector<int> value)
{
    LOCK_SECTION(_instanceMutex);

    char buf[16];
    string storeValue;
    vector<int>::const_iterator iter;
    for (iter = value.begin(); iter != value.end(); iter++)
    {
        if (iter != value.begin())
        {
            storeValue.append(",");
        }
        sprintf(buf, "%d", *iter);
        storeValue.append(buf); 
    }
    
    _instance.setValue(name, storeValue);
}

void Config::setStringArray(string name, vector<string> value)
{
    LOCK_SECTION(_instanceMutex);

    string storeValue;
    vector<string>::const_iterator iter;
    for (iter = value.begin(); iter != value.end(); iter++)
    {
        if (iter != value.begin())
        {
            storeValue.append(",");
        }
        storeValue.append(*iter); 
    }
    
    _instance.setValue(name, storeValue);
}

bool Config::getInt(string name, int* value)
{
    LOCK_SECTION(_instanceMutex);
    
    string result;
    if (_instance.getValue(name, &result))
    {
        if (sscanf(result.c_str(), "%d", value) == 1)
        {
            return true;
        }
        fprintf(stderr, "Config value '%s' is not a valid int\n", result.c_str());
    }
    
    return false;
}

bool Config::getBool(string name, bool* value)
{
    LOCK_SECTION(_instanceMutex);
    
    string result;
    if (_instance.getValue(name, &result))
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
        fprintf(stderr, "Config value '%s' is not a valid boolean ('true' or 'false')\n", result.c_str());
    }
    
    return false;
}

bool Config::getString(string name, string* value)
{
    LOCK_SECTION(_instanceMutex);
    
    return _instance.getValue(name, value);
}

bool Config::getIntArray(string name, vector<int>* value)
{
    LOCK_SECTION(_instanceMutex);
    
    string result;
    if (!_instance.getValue(name, &result))
    {
        value->clear();
        return false;
    }
    
    int intValue;
    size_t prevPos = 0;
    size_t pos = 0;
    while ((pos = result.find(",", prevPos)) != string::npos)
    {
        if (sscanf(result.substr(prevPos, pos - prevPos).c_str(), "%d", &intValue) != 1)
        {
            value->clear();
            return false;
        }
        value->push_back(intValue);
        
        prevPos = pos + 1;
    }
    if (prevPos < result.size())
    {
        if (sscanf(result.substr(prevPos, result.size() - prevPos).c_str(), "%d", &intValue) != 1)
        {
            value->clear();
            return false;
        }
        value->push_back(intValue);
    }
    
    return true;
}

bool Config::getStringArray(string name, vector<string>* value)
{
    LOCK_SECTION(_instanceMutex);
    
    string result;
    if (!_instance.getValue(name, &result))
    {
        value->clear();
        return false;
    }
    
    size_t prevPos = 0;
    size_t pos = 0;
    while ((pos = result.find(",", prevPos)) != string::npos)
    {
        value->push_back(result.substr(prevPos, pos - prevPos));
        prevPos = pos + 1;
    }
    if (prevPos < result.size())
    {
        value->push_back(result.substr(prevPos, result.size() - prevPos));
    }
    
    return true;
}

int Config::getIntD(string name, int defaultValue)
{
    int value;
    if (getInt(name, &value))
    {
        return value;
    }
    
    return defaultValue;
}

bool Config::getBoolD(string name, bool defaultValue)
{
    bool value;
    if (getBool(name, &value))
    {
        return value;
    }
    
    return defaultValue;
}

string Config::getStringD(string name, string defaultValue)
{
    string value;
    if (getString(name, &value))
    {
        return value;
    }
    
    return defaultValue;
}

vector<int> Config::getIntArray(string name, vector<int> defaultValue)
{
    vector<int> value;
    if (getIntArray(name, &value))
    {
        return value;
    }
    
    return defaultValue;
}

vector<string> Config::getStringArray(string name, vector<string> defaultValue)
{
    vector<string> value;
    if (getStringArray(name, &value))
    {
        return value;
    }
    
    return defaultValue;
}

int Config::getInt(string name)
{
    int value;
    REC_CHECK(getInt(name, &value));

    return value;
}

bool Config::getBool(string name)
{
    bool value;
    REC_CHECK(getBool(name, &value));

    return value;
}

string Config::getString(string name)
{
    string value;
    REC_CHECK(getString(name, &value));
    
    return value;
}

vector<int> Config::getIntArray(string name)
{
    vector<int> value;
    REC_CHECK(getIntArray(name, &value));
    
    return value;
}

vector<string> Config::getStringArray(string name)
{
    vector<string> value;
    REC_CHECK(getStringArray(name, &value));
    
    return value;
}

bool Config::clearValue(string name)
{
    LOCK_SECTION(_instanceMutex);
    
    return _instance.iclearValue(name);
}

void Config::clear()
{
    LOCK_SECTION(_instanceMutex);
    
    _instance._nameAndValues.clear();
}


bool Config::ihaveValue(string name)
{
    return _nameAndValues.find(name) != _nameAndValues.end();
}

bool Config::iisValidInt(string name)
{
    string result;
    if (!_instance.getValue(name, &result))
    {
        return false;
    }
    
    int value;
    if (sscanf(result.c_str(), "%d", &value) != 1)
    {
        return false;
    }
    
    return true;
}

bool Config::iisEmptyValue(string name)
{
    string result;
    _instance.getValue(name, &result);
    return result.empty();
}

bool Config::iisValidBool(string name)
{
    string result;
    if (!_instance.getValue(name, &result))
    {
        return false;
    }
    
    if (result.compare("true") != 0 && result.compare("false") != 0)
    {
        return false;
    }
    
    return true;
}

bool Config::iisValidIntArray(string name)
{
    string result;
    if (!_instance.getValue(name, &result))
    {
        return false;
    }
    
    int intValue;
    size_t prevPos = 0;
    size_t pos = 0;
    while ((pos = result.find(",", prevPos)) != string::npos)
    {
        if (sscanf(result.substr(prevPos, pos - prevPos).c_str(), "%d", &intValue) != 1)
        {
            return false;
        }
        
        prevPos = pos + 1;
    }
    if (prevPos < result.size())
    {
        if (sscanf(result.substr(prevPos, result.size() - prevPos).c_str(), "%d", &intValue) != 1)
        {
            return false;
        }
    }
    
    return true;
}

bool Config::iisValidStringArray(string name)
{
    string result;
    if (!_instance.getValue(name, &result))
    {
        return false;
    }
    
    return true;
}

bool Config::iisValidNonEmptyStringArray(string name)
{
    string result;
    if (!_instance.getValue(name, &result))
    {
        return false;
    }
    
    size_t prevPos = 0;
    size_t pos = 0;
    while ((pos = result.find(",", prevPos)) != string::npos)
    {
        if (result.substr(prevPos, pos - prevPos).empty())
        {
            return false;
        }
        prevPos = pos + 1;
    }
    if (prevPos < result.size())
    {
        if (result.substr(prevPos, result.size() - prevPos).empty())
        {
            return false;
        }
    }
    
    return true;
}

void Config::setValue(string name, string value)
{
    pair<map<string, string>::iterator, bool> result = _nameAndValues.insert(make_pair(name, value));
    if (!result.second)
    {
        // replace the existing value
        _nameAndValues.erase(result.first);
        _nameAndValues.insert(make_pair(name, value));
    }
}

bool Config::getValue(string name, string* value)
{
    map<string, string>::iterator result;
    if ((result = _nameAndValues.find(name)) != _nameAndValues.end())
    {
        *value = (*result).second;
        return true;
    }
    
    return false;
}

bool Config::iclearValue(string name)
{
    map<string, string>::iterator result;
    if ((result = _nameAndValues.find(name)) != _nameAndValues.end())
    {
        _nameAndValues.erase(result);
        return true;
    }
    
    return false;
}


