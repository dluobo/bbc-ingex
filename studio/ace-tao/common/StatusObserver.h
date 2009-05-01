/*
 * $Id: StatusObserver.h,v 1.2 2009/05/01 13:51:01 john_f Exp $
 *
 * Interface to which StatusClient can pass on received messages.
 *
 * Copyright (C) 2008  British Broadcasting Corporation.
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

#ifndef StatusObserver_h
#define StatusObserver_h

#include <string>

class StatusObserver
{
public:
    virtual void Observe (const std::string & name, const std::string & value) = 0;
};

#endif //#ifndef StatusObserver_h

