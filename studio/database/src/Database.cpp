/*
 * $Id: Database.cpp,v 1.4 2008/10/08 10:16:06 john_f Exp $
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
 
#include <cassert>

#include <odbc++/drivermanager.h>
#include <odbc++/connection.h>
#include <odbc++/statement.h>
#include <odbc++/preparedstatement.h>
#include <odbc++/resultset.h>
#include <odbc++/types.h>

#include "Database.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"

using namespace std;
using namespace prodauto;


// TODO: add basic checks to ensure valid objects are saved


static const int g_compatibilityVersion = 8;



#define START_QUERY_BLOCK \
    try
    
#define END_QUERY_BLOCK(message) \
    catch (odbc::SQLException& ex) \
    { \
        PA_LOGTHROW(DBException, (message ": %s", ex.getMessage().c_str())); \
    } \
    catch (DBException& ex) \
    { \
        throw; \
    } \
    catch (...) \
    { \
        PA_LOGTHROW(DBException, ("Unknown exception: " message)); \
    }

#define START_UPDATE_BLOCK \
    try
    
#define END_UPDATE_BLOCK(message) \
    catch (odbc::SQLException& ex) \
    { \
        try \
        { \
            connection->rollback(); \
        } \
        catch (...) {} \
        PA_LOGTHROW(DBException, (message ": %s", ex.getMessage().c_str())); \
    } \
    catch (DBException& ex) \
    { \
        try \
        { \
            connection->rollback(); \
        } \
        catch (...) {} \
        throw; \
    } \
    catch (...) \
    { \
        try \
        { \
            connection->rollback(); \
        } \
        catch (...) {} \
        PA_LOGTHROW(DBException, ("Unknown exception: " message)); \
    }

#define START_CHILD_UPDATE_BLOCK \
    try
    
#define END_CHILD_UPDATE_BLOCK(message) \
    END_QUERY_BLOCK(message)


    
#define SET_OPTIONAL_VARCHAR(prepStatement, paramIndex, value)  \
    { \
        string val = value; \
        if (val.length() > 0) \
        { \
            prepStatement->setString(paramIndex, val); \
        } \
        else \
        { \
            prepStatement->setNull(paramIndex, SQL_VARCHAR); \
        } \
    } \
    
#define SET_OPTIONAL_INT(condition, prepStatement, paramIndex, value)  \
    if (condition) \
    { \
        prepStatement->setInt(paramIndex, value); \
    } \
    else \
    { \
        prepStatement->setNull(paramIndex, SQL_INTEGER); \
    }



Database* Database::_instance = 0;
Mutex Database::_databaseMutex = Mutex();

void Database::initialise(string dsn, string username, string password,
    unsigned int initialConnections, unsigned int maxConnections)
{
    LOCK_SECTION(_databaseMutex);
    
    if (_instance != 0)
    {
        delete _instance;
        _instance = 0;
    }
    
    _instance = new Database(dsn, username, password, initialConnections, maxConnections);
}

void Database::close()
{
    LOCK_SECTION(_databaseMutex);
    
    delete _instance;
    _instance = 0;
}

Database* Database::getInstance()
{
    LOCK_SECTION(_databaseMutex);
    
    if (_instance == 0)
    {
        PA_LOGTHROW(DBException, ("Database has not been initialised"));
    }
    
    return _instance;
}


Database::Database(string dsn, string username, string password, unsigned int initialConnections, 
    unsigned int maxConnections)
: _getConnectionMutex(), _dsn(dsn), _username(username), _password(password), _numConnections(0), 
_maxConnections(maxConnections)
{
    unsigned int i;
    
    assert(initialConnections > 0 && maxConnections >= initialConnections);
    
    try
    {
        // fill odbc connection pool
        for (i = 0; i < initialConnections; i++)
        {
            _connectionPool.push_back(odbc::DriverManager::getConnection(dsn, username, password));
        }
        
        // check compatibility with database version
        checkVersion();
    }
    catch (odbc::SQLException& ex)
    {
        // clean up
        vector<odbc::Connection*>::iterator odbcIter;
        for (odbcIter = _connectionPool.begin(); odbcIter != _connectionPool.end(); odbcIter++)
        {
            try
            {
                delete (*odbcIter);
            }
            catch (...) {}
        }
        
        odbc::DriverManager::shutdown();
        
        PA_LOGTHROW(DBException, ("Failed to create database object:\n%s", ex.getMessage().c_str()));
    }
    catch (DBException& ex)
    {
        // clean up
        vector<odbc::Connection*>::iterator odbcIter;
        for (odbcIter = _connectionPool.begin(); odbcIter != _connectionPool.end(); odbcIter++)
        {
            try
            {
                delete (*odbcIter);
            }
            catch (...) {}
        }
        
        odbc::DriverManager::shutdown();
        
        throw;
    }
    catch (...)
    {
        // clean up
        vector<odbc::Connection*>::iterator odbcIter;
        for (odbcIter = _connectionPool.begin(); odbcIter != _connectionPool.end(); odbcIter++)
        {
            try
            {
                delete (*odbcIter);
            }
            catch (...) {}
        }
        
        odbc::DriverManager::shutdown();
        
        PA_LOGTHROW(DBException, ("Failed to create database object"));
    }
}

Database::~Database()
{
    while (_connectionsInUse.begin() != _connectionsInUse.end())
    {
        // this will return the connection to _connectionPool and delete the 
        // connection from _connectionsInUse
        try
        {
            delete *_connectionsInUse.begin();
        }
        catch (...) {}
    }
    
    vector<odbc::Connection*>::iterator odbcIter;
    for (odbcIter = _connectionPool.begin(); odbcIter != _connectionPool.end(); odbcIter++)
    {
        try
        {
            delete (*odbcIter);
        }
        catch (...) {}
    }
    
    odbc::DriverManager::shutdown();
}

Transaction* Database::getTransaction()
{
    return dynamic_cast<Transaction*>(getConnection(true));
}

Connection* Database::getConnection(bool isTransaction)
{
    vector<odbc::Connection*>::iterator iter;
    odbc::Connection* odbcConnection;
    Connection* connection = NULL;

    LOCK_SECTION(_getConnectionMutex);

    try
    {
        iter = _connectionPool.begin();
        
        // add a new connection to the pool if needed
        if (iter == _connectionPool.end())
        {
            if (_connectionsInUse.size() >= static_cast<size_t>(_maxConnections))
            {
                PA_LOGTHROW(DBException, ("No connections available in pool"));
            }
            
            try
            {
                _connectionPool.push_back(odbc::DriverManager::getConnection(_dsn, _username, _password));
            }
            catch (odbc::SQLException& ex)
            {
                PA_LOGTHROW(DBException, ("Failed to get new database connection:\n%s", ex.getMessage().c_str()));
            }
            catch (...)
            {
                PA_LOGTHROW(DBException, ("Failed to get new database connection"));
            }
    
            iter = _connectionPool.begin();
        }
    
        // transfer odbc connection from pool to connections in use
        
        odbcConnection = *iter;
        _connectionPool.erase(iter);
    
        if (isTransaction)
        {
            connection = new Transaction(this, odbcConnection);
        }
        else
        {
            connection = new Connection(this, odbcConnection);
        }
        _connectionsInUse.push_back(connection);
        
        
        return connection;
    }
    catch (odbc::SQLException& ex)
    {
        PA_LOGTHROW(DBException, ("Failed to get database connection:\n%s", ex.getMessage().c_str()));
    }
    catch (DBException& ex)
    {
        throw;
    }
    catch (...)
    {
        PA_LOGTHROW(DBException, ("Failed to get database connection"));
    }
    
}

void Database::returnConnection(Connection* connection)
{
    vector<Connection*>::iterator iter;
    Connection* knownConnection = 0;
    
    LOCK_SECTION(_getConnectionMutex);

    // find the connection
    for (iter = _connectionsInUse.begin(); iter != _connectionsInUse.end(); iter++)
    {
        if (connection == *iter)
        {
            knownConnection = connection;
            break;
        }
    }
    
    // it should be in there
    assert(knownConnection == connection);
    
    // return odbc connection to pool
    if (connection->_odbcConnection != 0)
    {
        // ensure transactions are ended
        if (!connection->_odbcConnection->getAutoCommit())
        {
            try
            {
                connection->_odbcConnection->rollback();
            }
            catch (...)
            {
                Logging::warning("Failed to end (rollback) transaction in Database::returnConnection");
            }
        }
    
        _connectionPool.push_back(connection->_odbcConnection);
        connection->_odbcConnection = 0;
    }
    _connectionsInUse.erase(iter);
}



#define SQL_GET_RECORDING_LOCATIONS \
" \
    SELECT \
        rlc_identifier, \
        rlc_name \
    FROM \
        recordinglocation \
"

map<long, string> Database::loadLiveRecordingLocations()
{
    auto_ptr<Connection> connection(getConnection());
    map<long, string> recordingLocations;
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());

        odbc::ResultSet* result = statement->executeQuery(SQL_GET_RECORDING_LOCATIONS);
        while (result->next())
        {
            recordingLocations.insert(pair<long, string>(result->getInt(1), result->getString(2)));
        }
        
        return recordingLocations;
    }
    END_QUERY_BLOCK("Failed to load recording locations")
}



#define SQL_GET_RECORDER \
" \
    SELECT \
        rer_identifier, \
        rer_name, \
        rer_conf_id \
    FROM Recorder \
    WHERE \
        rer_name = ? \
"

#define SQL_GET_RECORDER_CONFIG \
" \
    SELECT \
        rec_identifier, \
        rec_name \
    FROM RecorderConfig \
    WHERE \
        rec_recorder_id= ? \
"

#define SQL_GET_RECORDER_PARAMS \
" \
    SELECT \
        rep_identifier, \
        rep_name, \
        rep_value, \
        rep_type \
    FROM RecorderParameter \
    WHERE \
        rep_recorder_conf_id= ? \
"

#define SQL_GET_RECORDER_INPUT_CONFIG \
" \
    SELECT \
        ric_identifier, \
        ric_index, \
        ric_name \
    FROM RecorderInputConfig \
    WHERE \
        ric_recorder_conf_id = ? \
"

#define SQL_GET_RECORDER_INPUT_TRACK_CONFIG \
" \
    SELECT \
        rtc_identifier, \
        rtc_index, \
        rtc_track_number, \
        rtc_source_id, \
        rtc_source_track_id \
    FROM RecorderInputTrackConfig \
    WHERE \
        rtc_recorder_input_id = ? \
"

Recorder* Database::loadRecorder(string name)
{
    auto_ptr<Recorder> recorder(new Recorder());
    auto_ptr<Connection> connection(getConnection());
    long connectedRecorderConfigId;
    RecorderConfig* recorderConfig;
    RecorderInputConfig* recorderInputConfig;
    RecorderInputTrackConfig* recorderInputTrackConfig;
    
    START_QUERY_BLOCK
    {
        // load recorder

        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_RECORDER));
        prepStatement->setString(1, name);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            PA_LOGTHROW(DBException, ("Recorder with name '%s' does not exist", name.c_str()));
        }
        
        recorder->wasLoaded(result->getInt(1));
        recorder->name = result->getString(2);
        connectedRecorderConfigId = result->getInt(3);
        if (result->wasNull())
        {
            connectedRecorderConfigId = 0;
        }
        
        
        // load recorder configurations

        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_RECORDER_CONFIG));
        prepStatement->setInt(1, recorder->getDatabaseID());
        
        result = prepStatement->executeQuery();
        while (result->next())
        {
            recorderConfig = new RecorderConfig();
            recorder->setAlternateConfig(recorderConfig);
            recorderConfig->wasLoaded(result->getInt(1));
            recorderConfig->name = result->getString(2);
            
            if (recorderConfig->getDatabaseID() == connectedRecorderConfigId)
            {
                recorder->setConfig(recorderConfig);
            }


            // load recorder parameters
    
            auto_ptr<odbc::PreparedStatement> prepStatement5 = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_RECORDER_PARAMS));
            prepStatement5->setInt(1, recorderConfig->getDatabaseID());
            
            odbc::ResultSet* result5 = prepStatement5->executeQuery();
            while (result5->next())
            {
                RecorderParameter recParam;
                recParam.wasLoaded(result5->getInt(1));
                recParam.name = result5->getString(2);
                recParam.value = result5->getString(3);
                recParam.type = result5->getInt(4);
                recorderConfig->parameters.insert(pair<string, RecorderParameter>(recParam.name, recParam));
            }
            
            // load input configurations
    
            auto_ptr<odbc::PreparedStatement> prepStatement2 = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_RECORDER_INPUT_CONFIG));
            prepStatement2->setInt(1, recorderConfig->getDatabaseID());
            
            odbc::ResultSet* result2 = prepStatement2->executeQuery();
            while (result2->next())
            {
                recorderInputConfig = new RecorderInputConfig();
                recorderConfig->recorderInputConfigs.push_back(recorderInputConfig);
                recorderInputConfig->wasLoaded(result2->getInt(1));
                recorderInputConfig->index = result2->getInt(2);
                recorderInputConfig->name = result2->getString(3);
                
                // load track configurations
                
                auto_ptr<odbc::PreparedStatement> prepStatement3(connection->prepareStatement(SQL_GET_RECORDER_INPUT_TRACK_CONFIG));
                prepStatement3->setInt(1, recorderInputConfig->getDatabaseID());
                
                odbc::ResultSet* result3 = prepStatement3->executeQuery();
                while (result3->next())
                {
                    recorderInputTrackConfig = new RecorderInputTrackConfig();
                    recorderInputConfig->trackConfigs.push_back(recorderInputTrackConfig);
                    recorderInputTrackConfig->wasLoaded(result3->getInt(1));
                    recorderInputTrackConfig->index = result3->getInt(2);
                    recorderInputTrackConfig->number = result3->getInt(3);
                    long sourceConfigID = result3->getInt(4);
                    if (!result3->wasNull() && sourceConfigID != 0)
                    {
                        recorderInputTrackConfig->sourceTrackID = result3->getInt(5);
                        
                        recorderInputTrackConfig->sourceConfig = 
                            recorderConfig->getSourceConfig(sourceConfigID, recorderInputTrackConfig->sourceTrackID);
                            
                        if (recorderInputTrackConfig->sourceConfig == 0)
                        {
                            // load source config
                            recorderInputTrackConfig->sourceConfig = loadSourceConfig(sourceConfigID);
                            recorderConfig->sourceConfigs.push_back(recorderInputTrackConfig->sourceConfig);
                            if (recorderInputTrackConfig->sourceConfig->getTrackConfig(recorderInputTrackConfig->sourceTrackID) == 0)
                            {
                                PA_LOGTHROW(DBException, ("Reference to non-existing track in recorder input track config"));
                            }
                        }
                    }
                }
            }
        }
        
        if (connectedRecorderConfigId != 0 && !recorder->hasConfig())
        {
            PA_LOGTHROW(DBException, ("Recorder config with id '%ld' has incorrect link back "
                "to recorder '%s'", connectedRecorderConfigId, name.c_str()));
        }
    }
    END_QUERY_BLOCK("Failed to load recorder")
    
    return recorder.release();
}

#define SQL_INSERT_RECORDER \
" \
    INSERT INTO Recorder \
    ( \
        rer_identifier, \
        rer_name, \
        rer_conf_id \
    ) \
    VALUES \
    (?, ?, ?) \
"

#define SQL_INSERT_RECORDER_CONFIG \
" \
    INSERT INTO RecorderConfig \
    ( \
        rec_identifier, \
        rec_name, \
        rec_recorder_id \
    ) \
    VALUES \
    (?, ?, ?) \
"

#define SQL_INSERT_RECORDER_PARAM \
" \
    INSERT INTO RecorderParameter \
    ( \
        rep_identifier, \
        rep_name, \
        rep_value, \
        rep_type, \
        rep_recorder_conf_id \
    ) \
    VALUES \
    (?, ?, ?, ?, ?) \
"

#define SQL_INSERT_RECORDER_INPUT_CONFIG \
" \
    INSERT INTO RecorderInputConfig \
    ( \
        ric_identifier, \
        ric_index, \
        ric_name, \
        ric_recorder_conf_id \
    ) \
    VALUES \
    (?, ?, ?, ?) \
"

#define SQL_INSERT_RECORDER_INPUT_TRACK_CONFIG \
" \
    INSERT INTO RecorderInputTrackConfig \
    ( \
        rtc_identifier, \
        rtc_index, \
        rtc_track_number, \
        rtc_source_id, \
        rtc_source_track_id, \
        rtc_recorder_input_id \
    ) \
    VALUES \
    (?, ?, ?, ?, ?, ?) \
"

#define SQL_UPDATE_RECORDER \
" \
    UPDATE Recorder \
    SET rer_name = ?, \
        rer_conf_id = ? \
    WHERE \
        rer_identifier = ? \
"

#define SQL_UPDATE_RECORDER_CONNECTED_CONFIG \
" \
    UPDATE Recorder \
    SET rer_conf_id = ? \
    WHERE \
        rer_identifier = ? \
"

#define SQL_UPDATE_RECORDER_PARAM \
" \
    UPDATE RecorderParameter \
    SET rep_name = ?, \
        rep_value = ?, \
        rep_type = ?, \
        rep_recorder_conf_id = ? \
    WHERE \
        rep_identifier = ? \
"

#define SQL_UPDATE_RECORDER_CONFIG \
" \
    UPDATE RecorderConfig \
    SET rec_name = ?, \
        rec_recorder_id = ? \
    WHERE \
        rec_identifier = ? \
"

#define SQL_UPDATE_RECORDER_INPUT_CONFIG \
" \
    UPDATE RecorderInputConfig \
    SET ric_index = ?, \
        ric_name = ?, \
        ric_recorder_conf_id = ? \
    WHERE \
        ric_identifier = ? \
"

#define SQL_UPDATE_RECORDER_INPUT_TRACK_CONFIG \
" \
    UPDATE RecorderInputTrackConfig \
    SET rtc_index = ?, \
        rtc_track_number = ?, \
        rtc_source_id = ?, \
        rtc_source_track_id = ?, \
        rtc_recorder_input_id = ? \
    WHERE \
        rtc_identifier = ? \
"

void Database::saveRecorder(Recorder* recorder, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextRecorderDatabaseID = 0;
        int paramIndex = 1;
        
        // insert
        if (!recorder->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_RECORDER));
            nextRecorderDatabaseID = getNextDatabaseID(connection, "rer_id_seq");
            prepStatement->setInt(paramIndex++, nextRecorderDatabaseID);
            connection->registerCommitListener(nextRecorderDatabaseID, recorder);
        }
        // update
        else
        {
            nextRecorderDatabaseID = recorder->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_RECORDER));
        }
        
        prepStatement->setString(paramIndex++, recorder->name);
        SET_OPTIONAL_INT(recorder->hasConfig() && recorder->getConfig()->isPersistent(), 
            prepStatement, paramIndex++, recorder->getConfig()->getDatabaseID());

        // update
        if (recorder->isPersistent())
        {
            prepStatement->setInt(paramIndex++, recorder->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving recorder"));
        }


        // save recorder configs
        vector<RecorderConfig*>::const_iterator iter1;
        for (iter1 = recorder->getAllConfigs().begin(); iter1 != recorder->getAllConfigs().end(); iter1++)
        {
            RecorderConfig* recorderConfig = *iter1;
            
            long nextRecorderConfigDatabaseID = 0;
            paramIndex = 1;
            
            // insert
            if (!recorderConfig->isPersistent())
            {
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_RECORDER_CONFIG));
                nextRecorderConfigDatabaseID = getNextDatabaseID(connection, "rec_id_seq");
                prepStatement->setInt(paramIndex++, nextRecorderConfigDatabaseID);
                connection->registerCommitListener(nextRecorderConfigDatabaseID, recorderConfig);
            }
            // update
            else
            {
                nextRecorderConfigDatabaseID = recorderConfig->getDatabaseID();
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_RECORDER_CONFIG));
            }
            
            prepStatement->setString(paramIndex++, recorderConfig->name);
            prepStatement->setInt(paramIndex++, nextRecorderDatabaseID);
    
            // update
            if (recorderConfig->isPersistent())
            {
                prepStatement->setInt(paramIndex++, recorderConfig->getDatabaseID());
            }
            
            if (prepStatement->executeUpdate() != 1)
            {
                PA_LOGTHROW(DBException, ("No inserts/updates when saving recorder config"));
            }
    
            
            // update the recorder if the recorder config is connected and is new
            if (recorder->hasConfig() && recorderConfig == recorder->getConfig())
            {
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_RECORDER_CONNECTED_CONFIG));
                prepStatement->setInt(1, nextRecorderConfigDatabaseID);
                prepStatement->setInt(2, nextRecorderDatabaseID);
                
                if (prepStatement->executeUpdate() != 1)
                {
                    PA_LOGTHROW(DBException, ("No updates when updating recorder with connected config"));
                }
            }
           
            
            // save the recorder parameters
            
            map<string, RecorderParameter>::iterator iter5;
            for (iter5 = recorderConfig->parameters.begin(); iter5 != recorderConfig->parameters.end(); iter5++)
            {
                RecorderParameter& recParameter = (*iter5).second; // using ref so that object in map is updated
                
                long nextParameterDatabaseID = 0;
                paramIndex = 1;
                
                // insert
                if (!recParameter.isPersistent())
                {
                    prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_RECORDER_PARAM));
                    nextParameterDatabaseID = getNextDatabaseID(connection, "rep_id_seq");
                    prepStatement->setInt(paramIndex++, nextParameterDatabaseID);
                    connection->registerCommitListener(nextParameterDatabaseID, &recParameter);
                }
                // update
                else
                {
                    nextParameterDatabaseID = recParameter.getDatabaseID();
                    prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_RECORDER_PARAM));
                }
                
                prepStatement->setString(paramIndex++, recParameter.name);
                prepStatement->setString(paramIndex++, recParameter.value);
                prepStatement->setInt(paramIndex++, recParameter.type);
                prepStatement->setInt(paramIndex++, nextRecorderConfigDatabaseID);
        
                // update
                if (recParameter.isPersistent())
                {
                    prepStatement->setInt(paramIndex++, recParameter.getDatabaseID());
                }
                
                if (prepStatement->executeUpdate() != 1)
                {
                    PA_LOGTHROW(DBException, ("No inserts/updates when saving recorder parameter"));
                }
            }
           
            
            
            // save recorder input configs
            vector<RecorderInputConfig*>::const_iterator iter2;
            for (iter2 = recorderConfig->recorderInputConfigs.begin(); iter2 != recorderConfig->recorderInputConfigs.end(); iter2++)
            {
                RecorderInputConfig* inputConfig = *iter2;
                
                long nextInputConfigDatabaseID = 0;
                paramIndex = 1;
                
                // insert
                if (!inputConfig->isPersistent())
                {
                    prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_RECORDER_INPUT_CONFIG));
                    nextInputConfigDatabaseID = getNextDatabaseID(connection, "ric_id_seq");
                    prepStatement->setInt(paramIndex++, nextInputConfigDatabaseID);
                    connection->registerCommitListener(nextInputConfigDatabaseID, inputConfig);
                }
                // update
                else
                {
                    nextInputConfigDatabaseID = inputConfig->getDatabaseID();
                    prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_RECORDER_INPUT_CONFIG));
                }
                
                prepStatement->setInt(paramIndex++, inputConfig->index);
                SET_OPTIONAL_VARCHAR(prepStatement, paramIndex++, inputConfig->name);
                prepStatement->setInt(paramIndex++, nextRecorderConfigDatabaseID);
        
                // update
                if (inputConfig->isPersistent())
                {
                    prepStatement->setInt(paramIndex++, inputConfig->getDatabaseID());
                }
                
                if (prepStatement->executeUpdate() != 1)
                {
                    PA_LOGTHROW(DBException, ("No inserts/updates when saving recorder input config"));
                }
        
                // save recorder input track configs
                vector<RecorderInputTrackConfig*>::const_iterator iter3;
                for (iter3 = inputConfig->trackConfigs.begin(); iter3 != inputConfig->trackConfigs.end(); iter3++)
                {
                    RecorderInputTrackConfig* inputTrackConfig = *iter3;
                    
                    long nextInputTrackConfigDatabaseID = 0;
                    paramIndex = 1;
                    
                    // insert
                    if (!inputTrackConfig->isPersistent())
                    {
                        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_RECORDER_INPUT_TRACK_CONFIG));
                        nextInputTrackConfigDatabaseID = getNextDatabaseID(connection, "rtc_id_seq");
                        prepStatement->setInt(paramIndex++, nextInputTrackConfigDatabaseID);
                        connection->registerCommitListener(nextInputTrackConfigDatabaseID, inputTrackConfig);
                    }
                    // update
                    else
                    {
                        nextInputTrackConfigDatabaseID = inputTrackConfig->getDatabaseID();
                        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_RECORDER_INPUT_TRACK_CONFIG));
                    }
                    
                    prepStatement->setInt(paramIndex++, inputTrackConfig->index);
                    prepStatement->setInt(paramIndex++, inputTrackConfig->number);
                    SET_OPTIONAL_INT(inputTrackConfig->sourceConfig != 0, prepStatement, paramIndex++, 
                        inputTrackConfig->sourceConfig->getID());
                    SET_OPTIONAL_INT(inputTrackConfig->sourceConfig != 0, prepStatement, paramIndex++, 
                        inputTrackConfig->sourceTrackID);
                    prepStatement->setInt(paramIndex++, nextInputConfigDatabaseID);
            
                    // update
                    if (inputTrackConfig->isPersistent())
                    {
                        prepStatement->setInt(paramIndex++, inputTrackConfig->getDatabaseID());
                    }
                    
                    if (prepStatement->executeUpdate() != 1)
                    {
                        PA_LOGTHROW(DBException, ("No inserts/updates when saving recorder input track config"));
                    }
                }
            }
        }
        
		connection->commit();
    }
    END_UPDATE_BLOCK("Failed to save recorder")
}


#define SQL_DELETE_RECORDER \
" \
    DELETE FROM Recorder WHERE rer_identifier = ? \
"

void Database::deleteRecorder(Recorder* recorder, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_RECORDER));
        prepStatement->setInt(1, recorder->getDatabaseID());
        
        connection->registerCommitListener(0, recorder);

        // the database will cascade the delete down to the recorder input track configs
        // so we register additional listeners for those objects
        vector<RecorderConfig*>::const_iterator iter1;
        for (iter1 = recorder->getAllConfigs().begin(); iter1 != recorder->getAllConfigs().end(); iter1++)
        {
            RecorderConfig* recorderConfig = *iter1;
            connection->registerCommitListener(0, recorderConfig);
            
            map<string, RecorderParameter>::iterator iter5;
            for (iter5 = recorderConfig->parameters.begin(); iter5 != recorderConfig->parameters.end(); iter5++)
            {
                RecorderParameter& recParam = (*iter5).second; // using ref so that object in map is updated
                connection->registerCommitListener(0, &recParam);
            }
            
            vector<RecorderInputConfig*>::const_iterator iter2;
            for (iter2 = recorderConfig->recorderInputConfigs.begin(); iter2 != recorderConfig->recorderInputConfigs.end(); iter2++)
            {
                RecorderInputConfig* inputConfig = *iter2;
                connection->registerCommitListener(0, inputConfig);
                
                vector<RecorderInputTrackConfig*>::const_iterator iter3;
                for (iter3 = inputConfig->trackConfigs.begin(); iter3 != inputConfig->trackConfigs.end(); iter3++)
                {
                    RecorderInputTrackConfig* trackConfig = *iter3;
                    connection->registerCommitListener(0, trackConfig);
                    
                }
            }
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when delete recorder"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete recorder")
}




#define SQL_GET_SOURCE_CONFIG \
" \
    SELECT \
        scf_identifier, \
        scf_name, \
        scf_type, \
        scf_spool_number, \
        scf_recording_location \
    FROM SourceConfig \
    WHERE \
        scf_identifier = ? \
"

#define SQL_GET_SOURCE_TRACK_CONFIG \
" \
    SELECT \
        sct_identifier, \
        sct_track_id, \
        sct_track_number, \
        sct_track_name, \
        sct_track_data_def, \
        (sct_track_edit_rate).numerator, \
        (sct_track_edit_rate).denominator, \
        sct_track_length \
    FROM SourceTrackConfig \
    WHERE \
        sct_source_id = ? \
"

SourceConfig* Database::loadSourceConfig(long databaseID)
{
    auto_ptr<SourceConfig> sourceConfig(new SourceConfig());
    auto_ptr<Connection> connection(getConnection());
    SourceTrackConfig* sourceTrackConfig;
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_SOURCE_CONFIG));
        prepStatement->setInt(1, databaseID);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            PA_LOGTHROW(DBException, ("Source config %ld does not exist in database", databaseID));
        }
        
        sourceConfig->wasLoaded(result->getInt(1));
        sourceConfig->name = result->getString(2);
        sourceConfig->type = result->getInt(3);
        if (sourceConfig->type == TAPE_SOURCE_CONFIG_TYPE)
        {
            sourceConfig->spoolNumber = result->getString(4);
        }
        else // LIVE_SOURCE_CONFIG_TYPE
        {
            sourceConfig->recordingLocation = result->getInt(5);
        }
        

        // load track configurations

        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_SOURCE_TRACK_CONFIG));
        prepStatement->setInt(1, sourceConfig->getDatabaseID());
        
        result = prepStatement->executeQuery();
        while (result->next())
        {
            sourceTrackConfig = new SourceTrackConfig();
            sourceConfig->trackConfigs.push_back(sourceTrackConfig);
            sourceTrackConfig->wasLoaded(result->getInt(1));
            sourceTrackConfig->id = result->getInt(2);
            sourceTrackConfig->number = result->getInt(3);
            sourceTrackConfig->name = result->getString(4);
            sourceTrackConfig->dataDef = result->getInt(5);
            sourceTrackConfig->editRate.numerator = result->getInt(6);
            sourceTrackConfig->editRate.denominator = result->getInt(7);
            sourceTrackConfig->length = result->getLong(8);
        }
    }
    END_QUERY_BLOCK("Failed to load source config")
    
    return sourceConfig.release();
}


#define SQL_INSERT_SOURCE_CONFIG \
" \
    INSERT INTO SourceConfig \
    ( \
        scf_identifier, \
        scf_name, \
        scf_type, \
        scf_spool_number, \
        scf_recording_location \
    ) \
    VALUES \
    (?, ?, ?, ?, ?) \
"

#define SQL_INSERT_SOURCE_TRACK_CONFIG \
" \
    INSERT INTO SourceTrackConfig \
    ( \
        sct_identifier, \
        sct_track_id, \
        sct_track_number, \
        sct_track_name, \
        sct_track_data_def, \
        sct_track_edit_rate, \
        sct_track_length, \
        sct_source_id \
    ) \
    VALUES \
    (?, ?, ?, ?, ?, (?, ?), ?, ?) \
"

#define SQL_UPDATE_SOURCE_CONFIG \
" \
    UPDATE SourceConfig \
    SET scf_name = ?, \
        scf_type = ?, \
        scf_spool_number = ?, \
        scf_recording_location = ? \
    WHERE \
        scf_identifier = ? \
"

#define SQL_UPDATE_SOURCE_TRACK_CONFIG \
" \
    UPDATE SourceTrackConfig \
    SET sct_track_id = ?, \
        sct_track_number = ?, \
        sct_track_name = ?, \
        sct_track_data_def = ?, \
        sct_track_edit_rate = (?, ?), \
        sct_track_length = ?, \
        sct_source_id = ? \
    WHERE \
        sct_identifier = ? \
"

void Database::saveSourceConfig(SourceConfig* config, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextConfigDatabaseID = 0;
        int paramIndex = 1;
        
        // insert
        if (!config->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_SOURCE_CONFIG));
            nextConfigDatabaseID = getNextDatabaseID(connection, "scf_id_seq");
            prepStatement->setInt(paramIndex++, nextConfigDatabaseID);
            connection->registerCommitListener(nextConfigDatabaseID, config);
        }
        // update
        else
        {
            nextConfigDatabaseID = config->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_SOURCE_CONFIG));
        }
        
        prepStatement->setString(paramIndex++, config->name);
        prepStatement->setInt(paramIndex++, config->type);
        if (config->type == TAPE_SOURCE_CONFIG_TYPE)
        {
            prepStatement->setString(paramIndex++, config->spoolNumber);
            prepStatement->setNull(paramIndex++, SQL_INTEGER);
        }
        else // LIVE_SOURCE_CONFIG_TYPE
        {
            prepStatement->setNull(paramIndex++, SQL_VARCHAR);
            prepStatement->setInt(paramIndex++, config->recordingLocation);
        }

        // update
        if (config->isPersistent())
        {
            prepStatement->setInt(paramIndex++, config->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving source config"));
        }


        // save source track configs
        vector<SourceTrackConfig*>::const_iterator iter1;
        for (iter1 = config->trackConfigs.begin(); iter1 != config->trackConfigs.end(); iter1++)
        {
            SourceTrackConfig* trackConfig = *iter1;
            
            long nextTrackConfigDatabaseID = 0;
            paramIndex = 1;
            
            // insert
            if (!trackConfig->isPersistent())
            {
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_SOURCE_TRACK_CONFIG));
                nextTrackConfigDatabaseID = getNextDatabaseID(connection, "sct_id_seq");
                prepStatement->setInt(paramIndex++, nextTrackConfigDatabaseID);
                connection->registerCommitListener(nextTrackConfigDatabaseID, trackConfig);
            }
            // update
            else
            {
                nextTrackConfigDatabaseID = trackConfig->getDatabaseID();
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_SOURCE_TRACK_CONFIG));
            }
            
            prepStatement->setInt(paramIndex++, trackConfig->id);
            prepStatement->setInt(paramIndex++, trackConfig->number);
            prepStatement->setString(paramIndex++, trackConfig->name);
            prepStatement->setInt(paramIndex++, trackConfig->dataDef);
            prepStatement->setInt(paramIndex++, trackConfig->editRate.numerator);
            prepStatement->setInt(paramIndex++, trackConfig->editRate.denominator);
            prepStatement->setLong(paramIndex++, trackConfig->length);
            prepStatement->setInt(paramIndex++, nextConfigDatabaseID);
    
            // update
            if (trackConfig->isPersistent())
            {
                prepStatement->setInt(paramIndex++, trackConfig->getDatabaseID());
            }
            
            if (prepStatement->executeUpdate() != 1)
            {
                PA_LOGTHROW(DBException, ("No inserts/updates when saving source track config"));
            }
    
        }
        
		connection->commit();
    }
    END_UPDATE_BLOCK("Failed to save source config")
}


#define SQL_DELETE_SOURCE_CONFIG \
" \
    DELETE FROM SourceConfig WHERE scf_identifier = ? \
"

void Database::deleteSourceConfig(SourceConfig* config, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_SOURCE_CONFIG));
        prepStatement->setInt(1, config->getDatabaseID());
        
        connection->registerCommitListener(0, config);

        // the database will cascade the delete down to the source track configs
        // so we register additional listeners for those objects
        vector<SourceTrackConfig*>::const_iterator iter;
        for (iter = config->trackConfigs.begin(); iter != config->trackConfigs.end(); iter++)
        {
            SourceTrackConfig* trackConfig = *iter;
            connection->registerCommitListener(0, trackConfig);
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when delete source config"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete source config")
}


#define SQL_GET_ALL_ROUTER_CONFIGS \
" \
    SELECT \
        ror_identifier, \
        ror_name \
    FROM RouterConfig \
"

vector<RouterConfig*> Database::loadAllRouterConfigs()
{
    auto_ptr<Connection> connection(getConnection());
    VectorGuard<RouterConfig> allRouterConfigs;
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());
        
        odbc::ResultSet* result = statement->executeQuery(SQL_GET_ALL_ROUTER_CONFIGS);
        while (result->next())
        {
            RouterConfig* routerConfig = loadRouterConfig(result->getString(2));
            if (routerConfig)
            {
                allRouterConfigs.get().push_back(routerConfig);
            }
        }
    }
    END_QUERY_BLOCK("Failed to load all router configs")
    
    return allRouterConfigs.release();
}

#define SQL_GET_ROUTER_CONFIG \
" \
    SELECT \
        ror_identifier, \
        ror_name \
    FROM RouterConfig \
    WHERE \
        ror_name = ? \
"

#define SQL_GET_ROUTER_INPUT_CONFIG \
" \
    SELECT \
        rti_identifier, \
        rti_index, \
        rti_name, \
        rti_source_id, \
        rti_source_track_id \
    FROM RouterInputConfig \
    WHERE \
        rti_router_conf_id = ? \
"

#define SQL_GET_ROUTER_OUTPUT_CONFIG \
" \
    SELECT \
        rto_identifier, \
        rto_index, \
        rto_name \
    FROM RouterOutputConfig \
    WHERE \
        rto_router_conf_id = ? \
"


RouterConfig* Database::loadRouterConfig(string name)
{
    auto_ptr<RouterConfig> routerConfig(new RouterConfig());
    auto_ptr<Connection> connection(getConnection());
    RouterInputConfig* routerInputConfig;
    RouterOutputConfig* routerOutputConfig;
    long sourceConfigID;
    
    START_QUERY_BLOCK
    {
        // load router config

        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_ROUTER_CONFIG));
        prepStatement->setString(1, name);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            PA_LOGTHROW(DBException, ("Router config with name '%s' does not exist", name.c_str()));
        }
        
        routerConfig->wasLoaded(result->getInt(1));
        routerConfig->name = result->getString(2);
        
        
        // load input configurations

        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_ROUTER_INPUT_CONFIG));
        prepStatement->setInt(1, routerConfig->getDatabaseID());
        
        result = prepStatement->executeQuery();
        while (result->next())
        {
            routerInputConfig = new RouterInputConfig();
            routerConfig->inputConfigs.push_back(routerInputConfig);
            routerInputConfig->wasLoaded(result->getInt(1));
            routerInputConfig->index = result->getInt(2);
            routerInputConfig->name = result->getString(3);
            sourceConfigID = result->getInt(4);
            if (!result->wasNull() && sourceConfigID != 0)
            {
                routerInputConfig->sourceTrackID = result->getInt(5);
                
                routerInputConfig->sourceConfig = 
                    routerConfig->getSourceConfig(sourceConfigID, routerInputConfig->sourceTrackID);
                    
                if (routerInputConfig->sourceConfig == 0)
                {
                    // load source config
                    routerInputConfig->sourceConfig = loadSourceConfig(sourceConfigID);
                    routerConfig->sourceConfigs.push_back(routerInputConfig->sourceConfig);
                    if (routerInputConfig->sourceConfig->getTrackConfig(routerInputConfig->sourceTrackID) == 0)
                    {
                        PA_LOGTHROW(DBException, ("Reference to non-existing track in router input config"));
                    }
                }
            }
        }

        
        // load output configurations

        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_ROUTER_OUTPUT_CONFIG));
        prepStatement->setInt(1, routerConfig->getDatabaseID());
        
        result = prepStatement->executeQuery();
        while (result->next())
        {
            routerOutputConfig = new RouterOutputConfig();
            routerConfig->outputConfigs.push_back(routerOutputConfig);
            routerOutputConfig->wasLoaded(result->getInt(1));
            routerOutputConfig->index = result->getInt(2);
            routerOutputConfig->name = result->getString(3);
        }        
    }
    END_QUERY_BLOCK("Failed to load router config")
    
    return routerConfig.release();
}




#define SQL_GET_ALL_MC_CLIP_DEFS \
" \
    SELECT \
        mcd_identifier, \
        mcd_name \
    FROM MultiCameraClipDef \
"

#define SQL_GET_MC_TRACK_DEFS \
" \
    SELECT \
        mct_identifier, \
        mct_index, \
        mct_track_number \
    FROM MultiCameraTrackDef \
    WHERE \
        mct_multi_camera_def_id = ? \
"

#define SQL_GET_MC_SELECTOR_DEFS \
" \
    SELECT \
        mcs_identifier, \
        mcs_index, \
        mcs_source_id, \
        mcs_source_track_id \
    FROM MultiCameraSelectorDef \
    WHERE \
        mcs_multi_camera_track_def_id = ? \
"

vector<MCClipDef*> Database::loadAllMultiCameraClipDefs()
{
    VectorGuard<MCClipDef> allMCClipDefs;
    auto_ptr<Connection> connection(getConnection());
    MCTrackDef* mcTrackDef;
    MCSelectorDef* mcSelectorDef;
    uint32_t index;
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());
        
        odbc::ResultSet* result = statement->executeQuery(SQL_GET_ALL_MC_CLIP_DEFS);
        while (result->next())
        {
            MCClipDef* mcClipDef = new MCClipDef();
            allMCClipDefs.get().push_back(mcClipDef);
            
            mcClipDef->wasLoaded(result->getInt(1));
            mcClipDef->name = result->getString(2);

            
            // load track defs
    
            auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_MC_TRACK_DEFS));
            prepStatement->setInt(1, mcClipDef->getDatabaseID());
            
            odbc::ResultSet* result2 = prepStatement->executeQuery();
            while (result2->next())
            {
                index = result2->getInt(2);
                mcTrackDef = new MCTrackDef();
                mcClipDef->trackDefs.insert(pair<uint32_t, MCTrackDef*>(index, mcTrackDef));
                mcTrackDef->wasLoaded(result2->getInt(1));
                mcTrackDef->index = index;
                mcTrackDef->number = result2->getInt(3);
                
                // load selector defs
                
                auto_ptr<odbc::PreparedStatement> prepStatement2(connection->prepareStatement(SQL_GET_MC_SELECTOR_DEFS));
                prepStatement2->setInt(1, mcTrackDef->getDatabaseID());
                
                odbc::ResultSet* result3 = prepStatement2->executeQuery();
                while (result3->next())
                {
                    index = result3->getInt(2);
                    mcSelectorDef = new MCSelectorDef();
                    mcTrackDef->selectorDefs.insert(pair<uint32_t, MCSelectorDef*>(index, mcSelectorDef));
                    mcSelectorDef->wasLoaded(result3->getInt(1));
                    mcSelectorDef->index = index;
                    long sourceConfigID = result3->getInt(3);
                    if (!result3->wasNull() && sourceConfigID != 0)
                    {
                        mcSelectorDef->sourceTrackID = result3->getInt(4);
                        
                        mcSelectorDef->sourceConfig = 
                            mcClipDef->getSourceConfig(sourceConfigID, mcSelectorDef->sourceTrackID);
                            
                        if (mcSelectorDef->sourceConfig == 0)
                        {
                            // load source config
                            mcSelectorDef->sourceConfig = loadSourceConfig(sourceConfigID);
                            mcClipDef->sourceConfigs.push_back(mcSelectorDef->sourceConfig);
                            if (mcSelectorDef->sourceConfig->getTrackConfig(mcSelectorDef->sourceTrackID) == 0)
                            {
                                PA_LOGTHROW(DBException, ("Reference to non-existing track in multi-camera selector def"));
                            }
                        }
                    }
                }
            }
        }
    }
    END_QUERY_BLOCK("Failed to load multi-camera clip defs")
    
    return allMCClipDefs.release();
}

#if 0
void Database::saveMultiCameraClipDef(MCClipDef* mcClipDef, Transaction* transaction)
{
}

void Database::deleteMultiCameraClipDef(MCClipDef* mcClipDef, Transaction* transaction)
{
}
#endif



#define SQL_GET_ALL_SERIES \
" \
    SELECT \
        srs_identifier, \
        srs_name \
    FROM Series \
"

vector<Series*> Database::loadAllSeries()
{
    auto_ptr<Connection> connection(getConnection());
    VectorGuard<Series> allSeries;
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());
        
        odbc::ResultSet* result = statement->executeQuery(SQL_GET_ALL_SERIES);
        while (result->next())
        {
            Series* series = new Series();
            allSeries.get().push_back(series);
            
            series->wasLoaded(result->getInt(1));
            series->name = result->getString(2);
        }
    }
    END_QUERY_BLOCK("Failed to load all series")
    
    return allSeries.release();
}


#define SQL_GET_PROGRAMMES_IN_SERIES \
" \
    SELECT \
        prg_identifier, \
        prg_name \
    FROM Programme, \
        Series \
    WHERE \
        prg_series_id = srs_identifier AND \
        srs_identifier = ? \
"

void Database::loadProgrammesInSeries(Series* series)
{
    auto_ptr<Connection> connection(getConnection());
    
    // remove existing in-memory programmes first
    series->removeAllProgrammes();
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_PROGRAMMES_IN_SERIES));
        prepStatement->setInt(1, series->getDatabaseID());
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        while (result->next())
        {
            auto_ptr<Programme> programme(series->createProgramme());
            
            programme->wasLoaded(result->getInt(1));
            programme->name = result->getString(2);
            
            programme.release();
        }
    }
    END_QUERY_BLOCK("Failed to load programmes in series")
}


#define SQL_INSERT_SERIES \
" \
    INSERT INTO Series \
    ( \
        srs_identifier, \
        srs_name \
    ) \
    VALUES \
    (?, ?) \
"

#define SQL_UPDATE_SERIES \
" \
    UPDATE Series \
    SET srs_name = ? \
    WHERE \
        srs_identifier = ? \
"

void Database::saveSeries(Series* series, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    if (series->name.length() == 0)
    {
        PA_LOGTHROW(DBException, ("Series has empty name"));
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextDatabaseID = 0;
        int paramIndex = 1;
        
        // insert
        if (!series->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_SERIES));
            nextDatabaseID = getNextDatabaseID(connection, "srs_id_seq");
            prepStatement->setInt(paramIndex++, nextDatabaseID);
            connection->registerCommitListener(nextDatabaseID, series);
        }
        // update
        else
        {
            nextDatabaseID = series->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_SERIES));
        }
        
        prepStatement->setString(paramIndex++, series->name);

        // update
        if (series->isPersistent())
        {
            prepStatement->setInt(paramIndex++, series->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving series"));
        }
        
		connection->commit();
    }
    END_UPDATE_BLOCK("Failed to save series")
}

#define SQL_DELETE_SERIES \
" \
    DELETE FROM Series WHERE srs_identifier = ? \
"

void Database::deleteSeries(Series* series, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_SERIES));
        prepStatement->setInt(1, series->getDatabaseID());
        
        connection->registerCommitListener(0, series);

        // the database will cascade the delete down to the take
        // so we register additional listeners for those objects
        vector<Programme*>::const_iterator iter1;
        for (iter1 = series->getProgrammes().begin(); iter1 != series->getProgrammes().end(); iter1++)
        {
            Programme* programme = *iter1;
            connection->registerCommitListener(0, programme);
            
            vector<Item*>::const_iterator iter2;
            for (iter2 = programme->getItems().begin(); iter2 != programme->getItems().end(); iter2++)
            {
                Item* item = *iter2;
                connection->registerCommitListener(0, item);
                
                vector<Take*>::const_iterator iter3;
                for (iter3 = item->getTakes().begin(); iter3 != item->getTakes().end(); iter3++)
                {
                    Take* take = *iter3;

                    connection->registerCommitListener(0, take);
                }
            }
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when deleting series"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete series")
}


#define SQL_GET_ITEMS_IN_PROGRAMME \
" \
    SELECT \
        itm_identifier, \
        itm_description, \
        itm_script_section_ref, \
        itm_order_index \
    FROM Item, \
        Programme \
    WHERE \
        itm_programme_id = prg_identifier AND \
        prg_identifier = ? \
    ORDER BY itm_order_index \
"

void Database::loadItemsInProgramme(Programme* programme)
{
    auto_ptr<Connection> connection(getConnection());
    
    // remove existing in-memory items first
    programme->removeAllItems();
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_ITEMS_IN_PROGRAMME));
        prepStatement->setInt(1, programme->getDatabaseID());
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        while (result->next())
        {
            auto_ptr<Item> item(programme->appendNewItem());
            
            item->wasLoaded(result->getInt(1));
            item->description = result->getString(2);
            item->scriptSectionRefs = getScriptReferences(result->getString(3));
            item->setDBOrderIndex(result->getInt(4));
            
            item.release();
        }
    }
    END_QUERY_BLOCK("Failed to load items in programme")
}


#define SQL_INSERT_PROGRAMME \
" \
    INSERT INTO Programme \
    ( \
        prg_identifier, \
        prg_name, \
        prg_series_id \
    ) \
    VALUES \
    (?, ?, ?) \
"

#define SQL_UPDATE_PROGRAMME \
" \
    UPDATE Programme \
    SET prg_name = ?, \
        prg_series_id = ? \
    WHERE \
        prg_identifier = ? \
"

void Database::saveProgramme(Programme* programme, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    if (programme->name.length() == 0)
    {
        PA_LOGTHROW(DBException, ("Programme has empty name"));
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextDatabaseID = 0;
        int paramIndex = 1;
        
        // insert
        if (!programme->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_PROGRAMME));
            nextDatabaseID = getNextDatabaseID(connection, "prg_id_seq");
            prepStatement->setInt(paramIndex++, nextDatabaseID);
            connection->registerCommitListener(nextDatabaseID, programme);
        }
        // update
        else
        {
            nextDatabaseID = programme->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_PROGRAMME));
        }
        
        prepStatement->setString(paramIndex++, programme->name);
        prepStatement->setInt(paramIndex++, programme->getSeriesID());

        // update
        if (programme->isPersistent())
        {
            prepStatement->setInt(paramIndex++, programme->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving programme"));
        }
        
		connection->commit();
    }
    END_UPDATE_BLOCK("Failed to save programme")
}


#define SQL_DELETE_PROGRAMME \
" \
    DELETE FROM Programme WHERE prg_identifier = ? \
"

void Database::deleteProgramme(Programme* programme, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_PROGRAMME));
        prepStatement->setInt(1, programme->getDatabaseID());
        
        connection->registerCommitListener(0, programme);

        // the database will cascade the delete down to take
        // so we register additional listeners for those objects
        vector<Item*>::const_iterator iter2;
        for (iter2 = programme->getItems().begin(); iter2 != programme->getItems().end(); iter2++)
        {
            Item* item = *iter2;
            connection->registerCommitListener(0, item);
            
            vector<Take*>::const_iterator iter3;
            for (iter3 = item->getTakes().begin(); iter3 != item->getTakes().end(); iter3++)
            {
                Take* take = *iter3;

                connection->registerCommitListener(0, take);
            }
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when deleting programme"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete programme")
}

#define SQL_UPDATE_ITEM_ORDER_INDEX \
" \
    UPDATE Item \
    SET itm_order_index = ? \
    WHERE \
        itm_identifier = ? \
"

void Database::updateItemsOrder(Programme* programme, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        vector<Item*>::const_iterator iter;
        for (iter = programme->_items.begin(); iter != programme->_items.end(); iter++)
        {
            Item* item = *iter;

            if (item->_orderIndex == 0)
            {
                PA_LOGTHROW(DBException, ("Item has invalid order index 0"));
            }
            
            // only items that are persistent and have a new order index are updated
            if (!item->isPersistent() || item->_orderIndex == item->_prevOrderIndex)
            {
                continue;
            }
            
            // this will allow the Item to update it's _prevOrderIndex
            connection->registerCommitListener(item->getDatabaseID(), item);

            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_ITEM_ORDER_INDEX));
            prepStatement->setInt(1, item->_orderIndex);
            prepStatement->setInt(2, item->getDatabaseID());
            
            if (prepStatement->executeUpdate() != 1)
            {
                PA_LOGTHROW(DBException, ("No inserts/updates when updating item order index"));
            }
        }
        
		connection->commit();
    }
    END_UPDATE_BLOCK("Failed to update items order indexes")
}


#define SQL_GET_TAKES_IN_ITEM \
" \
    SELECT \
        tke_identifier, \
        tke_number, \
        tke_comment, \
        tke_result, \
        (tke_edit_rate).numerator, \
        (tke_edit_rate).denominator, \
        tke_length, \
        tke_start_position, \
        tke_start_date, \
        tke_recording_location \
    FROM Take, \
        Item \
    WHERE \
        tke_item_id = itm_identifier AND \
        itm_identifier = ? \
    ORDER BY tke_number \
"

void Database::loadTakesInItem(Item* item)
{
    auto_ptr<Connection> connection(getConnection());
    
    // remove existing in-memory takes first
    item->removeAllTakes();
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_TAKES_IN_ITEM));
        prepStatement->setInt(1, item->getDatabaseID());
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        while (result->next())
        {
            auto_ptr<Take> take(item->createTake());
            
            take->wasLoaded(result->getInt(1));
            take->number = result->getInt(2);
            take->comment = result->getString(3);
            take->result = result->getInt(4);
            take->editRate.numerator = result->getInt(5);
            take->editRate.denominator = result->getInt(6);
            take->length = result->getLong(7);
            take->startPosition = result->getLong(8);
            take->startDate = getDateFromODBC(result->getString(9));
            take->recordingLocation = result->getInt(10);
            
            take.release();
        }
    }
    END_QUERY_BLOCK("Failed to load takes in item")
}


#define SQL_INSERT_ITEM \
" \
    INSERT INTO Item \
    ( \
        itm_identifier, \
        itm_description, \
        itm_script_section_ref, \
        itm_order_index, \
        itm_programme_id \
    ) \
    VALUES \
    (?, ?, ?, ?, ?) \
"

#define SQL_UPDATE_ITEM \
" \
    UPDATE Item \
    SET itm_description = ?, \
        itm_script_section_ref = ?, \
        itm_order_index = ?, \
        itm_programme_id = ? \
    WHERE \
        itm_identifier = ? \
"

void Database::saveItem(Item* item, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextDatabaseID = 0;
        int paramIndex = 1;
        
        // insert
        if (!item->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_ITEM));
            nextDatabaseID = getNextDatabaseID(connection, "itm_id_seq");
            prepStatement->setInt(paramIndex++, nextDatabaseID);
            connection->registerCommitListener(nextDatabaseID, item);
        }
        // update
        else
        {
            nextDatabaseID = item->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_ITEM));
        }
        
        SET_OPTIONAL_VARCHAR(prepStatement, paramIndex++, item->description);
        SET_OPTIONAL_VARCHAR(prepStatement, paramIndex++, getScriptReferencesString(item->scriptSectionRefs));
        prepStatement->setInt(paramIndex++, item->_orderIndex);
        prepStatement->setInt(paramIndex++, item->getProgrammeID());

        // update
        if (item->isPersistent())
        {
            prepStatement->setInt(paramIndex++, item->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving item"));
        }
        
		connection->commit();
    }
    END_UPDATE_BLOCK("Failed to save item")
}


#define SQL_DELETE_ITEM \
" \
    DELETE FROM Item WHERE itm_identifier = ? \
"

void Database::deleteItem(Item* item, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_ITEM));
        prepStatement->setInt(1, item->getDatabaseID());
        
        connection->registerCommitListener(0, item);

        // the database will cascade the delete down to the take's material package
        // so we register additional listeners for those objects
        vector<Take*>::const_iterator iter3;
        for (iter3 = item->getTakes().begin(); iter3 != item->getTakes().end(); iter3++)
        {
            Take* take = *iter3;

            connection->registerCommitListener(0, take);
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when deleting item"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete item")
}



#define SQL_INSERT_TAKE \
" \
    INSERT INTO Take \
    ( \
        tke_identifier, \
        tke_number, \
        tke_comment, \
        tke_result, \
        tke_edit_rate, \
        tke_length, \
        tke_start_position, \
        tke_start_date, \
        tke_recording_location, \
        tke_item_id \
    ) \
    VALUES \
    (?, ?, ?, ?, (?, ?), ?, ?, ?, ?, ?) \
"

#define SQL_UPDATE_TAKE \
" \
    UPDATE Take \
    SET tke_number = ?, \
        tke_comment = ?, \
        tke_result = ?, \
        tke_edit_rate = (?, ?), \
        tke_length = ?, \
        tke_start_position = ?, \
        tke_start_date = ?, \
        tke_recording_location = ?, \
        tke_item_id = ? \
    WHERE \
        tke_identifier = ? \
"

void Database::saveTake(Take* take, Transaction* transaction)
{
    auto_ptr<Transaction> mTransaction;
    Transaction* localTransaction = transaction;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mTransaction = auto_ptr<Transaction>(getTransaction());
        localTransaction = mTransaction.get();
        connection = mTransaction.get();
    }

    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextDatabaseID = 0;
        int paramIndex = 1;
        
        // insert
        if (!take->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_TAKE));
            nextDatabaseID = getNextDatabaseID(connection, "tke_id_seq");
            prepStatement->setInt(paramIndex++, nextDatabaseID);
            connection->registerCommitListener(nextDatabaseID, take);
        }
        // update
        else
        {
            nextDatabaseID = take->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_TAKE));
        }
        
        prepStatement->setInt(paramIndex++, take->number);
        SET_OPTIONAL_VARCHAR(prepStatement, paramIndex++, take->comment);
        prepStatement->setInt(paramIndex++, take->result);
        prepStatement->setInt(paramIndex++, take->editRate.numerator);
        prepStatement->setInt(paramIndex++, take->editRate.denominator);
        prepStatement->setLong(paramIndex++, take->length);
        prepStatement->setLong(paramIndex++, take->startPosition);
        prepStatement->setString(paramIndex++, getDateString(take->startDate));
        prepStatement->setInt(paramIndex++, take->recordingLocation);
        prepStatement->setInt(paramIndex++, take->getItemID());

        // update
        if (take->isPersistent())
        {
            prepStatement->setInt(paramIndex++, take->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving take"));
        }
        
        // commit transaction if no transaction was passed into this function
        if (transaction == 0)
        {
            localTransaction->commitTransaction();
        }
        else
        {
            connection->commit();
        }
    }
    END_UPDATE_BLOCK("Failed to save take")
}


#define SQL_DELETE_TAKE \
" \
    DELETE FROM Take WHERE tke_identifier = ? \
"

void Database::deleteTake(Take* take, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_TAKE));
        prepStatement->setInt(1, take->getDatabaseID());
        
        connection->registerCommitListener(0, take);

        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when deleting take"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete take")
}



#define SQL_GET_TRANSCODES_WITH_STATUSES \
" \
    SELECT \
        trc_identifier, \
        trc_source_material_package_id, \
        trc_dest_material_package_id, \
        trc_created::varchar, \
        trc_target_video_resolution_id, \
        trc_status_id \
    FROM Transcode \
    WHERE \
        trc_status_id IN ( \
"

#define SQL_GET_TRANSCODES_WITH_STATUSES_SUFFIX \
" \
    ORDER BY trc_created \
"


vector<Transcode*> Database::loadTranscodes(vector<int>& statuses)
{
    auto_ptr<Connection> connection(getConnection());
    VectorGuard<Transcode> transcodes;
    vector<int>::const_iterator iter;
    int paramIndex;
    string query;
    
    START_QUERY_BLOCK
    {
        // complete the query string
        query = SQL_GET_TRANSCODES_WITH_STATUSES;
        for (iter = statuses.begin(); iter != statuses.end(); iter++)
        {
            if (iter == statuses.begin())
            {
                query.append("?");
            }
            else
            {
                query.append(",?");
            }
        }
        query.append(") ").append(SQL_GET_TRANSCODES_WITH_STATUSES_SUFFIX);

        
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(query));
        for (iter = statuses.begin(), paramIndex = 1; iter != statuses.end(); iter++, paramIndex++)
        {
            prepStatement->setInt(paramIndex, *iter);
        }
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        while (result->next())
        {
            Transcode* transcode = new Transcode();
            transcodes.get().push_back(transcode);
            
            transcode->wasLoaded(result->getInt(1));
            transcode->sourceMaterialPackageDbId = result->getInt(2);
            transcode->destMaterialPackageDbId = result->getInt(3);
            transcode->created = getTimestampFromODBC(result->getString(4));
            transcode->targetVideoResolution = result->getInt(5);
            transcode->status = result->getInt(6);
        }
    }
    END_QUERY_BLOCK("Failed to load transcodes")
    
    return transcodes.release();
}

#define SQL_GET_TRANSCODES_WITH_STATUS \
" \
    SELECT \
        trc_identifier, \
        trc_source_material_package_id, \
        trc_dest_material_package_id, \
        trc_created::varchar, \
        trc_target_video_resolution_id, \
        trc_status_id \
    FROM Transcode \
    WHERE \
        trc_status_id = ? \
    ORDER BY trc_created \
"

vector<Transcode*> Database::loadTranscodes(int status)
{
    auto_ptr<Connection> connection(getConnection());
    VectorGuard<Transcode> transcodes;
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_TRANSCODES_WITH_STATUS));
        prepStatement->setInt(1, status);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        while (result->next())
        {
            Transcode* transcode = new Transcode();
            transcodes.get().push_back(transcode);
            
            transcode->wasLoaded(result->getInt(1));
            transcode->sourceMaterialPackageDbId = result->getInt(2);
            transcode->destMaterialPackageDbId = result->getInt(3);
            transcode->created = getTimestampFromODBC(result->getString(4));
            transcode->targetVideoResolution = result->getInt(5);
            transcode->status = result->getInt(6);
        }
    }
    END_QUERY_BLOCK("Failed to load transcodes")
    
    return transcodes.release();
}

#define SQL_INSERT_TRANSCODE \
" \
    INSERT INTO Transcode \
    ( \
        trc_identifier, \
        trc_source_material_package_id, \
        trc_dest_material_package_id, \
        trc_target_video_resolution_id, \
        trc_status_id \
    ) \
    VALUES \
    (?, ?, ?, ?, ?) \
"

#define SQL_UPDATE_TRANSCODE \
" \
    UPDATE Transcode \
    SET trc_source_material_package_id = ?, \
        trc_dest_material_package_id = ?,\
        trc_target_video_resolution_id = ?,\
        trc_status_id = ? \
    WHERE \
        trc_identifier = ? \
"

void Database::saveTranscode(Transcode* transcode, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextDatabaseID = 0;
        int paramIndex = 1;
        
        // insert
        if (!transcode->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_TRANSCODE));
            nextDatabaseID = getNextDatabaseID(connection, "trc_id_seq");
            prepStatement->setInt(paramIndex++, nextDatabaseID);
            connection->registerCommitListener(nextDatabaseID, transcode);
        }
        // update
        else
        {
            nextDatabaseID = transcode->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_TRANSCODE));
        }
        
        prepStatement->setInt(paramIndex++, transcode->sourceMaterialPackageDbId);
        SET_OPTIONAL_INT(transcode->destMaterialPackageDbId != 0, prepStatement, paramIndex++, 
            transcode->destMaterialPackageDbId);
        SET_OPTIONAL_INT(transcode->targetVideoResolution != 0, prepStatement, paramIndex++,
            transcode->targetVideoResolution);
        prepStatement->setInt(paramIndex++, transcode->status);

        // update
        if (transcode->isPersistent())
        {
            prepStatement->setInt(paramIndex++, transcode->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving transcode"));
        }
        
		connection->commit();
    }
    END_UPDATE_BLOCK("Failed to save transcode")
}

#define SQL_DELETE_TRANSCODE \
" \
    DELETE FROM Transcode WHERE trc_identifier = ? \
"

void Database::deleteTranscode(Transcode* transcode, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_TRANSCODE));
        prepStatement->setInt(1, transcode->getDatabaseID());
        
        connection->registerCommitListener(0, transcode);

        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when deleting transcode"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete transcode")
}
    
#define SQL_RESET_TRANSCODES \
" \
    UPDATE Transcode \
    SET \
        trc_status_id = ? \
    WHERE \
        trc_status_id = ? \
"

int Database::resetTranscodeStatus(int fromStatus, int toStatus, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    int numUpdates = 0;
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_RESET_TRANSCODES));
        prepStatement->setInt(1, toStatus);
        prepStatement->setInt(2, fromStatus);
        
        numUpdates = prepStatement->executeUpdate();
            
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to reset transcode statuses")
    
    return numUpdates;
}
    

#define SQL_DELETE_TRANSCODES \
" \
    DELETE FROM Transcode \
    WHERE \
        trc_created < now() - ? ::interval AND \
        trc_status_id IN ( \
"

int Database::deleteTranscodes(std::vector<int>& statuses, Interval timeBeforeNow, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    int paramIndex;
    vector<int>::const_iterator iter;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    int numDeletes = 0;
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        // complete the query string
        string query = SQL_DELETE_TRANSCODES;
        for (iter = statuses.begin(); iter != statuses.end(); iter++)
        {
            if (iter == statuses.begin())
            {
                query.append("?");
            }
            else
            {
                query.append(",?");
            }
        }
        query.append(")");

        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(query));
        
        paramIndex = 1;
        prepStatement->setString(paramIndex++, getODBCInterval(timeBeforeNow));
        for (iter = statuses.begin(); iter != statuses.end(); iter++)
        {
            prepStatement->setInt(paramIndex++, *iter);
        }
        
        numDeletes = prepStatement->executeUpdate();
            
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete transcodes")
    
    return numDeletes;
}


#define SQL_LOAD_OR_CREATE_PROJECT_NAME \
" \
    SELECT load_or_create_project_name(?) \
"

#define SQL_GET_ALL_PROJECT_NAMES \
" \
    SELECT \
        pjn_identifier, \
        pjn_name \
    FROM ProjectName \
"

#define SQL_DELETE_PROJECT_NAME \
" \
    DELETE FROM ProjectName WHERE pjn_identifier = ? \
"



ProjectName Database::loadOrCreateProjectName(string name)
{
    if (name.empty())
    {
        PA_LOGTHROW(DBException, ("Project name is empty"));
    }
    
    auto_ptr<Connection> mConnection(getConnection());
    Connection* connection = mConnection.get();
    
    ProjectName projectName;
    projectName.name = name;

    // load or create the project name
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_LOAD_OR_CREATE_PROJECT_NAME));
        prepStatement->setString(1, name);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            PA_LOGTHROW(DBException, ("Failed to load or create project name"));
        }

        projectName.wasLoaded(result->getInt(1));

        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to load or create project name")
    
    return projectName;
}

vector<ProjectName> Database::loadProjectNames()
{
    auto_ptr<Connection> connection(getConnection());
    vector<ProjectName> allProjectNames;
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());
        
        odbc::ResultSet* result = statement->executeQuery(SQL_GET_ALL_PROJECT_NAMES);
        while (result->next())
        {
            ProjectName projectName;
            
            projectName.wasLoaded(result->getInt(1));
            projectName.name = result->getString(2);

            allProjectNames.push_back(projectName);
        }
    }
    END_QUERY_BLOCK("Failed to load all project names")
    
    return allProjectNames;
}

void Database::deleteProjectName(ProjectName* projectName)
{
    if (!projectName->isPersistent())
    {
        // project name is not persisted in the database
        return;
    }
    
    auto_ptr<Connection> mConnection(getConnection());
    Connection* connection = mConnection.get();
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_PROJECT_NAME));
        prepStatement->setInt(1, projectName->getDatabaseID());
        
        connection->registerCommitListener(0, projectName);

        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when deleting project name"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete project name")
}



// bypassing ODBC Timestamp processing because for pkg_creation_date because
// it fails to return the fraction
#define SQL_GET_SOURCE_PACKAGE \
" \
    SELECT \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date::varchar, \
        pkg_project_name_id, \
        pjn_name, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    FROM Package \
        LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier) \
    WHERE \
        pkg_name = ? AND \
        pkg_descriptor_id IS NOT NULL \
"

// bypassing ODBC Timestamp processing because for pkg_creation_date because
// it fails to return the fraction
#define SQL_GET_PACKAGE \
" \
    SELECT \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date::varchar, \
        pkg_project_name_id, \
        pjn_name, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    FROM Package \
        LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier) \
    WHERE \
        pkg_identifier = ? \
"

// bypassing ODBC Timestamp processing because for pkg_creation_date because
// it fails to return the fraction
#define SQL_GET_PACKAGE_WITH_UMID \
" \
    SELECT \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date::varchar, \
        pkg_project_name_id, \
        pjn_name, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    FROM Package \
        LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier) \
    WHERE \
        pkg_uid = ? \
"

#define SQL_GET_PACKAGE_USER_COMMENTS \
" \
    SELECT \
        uct_identifier, \
        uct_name, \
        uct_value, \
        uct_position, \
        uct_colour \
    FROM UserComment \
    WHERE \
        uct_package_id = ? \
"

#define SQL_GET_ESSENCE_DESCRIPTOR \
" \
    SELECT \
        eds_identifier, \
        eds_essence_desc_type, \
        eds_file_location, \
        eds_file_format, \
        eds_video_resolution_id, \
        (eds_image_aspect_ratio).numerator, \
        (eds_image_aspect_ratio).denominator, \
        eds_audio_quantization_bits, \
        eds_spool_number, \
        eds_recording_location \
    FROM EssenceDescriptor \
    WHERE \
        eds_identifier = ? \
"

#define SQL_GET_TRACKS \
" \
    SELECT \
        trk_identifier, \
        trk_id, \
        trk_number, \
        trk_name, \
        trk_data_def, \
        (trk_edit_rate).numerator, \
        (trk_edit_rate).denominator \
    FROM \
        track \
    WHERE \
        trk_package_id = ? \
"

#define SQL_GET_SOURCE_CLIP \
" \
    SELECT \
        scp_identifier, \
        scp_source_package_uid, \
        scp_source_track_id, \
        scp_length, \
        scp_position \
    FROM \
        sourceclip \
    WHERE \
        scp_track_id = ? \
"

#define SQL_LOCK_SOURCE_PACKAGES \
" \
    LOCK TABLE SourcePackageLock \
"

SourcePackage* Database::loadSourcePackage(string name)
{
    SourcePackage* sourcePackage = 0;
    auto_ptr<Connection> connection(getConnection());
    
    START_QUERY_BLOCK
    {
        // load the source package
        
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_SOURCE_PACKAGE));
        prepStatement->setString(1, name);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            return 0; // no exception if not found
        }
        
        Package* package;
        loadPackage(connection.get(), result, &package);
        if ((sourcePackage = dynamic_cast<SourcePackage*>(package)) == 0)
        {
            // should never be here
            PA_LOGTHROW(DBException, ("Package with name '%s' is not a source package", name.c_str()));
        }
    }
    END_QUERY_BLOCK("Failed to load source package")
    
    return sourcePackage;
}

SourcePackage* Database::loadSourcePackage(string name, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    SourcePackage* sourcePackage = 0;
    
    
    START_QUERY_BLOCK
    {
        // load the source package
        
        auto_ptr<odbc::PreparedStatement> prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_SOURCE_PACKAGE));
        prepStatement->setString(1, name);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            return 0; // no exception if not found
        }
        
        Package* package;
        loadPackage(connection, result, &package);
        if ((sourcePackage = dynamic_cast<SourcePackage*>(package)) == 0)
        {
            // should never be here
            PA_LOGTHROW(DBException, ("Package with name '%s' is not a source package", name.c_str()));
        }
    }
    END_QUERY_BLOCK("Failed to load source package")
    
    return sourcePackage;
}

void Database::lockSourcePackages(Transaction* transaction)
{
    START_QUERY_BLOCK
    {
        // lock the source package access
        auto_ptr<odbc::PreparedStatement> prepStatement(transaction->prepareStatement(SQL_LOCK_SOURCE_PACKAGES));
        prepStatement->execute();
    }
    END_QUERY_BLOCK("Failed to aquire a source packages table lock")
}

Package* Database::loadPackage(long databaseID)
{
    Package* package = 0;
    auto_ptr<Connection> connection(getConnection());
    
    START_QUERY_BLOCK
    {
        // load the package
        
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_PACKAGE));
        prepStatement->setLong(1, databaseID);
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            PA_LOGTHROW(DBException, ("Package %ld does not exist in database", databaseID));
        }
        
        loadPackage(connection.get(), result, &package);
    }
    END_QUERY_BLOCK("Failed to load package")

    return package;
}

Package* Database::loadPackage(UMID packageUID, bool assumeExists)
{
    Package* package = 0;
    auto_ptr<Connection> connection(getConnection());
    
    START_QUERY_BLOCK
    {
        // load the package
        
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_PACKAGE_WITH_UMID));
        prepStatement->setString(1, getUMIDString(packageUID));
        
        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            if (assumeExists)
            {
                PA_LOGTHROW(DBException, ("Package '%s' does not exist in database", getUMIDString(packageUID).c_str()));
            }
            else
            {
                return 0;
            }
        }
        
        loadPackage(connection.get(), result, &package);
    }
    END_QUERY_BLOCK("Failed to load package")

    return package;
}

void Database::loadPackage(Connection* connection, odbc::ResultSet* result, Package** package)
{
    auto_ptr<Package> newPackage;
    MaterialPackage* materialPackage = 0;
    SourcePackage* sourcePackage = 0;
    long essenceDescDatabaseID;
    FileEssenceDescriptor* fileEssDescriptor;
    TapeEssenceDescriptor* tapeEssDescriptor;
    LiveEssenceDescriptor* liveEssDescriptor;

    essenceDescDatabaseID = result->getInt(7);
    if (result->wasNull())
    {
        // material package
        
        materialPackage = new MaterialPackage();
        newPackage = auto_ptr<Package>(materialPackage);
    }
    else
    {
        // source package
        
        sourcePackage = new SourcePackage();
        newPackage = auto_ptr<Package>(sourcePackage);
    }
    
    newPackage->wasLoaded(result->getInt(1));
    newPackage->uid = getUMID(result->getString(2));
    newPackage->name = result->getString(3);
    newPackage->creationDate = getTimestampFromODBC(result->getString(4));
    newPackage->projectName.name = result->getString(6);
    if (!result->wasNull())
    {
        newPackage->projectName.wasLoaded(result->getInt(5));
    }
    
    if (sourcePackage != 0)
    {
        sourcePackage->sourceConfigName = result->getString(8);

        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_ESSENCE_DESCRIPTOR));
        prepStatement->setLong(1, essenceDescDatabaseID);
        
        odbc::ResultSet* result2 = prepStatement->executeQuery();
        if (!result2->next())
        {
            // should never be here
            PA_LOGTHROW(DBException, ("Essence descriptor %ld does not exist in database", essenceDescDatabaseID));
        }
        
        switch (result2->getInt(2))
        {
            case FILE_ESSENCE_DESC_TYPE:
                fileEssDescriptor = new FileEssenceDescriptor();
                sourcePackage->descriptor = fileEssDescriptor;
                fileEssDescriptor->fileLocation = result2->getString(3);
                fileEssDescriptor->fileFormat = result2->getInt(4);
                fileEssDescriptor->videoResolutionID = result2->getInt(5);
                if (result2->wasNull())
                {
                    fileEssDescriptor->videoResolutionID = 0;
                    fileEssDescriptor->imageAspectRatio.numerator = 0;
                    fileEssDescriptor->imageAspectRatio.denominator = 0;
                }
                else
                {
                    fileEssDescriptor->imageAspectRatio.numerator = result2->getInt(6);
                    fileEssDescriptor->imageAspectRatio.denominator = result2->getInt(7);
                }
                fileEssDescriptor->audioQuantizationBits = result2->getInt(8);
                if (result2->wasNull())
                {
                    fileEssDescriptor->audioQuantizationBits = 0;
                }
                break;
                
            case TAPE_ESSENCE_DESC_TYPE:
                tapeEssDescriptor = new TapeEssenceDescriptor();
                sourcePackage->descriptor = tapeEssDescriptor;
                tapeEssDescriptor->spoolNumber = result2->getString(9);
                break;
                
            case LIVE_ESSENCE_DESC_TYPE:
                liveEssDescriptor = new LiveEssenceDescriptor();
                sourcePackage->descriptor = liveEssDescriptor;
                liveEssDescriptor->recordingLocation = result2->getInt(10);
                break;
                
            default:
                assert(false);
                PA_LOGTHROW(DBException, ("Unknown essence descriptor type"));
        }

        sourcePackage->descriptor->wasLoaded(result2->getInt(1));
    }


    // load the tracks
    
    auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_TRACKS));
    prepStatement->setLong(1, newPackage->getDatabaseID());
    
    odbc::ResultSet* result2 = prepStatement->executeQuery();
    while (result2->next())
    {
        Track* track = new Track();
        newPackage->tracks.push_back(track);
        track->wasLoaded(result2->getInt(1));
        track->id = result2->getInt(2);
        track->number = result2->getInt(3);
        track->name = result2->getString(4);
        track->dataDef = result2->getInt(5);
        track->editRate.numerator = result2->getInt(6);
        track->editRate.denominator = result2->getInt(7);
        
        // load source clip
        
        auto_ptr<odbc::PreparedStatement> prepStatement2(connection->prepareStatement(SQL_GET_SOURCE_CLIP));
        prepStatement2->setLong(1, track->getDatabaseID());
        
        odbc::ResultSet* result3 = prepStatement2->executeQuery();
        if (!result3->next())
        {
            PA_LOGTHROW(DBException, ("Track (db id %d) is missing a SourceClip", track->getDatabaseID()));
        }
        
        track->sourceClip = new SourceClip();
        track->sourceClip->wasLoaded(result3->getInt(1));
        track->sourceClip->sourcePackageUID = getUMID(result3->getString(2));
        track->sourceClip->sourceTrackID = result3->getInt(3);
        track->sourceClip->length = result3->getLong(4);
        track->sourceClip->position = result3->getLong(5);
    }

    // get the user comments    
    prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_GET_PACKAGE_USER_COMMENTS));
    prepStatement->setLong(1, newPackage->getDatabaseID());
    
    odbc::ResultSet* result3 = prepStatement->executeQuery();
    while (result3->next())
    {
        UserComment userComment;
        userComment.wasLoaded(result3->getInt(1));
        userComment.name = result3->getString(2);
        userComment.value = result3->getString(3);
        userComment.position = result3->getLong(4);
        if (result3->wasNull())
        {
            userComment.position = STATIC_COMMENT_POSITION;
        }
        userComment.colour = result3->getInt(5);
        if (result3->wasNull())
        {
            userComment.colour = 0;
        }
        newPackage->_userComments.push_back(userComment);
    }
    
    
    *package = newPackage.release();
}


#define SQL_INSERT_PACKAGE \
" \
    INSERT INTO Package \
    ( \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date, \
        pkg_project_name_id, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    ) \
    VALUES \
    (?, ?, ?, ?, ?, ?, ?) \
"

#define SQL_UPDATE_PACKAGE \
" \
    UPDATE Package \
    SET pkg_uid = ?, \
        pkg_name = ?, \
        pkg_creation_date = ?, \
        pkg_project_name_id = ?, \
        pkg_descriptor_id = ?, \
        pkg_source_config_name = ? \
    WHERE \
        pkg_identifier = ? \
"

#define SQL_INSERT_PACKAGE_USER_COMMENT \
" \
    INSERT INTO UserComment \
    ( \
        uct_identifier, \
        uct_package_id, \
        uct_name, \
        uct_value, \
        uct_position, \
        uct_colour \
    ) \
    VALUES \
    (?, ?, ?, ?, ?, ?) \
"

#define SQL_UPDATE_PACKAGE_USER_COMMENT \
" \
    UPDATE UserComment \
    SET uct_package_id = ?, \
        uct_name = ?, \
        uct_value = ?, \
        uct_position = ?, \
        uct_colour = ? \
    WHERE \
        uct_identifier = ? \
"

#define SQL_INSERT_ESSENCE_DESCRIPTOR \
" \
    INSERT INTO EssenceDescriptor \
    ( \
        eds_identifier, \
        eds_essence_desc_type, \
        eds_file_location, \
        eds_file_format, \
        eds_video_resolution_id, \
        eds_image_aspect_ratio, \
        eds_audio_quantization_bits, \
        eds_spool_number, \
        eds_recording_location \
    ) \
    VALUES \
    (?, ?, ?, ?, ?, (?, ?), ?, ?, ?) \
"

#define SQL_UPDATE_ESSENCE_DESCRIPTOR \
" \
    UPDATE EssenceDescriptor \
    SET eds_essence_desc_type = ?, \
        eds_file_location = ?, \
        eds_file_format = ?, \
        eds_video_resolution_id = ?, \
        eds_image_aspect_ratio = (?, ?), \
        eds_audio_quantization_bits = ?, \
        eds_spool_number = ?, \
        eds_recording_location = ? \
    WHERE \
        eds_identifier = ? \
"

#define SQL_INSERT_TRACK \
" \
    INSERT INTO Track \
    ( \
        trk_identifier, \
        trk_id, \
        trk_number, \
        trk_name, \
        trk_data_def, \
        trk_edit_rate, \
        trk_package_id \
    ) \
    VALUES \
    (?, ?, ?, ?, ?, (?, ?), ?) \
"

#define SQL_UPDATE_TRACK \
" \
    UPDATE Track \
    SET trk_id = ?, \
        trk_number = ?, \
        trk_name = ?, \
        trk_data_def = ?, \
        trk_edit_rate = (?, ?), \
        trk_package_id = ? \
    WHERE \
        trk_identifier = ? \
"

#define SQL_INSERT_SOURCE_CLIP \
" \
    INSERT INTO SourceClip \
    ( \
        scp_identifier, \
        scp_source_package_uid, \
        scp_source_track_id, \
        scp_length, \
        scp_position, \
        scp_track_id \
    ) \
    VALUES \
    (?, ?, ?, ?, ?, ?) \
"

#define SQL_UPDATE_SOURCE_CLIP \
" \
    UPDATE SourceClip \
    SET scp_source_package_uid = ?, \
        scp_source_track_id = ?, \
        scp_length = ?, \
        scp_position = ?, \
        scp_track_id = ? \
    WHERE \
        scp_identifier = ? \
"

void Database::savePackage(Package* package, Transaction* transaction)
{
    auto_ptr<Transaction> mTransaction;
    Transaction* connection = transaction;
    if (connection == 0)
    {
        mTransaction = auto_ptr<Transaction>(getTransaction());
        connection = mTransaction.get();
    }
    // connection is actually a transaction, but we call it connection so that
    // the END_UPDATE_BLOCK is happy
    
    if (package == 0)
    {
        PA_LOGTHROW(DBException, ("Can't save null Package"));
    }
    if (!package->projectName.isPersistent() && !package->projectName.name.empty())
    {
        PA_LOGTHROW(DBException, ("Project name referenced by package is not persistent"));
    }

    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        long nextPackageDatabaseID = 0;
        long nextDescriptorDatabaseID = 0;
        int paramIndex = 1;

        
        // save the source package essence descriptor
            
        if (package->getType() == SOURCE_PACKAGE)
        {
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            FileEssenceDescriptor* fileEssDescriptor;
            TapeEssenceDescriptor* tapeEssDescriptor;
            LiveEssenceDescriptor* liveEssDescriptor;
            
            // insert
            if (!sourcePackage->descriptor->isPersistent())
            {
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_ESSENCE_DESCRIPTOR));
                nextDescriptorDatabaseID = getNextDatabaseID(connection, "eds_id_seq");
                prepStatement->setInt(paramIndex++, nextDescriptorDatabaseID);
                connection->registerCommitListener(nextDescriptorDatabaseID, package);
            }
            // update
            else
            {
                nextDescriptorDatabaseID = sourcePackage->descriptor->getDatabaseID();
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_ESSENCE_DESCRIPTOR));
            }
            
            
            prepStatement->setInt(paramIndex++, sourcePackage->descriptor->getType());
            switch (sourcePackage->descriptor->getType())
            {
                case FILE_ESSENCE_DESC_TYPE:
                    fileEssDescriptor = dynamic_cast<FileEssenceDescriptor*>(
                        sourcePackage->descriptor);
                    prepStatement->setString(paramIndex++, fileEssDescriptor->fileLocation);
                    prepStatement->setInt(paramIndex++, fileEssDescriptor->fileFormat);
                    SET_OPTIONAL_INT(fileEssDescriptor->videoResolutionID != 0, prepStatement, paramIndex++, 
                        fileEssDescriptor->videoResolutionID);
                    SET_OPTIONAL_INT(fileEssDescriptor->videoResolutionID != 0, prepStatement, paramIndex++, 
                        fileEssDescriptor->imageAspectRatio.numerator);
                    SET_OPTIONAL_INT(fileEssDescriptor->videoResolutionID != 0, prepStatement, paramIndex++, 
                        fileEssDescriptor->imageAspectRatio.denominator);
                    SET_OPTIONAL_INT(fileEssDescriptor->audioQuantizationBits != 0, prepStatement, paramIndex++, 
                        fileEssDescriptor->audioQuantizationBits);
                        
                    prepStatement->setNull(paramIndex++, SQL_VARCHAR);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    break;
                    
                case TAPE_ESSENCE_DESC_TYPE:
                    tapeEssDescriptor = dynamic_cast<TapeEssenceDescriptor*>(
                        sourcePackage->descriptor);
                    prepStatement->setNull(paramIndex++, SQL_VARCHAR);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    
                    prepStatement->setString(paramIndex++, tapeEssDescriptor->spoolNumber);
                    
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    break;
                    
                case LIVE_ESSENCE_DESC_TYPE:
                    liveEssDescriptor = dynamic_cast<LiveEssenceDescriptor*>(
                        sourcePackage->descriptor);
                    prepStatement->setNull(paramIndex++, SQL_VARCHAR);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_INTEGER);
                    prepStatement->setNull(paramIndex++, SQL_VARCHAR);
                    
                    prepStatement->setInt(paramIndex++, liveEssDescriptor->recordingLocation);
                    break;
                    
                default:
                    assert(false);
                    PA_LOGTHROW(DBException, ("Unknown essence descriptor type"));
            }

            // update
            if (sourcePackage->descriptor->isPersistent())
            {
                prepStatement->setInt(paramIndex++, sourcePackage->descriptor->getDatabaseID());
            }
            
            if (prepStatement->executeUpdate() != 1)
            {
                PA_LOGTHROW(DBException, ("No inserts/updates when saving EssenceDescriptor"));
            }
    
        }

        // save the package

        paramIndex = 1;
        
        // insert
        if (!package->isPersistent())
        {
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_PACKAGE));
            nextPackageDatabaseID = getNextDatabaseID(connection, "pkg_id_seq");
            prepStatement->setInt(paramIndex++, nextPackageDatabaseID);
            connection->registerCommitListener(nextPackageDatabaseID, package);
        }
        // update
        else
        {
            nextPackageDatabaseID = package->getDatabaseID();
            prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_PACKAGE));
        }
        
        prepStatement->setString(paramIndex++, getUMIDString(package->uid));
        SET_OPTIONAL_VARCHAR(prepStatement, paramIndex++, package->name);
        prepStatement->setString(paramIndex++, getODBCTimestamp(package->creationDate));
        SET_OPTIONAL_INT(package->projectName.isPersistent(), prepStatement, paramIndex++, package->projectName.getDatabaseID());
        if (package->getType() == MATERIAL_PACKAGE)
        {
            prepStatement->setNull(paramIndex++, SQL_INTEGER);
        }
        else
        {
            prepStatement->setInt(paramIndex++, nextDescriptorDatabaseID);
        }
        if (package->getType() == SOURCE_PACKAGE)
        {
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            SET_OPTIONAL_VARCHAR(prepStatement, paramIndex++, sourcePackage->sourceConfigName);
        }
        else
        {
            prepStatement->setNull(paramIndex++, SQL_VARCHAR);
        }

        // update
        if (package->isPersistent())
        {
            prepStatement->setInt(paramIndex++, package->getDatabaseID());
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No inserts/updates when saving Package"));
        }

        
        
        // save the tracks
        
        vector<Track*>::const_iterator iter;
        for (iter = package->tracks.begin(); iter != package->tracks.end(); iter++)
        {
            Track* track = *iter;

            long nextTrackDatabaseID = 0;
            paramIndex = 1;
            
            // insert
            if (!track->isPersistent())
            {
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_TRACK));
                nextTrackDatabaseID = getNextDatabaseID(connection, "trk_id_seq");
                prepStatement->setInt(paramIndex++, nextTrackDatabaseID);
                connection->registerCommitListener(nextTrackDatabaseID, track);
            }
            // update
            else
            {
                nextTrackDatabaseID = track->getDatabaseID();
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_TRACK));
            }
            
            prepStatement->setInt(paramIndex++, track->id);
            prepStatement->setInt(paramIndex++, track->number);
            SET_OPTIONAL_VARCHAR(prepStatement, paramIndex++, track->name);
            prepStatement->setInt(paramIndex++, track->dataDef);
            prepStatement->setInt(paramIndex++, track->editRate.numerator);
            prepStatement->setInt(paramIndex++, track->editRate.denominator);
            prepStatement->setInt(paramIndex++, nextPackageDatabaseID);
            
            // update
            if (track->isPersistent())
            {
                prepStatement->setInt(paramIndex++, track->getDatabaseID());
            }
            
            if (prepStatement->executeUpdate() != 1)
            {
                PA_LOGTHROW(DBException, ("No inserts/updates when saving Track"));
            }
            
            
            // save the source clip

            long nextSourceClipDatabaseID = 0;
            paramIndex = 1;
            SourceClip* sourceClip = track->sourceClip;
            if (sourceClip == 0)
            {
                PA_LOGTHROW(DBException, ("Cannot save track that is missing a source clip"));
            }
            
            // insert
            if (!sourceClip->isPersistent())
            {
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_SOURCE_CLIP));
                nextSourceClipDatabaseID = getNextDatabaseID(connection, "scp_id_seq");
                prepStatement->setInt(paramIndex++, nextSourceClipDatabaseID);
                connection->registerCommitListener(nextSourceClipDatabaseID, sourceClip);
            }
            // update
            else
            {
                nextSourceClipDatabaseID = sourceClip->getDatabaseID();
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_SOURCE_CLIP));
            }
            
            prepStatement->setString(paramIndex++, getUMIDString(sourceClip->sourcePackageUID));
            prepStatement->setInt(paramIndex++, sourceClip->sourceTrackID);
            prepStatement->setLong(paramIndex++, sourceClip->length);
            prepStatement->setLong(paramIndex++, sourceClip->position);
            prepStatement->setInt(paramIndex++, nextTrackDatabaseID);
            
            // update
            if (sourceClip->isPersistent())
            {
                prepStatement->setInt(paramIndex++, sourceClip->getDatabaseID());
            }
            
            if (prepStatement->executeUpdate() != 1)
            {
                PA_LOGTHROW(DBException, ("No inserts/updates when saving SourceClip"));
            }
            
        }


        // save the user comments

        vector<UserComment>::iterator uctIter;
        for (uctIter = package->_userComments.begin(); uctIter != package->_userComments.end(); uctIter++)
        {
            UserComment& userComment = *uctIter;  // using ref so that object in vector is updated
            
            long nextUserCommentDatabaseID = 0;
            paramIndex = 1;
            
            // insert
            if (!userComment.isPersistent())
            {
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_INSERT_PACKAGE_USER_COMMENT));
                nextUserCommentDatabaseID = getNextDatabaseID(connection, "uct_id_seq");
                prepStatement->setInt(paramIndex++, nextUserCommentDatabaseID);
                connection->registerCommitListener(nextUserCommentDatabaseID, &userComment);
            }
            // update
            else
            {
                nextUserCommentDatabaseID = userComment.getDatabaseID();
                prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_UPDATE_PACKAGE_USER_COMMENT));
            }
        
            prepStatement->setInt(paramIndex++, nextPackageDatabaseID);
            prepStatement->setString(paramIndex++, userComment.name);
            prepStatement->setString(paramIndex++, userComment.value);
            if (userComment.position < 0)
            {
                prepStatement->setNull(paramIndex++, SQL_INTEGER);
            }
            else
            {
                prepStatement->setLong(paramIndex++, userComment.position);
            }
            if (userComment.colour <= 0)
            {
                prepStatement->setNull(paramIndex++, SQL_INTEGER);
            }
            else
            {
                prepStatement->setInt(paramIndex++, userComment.colour);
            }
            
            // update
            if (userComment.isPersistent())
            {
                prepStatement->setInt(paramIndex++, userComment.getDatabaseID());
            }
            
            if (prepStatement->executeUpdate() != 1)
            {
                PA_LOGTHROW(DBException, ("No inserts/updates when saving tagged value"));
            }
        }
        
        if (transaction == 0)
        {
            connection->commitTransaction();
        }
    }
    END_UPDATE_BLOCK("Failed to save package")
}


#define SQL_DELETE_PACKAGE \
" \
    DELETE FROM Package WHERE pkg_identifier = ? \
"

void Database::deletePackage(Package* package, Transaction* transaction)
{
    auto_ptr<Connection> mConnection;
    Connection* connection = transaction;
    if (connection == 0)
    {
        mConnection = auto_ptr<Connection>(getConnection());
        connection = mConnection.get();
    }
    
    START_UPDATE_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement;
        
        prepStatement = auto_ptr<odbc::PreparedStatement>(connection->prepareStatement(SQL_DELETE_PACKAGE));
        prepStatement->setInt(1, package->getDatabaseID());
        
        connection->registerCommitListener(0, package);

        // the database will cascade the delete down to the source clips
        // so we register additional listeners for those objects
        vector<Track*>::const_iterator iter;
        for (iter = package->tracks.begin(); iter != package->tracks.end(); iter++)
        {
            Track* track = *iter;
            connection->registerCommitListener(0, track);
            
            if (track->sourceClip != 0)
            {
                connection->registerCommitListener(0, track->sourceClip);
            }
        }

        // the database delete will cascade to the tagged value so we register 
        // additional listeners for those objects
        vector<UserComment>::iterator iter2;
        for (iter2 = package->_userComments.begin(); iter2 != package->_userComments.end(); iter2++)
        {
            UserComment& userComment = *iter2;  // using ref so that object in vector is updated
            connection->registerCommitListener(0, &userComment);
        }
        
        if (prepStatement->executeUpdate() != 1)
        {
            PA_LOGTHROW(DBException, ("No updates when delete package"));
        }
        
        connection->commit();
    }
    END_UPDATE_BLOCK("Failed to delete package")
}




#define SQL_GET_REFERENCED_PACKAGE_ID \
" \
    SELECT \
        pkg_identifier \
    FROM \
        package \
    WHERE \
        pkg_uid = ? \
"

int Database::loadSourceReference(UMID sourcePackageUID, uint32_t sourceTrackID, 
    Package** referencedPackage, Track** referencedTrack)
{
    auto_ptr<Connection> connection(getConnection());
    
    START_QUERY_BLOCK
    {
        // get the referenced Package database id
        
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_REFERENCED_PACKAGE_ID));
        prepStatement->setString(1, getUMIDString(sourcePackageUID));

        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            return -1;
        }

        // load the package and get the track
        
        auto_ptr<Package> package(loadPackage(result->getLong(1)));
        Track* track = package->getTrack(sourceTrackID);
        if (track == 0)
        {
            return -2;
        }
        
        *referencedPackage = package.release();
        *referencedTrack = track;
    }
    END_QUERY_BLOCK("Failed to load source reference")

    return 1;    
}


void Database::loadPackageChain(Package* topPackage, PackageSet* packages)
{
    vector<Track*>::const_iterator iter;
    for (iter = topPackage->tracks.begin(); iter != topPackage->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        if (track->sourceClip->sourcePackageUID != g_nullUMID)
        {
            Package* referencedPackage;
            Track* referencedTrack;

            // check that we don't already have the package before
            // loading it from the database
            bool havePackage = false;
            PackageSet::const_iterator iter2;
            for (iter2 = packages->begin(); iter2 != packages->end(); iter2++)
            {
                Package* package = *iter2;
                
                if (track->sourceClip->sourcePackageUID == package->uid)
                {
                    havePackage = true;
                    break;
                }
            }
            if (havePackage)
            {
                // have package so skip to next track
                continue;
            }
            
            // load referenced package
            if (loadSourceReference(track->sourceClip->sourcePackageUID, track->sourceClip->sourceTrackID,
                &referencedPackage, &referencedTrack) == 1)
            {
                pair<PackageSet::iterator, bool> result = packages->insert(referencedPackage);
                if (!result.second)
                {
                    delete referencedPackage;
                    referencedPackage = *result.first;
                }
                
                // load recursively (depth first)
                loadPackageChain(referencedPackage, packages);
            }
        }
    }
}

void Database::loadPackageChain(long databaseID, Package** topPackage, PackageSet* packages)
{
    // load the top first package
    *topPackage = loadPackage(databaseID);
    pair<PackageSet::iterator, bool> result = packages->insert(*topPackage);
    if (!result.second)
    {
        delete *topPackage;
        *topPackage = *result.first;
    }
    
    // load referenced packages recursively
    loadPackageChain(*topPackage, packages);
}


#define SQL_GET_MATERIAL_1 \
" \
    SELECT \
        pkg_identifier \
    FROM \
        Package \
    WHERE \
        pkg_creation_date >= ? AND \
        pkg_creation_date < ? AND \
        pkg_descriptor_id IS NULL \
"

void Database::loadMaterial(Timestamp& after, Timestamp& before, MaterialPackageSet* topPackages, 
    PackageSet* packages)
{
    auto_ptr<Connection> connection(getConnection());
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_MATERIAL_1));
        prepStatement->setString(1, getODBCTimestamp(after));
        prepStatement->setString(2, getODBCTimestamp(before));

        odbc::ResultSet* result = prepStatement->executeQuery();
        Package* topPackage;
        while (result->next())
        {
            loadPackageChain(result->getInt(1), &topPackage, packages);
            if (topPackage->getType() != MATERIAL_PACKAGE)
            {
                // shouldn't ever be here
                delete topPackage;
                PA_LOGTHROW(DBException, ("Database package is not a material package"));
            }
            topPackages->insert(dynamic_cast<MaterialPackage*>(topPackage));
        }
    }
    END_QUERY_BLOCK("Failed to load material")
}


#define SQL_GET_MATERIAL_2 \
" \
    SELECT \
        pkg_identifier \
    FROM \
        Package, \
        UserComment \
    WHERE \
        uct_package_id = pkg_identifier AND \
        uct_name = ? AND \
        uct_value = ? AND \
        pkg_descriptor_id IS NULL \
"

void Database::loadMaterial(string ucName, string ucValue, MaterialPackageSet* topPackages, PackageSet* packages)
{
    auto_ptr<Connection> connection(getConnection());
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_MATERIAL_2));
        prepStatement->setString(1, ucName);
        prepStatement->setString(2, ucValue);

        odbc::ResultSet* result = prepStatement->executeQuery();
        Package* topPackage;
        while (result->next())
        {
            loadPackageChain(result->getInt(1), &topPackage, packages);
            if (topPackage->getType() != MATERIAL_PACKAGE)
            {
                // shouldn't ever be here
                delete topPackage;
                PA_LOGTHROW(DBException, ("Database package is not a material package"));
            }
            topPackages->insert(dynamic_cast<MaterialPackage*>(topPackage));
        }
    }
    END_QUERY_BLOCK("Failed to load material")
}


#define SQL_GET_NEXT_ID \
" \
    SELECT nextval(?) \
"

long Database::getNextDatabaseID(Connection* connection, string sequenceName)
{
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::PreparedStatement> prepStatement(connection->prepareStatement(SQL_GET_NEXT_ID));
        prepStatement->setString(1, sequenceName);

        odbc::ResultSet* result = prepStatement->executeQuery();
        if (!result->next())
        {
            PA_LOGTHROW(DBException, ("Failed to get next unique id from '%s' sequence", sequenceName.c_str()));
        }

        return result->getInt(1);
    }
    END_QUERY_BLOCK("Failed to get database sequence id")
}


#define SQL_GET_VERSION \
" \
    SELECT \
        ver_version \
    FROM \
        Version \
"

void Database::checkVersion()
{
    auto_ptr<Connection> connection(getConnection());
    
    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());

        odbc::ResultSet* result = statement->executeQuery(SQL_GET_VERSION);
        if (!result->next())
        {
            PA_LOGTHROW(DBException, ("Database is missing a version in the Version table"));
        }
        
        int version = result->getInt(1);
        
        if (version != g_compatibilityVersion)
        {
            PA_LOGTHROW(DBException, ("Database version %d not equal to required version %d",
                version, g_compatibilityVersion));
        }
    }
    END_QUERY_BLOCK("Failed to read database version")
}

#define SQL_GET_ALL_RESOLUTION_NAMES \
" \
    SELECT \
        vrn_identifier, \
        vrn_name \
    FROM VideoResolution \
"

void Database::loadResolutionNames(std::map<int, std::string> & resolution_names)
{
    auto_ptr<Connection> connection(getConnection());
    resolution_names.clear();

    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());
        
        odbc::ResultSet* result = statement->executeQuery(SQL_GET_ALL_RESOLUTION_NAMES);
        while (result->next())
        {
            resolution_names[result->getInt(1)] = result->getString(2);
        }
    }
    END_QUERY_BLOCK("Failed to load all video resolution names")
}

#define SQL_GET_ALL_FILE_FORMAT_NAMES \
" \
    SELECT \
        fft_identifier, \
        fft_name \
    FROM FileFormat \
"

void Database::loadFileFormatNames(std::map<int, std::string> & file_format_names)
{
    auto_ptr<Connection> connection(getConnection());
    file_format_names.clear();

    START_QUERY_BLOCK
    {
        auto_ptr<odbc::Statement> statement(connection->createStatement());
        
        odbc::ResultSet* result = statement->executeQuery(SQL_GET_ALL_FILE_FORMAT_NAMES);
        while (result->next())
        {
            file_format_names[result->getInt(1)] = result->getString(2);
        }
    }
    END_QUERY_BLOCK("Failed to load all file format names")
}



