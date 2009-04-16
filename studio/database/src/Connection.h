/*
 * $Id: Connection.h,v 1.2 2009/04/16 18:13:02 john_f Exp $
 *
 * Wraps a ODBC connection to the database
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
 
#ifndef __PRODAUTO_CONNECTION_H__
#define __PRODAUTO_CONNECTION_H__

#include <vector>
#include <cstring>

#include <odbc++/connection.h>
#include <odbc++/statement.h>
#include <odbc++/preparedstatement.h>

#include "DatabaseObject.h"


namespace prodauto
{

class Database;

class Connection
{
public:
    friend class Database;
    
public:
    virtual ~Connection();

    virtual void commit();
    virtual void rollback();
    
    odbc::Statement* createStatement() { return _odbcConnection->createStatement(); }
    odbc::PreparedStatement* prepareStatement(const std::string& sql) 
        { return _odbcConnection->prepareStatement(sql); }

        
    void registerCommitListener(long tentativeDatabaseID, DatabaseObject* obj);
 
    long getTentativeDatabaseID(DatabaseObject* obj);
    
protected:
    Connection(Database* database, odbc::Connection* odbcConnection);
    
    Database* _database;
    odbc::Connection* _odbcConnection;
    
    std::vector<std::pair<long, DatabaseObject*> > _commitListeners;
};
    
};



#endif


