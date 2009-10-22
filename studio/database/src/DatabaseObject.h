/*
 * $Id: DatabaseObject.h,v 1.2 2009/10/22 13:53:09 john_f Exp $
 *
 * An object corresponding to a row in a table in the database
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
 
#ifndef __PRODAUTO_DATABASEOBJECT_H__
#define __PRODAUTO_DATABASEOBJECT_H__


namespace prodauto
{

class DatabaseObject
{
public:
    DatabaseObject();
    DatabaseObject(const DatabaseObject& dbObject);
    virtual ~DatabaseObject();

    bool isPersistent() const { return _databaseID != 0; }    

    virtual void wasCommitted(long databaseID);
    virtual void wasLoaded(long databaseID);
  
    virtual void cloneInPlace();
    void clone(DatabaseObject* clonedObject);
    
    long getDatabaseID() const;
    
private:
    long _databaseID;
};

};





#endif






