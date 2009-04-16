/*
 * $Id: Track.cpp,v 1.2 2009/04/16 18:16:25 john_f Exp $
 *
 * A Track in a Package
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
#include <sstream>

#include "Track.h"
#include "Database.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;


Track::Track()
: DatabaseObject(), id(0), number(0), editRate(g_palEditRate), origin(0), dataDef(0), sourceClip(0)
{}

Track::~Track()
{
    delete sourceClip;
}

string Track::toString()
{
    stringstream trackStr;
    
    trackStr << "ID = " << id << endl;
    trackStr << "Number = " << number << endl;
    trackStr << "Name = " << name << endl;
    switch (dataDef)
    {
        case PICTURE_DATA_DEFINITION:
            trackStr << "Data def = Picture" << endl;
            break;
        
        case SOUND_DATA_DEFINITION:
            trackStr << "Data def = Sound" << endl;
            break;
        
        default:
            assert(false);
            PA_LOGTHROW(DBException, ("Unknown data def"));
    }
    trackStr << "Edit rate = " << editRate.numerator << "/" << editRate.denominator << endl;
    
    if (sourceClip)
    {
        trackStr << "SourceClip:" << endl;
        trackStr << sourceClip->toString();
    }
    
    return trackStr.str();
}

void Track::cloneInPlace(bool resetLengths)
{
    DatabaseObject::cloneInPlace();
    
    if (sourceClip != 0)
    {
        sourceClip->cloneInPlace(resetLengths);
    }
}
    


