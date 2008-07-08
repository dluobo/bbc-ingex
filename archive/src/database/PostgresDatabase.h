/*
 * $Id: PostgresDatabase.h,v 1.1 2008/07/08 16:23:04 philipn Exp $
 *
 * Provides a PostgreSQL database connection and utility methods
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#ifndef __RECORDER_POSTGRES_DATABASE_H__
#define __RECORDER_POSTGRES_DATABASE_H__


#include <string>

#include <pqxx/pqxx>

#include "Threads.h"
#include "Types.h"


#define START_WORK \
    try

#define END_WORK(descr) \
    catch (const exception& ex) \
    { \
        REC_LOGTHROW(("Database access failed at '%s': %s", descr, ex.what())); \
    } \
    catch (const RecorderException& ex) \
    { \
        throw; \
    } \
    catch (...) \
    { \
        REC_LOGTHROW(("Database access failed at '%s': unknown exception", descr)); \
    }

    


namespace rec
{


class PostgresDatabase
{
public:
    PostgresDatabase(std::string host, std::string name, std::string user, std::string password);
    virtual ~PostgresDatabase();

    bool haveConnection();
    
    long getNextId(pqxx::work& ts, std::string seqName);
    Timestamp getTimestampNow(pqxx::work& ts);

    
    long readInt(pqxx::result::field field, int nullValue);
    long readLong(pqxx::result::field field, long nullValue);
    int64_t readInt64(pqxx::result::field field, int64_t nullValue);
    bool readBool(pqxx::result::field field, bool nullValue);
    std::string readString(pqxx::result::field field);
    Date readDate(pqxx::result::field field, Date nullValue);
    Timestamp readTimestamp(pqxx::result::field field, Timestamp nullValue);
    
    std::string writeTimestamp(Timestamp value);
    std::string writeDate(Date value);
    
protected:    
    void checkVersion();

    pqxx::connection* _connection;
    Mutex _accessMutex;
};


};




#endif

