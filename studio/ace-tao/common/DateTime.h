/*
 * $Id: DateTime.h,v 1.3 2009/01/29 07:36:58 stuart_hc Exp $
 *
 * Date/time to text functions.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
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

#ifndef DateTime_h
#define DateTime_h

#include <string>

class DateTime
{
public:
    static std::string DateNoSeparators();
    static std::string DateTimeNoSeparators();
    static std::string Timecode();
    static std::string Timecode25();
    static void GetDate(int & year, int & month, int & day);
};

#endif //#ifndef DateTime_h
