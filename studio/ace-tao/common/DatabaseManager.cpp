/*
 * $Id: DatabaseManager.cpp,v 1.1 2009/01/23 19:40:09 john_f Exp $
 *
 * Manager for prodauto::Database class
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

#include "DatabaseManager.h"
#include "Database.h"

// static instance pointer
DatabaseManager * DatabaseManager::mInstance = 0;

// constants
const std::string DSN = "prodautodb";


DatabaseManager::DatabaseManager()
: mInitialConnections(0), mMaxConnections(0)
{ }

void DatabaseManager::Initialise(const std::string & username, const std::string password,
                                 unsigned int initial_connections, unsigned int max_connections)
{
    mUsername = username;
    mPassword = password;
    mInitialConnections = initial_connections;
    mMaxConnections = max_connections;

    this->ReInitialise();
}

void DatabaseManager::ReInitialise()
{
    prodauto::Database::initialise(DSN, mUsername, mPassword, mInitialConnections, mMaxConnections);
}


