/*
 * $Id: RouterDestination.h,v 1.1 2008/10/08 10:16:06 john_f Exp $
 *
 * Class to describe a router destination
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

#ifndef RouterDestination_h
#define RouterDestination_h

#include <string>

class RouterDestination
{
public:
    RouterDestination(const char * s, unsigned int n) : name(s), output_number(n) {}
    std::string name;
    unsigned int output_number;
};

#endif //#ifndef RouterDestination_h

