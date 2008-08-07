/*
 * $Id: Recording.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to represent a recording.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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

#include "Recording.h"

/*
const char * Recording::TypeText(Recording::EnumeratedType type)
{
    switch(type)
    {
    case Recording::TAPE:
        return "TAPE";
        break;
    case Recording::FILE:
        return "FILE";
        break;
    default:
        return "";
        break;
    }
}
*/

const char * Recording::FormatText(Recording::EnumeratedFormat fmt)
{
    switch(fmt)
    {
    case Recording::TAPE:
        return "TAPE";
        break;
    case Recording::FILE:
        return "FILE";
        break;
    case Recording::DV:
        return "DV";
        break;
    case Recording::SDI:
        return "SDI";
        break;
    default:
        return "";
        break;
    }
}

