/*
 * $Id: Database.h,v 1.12 2009/10/12 15:44:54 philipn Exp $
 *
 * Provides access to the data in the database
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

#ifndef __PRODAUTO_DATABASE_H__
#define __PRODAUTO_DATABASE_H__

#include <vector>
#include <map>
#include <string>
#include <cstring>

#include "DatabaseEnums.h"
#include "Threads.h"
#include "Transaction.h"
#include "Recorder.h"
#include "RouterConfig.h"
#include "Package.h"
#include "Track.h"
#include "SourceClip.h"
#include "Series.h"
#include "Programme.h"
#include "Item.h"
#include "Take.h"
#include "MCClipDef.h"
#include "Transcode.h"



namespace prodauto
{


class Database
{
public:
    friend class Transaction;

public:
    static void initialise(std::string hostname, std::string dbname, std::string username, std::string password,
                           unsigned int initialConnections, unsigned int maxConnections);
    static void close();

    static Database* getInstance();
    
private:
    static Database *_instance;
    static Mutex _databaseMutex;


public:    
    Transaction* getTransaction(std::string name = "");
    
    
    // UMID generation offset
    
    uint32_t getUMIDGenOffset() { return _umidGenOffset; }
    

    // live recording locations
    
    std::map<long, std::string> loadLiveRecordingLocations(Transaction *transaction = 0);
    long saveLiveRecordingLocation(std::string name, Transaction *transaction = 0);



    // Configurations

    Recorder* loadRecorder(std::string name, Transaction *transaction = 0);
    void saveRecorder(Recorder *recorder, Transaction *transaction = 0);
    // the source configs referenced by a recorder config will not be deleted
    void deleteRecorder(Recorder *recorder, Transaction *transaction = 0);

    SourceConfig* loadSourceConfig(long databaseID, Transaction *transaction = 0);
    SourceConfig* loadSourceConfig(std::string name, bool assume_exists = true, Transaction *transaction = 0);
    void saveSourceConfig(SourceConfig *config, Transaction *transaction = 0);
    void deleteSourceConfig(SourceConfig *config, Transaction *transaction = 0);

    std::vector<RouterConfig*> loadAllRouterConfigs(Transaction *transaction = 0);
    RouterConfig* loadRouterConfig(std::string name, Transaction *transaction = 0);


    // Multi-camera clip definitions

    std::vector<MCClipDef*> loadAllMultiCameraClipDefs(Transaction *transaction = 0);
    MCClipDef* loadMultiCameraClipDef(std::string name, bool assumeExists = true, Transaction *transaction = 0);
    void saveMultiCameraClipDef(MCClipDef *mcClipDef, Transaction *transaction = 0);
    void deleteMultiCameraClipDef(MCClipDef *mcClipDef, Transaction *transaction = 0);

    // Multi-camera cuts (Director's cut)
    std::vector<MCCut*> loadAllMultiCameraCuts(Transaction *transaction = 0);
    std::vector<MCCut*> loadMultiCameraCuts(MCTrackDef *mc_track_def, Date start_date, int64_t start_timecode,
                                             Date end_date, int64_t end_timecode, Transaction *transaction = 0);
    void saveMultiCameraCut(MCCut *mc_cut, Transaction *transaction = 0);


    // Editorial

    std::vector<Series*> loadAllSeries(Transaction *transaction = 0);

    void loadProgrammesInSeries(Series *series, Transaction *transaction = 0);
    void saveSeries(Series *series, Transaction *transaction = 0);
    void deleteSeries(Series *series, Transaction *transaction = 0);

    void loadItemsInProgramme(Programme *programme, Transaction *transaction = 0);
    void saveProgramme(Programme *programme, Transaction *transaction = 0);
    void deleteProgramme(Programme *programme, Transaction *transaction = 0);
    void updateItemsOrder(Programme *programme, Transaction *transaction = 0);

    void loadTakesInItem(Item *item, Transaction *transaction = 0);
    void saveItem(Item *item, Transaction *transaction = 0);
    void deleteItem(Item *item, Transaction *transaction = 0);

    void saveTake(Take *take, Transaction *transaction = 0);
    void deleteTake(Take *take, Transaction *transaction = 0);



    // Transcode

    std::vector<Transcode*> loadTranscodes(int status, Transaction *transaction = 0);

    void saveTranscode(Transcode *transcode, Transaction *transaction = 0);
    void deleteTranscode(Transcode *transcode, Transaction *transaction = 0);

    int resetTranscodeStatus(int from_status, int to_status, Transaction *transaction = 0);

    int deleteTranscodes(int status, Interval time_before_now, Transaction *transaction = 0);



    // Packages

    // project names

    // loads or creates a project name
    // Note: an exception is thrown if name is empty
    ProjectName loadOrCreateProjectName(std::string name, Transaction *transaction = 0);
    std::vector<ProjectName> loadProjectNames(Transaction *transaction = 0);
    void deleteProjectName(ProjectName *projectName, Transaction *transaction = 0);

    // returns 0 if source package not in database
    SourcePackage* loadSourcePackage(std::string name, Transaction *transaction = 0);
    void lockSourcePackages(Transaction *transaction);
    SourcePackage * createSourcePackage(const std::string & name, const SourceConfig * sc, int type, const std::string & spool, int64_t origin, Transaction * transaction = 0);

    Package* loadPackage(long databaseID, Transaction *transaction = 0);
    // assume_exists == true means an exception will be thrown if it doesn't exist,
    // assume_exists == false will return 0 if the package doesn't exist
    Package* loadPackage(UMID packageUID, bool assume_exists = true, Transaction *transaction = 0);
    void savePackage(Package *package, Transaction *transaction = 0);
    void deletePackage(Package *package, Transaction *transaction = 0);
    
    //delete every package id in supplied array
    void deletePackageChain(Package *package, Transaction *transaction = 0);

    //do package references exist?
    bool packageRefsExist(Package *package, Transaction *transaction = 0);


    // returns 1 on success and sets both sourcePackage and sourceTrack, else
    // -1 if source package is not present, -2 if track is not present
    int loadSourceReference(UMID sourcePackageUID, uint32_t sourceTrackID,
        Package **sourcePackage, Track **sourceTrack, Transaction *transaction = 0);

    // load all packages in the reference chain
    void loadPackageChain(Package *topPackage, PackageSet *packages, Transaction *transaction = 0);
    void loadPackageChain(long databaseID, Package **topPackage, PackageSet *packages, Transaction *transaction = 0);

    // load all material packages (plus reference chain) referencing file packages
    // where after <= material package creation date < before
    void loadMaterial(Timestamp& after, Timestamp& before, MaterialPackageSet *topPackages,
        PackageSet *packages, Transaction *transaction = 0);

    // load all material packages (plus reference chain) referencing file packages
    // where material package has user comment name/value
    void loadMaterial(std::string ucName, std::string ucValue, MaterialPackageSet *topPackages, PackageSet *packages,
        Transaction *transaction = 0);

    // load material packages based on list of material package database IDs
    void loadMaterial(const std::vector<long> & packageIDs, MaterialPackageSet *topPackages, PackageSet *packages,
        Transaction *transaction = 0);

    // enumerations
    void loadResolutionNames(std::map<int, std::string> & resolution_names, Transaction *transaction = 0);
    void loadFileFormatNames(std::map<int, std::string> & file_format_names, Transaction *transaction = 0);
    void loadTimecodeNames(std::map<int, std::string> & timecode_names, Transaction *transaction = 0);

    // recording location
    std::string loadLocationName(long databaseID, Transaction *transaction = 0);

protected:
    Database(std::string hostname, std::string dbname, std::string username, std::string password,
             unsigned int initialConnections, unsigned int maxConnections);
    ~Database();

    // called by Transaction when destructing
    void returnConnection(Transaction *transaction);

private:
    pqxx::connection* openConnection(std::string hostname, std::string dbname, std::string username,
                                     std::string password);

    long loadNextId(std::string seq_name, Transaction *transaction = 0);
    void checkVersion(Transaction *transaction = 0);
    void loadUMIDGenerationOffset(Transaction *transaction = 0);

    void loadPackage(Transaction *transaction, const pqxx::result::tuple &tup, Package **package);
    
    long readId(const pqxx::result::field &field);
    int readInt(const pqxx::result::field &field, int null_value);
    unsigned int readUInt(const pqxx::result::field &field, unsigned int null_value);
    int readEnum(const pqxx::result::field &field);
    long readLong(const pqxx::result::field &field, long null_value);
    int64_t readInt64(const pqxx::result::field &field, int64_t null_value);
    bool readBool(const pqxx::result::field &field, bool null_value);
    std::string readString(const pqxx::result::field &field);
    Date readDate(const pqxx::result::field &field);
    Timestamp readTimestamp(const pqxx::result::field &field);
    Rational readRational(const pqxx::result::field &field1, const pqxx::result::field &field2);
    UMID readUMID(const pqxx::result::field &field);

    std::string writeTimestamp(Timestamp value);
    std::string writeDate(Date value);
    std::string writeInterval(Interval value);
    std::string writeUMID(UMID value);

private:
    std::string _hostname;
    std::string _dbname;
    std::string _username;
    std::string _password;
    int _numConnections;
    int _maxConnections;
    
    Mutex _connectionMutex;
    std::vector<pqxx::connection*> _connectionPool;
    std::vector<Transaction*> _transactionsInUse;
    
    uint32_t _umidGenOffset;
};



};



#endif

