/*
 * $Id: Config.h,v 1.1 2008/07/08 16:23:32 philipn Exp $
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
 
#ifndef __RECORDER_CONFIG_H__
#define __RECORDER_CONFIG_H__


#include <string>
#include <map>
#include <vector>

#include "Threads.h"


namespace rec
{

typedef enum ConfigValueType
{
    STRING_CONFIG_TYPE,
    NON_EMPTY_STRING_CONFIG_TYPE,
    INT_CONFIG_TYPE,
    BOOL_CONFIG_TYPE,
    INT_ARRAY_CONFIG_TYPE,
    STRING_ARRAY_CONFIG_TYPE,
    NON_EMPTY_STRING_ARRAY_CONFIG_TYPE,
};
    
typedef struct
{
    const char* name;
    ConfigValueType type;
    bool isRequired;
    const char* defaultValue;
} ConfigSpec;
    

class Config
{
public:
    ~Config();
    
    static bool setSpec(const ConfigSpec* specs, int numConfigs);
    static bool validate(std::string* error);
    
    static bool readFromFile(std::string filename);
    static bool readFromString(std::string configString);
    static bool writeToFile(std::string filename);
    static std::string writeToString();
    
    static bool haveValue(std::string name);
    static bool isValidInt(std::string name);
    static bool isValidBool(std::string name);
    static bool isValidIntArray(std::string name);
    static bool isValidStringArray(std::string name);
    
    static void setInt(std::string name, int value);
    static void setBool(std::string name, bool value);
    static void setString(std::string name, std::string value);
    static void setIntArray(std::string name, std::vector<int> value);
    static void setStringArray(std::string name, std::vector<std::string> value);
    
    static bool getInt(std::string name, int* value);
    static bool getBool(std::string name, bool* value);
    static bool getString(std::string name, std::string* value);
    static bool getIntArray(std::string name, std::vector<int>* value);
    static bool getStringArray(std::string name, std::vector<std::string>* value);
    
    static int getIntD(std::string name, int defaultValue);
    static bool getBoolD(std::string name, bool defaultValue);
    static std::string getStringD(std::string name, std::string defaultValue);
    static std::vector<int> getIntArray(std::string name, std::vector<int> defaultValue);
    static std::vector<std::string> getStringArray(std::string name, std::vector<std::string> defaultValue);
    
    static int getInt(std::string name);
    static bool getBool(std::string name);
    static std::string getString(std::string name);
    static std::vector<int> getIntArray(std::string name);
    static std::vector<std::string> getStringArray(std::string name);
    
    static bool clearValue(std::string name);
    static void clear();
    
private:
    static Config _instance;
    static Mutex _instanceMutex;
    
private:
    Config();
    
    bool ihaveValue(std::string name);
    bool iisEmptyValue(std::string name);
    bool iisValidInt(std::string name);
    bool iisValidBool(std::string name);
    bool iisValidIntArray(std::string name);
    bool iisValidStringArray(std::string name);
    bool iisValidNonEmptyStringArray(std::string name);
    void setValue(std::string name, std::string value);
    bool getValue(std::string name, std::string* value);
    bool iclearValue(std::string name);
    
    std::map<std::string, std::string> _nameAndValues;
    
    typedef struct
    {
        ConfigValueType type;
        bool isRequired;
    } ValueSpec;
    std::map<std::string, ValueSpec> _spec;
};

};



#endif
