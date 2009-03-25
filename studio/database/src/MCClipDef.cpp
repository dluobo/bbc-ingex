/*
 * $Id: MCClipDef.cpp,v 1.2 2009/03/25 13:59:52 john_f Exp $
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
 
#include <cassert>

#include "MCClipDef.h"
#include "Utilities.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


MCSelectorDef::MCSelectorDef()
: DatabaseObject(), index(0), sourceConfig(0), sourceTrackID(0)
{}

MCSelectorDef::~MCSelectorDef()
{}


MCTrackDef::MCTrackDef()
: DatabaseObject(), index(0), number(0)
{}

MCTrackDef::~MCTrackDef()
{
    map<uint32_t, MCSelectorDef*>::const_iterator iter;
    for (iter = selectorDefs.begin(); iter != selectorDefs.end(); iter++)
    {
        delete (*iter).second;
    }
}


MCClipDef::MCClipDef()
: DatabaseObject()
{}

MCClipDef::~MCClipDef()
{
    vector<SourceConfig*>::const_iterator iter1;
    for (iter1 = sourceConfigs.begin(); iter1 != sourceConfigs.end(); iter1++)
    {
        delete *iter1;
    }

    map<uint32_t, MCTrackDef*>::const_iterator iter2;
    for (iter2 = trackDefs.begin(); iter2 != trackDefs.end(); iter2++)
    {
        delete (*iter2).second;
    }
}

SourceConfig* MCClipDef::getSourceConfig(long sourceConfigID, uint32_t sourceTrackID)
{
    if (sourceConfigID == 0)
    {
        return 0;
    }
    
    vector<SourceConfig*>::const_iterator iter;
    for (iter = sourceConfigs.begin(); iter != sourceConfigs.end(); iter++)
    {
        if ((*iter)->getID() == sourceConfigID)
        {
            if ((*iter)->getTrackConfig(sourceTrackID) == 0)
            {
                break; // track with id sourceTrackID not present
            }
            return *iter;
        }
    }
    
    return 0;
}
    
MCCut::MCCut()
: DatabaseObject(), mcTrackId(0), mcSelectorId(0), position(0), editRate(g_nullRational)
{}

MCCut::~MCCut()
{
}

