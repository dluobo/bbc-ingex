/*
 * $Id: PostgresDatabase.cpp,v 1.1 2008/07/08 16:23:00 philipn Exp $
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
 
#include "PostgresDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"


using namespace std;
using namespace rec;
using namespace pqxx;




PostgresDatabase::PostgresDatabase(string host, string name, string user, string password)
: _connection(0)
{
    try
    {
        _connection = new connection("host=" + host + " dbname=" + name + " user=" + user +
            " password=" + password);

        checkVersion();
        
        Logging::info("Opened Postgres database connection to '%s' running on '%s'\n", name.c_str(), host.c_str());
    }
    catch (const exception& ex)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
            "host='%s', name='%s', user='%s':\n%s",
            host.c_str(), name.c_str(), user.c_str(),
            ex.what()));
    }
    catch (const RecorderException& ex)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
            "host='%s', name='%s', user='%s':\n%s",
            host.c_str(), name.c_str(), user.c_str(),
            ex.getMessage().c_str()));
    }
    catch (...)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
            "host='%s', name='%s', user='%s':\nunknown database exception",
            host.c_str(), name.c_str(), user.c_str()));
    }
}

PostgresDatabase::~PostgresDatabase()
{
    delete _connection;
}


bool PostgresDatabase::haveConnection()
{
    // TODO: handle disconnects and report status here
    return true;
}

void PostgresDatabase::checkVersion()
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "get db version");
        result res(ts.exec("SELECT * FROM version"));
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Recorder database version row is missing"));
        }
        
        // TODO: check version is compatible
    }
    END_WORK("Get the recorder database version")
}

long PostgresDatabase::getNextId(work& ts, string seqName)
{
    START_WORK
    {
        string sql = "SELECT nextval('";
        sql += seqName + "')";
        
        result res(ts.exec(sql));
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to get next id from database sequence '%s'", seqName.c_str()));
        }
        
        return readLong(res[0][0], 0);
    }
    END_WORK("Get the next table identifier")
}

Timestamp PostgresDatabase::getTimestampNow(pqxx::work& ts)
{
    START_WORK
    {
        result res(ts.exec("SELECT now()"));
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to get database timestamp now"));
        }
        
        return readTimestamp(res[0][0], g_nullTimestamp);
    }
    END_WORK("Get database timestamp now")
}
long PostgresDatabase::readInt(result::field field, int nullValue)
{
    int result;
    
    if (field.is_null())
    {
        return nullValue;
    }
    
    field.to(result);
    return result;
}
    
long PostgresDatabase::readLong(result::field field, long nullValue)
{
    long result;
    
    if (field.is_null())
    {
        return nullValue;
    }
    
    field.to(result);
    return result;
}
    
int64_t PostgresDatabase::readInt64(result::field field, int64_t nullValue)
{
    const char* strResult = "";

    if (field.is_null())
    {
        return nullValue;
    }

    field.to(strResult);
    return strtoll(strResult, NULL, 10);
}

bool PostgresDatabase::readBool(result::field field, bool nullValue)
{
    bool value;

    if (field.is_null())
    {
        return nullValue;
    }

    field.to(value);
    return value;
}
    
string PostgresDatabase::readString(result::field field)
{
    const char* result = "";
    
    if (field.is_null())
    {
        return "";
    }
    
    field.to(result);
    return result;
}
    
Date PostgresDatabase::readDate(result::field field, Date nullValue)
{
    Date result;
    const char* strResult = "";
    
    if (field.is_null())
    {
        return nullValue;
    }
    
    field.to(strResult);
    if (sscanf(strResult, "%d-%d-%d", &result.year, &result.month, &result.day) != 3)
    {
        Logging::warning("Failed to parse date string '%s' from database\n", strResult);
        return nullValue;
    }
    return result;
}
    
Timestamp PostgresDatabase::readTimestamp(result::field field, Timestamp nullValue)
{
    Timestamp result;
    const char* strResult = "";
    
    if (field.is_null())
    {
        return nullValue;
    }
    
    float tmpFloat;
    field.to(strResult);
    if (sscanf(strResult, "%d-%d-%d %d:%d:%f", 
        &result.year, &result.month, &result.day,
        &result.hour, &result.min, &tmpFloat) != 6)
    {
        Logging::warning("Failed to parse timestamp string '%s' from database\n", strResult);
        return nullValue;
    }
    result.sec = (int)tmpFloat;
    result.msec = (int)(((long)(tmpFloat * 1000.0) - (long)(tmpFloat) * 1000) + 0.5);
    
    return result;
}
    
string PostgresDatabase::writeTimestamp(Timestamp value)
{
    char buf[128];
    
    float secFloat = (float)value.sec + value.msec / 1000.0;
    if (sprintf(buf, "%d-%d-%d %d:%d:%.6f", 
        value.year, value.month, value.day,
        value.hour, value.min, secFloat) < 0)
    {
        REC_LOGTHROW(("Failed to write timestamp to string for database"));
    }
    
    return buf;
}
    
string PostgresDatabase::writeDate(Date value)
{
    char buf[128];
    
    if (sprintf(buf, "%d-%d-%d", 
        value.year, value.month, value.day) < 0)
    {
        REC_LOGTHROW(("Failed to write date to string for database"));
    }
    
    return buf;
}


