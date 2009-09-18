/*
 * $Id: Recorder.h,v 1.3 2009/09/18 17:48:40 philipn Exp $
 *
 * Recorder and configuration
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
 
#ifndef __PRODAUTO_RECORDER_H__
#define __PRODAUTO_RECORDER_H__


#include <string>
#include <vector>
#include <map>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "SourceConfig.h"


// parameter may not exceed this amount of characters
#define MAX_PARAMETER_LENGTH        250



// TODO: aspect ratio


namespace prodauto
{

class RecorderInputTrackConfig : public DatabaseObject
{
public:
    RecorderInputTrackConfig();
    ~RecorderInputTrackConfig();

    bool isConnectedToSource() { return sourceConfig != 0; }
    
    uint32_t index;
    uint32_t number;
    SourceConfig* sourceConfig; // can be 0
    uint32_t sourceTrackID;
};


class RecorderInputConfig : public DatabaseObject
{
public:
    RecorderInputConfig();
    ~RecorderInputConfig();

    // note that it is the track index and not the track number!
    RecorderInputTrackConfig* getTrackConfig(uint32_t index);
    
    uint32_t index;
    std::string name;
    std::vector<RecorderInputTrackConfig*> trackConfigs;
    
};

class RecorderParameter : public DatabaseObject
{
public:
    RecorderParameter();
    RecorderParameter(std::string name, std::string value, int type);
    RecorderParameter(const RecorderParameter& param);
    virtual ~RecorderParameter();
    
    std::string name;
    std::string value;
    int type;
};

class RecorderConfig : public DatabaseObject
{
public:
    RecorderConfig();
    ~RecorderConfig();
    
    SourceConfig* getSourceConfig(long sourceConfigID, uint32_t sourceTrackID);
    
    // note: the source config can be 0 if the input track is not connected
    SourceConfig* getSourceConfig(uint32_t inputIndex, uint32_t inputTrackIndex);
    
    RecorderInputConfig* getInputConfig(uint32_t index);
    
    
    bool haveParam(std::string name);

    // if the parameter exists and is well formed then the value is returned 
    // otherwse the default value is returned (and a warning logged if the
    // value is not well formed)
    std::string getParam(std::string name, std::string defaultValue);
    std::string getStringParam(std::string name, std::string defaultValue);
    int getIntParam(std::string name, int defaultValue);
    bool getBoolParam(std::string name, bool defaultValue);
    Rational getRationalParam(std::string name, Rational defaultValue);

    void setParam(std::string name, std::string value);    
    void setStringParam(std::string name, std::string value);    
    void setIntParam(std::string name, int value);    
    void setBoolParam(std::string name, bool value);    
    void setRationalParam(std::string name, Rational value);    
    
    
    std::string name;
    std::vector<SourceConfig*> sourceConfigs;
    std::vector<RecorderInputConfig*> recorderInputConfigs;
    std::map<std::string, RecorderParameter> parameters; 
};

class Recorder : public DatabaseObject
{
public:
    Recorder();
    ~Recorder();

    void setConfig(RecorderConfig* config); // transfers ownership
    void setAlternateConfig(RecorderConfig* config); // transfers ownership
    bool hasConfig();
    RecorderConfig* getConfig();
    std::vector<RecorderConfig*>& getAllConfigs();
    
    std::string name;
    
private:
    RecorderConfig* _config;
    std::vector<RecorderConfig*> _allConfigs;
};



};


#endif

