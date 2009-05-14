/*
 * $Id: SourceConfig.cpp,v 1.3 2009/05/14 10:48:25 john_f Exp $
 *
 * Live recording or tape Source configuration
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
#include <memory>

#include "SourceConfig.h"
#include "Database.h"
#include "Utilities.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


SourceTrackConfig::SourceTrackConfig()
: DatabaseObject(), id(0), number(0), dataDef(0), editRate(g_palEditRate), length(0)
{}

SourceTrackConfig::~SourceTrackConfig()
{}


SourceConfig::SourceConfig()
: DatabaseObject(), type(0), recordingLocation(0), _sourcePackage(0)
{}

SourceConfig::~SourceConfig()
{
    delete _sourcePackage;
    
    vector<SourceTrackConfig*>::const_iterator iter;
    for (iter = trackConfigs.begin(); iter != trackConfigs.end(); iter++)
    {
        delete *iter;
    }
}
    
SourceTrackConfig* SourceConfig::getTrackConfig(uint32_t id)
{
    vector<SourceTrackConfig*>::const_iterator iter;
    for (iter = trackConfigs.begin(); iter != trackConfigs.end(); iter++)
    {
        if ((*iter)->id == id)
        {
            return *iter;
        }
    }
    
    return 0;
}

void SourceConfig::setSessionSourcePackage()
{
    string sourcePackageName = name;
    sourcePackageName += " - " + getDateString(generateDateNow());

    setSourcePackage(sourcePackageName);
}

void SourceConfig::setSourcePackage(string name)
{
    if (_sourcePackage != 0 && _sourcePackage->name.compare(name) == 0)
    {
        // current source package has the required name
        return;
    }
    
    if (_sourcePackage != 0 && _sourcePackage->name.compare(name) != 0)
    {
        // delete the existing package with a different name
        delete _sourcePackage;
        _sourcePackage = 0;
    }

    // NB. This code should really be in the Database class.

    // use a transaction to get exclusive access to the source packages, preventing
    // other threads or processes interferring
    Database* database = Database::getInstance();
    auto_ptr<Transaction> transaction(database->getTransaction());
    database->lockSourcePackages(transaction.get());
    
    
    // try load it from the database
    _sourcePackage = database->loadSourcePackage(name, transaction.get());
    if (_sourcePackage != 0)
    {
        // got it
        transaction->commitTransaction();
        return;
    }

    // create a new one    
    
    auto_ptr<SourcePackage> newSourcePackage(new SourcePackage());
    newSourcePackage->uid = generateUMID();
    newSourcePackage->name = name;
    newSourcePackage->creationDate = generateTimestampNow();
    if (type == TAPE_SOURCE_CONFIG_TYPE)
    {
        TapeEssenceDescriptor* tapeEssDesc = new TapeEssenceDescriptor();
        newSourcePackage->descriptor = tapeEssDesc;
        tapeEssDesc->spoolNumber = spoolNumber;
    }
    else // LIVE_SOURCE_CONFIG_TYPE
    {
        LiveEssenceDescriptor* liveEssDesc = new LiveEssenceDescriptor();
        newSourcePackage->descriptor = liveEssDesc;
        liveEssDesc->recordingLocation = recordingLocation;
    }
    
    
    // create source package tracks
    
    vector<SourceTrackConfig*>::const_iterator iter2;
    for (iter2 = trackConfigs.begin(); iter2 != trackConfigs.end(); iter2++)
    {
        SourceTrackConfig* sourceTrackConfig = *iter2;

        newSourcePackage->tracks.push_back(new Track());
        Track* track = newSourcePackage->tracks.back();
        
        track->id = sourceTrackConfig->id;
        track->number = sourceTrackConfig->number;
        track->name = sourceTrackConfig->name;
        track->dataDef = sourceTrackConfig->dataDef;
        track->editRate = sourceTrackConfig->editRate;
        track->sourceClip = new SourceClip();
        
        track->sourceClip->sourcePackageUID = g_nullUMID;
        track->sourceClip->sourceTrackID = 0;
        track->sourceClip->length = sourceTrackConfig->length;
        track->sourceClip->position = 0;
    }
        
    
    // save new source packages to the database
    
    database->savePackage(newSourcePackage.get(), transaction.get());
    transaction->commitTransaction();
    
    _sourcePackage = newSourcePackage.release();
}
    
SourcePackage* SourceConfig::getSourcePackage()
{
    return _sourcePackage;
}


