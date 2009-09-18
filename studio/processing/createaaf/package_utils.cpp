/*
 * $Id: package_utils.cpp,v 1.2 2009/09/18 17:05:47 philipn Exp $
 *
 * Package-related utility functions
 *
 * Copyright (C) 2009 British Broadcasting Corporation
 * All rights reserved
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

#include "package_utils.h"
#include "CreateAAFException.h"
#include "Logging.h"

#include <vector>

using namespace prodauto;

static bool isVideoEditRate(Rational editRate)
{
    return editRate == g_palEditRate || editRate == g_ntscEditRate;
}

static bool getPhysicalSourcePackage(Track* track, PackageSet& packages, SourcePackage** physicalSourcePackage)
{
    if (track->sourceClip->sourcePackageUID == g_nullUMID)
    {
        return false;
    }
    
    SourcePackage dummy; // dummy acting as key
    dummy.uid = track->sourceClip->sourcePackageUID;

    PackageSet::iterator result = packages.find(&dummy);
    if (result != packages.end())
    {
        Package* package = *result;
        Track* trk = package->getTrack(track->sourceClip->sourceTrackID);
        if (trk == 0)
        {
            return false;
        }
        
        // if the package is a tape or live recording then we take that to be the physical source
        if (package->getType() == SOURCE_PACKAGE)
        {
            SourcePackage* srcPackage = dynamic_cast<SourcePackage*>(package);
            if (srcPackage->descriptor->getType() == TAPE_ESSENCE_DESC_TYPE ||
                srcPackage->descriptor->getType() == LIVE_ESSENCE_DESC_TYPE)
            {
                *physicalSourcePackage = srcPackage;
                return true;
            }
        }
        
        // go further down the reference chain to find the tape/live source
        return getPhysicalSourcePackage(trk, packages, physicalSourcePackage);
    }
        
    return false;
}

static Rational getVideoEditRate(SourceConfig* sourceConfig)
{
    std::vector<SourceTrackConfig*>::const_iterator iter;
    for (iter = sourceConfig->trackConfigs.begin(); iter != sourceConfig->trackConfigs.end(); iter++)
    {
        SourceTrackConfig* trackConfig = *iter;
        
        if (trackConfig->dataDef == PICTURE_DATA_DEFINITION || isVideoEditRate(trackConfig->editRate))
        {
            return trackConfig->editRate;
        }
    }

    return g_nullRational; // no video track and no track with a known video edit rate
}

int64_t prodauto::getStartTime(MaterialPackage* materialPackage, PackageSet& packages, Rational editRate)
{
    std::vector<Track*>::const_iterator iter;
    for (iter = materialPackage->tracks.begin(); iter != materialPackage->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        SourcePackage dummy;
        dummy.uid = track->sourceClip->sourcePackageUID;
        PackageSet::iterator result = packages.find(&dummy);
        if (result != packages.end())
        {
            Package* package = *result;
            
            // were only interested in references to file SourcePackages
            if (package->getType() != SOURCE_PACKAGE || package->tracks.size() == 0)
            {
                continue;
            }
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            if (sourcePackage->descriptor->getType() != FILE_ESSENCE_DESC_TYPE)
            {
                continue;
            }
            
            // get the startPosition in editRate units
            Track* sourceTrack = *sourcePackage->tracks.begin();
            if (editRate == sourceTrack->editRate)
            {
                return sourceTrack->sourceClip->position;
            }
            else
            {
                double factor = editRate.numerator * sourceTrack->editRate.denominator / 
                    (double)(editRate.denominator * sourceTrack->editRate.numerator);
                return (int64_t)(sourceTrack->sourceClip->position * factor + 0.5);
            }
        }
    }
    
    PA_LOGTHROW(CreateAAFException, ("Could not determine package start time"));
}

void prodauto::getStartAndEndTimes(MaterialPackage * materialPackage, PackageSet & packages, Rational editRate,
                                int64_t & startTimecode, Date & startDate, int64_t & endTimecode, Date & endDate)
{
    int64_t length = 0;
    int64_t position = 0;
    prodauto::Timestamp timestamp = g_nullTimestamp;
    bool done = false;

    // Start date from material package creation date
    timestamp = materialPackage->creationDate;

    for (std::vector<prodauto::Track *>::iterator
        it = materialPackage->tracks.begin(); !done && it != materialPackage->tracks.end(); ++it)
    {
        prodauto::Track * mt = *it;

        // Look for video track
        if (mt->dataDef == PICTURE_DATA_DEFINITION || mt->dataDef == SOUND_DATA_DEFINITION)
        {
            // Length from material package track sourceclip
            prodauto::SourceClip * mt_sc = mt->sourceClip;
            if (mt_sc)
            {
                length = mt_sc->length;
                if (mt->editRate != editRate)
                {
                    double factor = editRate.numerator * mt->editRate.denominator / 
                        (double)(editRate.denominator * mt->editRate.numerator);
                    length = (int64_t) (mt->sourceClip->length * factor + 0.5);
                }

                // Start timecode from file package track sourcelip
                SourcePackage dummy;
                dummy.uid = mt_sc->sourcePackageUID;
                uint32_t file_track_id = mt->sourceClip->sourceTrackID;
                PackageSet::iterator result = packages.find(&dummy);
                if (result != packages.end())
                {
                    prodauto::Package * file_package = (*result);
                    // It would be nice to find track straight from the ID but instead
                    // we have to search for it.
                    for (std::vector<prodauto::Track *>::const_iterator
                        it = file_package->tracks.begin(); !done && it != file_package->tracks.end(); ++it)
                    {
                        prodauto::Track * file_track = *it;
                        if (file_track->id == file_track_id)
                        {
                            prodauto::SourceClip * sc = file_track->sourceClip;
                            if (sc)
                            {
                                position = sc->position;
                                if (mt->editRate != editRate)
                                {
                                    double factor = editRate.numerator * mt->editRate.denominator / 
                                        (double)(editRate.denominator * mt->editRate.numerator);
                                    position = (int64_t) (sc->position * factor + 0.5);
                                }
                                done = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (!done)
    {
        PA_LOGTHROW(CreateAAFException, ("Could not determine package start and end times"));
    }

    // Return results
    startTimecode = position;
    endTimecode = position + length;

    startDate.year = timestamp.year;
    startDate.month = timestamp.month;
    startDate.day = timestamp.day;

    // Not correct for recordings over midnight but all this date
    // stuff needs to be revised anyway.
    endDate.year = timestamp.year;
    endDate.month = timestamp.month;
    endDate.day = timestamp.day;
}


Rational prodauto::getVideoEditRate(Package* package)
{
    std::vector<Track*>::const_iterator iter;
    for (iter = package->tracks.begin(); iter != package->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        if (track->dataDef == PICTURE_DATA_DEFINITION || isVideoEditRate(track->editRate))
        {
            return track->editRate;
        }
    }
    

    return g_nullRational; // no video track and no track with a known video edit rate
}

Rational prodauto::getVideoEditRate(Package* package, PackageSet& packages)
{
    // check the package and any referenced physcial source package
    std::vector<Track*>::const_iterator iter1;
    for (iter1 = package->tracks.begin(); iter1 != package->tracks.end(); iter1++)
    {
        Track* track = *iter1;
        
        if (track->dataDef == PICTURE_DATA_DEFINITION || isVideoEditRate(track->editRate))
        {
            return track->editRate;
        }

        SourcePackage* physSourcePackage = 0;
        if (::getPhysicalSourcePackage(track, packages, &physSourcePackage))
        {
            Rational editRate = getVideoEditRate(physSourcePackage);
            if (editRate != g_nullRational)
            {
                return editRate;
            }
        }
    }
    

    return g_nullRational; // no video track and no track with a known video edit rate
}

Rational prodauto::getVideoEditRate(MCClipDef* mcClipDef)
{
    std::vector<SourceConfig*>::const_iterator iter;
    for (iter = mcClipDef->sourceConfigs.begin(); iter != mcClipDef->sourceConfigs.end(); iter++)
    {
        SourceConfig* sourceConfig = *iter;
        
        Rational editRate = ::getVideoEditRate(sourceConfig);
        if (editRate != g_nullRational)
        {
            return editRate;
        }
    }

    return g_nullRational; // no video track and no track with a known video edit rate
}


int64_t prodauto::convertLength(Rational inRate, int64_t inLength, Rational outRate)
{
    double factor;
    int64_t outLength;
    
    if (inLength < 0 || inRate == outRate || inRate == g_nullRational || outRate == g_nullRational)
    {
        return inLength;
    }
    
    factor = outRate.numerator * inRate.denominator / (double)(outRate.denominator * inRate.numerator);
    outLength = (int64_t)(inLength * factor + 0.5);
    
    if (outLength < 0)
    {
        // e.g. if length was the max value and the new rate is faster
        return inLength;
    }
    
    return outLength;
}

uint16_t prodauto::getTimecodeBase(Rational rate)
{
    return (int16_t)(rate.numerator / (double)rate.denominator + 0.5);
}

