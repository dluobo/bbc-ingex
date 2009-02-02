/*
 * $Id: CommonTypes.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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
 
#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__


#include <mxf/mxf_types.h>


#define SAFE_DELETE(var) \
    delete var; \
    var = 0;

#define SAFE_ARRAY_DELETE(var) \
    delete [] var; \
    var = 0;


class Timecode
{
public:
    Timecode() : hour(0), min(0), sec(0), frame(0) {}
    Timecode(bool invalid)
    : hour(0), min(0), sec(0), frame(0)
    {
        if (invalid)
        {
            hour = -1;
        }
    }
    Timecode(int h, int m, int s, int f) : hour(h), min(m), sec(s), frame(f) {}
    
    bool isValid() { return hour >= 0 && min >= 0 && sec >= 0 && frame >= 0; }

    bool operator==(const Timecode& rhs)
    {
        return hour == rhs.hour && min == rhs.min && sec == rhs.sec && frame == rhs.frame;
    }
    
    int hour;
    int min;
    int sec;
    int frame;
};


const mxfRational g_4by3AspectRatio = {4, 3};
const mxfRational g_16by9AspectRatio = {16, 9};


#endif


