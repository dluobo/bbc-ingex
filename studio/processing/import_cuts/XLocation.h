/*
 * $Id: XLocation.h,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Parse recording Location from XML file.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __X_LOCATION_H__
#define __X_LOCATION_H__


#include <string>

#include <xercesc/dom/DOMElement.hpp>


class XLocation
{
public:
    XLocation();
    ~XLocation();

    void Parse(xercesc::DOMElement *element);

    std::string Name() { return mName; }

public:
    int operator<(const XLocation &right) const
    {
        return mName < right.mName;
    }
    
private:
    std::string mName;
};


// used when declaring XLocation STL set
class XLocationComparator
{
public:
    int operator()(const XLocation *left, const XLocation *right) const
    {
        return (*left) < (*right);
    }
};


#endif
