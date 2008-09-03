/*
 * $Id: Database.h,v 1.3 2008/09/03 14:27:23 john_f Exp $
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

#include <odbc++/connection.h>
#include <odbc++/resultset.h>

#include "DatabaseEnums.h"
#include "Threads.h"
#include "Connection.h"
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

// TODO: getting source session without recorder config, eg. for non-recorder apps



namespace prodauto
{

    
class Database
{
public:
    friend class Connection;
    
public:
    static void initialise(std::string dsn, std::string username, std::string password,
        unsigned int initialConnections, unsigned int maxConnections);

    static void close();
    
    static Database* getInstance();    

    Connection* getConnection(bool isTransaction = false);
    // calls getConnection(true) and casts to Transaction
    Transaction* getTransaction();
    
    
    std::map<long, std::string> loadLiveRecordingLocations();

    
    
    // Configurations

    Recorder* loadRecorder(std::string name);
    void saveRecorder(Recorder* recorder, Transaction* transaction = 0);
    // the source configs referenced by a recorder config will not be deleted
    void deleteRecorder(Recorder* recorder, Transaction* transaction = 0);
    
    SourceConfig* loadSourceConfig(long databaseID);
    void saveSourceConfig(SourceConfig* config, Transaction* transaction = 0);
    void deleteSourceConfig(SourceConfig* config, Transaction* transaction = 0);
    
    std::vector<RouterConfig*> loadAllRouterConfigs();
    RouterConfig* loadRouterConfig(std::string name);
    
    
    // Multi-camera clip definitions
    
    std::vector<MCClipDef*> loadAllMultiCameraClipDefs();
    void saveMultiCameraClipDef(MCClipDef* mcClipDef, Transaction* transaction = 0);
    void deleteMultiCameraClipDef(MCClipDef* mcClipDef, Transaction* transaction = 0);
    
    
    
    // Editorial

    std::vector<Series*> loadAllSeries();
    
    void loadProgrammesInSeries(Series* series);
    void saveSeries(Series* series, Transaction* transaction = 0);
    void deleteSeries(Series* series, Transaction* transaction = 0);
    
    void loadItemsInProgramme(Programme* programme);
    void saveProgramme(Programme* programme, Transaction* transaction = 0);
    void deleteProgramme(Programme* programme, Transaction* transaction = 0);
    void updateItemsOrder(Programme* programme, Transaction* transaction = 0);
    
    void loadTakesInItem(Item* item);
    void saveItem(Item* item, Transaction* transaction = 0);
    void deleteItem(Item* item, Transaction* transaction = 0);
    
    void saveTake(Take* take, Transaction* transaction = 0);
    void deleteTake(Take* take, Transaction* transaction = 0);
    
    
    
    // Transcode
    
    std::vector<Transcode*> loadTranscodes(int status);
    std::vector<Transcode*> loadTranscodes(std::vector<int>& statuses);

    void saveTranscode(Transcode* transcode, Transaction* transaction = 0);
    void deleteTranscode(Transcode* transcode, Transaction* transaction = 0);

    int resetTranscodeStatus(int fromStatus, int toStatus, Transaction* transaction = 0);
    
    int deleteTranscodes(std::vector<int>& statuses, Interval timeBeforeNow, Transaction* transaction = 0);
    
    
    
    // Packages

    // project names
    
    // loads or creates a project name
    // Note: an exception is thrown if name is empty
    ProjectName loadOrCreateProjectName(std::string name, Transaction* transaction = 0);
    std::vector<ProjectName> loadProjectNames();
    void deleteProjectName(ProjectName* projectName, Transaction* transaction = 0);
    
    // returns 0 if source package not in database
    SourcePackage* loadSourcePackage(std::string name);
    SourcePackage* loadSourcePackage(std::string name, Transaction* transaction = 0);
    void lockSourcePackages(Transaction* transaction);
    
    Package* loadPackage(long databaseID);
    // assumeExists == true means an exception will be thrown if it doesn't exist, 
    // assumeExists == false will return 0 if the package doesn't exist
    Package* loadPackage(UMID packageUID, bool assumeExists = true);
    void savePackage(Package* package, Transaction* transaction = 0);
    void deletePackage(Package* package, Transaction* transaction = 0);

    // returns 1 on success and sets both sourcePackage and sourceTrack, else
    // -1 if source package is not present, -2 if track is not present
    int loadSourceReference(UMID sourcePackageUID, uint32_t sourceTrackID, 
        Package** sourcePackage, Track** sourceTrack);

    // load all packages in the reference chain
    void loadPackageChain(Package* topPackage, PackageSet* packages);
    void loadPackageChain(long databaseID, Package** topPackage, PackageSet* packages);
        
    // load all material packages (plus reference chain) referencing file packages 
    // where after <= material package creation date < before
    void loadMaterial(Timestamp& after, Timestamp& before, MaterialPackageSet* topPackages, 
        PackageSet* packages);
    
    // load all material packages (plus reference chain) referencing file packages 
    // where material package has user comment name/value
    void loadMaterial(std::string ucName, std::string ucValue, MaterialPackageSet* topPackages, PackageSet* packages);
    
    // enumerations
    void loadResolutionNames(std::map<int, std::string> & resolution_names);
    void loadFileFormatNames(std::map<int, std::string> & file_format_names);
    
protected:
    Database(std::string dsn, std::string username, std::string password,
        unsigned int initialConnections, unsigned int maxConnections);
    ~Database();

    // called by Connection when destructing
    void returnConnection(Connection* connection);
    
private:
    void checkVersion();
    void loadPackage(Connection* connection, odbc::ResultSet* result, Package** package);
    long getNextDatabaseID(Connection* connection, std::string sequenceName);

    
    static Database* _instance;
    static Mutex _databaseMutex;
    
    Mutex _getConnectionMutex;
    
    std::vector<odbc::Connection*> _connectionPool;
    std::vector<Connection*> _connectionsInUse;
    std::string _dsn;
    std::string _username;
    std::string _password;
    int _numConnections;
    int _maxConnections;
};



};



#endif


