/*
 * $Id: MCClipDef.h,v 1.2 2009/03/25 13:59:52 john_f Exp $
 *
 * A multi-camera clip definition
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
 
#ifndef __PRODAUTO_MCCLIPDEF_H__
#define __PRODAUTO_MCCLIPDEF_H__


#include <string>
#include <vector>
#include <map>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "SourceConfig.h"


namespace prodauto
{

class MCSelectorDef : public DatabaseObject
{
public:
    MCSelectorDef();
    ~MCSelectorDef();

    bool isConnectedToSource() { return sourceConfig != 0; }

    uint32_t index;
    SourceConfig* sourceConfig; // can be 0
    uint32_t sourceTrackID;
};


class MCTrackDef : public DatabaseObject
{
public:
    MCTrackDef();
    ~MCTrackDef();
    
    uint32_t index;
    uint32_t number;
    std::map<uint32_t, MCSelectorDef*> selectorDefs; // MCSelectorDef index as key
};

    
class MCClipDef : public DatabaseObject
{
public:
    MCClipDef();
    ~MCClipDef();
    
    SourceConfig* getSourceConfig(long sourceConfigID, uint32_t sourceTrackID);
    
    std::string name;
    std::vector<SourceConfig*> sourceConfigs;
    std::map<uint32_t, MCTrackDef*> trackDefs; // MCTrackDef index as key
};

class MCCut : public DatabaseObject
{
public:
    MCCut();
    ~MCCut();

    long mcTrackId;
    long mcSelectorId;
    Date cutDate;
    int64_t position;
    Rational editRate;
};




};














#endif

