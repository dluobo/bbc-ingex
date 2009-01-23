/*
 * $Id: EditorsFile.cpp,v 1.1 2009/01/23 19:42:44 john_f Exp $
 *
 * Editor's file
 *
 * Copyright (C) 2008, BBC, Philip de Nier <philipn@users.sourceforge.net>
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

#include "EditorsFile.h"
#include "CreateAAFException.h"
#include <Logging.h>


using namespace std;
using namespace prodauto;



int64_t prodauto::getStartTime(MaterialPackage* materialPackage, PackageSet& packages, Rational editRate)
{
    vector<Track*>::const_iterator iter;
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



