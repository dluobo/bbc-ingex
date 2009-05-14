/*
 * $Id: Take.cpp,v 1.2 2009/05/14 10:48:25 john_f Exp $
 *
 * An Item take
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

#include "Take.h"
#include "Item.h"
#include "Utilities.h"
#include "Logging.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;



Take::Take()
: DatabaseObject(), number(0), result(0), editRate(g_palEditRate), length(0), 
  startPosition(0), recordingLocation(0), _item(0)
{}

Take::~Take()
{
    if (_item != 0)
    {
        _item->removeTake(this);
    }
}


void Take::setItem(Item* item)
{
    if (item != 0 && _item != 0)
    {
        _item->removeTake(this);
    }
    _item = item;
}

long Take::getItemID()
{
    if (_item != 0)
    {
        return _item->getDatabaseID();
    }
    PA_LOGTHROW(DBException, ("Take does not belong to item so item ID is unknown"));
}



