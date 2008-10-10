/*
 * $Id: test.cpp,v 1.4 2008/10/10 09:52:34 philipn Exp $
 *
 * Tests the database library
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include <Database.h>
#include <Utilities.h>
#include <DBException.h>


using namespace std;
using namespace prodauto;

#define CHECK(chk) \
    if (!(chk)) \
    { \
        fprintf(stderr, "'%s' check failed, at %s:%d\n", #chk, __FILE__, __LINE__); \
        throw "CHECK error"; \
    }

    
// utility class to clean-up Package pointers
class MaterialHolder
{
public:
    ~MaterialHolder()
    {
        PackageSet::iterator iter;
        for (iter = packages.begin(); iter != packages.end(); iter++)
        {
            delete *iter;
        }
        
        // topPackages only holds references so don't delete
    }
    
    MaterialPackageSet topPackages; 
    PackageSet packages;
};

    
// max connections should be >= NUM_THREADS below
#define MAX_CONNECTIONS     3


static const char* g_testProjectName = "xxxxTestProjectzzzz";



#if defined(TEST_THREADING)

#define NUM_THREADS         3


static void* thread_routine(void* data)
{
    int i;
    try
    {
        for (i = 0; i < 100; i++)
        {
            auto_ptr<Connection> connection(Database::getInstance()->getConnection());

            // pause a bit
            int j;
            int k = 0;
            for (j = 0; j < 50000; j++)
            {
                k *=5;
            }
        }
    }
    catch (...)
    {
        fprintf(stderr, "failed to get connection\n");
        pthread_exit((void *) 0);
        throw "thread error";
    }
    
    pthread_exit((void *) 0);
    return 0;
}

static void test_threading()
{
    pthread_t thread[NUM_THREADS];
    pthread_attr_t attr;
    int i, rc, status;
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    
    for (i = 0; i < NUM_THREADS; i++)
    {
        if (pthread_create(&thread[i], &attr, thread_routine, 0))
        {
            fprintf(stderr, "Failed to create thread\n");
            throw "Thread error";
        }

        // pause a bit
        int j;
        int k = 0;
        for (j = 0; j < 50000; j++)
        {
            k *=5;
        }
    }
    
    for (i = 0; i < NUM_THREADS; i++)
    {
        rc = pthread_join(thread[i], (void **)&status);
        if (rc)
        {
            fprintf(stderr, "Return code from pthread_join() is %d\n", rc);
            throw "Thread error";
        }
    }
}

#endif


static ProjectName create_project_name(string name)
{
    Database* database = Database::getInstance();
    
    return database->loadOrCreateProjectName(name);
}

static SourceConfig* create_source_config()
{
    Database* database = Database::getInstance();
    
    auto_ptr<SourceConfig> sourceConfigInDB(new SourceConfig());
    SourceTrackConfig* trackConfig;
    
    sourceConfigInDB->name = "TestIngex";
    sourceConfigInDB->type = LIVE_SOURCE_CONFIG_TYPE;
    sourceConfigInDB->recordingLocation = 1;
 
    sourceConfigInDB->trackConfigs.push_back(new SourceTrackConfig());
    trackConfig = sourceConfigInDB->trackConfigs.back();        
    trackConfig->id = 1;
    trackConfig->number = 2;
    trackConfig->name = "IsoX";
    trackConfig->dataDef = PICTURE_DATA_DEFINITION;
    trackConfig->editRate = g_palEditRate;
    trackConfig->length = 120 * 60 * 60 * 25;
    
    sourceConfigInDB->trackConfigs.push_back(new SourceTrackConfig());
    trackConfig = sourceConfigInDB->trackConfigs.back();        
    trackConfig->id = 2;
    trackConfig->number = 3;
    trackConfig->name = "MicY";
    trackConfig->dataDef = SOUND_DATA_DEFINITION;
    trackConfig->editRate = g_palEditRate;
    trackConfig->length = 120 * 60 * 60 * 25;
    
    database->saveSourceConfig(sourceConfigInDB.get());
    return sourceConfigInDB.release();
}


static void test_source_config()
{
    Database* database = Database::getInstance();
    auto_ptr<SourceConfig> sourceConfigInDB;
    
    try
    {
        // create
        sourceConfigInDB = auto_ptr<SourceConfig>(create_source_config());
        
        // try loading it back        
        auto_ptr<SourceConfig> sourceConfig(database->loadSourceConfig(sourceConfigInDB->getID()));

        CHECK(sourceConfig->name.compare("TestIngex") == 0);
        CHECK(sourceConfig->type = LIVE_SOURCE_CONFIG_TYPE);
        CHECK(sourceConfig->recordingLocation == 1);
        CHECK(sourceConfig->trackConfigs.size() == 2);
        
        vector<SourceTrackConfig*>::const_iterator trackIter = sourceConfigInDB->trackConfigs.begin();
        SourceTrackConfig* trackConfig = (*trackIter++);
        CHECK(trackConfig->id == 1);        
        CHECK(trackConfig->number == 2);        
        CHECK(trackConfig->name.compare("IsoX") == 0);        
        CHECK(trackConfig->dataDef == PICTURE_DATA_DEFINITION);        
        CHECK(trackConfig->editRate == g_palEditRate);        
        CHECK(trackConfig->length == 120 * 60 * 60 * 25);        
        trackConfig = (*trackIter++);
        CHECK(trackConfig->id == 2);        
        CHECK(trackConfig->number == 3);        
        CHECK(trackConfig->name.compare("MicY") == 0);        
        CHECK(trackConfig->dataDef == SOUND_DATA_DEFINITION);        
        CHECK(trackConfig->editRate == g_palEditRate);        
        CHECK(trackConfig->length == 120 * 60 * 60 * 25);        

        // clean up
        if (sourceConfigInDB.get() != 0 && sourceConfigInDB->isPersistent())
        {
            database->deleteSourceConfig(sourceConfigInDB.get());
        }
    }
    catch (...)
    {
        try
        {
            if (sourceConfigInDB.get() != 0 && sourceConfigInDB->isPersistent())
            {
                database->deleteSourceConfig(sourceConfigInDB.get());
            }
        }
        catch (...) {}
        throw;        
    }
}


static RecorderConfig* create_recorder_config(string name, long sourceConfigID)
{
    Database* database = Database::getInstance();
    
    auto_ptr<RecorderConfig> recorderConfig(new RecorderConfig());

    SourceConfig* sourceConfig;
    RecorderInputConfig* inputConfig;
    RecorderInputTrackConfig* inputTrackConfig;

    recorderConfig->name = name;
    recorderConfig->setStringParam("string param", "string value");
    recorderConfig->setIntParam("int param", 1);
    recorderConfig->setBoolParam("bool param", true);
    Rational rational = {4,3};
    recorderConfig->setRationalParam("rational param", rational);
    
    recorderConfig->sourceConfigs.push_back(database->loadSourceConfig(sourceConfigID));
    sourceConfig = recorderConfig->sourceConfigs.back();        
    
    recorderConfig->recorderInputConfigs.push_back(new RecorderInputConfig());
    inputConfig = recorderConfig->recorderInputConfigs.back();
    inputConfig->index = 1;
    inputConfig->name = "SDI Input A";
    
    inputConfig->trackConfigs.push_back(new RecorderInputTrackConfig());
    inputTrackConfig = inputConfig->trackConfigs.back();
    inputTrackConfig->index = 1;
    inputTrackConfig->number = 2;
    inputTrackConfig->sourceConfig = sourceConfig;
    inputTrackConfig->sourceTrackID = 1;
    
    inputConfig->trackConfigs.push_back(new RecorderInputTrackConfig());
    inputTrackConfig = inputConfig->trackConfigs.back();
    inputTrackConfig->index = 2;
    inputTrackConfig->number = 3;
    inputTrackConfig->sourceConfig = 0;
    inputTrackConfig->sourceTrackID = 0;
    
    return recorderConfig.release();
}

static Recorder* create_recorder(long sourceConfigID)
{
    Database* database = Database::getInstance();
    
    auto_ptr<Recorder> recorderInDB(new Recorder());

    try
    {
        recorderInDB->name = "TestIngex";
        recorderInDB->setConfig(create_recorder_config("TestIngexConfig1", sourceConfigID));
        recorderInDB->setAlternateConfig(create_recorder_config("TestIngexConfig2", sourceConfigID));
    
        database->saveRecorder(recorderInDB.get());
    }
    catch (...)
    {
        try
        {
            if (recorderInDB.get() != 0 && recorderInDB->isPersistent())
            {
                database->deleteRecorder(recorderInDB.get());
            }
        }
        catch (...) {}
        throw;        
    }
    return recorderInDB.release();
}


static void test_recorder()
{
    Database* database = Database::getInstance();
    auto_ptr<SourceConfig> sourceConfigInDB;
    auto_ptr<Recorder> recorderInDB;;

    try
    {
        sourceConfigInDB = auto_ptr<SourceConfig>(create_source_config());
        
        // create
        recorderInDB = auto_ptr<Recorder>(create_recorder(sourceConfigInDB->getID()));

        // try loading it back        
        auto_ptr<Recorder> recorder(database->loadRecorder(recorderInDB->name));

        CHECK(recorder->name.compare("TestIngex") == 0);
        CHECK(recorder->getAllConfigs().size() == 2);
        CHECK(recorder->hasConfig());

        vector<RecorderConfig*>::const_iterator confIter = recorder->getAllConfigs().begin();
        if (*confIter == recorder->getConfig())
        {
            confIter++;
        }
        RecorderConfig* recorderConfig = *confIter;
        CHECK(recorderConfig->name.compare("TestIngexConfig2") == 0);

        CHECK(recorderConfig->getStringParam("string param", "").compare("string value") == 0);
        CHECK(recorderConfig->getStringParam("not string param", "").length() == 0);
        CHECK(recorderConfig->getIntParam("int param", 1) == 1);
        CHECK(recorderConfig->getIntParam("not int param", 0) == 0);
        CHECK(recorderConfig->getBoolParam("bool param", false) == true);
        CHECK(recorderConfig->getBoolParam("not bool param", false) == false);
        Rational rational = {4,3};
        CHECK(recorderConfig->getRationalParam("rational param", g_nullRational) == rational);
        CHECK(recorderConfig->getRationalParam("not rational param", g_nullRational) == g_nullRational);

        CHECK(recorderConfig->recorderInputConfigs.size() == 1);

        recorderConfig = recorder->getConfig();
        CHECK(recorderConfig->name.compare("TestIngexConfig1") == 0);
        CHECK(recorderConfig->recorderInputConfigs.size() == 1);
        
        RecorderInputConfig* inputConfig = recorderConfig->recorderInputConfigs.back();
        CHECK(inputConfig->index == 1);        
        CHECK(inputConfig->name.compare("SDI Input A") == 0);
        CHECK(inputConfig->trackConfigs.size() == 2);
        
        vector<RecorderInputTrackConfig*>::const_iterator trackIter = inputConfig->trackConfigs.begin();
        RecorderInputTrackConfig* inputTrackConfig = (*trackIter++);
        CHECK(inputTrackConfig->index == 1);        
        CHECK(inputTrackConfig->number == 2);        
        CHECK(inputTrackConfig->sourceTrackID == 1);        
        inputTrackConfig = (*trackIter++);
        CHECK(inputTrackConfig->index == 2);        
        CHECK(inputTrackConfig->number == 3);        
        CHECK(inputTrackConfig->sourceConfig == 0);        
        CHECK(inputTrackConfig->sourceTrackID == 0);        

        
        // clean up
        if (recorderInDB.get() != 0 && recorderInDB->isPersistent())
        {
            database->deleteRecorder(recorderInDB.get());
        }
        if (sourceConfigInDB.get() != 0 && sourceConfigInDB->isPersistent())
        {
            database->deleteSourceConfig(sourceConfigInDB.get());
        }
    }
    catch (...)
    {
        try
        {
            if (recorderInDB.get() != 0 && recorderInDB->isPersistent())
            {
                database->deleteRecorder(recorderInDB.get());
            }
        }
        catch (...) {}
        try
        {
            if (sourceConfigInDB.get() != 0 && sourceConfigInDB->isPersistent())
            {
                database->deleteSourceConfig(sourceConfigInDB.get());
            }
        }
        catch (...) {}
        throw;        
    }
}


static void test_router_config()
{
    Database* database = Database::getInstance();
    
    try
    {
        // load all from database
        VectorGuard<RouterConfig> allRouterConfigs;
        allRouterConfigs.get() = database->loadAllRouterConfigs();
    }
    catch (...)
    {
        throw;
    }
}


static void test_source_session()
{
    Database* database = Database::getInstance();
    auto_ptr<SourceConfig> sourceConfigInDB;
    auto_ptr<Recorder> recorderInDB;;

    try
    {
        sourceConfigInDB = auto_ptr<SourceConfig>(create_source_config());
        recorderInDB = auto_ptr<Recorder>(create_recorder(sourceConfigInDB->getID()));

        // create old style using session start timestamp
        sourceConfigInDB->setSessionSourcePackage();
        CHECK(sourceConfigInDB->getSourcePackage() != 0);
        database->deletePackage(sourceConfigInDB->getSourcePackage());
            
        // create new style
        sourceConfigInDB->setSourcePackage("xxx123456");
        CHECK(sourceConfigInDB->getSourcePackage() != 0);
        database->deletePackage(sourceConfigInDB->getSourcePackage());

            
        // clean up
        if (recorderInDB.get() != 0 && recorderInDB->isPersistent())
        {
            database->deleteRecorder(recorderInDB.get());
        }
        if (sourceConfigInDB.get() != 0 && sourceConfigInDB->isPersistent())
        {
            database->deleteSourceConfig(sourceConfigInDB.get());
        }
    }
    catch (...)
    {
        try
        {
            if (recorderInDB.get() != 0 && recorderInDB->isPersistent())
            {
                database->deleteRecorder(recorderInDB.get());
            }
        }
        catch (...) {}
        try
        {
            if (sourceConfigInDB.get() != 0 && sourceConfigInDB->isPersistent())
            {
                database->deleteSourceConfig(sourceConfigInDB.get());
            }
        }
        catch (...) {}
        throw;
    }

}


static void test_multicam_clip_def()
{
    Database* database = Database::getInstance();

    try
    {
        // test loading all multi-camera defs
        VectorGuard<MCClipDef> allMCClipDefs;
        allMCClipDefs.get() = database->loadAllMultiCameraClipDefs();
    }
    catch (...)
    {
        throw;
    }
}


static void test_editorial()
{
    Database* database = Database::getInstance();
    auto_ptr<Series> seriesInDB;

    try
    {
        // test creating series and saving
        seriesInDB = auto_ptr<Series>(new Series());
        seriesInDB->name = "Test series";
        database->saveSeries(seriesInDB.get());

        // test loading all series
        VectorGuard<Series> allSeries;
        allSeries.get() = database->loadAllSeries();

        // get the series created previously
        auto_ptr<Series> series;
        vector<Series*>::iterator iter;
        for (iter = allSeries.get().begin(); iter != allSeries.get().end(); iter++)
        {
            if ((*iter)->name.compare("Test series") == 0)
            {
                series = auto_ptr<Series>(*iter);
                allSeries.get().erase(iter);
                break;
            }
        }
        if (series.get() == 0)
        {
            throw "Failed to find previously created Series";
        }
        
                
        // test creating programme and saving
        Programme* programme = series->createProgramme();
        programme->name = "Test programme";
        database->saveProgramme(programme);
        
        // test loading all programmes in series
        database->loadProgrammesInSeries(series.get());
        if (series->getProgrammes().size() == 0)
        {
            throw "No programme loaded from series";
        }
        programme = *series->getProgrammes().begin();
        CHECK(programme->name.compare("Test programme") == 0);
        
        
        // test creating item and saving
        Item* item1 = programme->appendNewItem();
        item1->description = "Test item 1";
        item1->scriptSectionRefs = getScriptReferences("{\"sequence 7\",\"sequence11}\",\"sequence 19 \\\"\"}");

        Item* item2 = programme->appendNewItem();
        item2->description = "Test item 2";
        Item* item3 = programme->insertNewItem(0);
        item3->description = "Test item 3";
        item3->scriptSectionRefs.push_back("}");
        Item* item4 = programme->insertNewItem(1);
        item4->description = "Test item 4";
        Item* item5 = programme->insertNewItem(3);
        item5->description = "Test item 5";
        
        vector<Item*>::const_iterator iterItem;
        for (iterItem = programme->getItems().begin(); iterItem != programme->getItems().end(); iterItem++)
        {
            database->saveItem(*iterItem);
        }
        
        programme->moveItem(item1, 0);
        programme->moveItem(item2, 1);
        programme->moveItem(item2, 0);
        programme->moveItem(item1, 0);
        programme->moveItem(item2, 0);
        programme->moveItem(item1, 0);

        database->updateItemsOrder(programme);

        programme->moveItem(item2, 0);
        programme->moveItem(item1, 0);
        programme->moveItem(item2, 0);
        programme->moveItem(item1, 0);
        programme->moveItem(item2, 4);
        programme->moveItem(item5, 4);
        programme->moveItem(item2, 1);
        
        database->updateItemsOrder(programme);
        
        // test loading all items in programme
        database->loadItemsInProgramme(programme);
        if (programme->getItems().size() != 5)
        {
            throw "Failed to load items from programme";
        }
        iterItem = programme->getItems().begin();
        CHECK((*iterItem)->description.compare("Test item 1") == 0);
        CHECK((*iterItem)->scriptSectionRefs.size() == 3);
        vector<string>::const_iterator refIter = (*iterItem)->scriptSectionRefs.begin();
        CHECK((*refIter++).compare("sequence 7") == 0);
        CHECK((*refIter++).compare("sequence11}") == 0);
        CHECK((*refIter++).compare("sequence 19 \"") == 0);
        CHECK((*(++iterItem))->description.compare("Test item 2") == 0);
        CHECK((*iterItem)->scriptSectionRefs.size() == 0);
        CHECK((*(++iterItem))->description.compare("Test item 3") == 0);
        CHECK((*iterItem)->scriptSectionRefs.size() == 1);
        CHECK((*(*iterItem)->scriptSectionRefs.begin()).compare("}") == 0);
        CHECK((*(++iterItem))->description.compare("Test item 4") == 0);
        CHECK((*(++iterItem))->description.compare("Test item 5") == 0);

        Item* item = *programme->getItems().begin();
        
     
        // test creating take and saving
        Date testDate = {1972, 10, 22};
        Take* take = item->createTake();
        take->number = 1;
        take->comment = "An excellent take according to the director";
        take->result = GOOD_TAKE_RESULT;
        take->editRate = g_palEditRate;
        take->length = 100;
        take->startPosition = 90000;
        take->startDate = testDate;
        take->recordingLocation = 1;
        database->saveTake(take);
        
        // test loading all takes in item
        database->loadTakesInItem(item);
        if (item->getTakes().size() != 1)
        {
            throw "Failed to load take in item";
        }
        take = *item->getTakes().begin();
        CHECK(take->number == 1);
        CHECK(take->comment.compare("An excellent take according to the director") == 0);
        CHECK(take->result == GOOD_TAKE_RESULT);
        CHECK(take->editRate == g_palEditRate);
        CHECK(take->length == 100);
        CHECK(take->startPosition == 90000);
        CHECK(take->startDate == testDate);
        CHECK(take->recordingLocation == 1);

        // test loading all takes again
        database->loadTakesInItem(item);
        if (item->getTakes().size() != 1)
        {
            throw "Failed to load takes again";
        }
        
        
        // clean up
        if (seriesInDB.get() != 0 && seriesInDB->isPersistent())
        {
            database->deleteSeries(seriesInDB.get());
        }
    }
    catch (...)
    {
        try
        {
            if (seriesInDB.get() != 0 && seriesInDB->isPersistent())
            {
                database->deleteSeries(seriesInDB.get());
            }
        }
        catch (...) {}
        throw;        
    }
    
}


static SourcePackage* create_source_package(UMID uid, Timestamp now, 
    UMID sourcePackageUID, uint32_t sourceTrackID)
{
    Database* database = Database::getInstance();
    auto_ptr<SourcePackage> sourcePackage(new SourcePackage());
    Track* track;
    SourceClip* sourceClip;
    
    sourcePackage->uid = uid;
    sourcePackage->name = "Test package";
    sourcePackage->creationDate = now;
    sourcePackage->projectName = create_project_name(g_testProjectName);
    FileEssenceDescriptor* fDesc = new FileEssenceDescriptor();
    sourcePackage->descriptor = fDesc;
    fDesc->fileLocation = "file:///media";
    fDesc->fileFormat = MXF_FILE_FORMAT_TYPE;
    fDesc->videoResolutionID = UNC_MATERIAL_RESOLUTION;
    fDesc->imageAspectRatio.numerator = 16;
    fDesc->imageAspectRatio.denominator = 9;
    //fDesc->audioQuantizationBits = 16;
    
    track = new Track();
    sourcePackage->tracks.push_back(track);
    track->id = 1;
    track->number = 2;
    track->name = "V2";
    track->dataDef = PICTURE_DATA_DEFINITION;
    track->editRate = g_palEditRate;

    sourceClip = new SourceClip();
    track->sourceClip = sourceClip;
    sourceClip->sourcePackageUID = sourcePackageUID;
    sourceClip->sourceTrackID = sourceTrackID;
    sourceClip->length = 898;
    sourceClip->position = 0;
    
    sourcePackage->addUserComment("testname1", "testvalue1", 0, 1);
    sourcePackage->addUserComment("testname1", "testvalue2", STATIC_COMMENT_POSITION, 0);
    sourcePackage->addUserComment("testname2", "testvalue1", 100, 2);
    sourcePackage->addUserComment("testname3", "testvalue3", 200, 3);
    
    database->savePackage(sourcePackage.get());
    return sourcePackage.release();
}

static MaterialPackage* create_material_package(UMID uid, Timestamp now)
{
    Database* database = Database::getInstance();
    auto_ptr<MaterialPackage> materialPackage(new MaterialPackage());
    Track* track;
    SourceClip* sourceClip;
    
    materialPackage->uid = uid;
    materialPackage->name = "Test material package";
    materialPackage->creationDate = now;
    materialPackage->projectName = create_project_name(g_testProjectName);
    
    track = new Track();
    materialPackage->tracks.push_back(track);
    track->id = 1;
    track->number = 2;
    track->name = "V2";
    track->dataDef = PICTURE_DATA_DEFINITION;
    track->editRate = g_palEditRate;

    sourceClip = new SourceClip();
    track->sourceClip = sourceClip;
    sourceClip->sourcePackageUID = g_nullUMID;
    sourceClip->sourceTrackID = 0;
    sourceClip->length = 898;
    sourceClip->position = 0;
    
    materialPackage->addUserComment("testname", "testvalue", 5, 1);
    
    database->savePackage(materialPackage.get());
    return materialPackage.release();
}

static void test_package()
{
    Database* database = Database::getInstance();
    auto_ptr<SourcePackage> sourcePackageInDB;
    auto_ptr<MaterialPackage> materialPackageInDB;

    try
    {
        UMID uid = generateUMID();
        Timestamp now = generateTimestampNow();
        now.qmsec = 10;
        
        // create
        sourcePackageInDB = auto_ptr<SourcePackage>(create_source_package(uid, now, g_nullUMID, 0));

        // try loading it back        
        auto_ptr<Package> package1(database->loadPackage(sourcePackageInDB->getDatabaseID()));

        SourcePackage* sourcePackage;
        CHECK((sourcePackage = dynamic_cast<SourcePackage*>(package1.get())) != 0);
        CHECK(sourcePackage->name.compare("Test package") == 0);
        CHECK(sourcePackage->creationDate == now);

        FileEssenceDescriptor* fileDescriptor;
        CHECK(sourcePackage->descriptor != 0);
        CHECK((fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(sourcePackage->descriptor)) != 0);
        CHECK(fileDescriptor->fileLocation.compare("file:///media") == 0);
        CHECK(fileDescriptor->fileFormat == MXF_FILE_FORMAT_TYPE);
        CHECK(fileDescriptor->videoResolutionID == UNC_MATERIAL_RESOLUTION);        
        CHECK(fileDescriptor->imageAspectRatio.numerator == 16);        
        CHECK(fileDescriptor->imageAspectRatio.denominator == 9);        
        //CHECK(fileDescriptor->audioQuantizationBits == 16);        

        Track* track;
        CHECK(sourcePackage->tracks.size() == 1);
        track = sourcePackage->tracks.back();
        CHECK(track->id == 1);
        CHECK(track->number = 2);
        CHECK(track->name.compare("V2") == 0);
        CHECK(track->dataDef == PICTURE_DATA_DEFINITION);
        CHECK(track->editRate == g_palEditRate);
        
        CHECK(track->sourceClip != 0);
        CHECK(track->sourceClip->sourcePackageUID == g_nullUMID);
        CHECK(track->sourceClip->sourceTrackID == 0);
        CHECK(track->sourceClip->length == 898);
        CHECK(track->sourceClip->position == 0);

        
        // get the tagged values
        vector<UserComment> userComments = sourcePackageInDB->getUserComments("testname1");
        CHECK(userComments.size() == 2);
        CHECK(((*userComments.begin()).value.compare("testvalue1") == 0 &&
                (*(++userComments.begin())).value.compare("testvalue2") == 0) ||
            ((*userComments.begin()).value.compare("testvalue2") == 0 && 
                (*(++userComments.begin())).value.compare("testvalue1") == 0));
        CHECK(((*userComments.begin()).position == 0 &&
                (*(++userComments.begin())).position == -1) ||
            ((*userComments.begin()).position == -1 &&
                (*(++userComments.begin())).position == 0));
        CHECK(((*userComments.begin()).colour == 1 &&
                (*(++userComments.begin())).colour == 0) ||
            ((*userComments.begin()).colour == 0 &&
                (*(++userComments.begin())).colour == 1));
        userComments = sourcePackageInDB->getUserComments("testname2");
        CHECK(userComments.size() == 1);
        CHECK((*userComments.begin()).value.compare("testvalue1") == 0);
        CHECK((*userComments.begin()).position == 100);
        CHECK((*userComments.begin()).colour == 2);
        userComments = sourcePackageInDB->getUserComments("testname3");
        CHECK(userComments.size() == 1);
        CHECK((*userComments.begin()).value.compare("testvalue3") == 0);
        CHECK((*userComments.begin()).position == 200);
        CHECK((*userComments.begin()).colour == 3);

        
        // load material associated with tagged value
        UMID uid2 = generateUMID();
        materialPackageInDB = auto_ptr<MaterialPackage>(create_material_package(uid2, now));

        MaterialHolder material;
        database->loadMaterial("testname", "testvalue", &material.topPackages, &material.packages);
        CHECK(material.topPackages.size() == 1);
        
        
        // load a source reference
        Package* package2;
        Track* sourceTrack;
        if (database->loadSourceReference(sourcePackageInDB->uid, 1, &package2, &sourceTrack) != 1)
        {
            throw "Failed to load source reference to package";
        }
        delete package2;

        
        // test live recording locations
        map<long, string> locations = database->loadLiveRecordingLocations();
        CHECK(locations.find(1) != locations.end());
        CHECK((*locations.find(1)).second.compare("Unspecified") == 0);
        
        
        // clean up
        if (sourcePackageInDB.get() != 0 && sourcePackageInDB->isPersistent())
        {
            database->deletePackage(sourcePackageInDB.get());
        }
        if (materialPackageInDB.get() != 0 && materialPackageInDB->isPersistent())
        {
            database->deletePackage(materialPackageInDB.get());
        }
    }
    catch (...)
    {
        try
        {
            if (sourcePackageInDB.get() != 0 && sourcePackageInDB->isPersistent())
            {
                database->deletePackage(sourcePackageInDB.get());
            }
        }
        catch (...) {}
        try
        {
            if (materialPackageInDB.get() != 0 && materialPackageInDB->isPersistent())
            {
                database->deletePackage(materialPackageInDB.get());
            }
        }
        catch (...) {}
        throw;
    }
}


static void test_transcode()
{
    Database* database = Database::getInstance();
    auto_ptr<MaterialPackage> materialPackageInDB;

    try
    {
        // create a material package, which should create an entry in the transcode table
        UMID uid = generateUMID();
        Timestamp now = generateTimestampNow();
        materialPackageInDB = auto_ptr<MaterialPackage>(create_material_package(uid, now));

        
        // test loading all transcodes 1
        VectorGuard<Transcode> transcodes;
        transcodes.get() = database->loadTranscodes(TRANSCODE_STATUS_NOTSTARTED);

        // test loading all transcodes 2
        vector<int> statuses;
        statuses.push_back(TRANSCODE_STATUS_NOTSTARTED);
        transcodes.get() = database->loadTranscodes(statuses);

        
        // find the transcode entry matching the material package
        Transcode* existTranscode = 0;  // reference to transcode in allTranscode
        bool foundTranscode = false;
        vector<Transcode*>::const_iterator iter;
        for (iter = transcodes.get().begin(); iter != transcodes.get().end(); iter++)
        {
             Transcode* transcode = *iter;
            
            if (transcode->sourceMaterialPackageDbId == materialPackageInDB->getDatabaseID())
            {
                foundTranscode = true;
                existTranscode = transcode;
                break;
            }
        }
        CHECK(foundTranscode);
        
        CHECK(existTranscode->destMaterialPackageDbId == 0);
        CHECK(existTranscode->targetVideoResolution == 0);
        CHECK(existTranscode->status == TRANSCODE_STATUS_NOTSTARTED);

        
        // test modifying transcode
        existTranscode->targetVideoResolution = MJPEG201_MATERIAL_RESOLUTION;
        existTranscode->status = TRANSCODE_STATUS_STARTED;
        database->saveTranscode(existTranscode);

        
        // test creating and saving
        // setting sourceMaterialPackageDbId to the previously created material package 
        // so that it gets deleted by the DELETE ON CASCADE
        auto_ptr<Transcode> transcode(new Transcode());
        transcode->sourceMaterialPackageDbId = materialPackageInDB->getDatabaseID(); 
        transcode->destMaterialPackageDbId = 0; // none
        transcode->targetVideoResolution = MJPEG201_MATERIAL_RESOLUTION;
        transcode->status = TRANSCODE_STATUS_NOTSTARTED;
        database->saveTranscode(transcode.get());

        
        // clean up
        if (materialPackageInDB.get() != 0 && materialPackageInDB->isPersistent())
        {
            database->deletePackage(materialPackageInDB.get());
        }
    }
    catch (...)
    {
        try
        {
            if (materialPackageInDB.get() != 0 && materialPackageInDB->isPersistent())
            {
                database->deletePackage(materialPackageInDB.get());
            }
        }
        catch (...) {}
        throw;        
    }
    
}



static void usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [<dns> <username> <password>]\n", prog);
}


int main(int argc, const char* argv[])
{
    string dns = "prodautodb";
    string username = "bamzooki";
    string password = "bamzooki";
    
    if (argc != 1 && argc != 4)
    {
        usage(argv[0]);
        return 1;
    }
    
    if (argc == 4)
    {
        dns = argv[1];
        username = argv[2];
        password = argv[3];
    }
    
    try
    {
        Database::initialise(dns, username, password, 1, MAX_CONNECTIONS);
    }
    catch (DBException& ex)
    {
        fprintf(stderr, "Failed to connect to database:\n  %s\n", ex.getMessage().c_str());
        return 1;
    }

    
    printf("Testing...\n");
    
    string testName;
    ProjectName projectName;
    try
    {
        vector<ProjectName> allProjectNames = Database::getInstance()->loadProjectNames();
        
        projectName = create_project_name(g_testProjectName);
        
#if defined(TEST_THREADING)
        testName = "test_threading";
        test_threading();
#endif        
        
        testName = "test_source_config"; 
        test_source_config();
        
        testName = "test_recorder"; 
        test_recorder();
        
        testName = "test_router_config"; 
        test_router_config();
        
        testName = "test_multicam_clip_def";
        test_multicam_clip_def();

        testName = "test_package"; 
        test_package();
        
        testName = "test_source_session"; 
        test_source_session();
        
        testName = "test_editorial"; 
        test_editorial();
        
        testName = "test_transcode"; 
        test_transcode();

        Database::getInstance()->deleteProjectName(&projectName);
    }
    catch (DBException& ex)
    {
        fprintf(stderr, "%s FAILED: %s\n", testName.c_str(), ex.getMessage().c_str());
        Database::getInstance()->deleteProjectName(&projectName);
        Database::close();
        return 1;
    }
    catch (const char*& ex)
    {
        fprintf(stderr, "%s FAILED: %s\n", testName.c_str(), ex);
        Database::getInstance()->deleteProjectName(&projectName);
        Database::close();
        return 1;
    }
    catch (...)
    {
        Database::getInstance()->deleteProjectName(&projectName);
        Database::close();
        fprintf(stderr, "%s FAILED: unknown exception thrown\n", testName.c_str());
        return 1;
    }
    
    printf("Passed.\n");

    Database::close();
    
    return 0;
}


