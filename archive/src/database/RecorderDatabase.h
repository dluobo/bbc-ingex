/*
 * $Id: RecorderDatabase.h,v 1.1 2008/07/08 16:23:05 philipn Exp $
 *
 * Provides access to the recorder data in a PostgreSQL database
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
 
#ifndef __RECORDER_RECORDER_DATABASE_H__
#define __RECORDER_RECORDER_DATABASE_H__


#include <string>

#include <pqxx/pqxx>

#include "DatabaseThings.h"
#include "Threads.h"
#include "PostgresDatabase.h"


namespace rec
{


class RecorderDatabase
{
public:
    static RecorderDatabase* getInstance();
    static void close();
    
public:
    virtual ~RecorderDatabase() {}

    virtual bool haveConnection() = 0;

    virtual RecorderTable* loadRecorder(std::string name) = 0;
    virtual void saveRecorder(RecorderTable* recorder) = 0;
    
    virtual Source* loadSource(std::string barcode) = 0;
    virtual long getNewSourceRecordingInstance(long sourceId) = 0;
    virtual void resetSourceRecordingInstance(long sourceId, long firstTry, long lastTry) = 0;
    virtual void updateConcreteSource(ConcreteSource* source) = 0;
    
    virtual std::vector<RecordingSessionTable*> loadMinimalSessions(long recorderId, int status) = 0;
    virtual void saveSession(RecordingSessionTable* session) = 0;
    virtual void saveSessionDestinations(RecordingSessionTable* session) = 0;
    virtual void updateSession(RecordingSessionTable* session) = 0;
    virtual void deleteSession(RecordingSessionTable* session) = 0;
    virtual bool sessionInProgress(std::string sourceBarcode) = 0;
    
    virtual CacheTable* loadCache(long recorderId, std::string path) = 0;
    virtual void saveCache(CacheTable* cache) = 0;
    virtual std::vector<CacheItem*> loadCacheItems(long cacheId) = 0;
    virtual CacheItem* loadCacheItem(long cacheId, std::string name) = 0;
    
    virtual std::vector<Destination*> loadSessionDestinations(long sessionId) = 0;
    virtual void deleteDestination(Destination* dest) = 0;
    virtual void deleteSessionDestination(RecordingSessionDestinationTable* sessionDest) = 0;
    virtual void updateHardDiskDestination(HardDiskDestination* hdDest) = 0;
    virtual void removeHardDiskDestinationFromCache(long hdDestId) = 0;
    
    virtual std::vector<Destination*> loadDestinations(std::string barcode) = 0;
    virtual HardDiskDestination* loadHDDest(long hdDestId) = 0;
    
    virtual Source* loadHDDSource(long hdDestId) = 0;

    virtual std::vector<LTOTransferSessionTable*> loadLTOTransferSessions(long recorderId, int status) = 0;
    virtual void saveLTOTransferSession(LTOTransferSessionTable* session) = 0;
    virtual void updateLTOTransferSession(LTOTransferSessionTable* session) = 0;
    virtual void deleteLTOTransferSession(LTOTransferSessionTable* session) = 0;
    
    virtual void updateLTOFile(long id, int status) = 0;
    
    virtual bool ltoUsedInCompletedSession(std::string spoolNo) = 0;
    virtual bool digibetaUsedInCompletedSession(std::string spoolNo) = 0;
    virtual bool d3UsedInCompletedSession(std::string spoolNo) = 0;

    virtual void saveInfaxExport(InfaxExportTable* infaxExport) = 0;
    
protected:
    static RecorderDatabase* _instance;
};


class PostgresRecorderDatabase : public PostgresDatabase, public RecorderDatabase
{
public:
    static void initialise(std::string host, std::string name, std::string user, std::string password);
    
public:
    virtual ~PostgresRecorderDatabase();
    
    // from Database
    
    virtual bool haveConnection();

    virtual RecorderTable* loadRecorder(std::string name);
    virtual void saveRecorder(RecorderTable* recorder);
    
    virtual Source* loadSource(std::string barcode);
    virtual long getNewSourceRecordingInstance(long sourceId);
    virtual void resetSourceRecordingInstance(long sourceId, long firstTry, long lastTry);
    virtual void updateConcreteSource(ConcreteSource* source);

    virtual std::vector<RecordingSessionTable*> loadMinimalSessions(long recorderId, int status);
    virtual void saveSession(RecordingSessionTable* session);
    virtual void saveSessionDestinations(RecordingSessionTable* session);
    virtual void updateSession(RecordingSessionTable* session);
    virtual void deleteSession(RecordingSessionTable* session);
    virtual bool sessionInProgress(std::string sourceBarcode);
    
    virtual CacheTable* loadCache(long recorderId, std::string path);
    virtual void saveCache(CacheTable* cache);
    virtual std::vector<CacheItem*> loadCacheItems(long cacheId);
    virtual CacheItem* loadCacheItem(long cacheId, std::string name);

    virtual std::vector<Destination*> loadSessionDestinations(long sessionId);
    virtual void deleteDestination(Destination* dest);
    virtual void deleteSessionDestination(RecordingSessionDestinationTable* sessionDest);
    virtual void updateHardDiskDestination(HardDiskDestination* hdDest);
    virtual void removeHardDiskDestinationFromCache(long hdDestId);
    
    virtual std::vector<Destination*> loadDestinations(std::string barcode);
    virtual HardDiskDestination* loadHDDest(long hdDestId);
    
    virtual Source* loadHDDSource(long hdDestId);
    
    virtual std::vector<LTOTransferSessionTable*> loadLTOTransferSessions(long recorderId, int status);
    virtual void saveLTOTransferSession(LTOTransferSessionTable* session);
    virtual void updateLTOTransferSession(LTOTransferSessionTable* session);
    virtual void deleteLTOTransferSession(LTOTransferSessionTable* session);
    
    virtual void updateLTOFile(long id, int status);

    virtual bool ltoUsedInCompletedSession(std::string spoolNo);
    virtual bool digibetaUsedInCompletedSession(std::string spoolNo);
    virtual bool d3UsedInCompletedSession(std::string spoolNo);
    
    virtual void saveInfaxExport(InfaxExportTable* infaxExport);

private:
    PostgresRecorderDatabase(std::string host, std::string name, std::string user, std::string password);

    void prepareStatements();
    
    void saveDestination(pqxx::work& ts, Destination* destination);
    void saveHardDiskDestination(pqxx::work& ts, long destId, HardDiskDestination* hdd);
    void saveDigibetaDestination(pqxx::work& ts, long destId, DigibetaDestination* hdd);
    
    HardDiskDestination* loadHardDiskDestination(pqxx::work& ts, long destDatabaseId);
    DigibetaDestination* loadDigibetaDestination(pqxx::work& ts, long destDatabaseId);
    
    std::vector<ConcreteSource*> loadD3Sources(pqxx::work& ts, long databaseId);
    
    LTOTable* loadSessionLTO(pqxx::work& ts, long sessionId);
    std::vector<LTOFileTable*> loadLTOFiles(pqxx::work& ts, long ltoId);
    void saveLTO(pqxx::work& ts, long sessionId, LTOTable* lto);
    void saveLTOFile(pqxx::work& ts, long ltoId, LTOFileTable* ltoFile);
    void deleteLTO(pqxx::work& ts, LTOTable* lto);
};


};




#endif

