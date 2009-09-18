/*
 * $Id: DatabaseManager.h,v 1.2 2009/09/18 15:49:10 john_f Exp $
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

#ifndef DatabaseManager_h
#define DatabaseManager_h

#include <string>

/**
Singleton class to manage initialisation of prodauto::database
*/
class DatabaseManager
{
public:
// methods
    static DatabaseManager * Instance();
    void Initialise(const std::string & host, const std::string & dbname,
        const std::string & username, const std::string & password,
        unsigned int initial_connections, unsigned int max_connections);
    void ReInitialise();

protected:
    // Protect constructors to force use of Instance()
    DatabaseManager();
    DatabaseManager(const DatabaseManager &);
    DatabaseManager & operator= (const DatabaseManager &);

private:
// methods
// variables
    static DatabaseManager * mInstance;
    std::string mHost;
    std::string mDbName;
    std::string mUsername;
    std::string mPassword;
    unsigned int mInitialConnections;
    unsigned int mMaxConnections;
};


// Inline definition of Instance method
inline DatabaseManager * DatabaseManager::Instance()
{
    if (mInstance == 0)  // is it the first call?
    {  
        mInstance = new DatabaseManager; // create sole instance
    }
    return mInstance;// address of sole instance
}

#endif //ifndef DatabaseManager_h
