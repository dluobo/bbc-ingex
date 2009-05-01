/*
 * $Id: CutInfo.h,v 1.2 2009/05/01 13:34:05 john_f Exp $
 *
 * Vision mix cut info.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
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

#ifndef CutInfo_h
#define CutInfo_h

#include <DataTypes.h>
#include <string>

namespace prodauto
{

class CutInfo
{
public:
    CutInfo() : position(0) {}
    
    int64_t position;  ///< position in sequence, not the timecode
    std::string source;
};

};

#endif //ifndef CutInfo_h

