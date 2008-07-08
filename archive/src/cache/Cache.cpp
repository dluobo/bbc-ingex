/*
 * $Id: Cache.cpp,v 1.1 2008/07/08 16:21:59 philipn Exp $
 *
 * Manages the files in the disk cache
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
 
/*
    Manages the disk cache which consists of the MXF cache/ and cache/creating/,
    browse copy browse/ and PSE pse/ directory.
    
    Provides information about the items in the MXF cache/.
    
    Watches the MXF cache/ directory for changes, allowing changes made in one 
    process (eg. the recorder) to be visible to other processes (eg. tape 
    export). 
    
    Creates, moves and deletes files. Creates unique filenames.
    
    Keeps an update count to allow multiple clients to detect changes.
    
    At startup removes all files in the cache/creating/ directory, reports 
    unknown files in cache/ and deletes database entries for non-existing files 
    in cache/.
*/
 
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

#include <set>
#include <map>
#include <algorithm>

#include "Cache.h"
#include "RecorderDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Timing.h"


using namespace std;
using namespace rec;


// Initially wait ADDED_FILE_INITIAL_WAIT_MSEC before trying to load from the database info about a file
// added to the cache. Try this ADDED_FILE_RETRIES times and wait ADDED_FILE_WAIT_MSEC between tries
#define ADDED_FILE_INITIAL_WAIT_MSEC    100
#define ADDED_FILE_WAIT_MSEC            500 
#define ADDED_FILE_TRIES                10


static const char* g_creatingDirectory = "creating";
static const char* g_mxfPageFileSuffix = ".mxfp";



static string get_filename_instance(long instance)
{
    char buf[32];
    sprintf(buf, "%02ld", instance);
    return buf;
}



class CacheStatusUpdateHelper
{
public:
    CacheStatusUpdateHelper(Cache* cache)
    : _cache(cache), _enable(false)
    {}
    ~CacheStatusUpdateHelper()
    {
        if (_enable)
        {
            _cache->updateStatus();
        }
    }
    
    void enable()
    {
        _enable = true;
    }
    
private:
    Cache* _cache;
    bool _enable;
};



CacheContentItem::CacheContentItem()
: sessionStatus(-1), size(-1), duration(-1)
{}

CacheContentItem::~CacheContentItem()
{}


CacheContents::CacheContents()
{
}

CacheContents::~CacheContents()
{
    vector<CacheContentItem*>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        delete *iter;
    }
}


CreatingCacheItem::CreatingCacheItem()
: CacheItem(), isTemp(false)
{
}
CreatingCacheItem::~CreatingCacheItem()
{
}



bool Cache::directoryExists(string directory)
{
    struct stat statBuf;
    
    if (stat(directory.c_str(), &statBuf) == 0 && S_ISDIR(statBuf.st_mode))
    {
        return true;
    }
    
    return false;
}



Cache::Cache(long recorderId, string directory, string browseDirectory, string pseDirectory)
: _directory(directory), _browseDirectory(browseDirectory), _pseDirectory(pseDirectory) , _cacheTable(0)
{
    REC_CHECK(directoryExists(directory));
    
    // always have '/' at end of directory
    _directory = join_path(directory, "");
    Logging::info("Cache directory is '%s'\n", _directory.c_str());
    _browseDirectory = join_path(browseDirectory, "");
    Logging::info("Browse directory is '%s'\n", _browseDirectory.c_str());
    _pseDirectory = join_path(pseDirectory, "");
    Logging::info("PSE reports directory is '%s'\n", _pseDirectory.c_str());


    {
        // this will block events from the directory watch until initialise()
        // has completed
        LOCK_SECTION(_itemsMutex);

        // watch the cache directory for file deletions/moves
        _directoryWatch = new DirectoryWatch(directory, DIR_WATCH_DELETION | DIR_WATCH_MOVED_FROM, 500000);
        _directoryWatch->registerListener(this);
        
        // make sure the directory watch is running - 2 seconds should be more than enough - before
        // initialising the cache
        Timer timer;
        timer.start(2 * SEC_IN_USEC);
        while (!_directoryWatch->isReady() && timer.timeLeft() > 0)
        {
            sleep_msec(1);
        }
        if (!_directoryWatch->isReady())
        {
            REC_LOGTHROW(("Cache directory watch failed to startup"));
        }
        
        // load and init cache contents for recorder
        initialise(recorderId, true);
    }

    // sync status with contents
    updateStatus();
}

Cache::Cache(string recorderName, string directory)
: _directory(directory), _cacheTable(0)
{
    REC_CHECK(directoryExists(directory));
    
    // always have '/' at end of directory
    _directory = join_path(directory, "");
    Logging::info("Cache directory is '%s'\n", _directory.c_str());

    {
        // this will block events from the directory watch until initialise()
        // has completed
        LOCK_SECTION(_itemsMutex);

        // watch the cache directory for file creations/deletions/moves
        _directoryWatch = new DirectoryWatch(directory, 
            DIR_WATCH_DELETION | DIR_WATCH_MOVED_FROM | DIR_WATCH_CREATION | DIR_WATCH_MOVED_TO, 500000);
        _directoryWatch->registerListener(this);
        
        // make sure the directory watch is running - 2 seconds should be more than enough - before
        // initialising the cache
        Timer timer;
        timer.start(2 * SEC_IN_USEC);
        while (!_directoryWatch->isReady() && timer.timeLeft() > 0)
        {
            sleep_msec(1);
        }
        if (!_directoryWatch->isReady())
        {
            REC_LOGTHROW(("Cache directory watch failed to startup"));
        }
        
        // load recorder
        auto_ptr<RecorderTable> recorderTable(RecorderDatabase::getInstance()->loadRecorder(recorderName));
        if (recorderTable.get() == 0)
        {
            REC_LOGTHROW(("Unknown recorder '%s'", recorderName.c_str()));
        }
        
        // load and init cache for non-recorder app
        initialise(recorderTable->databaseId, false);
    }

    // sync status with contents
    updateStatus();
}

Cache::~Cache()
{
    delete _directoryWatch;
    delete _cacheTable;
    
    vector<CreatingCacheItem*>::const_iterator iter1;
    for (iter1 = _creatingItems.begin(); iter1 != _creatingItems.end(); iter1++)
    {
        delete *iter1;
    }
    vector<CacheItem*>::const_iterator iter2;
    for (iter2 = _items.begin(); iter2 != _items.end(); iter2++)
    {
        delete *iter2;
    }
}

void Cache::thisDirectoryDeletion(string directory)
{
    Logging::error("Cache directory '%s' was deleted - this is bad!\n", directory.c_str());
    // TODO: recorder should shutdown - signal kill -9 ?
}

void Cache::deletion(string directory, string name)
{
    Logging::debug("File '%s' was deleted from cache\n", name.c_str());
    processItemRemoved(name);
}

void Cache::creation(string directory, string name)
{
    Logging::debug("File '%s' was created in cache\n", name.c_str());
    sleep_msec(ADDED_FILE_INITIAL_WAIT_MSEC);

    bool found = false;
    int tries = 0;
    while (tries < ADDED_FILE_TRIES)
    {
        found = processItemAdded(name);
        if (found)
        {
            break;
        }
        
        tries++;
        sleep_msec(ADDED_FILE_WAIT_MSEC);
    }

    if (!found)
    {
        Logging::warning("Failed to load information from database about item '%s' added to cache\n", name.c_str());
    }
}

void Cache::movedFrom(string directory, string name)
{
    Logging::debug("File '%s' was moved from cache\n", name.c_str());
    processItemRemoved(name);
}

void Cache::movedTo(string directory, string name)
{
    Logging::debug("File '%s' was moved to cache\n", name.c_str());
    sleep_msec(ADDED_FILE_INITIAL_WAIT_MSEC);

    bool found = false;
    int tries = 0;
    while (tries < ADDED_FILE_TRIES)
    {
        found = processItemAdded(name);
        if (found)
        {
            break;
        }
        
        tries++;
        sleep_msec(ADDED_FILE_WAIT_MSEC);
    }

    if (!found)
    {
        Logging::warning("Failed to load information from database about item '%s' added to cache\n", name.c_str());
    }
}

bool Cache::checkMultiItemMXF(string barcode, int numItems)
{
    // base filename is the barcode with spaces replaced with '_'
    string baseFilename = barcode;
    size_t index = 0;
    while ((index = baseFilename.find(" ", index)) != string::npos)
    {
        baseFilename.at(index) = '_';
    }

    // check the multi-item MXF files do not exist in the cache
    string testItemMXFFilename;
    int i;
    for (i = 0; i < numItems; i++)
    {
        testItemMXFFilename = getMultiItemFilename(baseFilename, i + 1, ".mxf");
        
        if (creatingFileExists(testItemMXFFilename) ||
            fileExists(testItemMXFFilename))
        {
            return false;
        }
    }
    
    return true;
}

bool Cache::getMultiItemTemplateFilename(string barcode, int numItems, string* mxfFilenameTemplate)
{
    if (!checkMultiItemMXF(barcode, numItems))
    {
        return false;
    }

    // base filename is the barcode with spaces replaced with '_'
    string baseFilename = barcode;
    size_t index = 0;
    while ((index = baseFilename.find(" ", index)) != string::npos)
    {
        baseFilename.at(index) = '_';
    }

    string pageFileTemplate = getPageFilenameTemplate(baseFilename);

    // check the first page file does not exist in the creating directory
    string testPageFilename = getPageFilename(pageFileTemplate, 0);
    if (creatingFileExists(testPageFilename))
    {
        return false;
    }

    
    *mxfFilenameTemplate = pageFileTemplate;
    return true;
}

bool Cache::getMultiItemFilenames(string barcode, int itemNumber,
    string* mxfFilename, string* browseFilename, string* browseTimecodeFilename, 
    string* browseInfoFilename, string* pseFilename)
{
    // base filename is the barcode with spaces replaced with '_'
    string baseFilename = barcode;
    size_t index = 0;
    while ((index = baseFilename.find(" ", index)) != string::npos)
    {
        baseFilename.at(index) = '_';
    }

    string testItemMXFFilename = getMultiItemFilename(baseFilename, itemNumber, ".mxf");
    string testItemBrowseFilename = getMultiItemFilename(baseFilename, itemNumber, ".mpg");
    string testItemBrowseTimecodeFilename = getMultiItemFilename(baseFilename, itemNumber, ".tc");
    string testItemBrowseInfoFilename = getMultiItemFilename(baseFilename, itemNumber, ".txt");
    string testItemPSEFilename = getMultiItemFilename(baseFilename, itemNumber, ".PSEreport.html");
        
    if (creatingFileExists(testItemMXFFilename) ||
        fileExists(testItemMXFFilename))
    {
        return false;
    }
    
    *mxfFilename = testItemMXFFilename;
    *browseFilename = testItemBrowseFilename;
    *browseTimecodeFilename = testItemBrowseTimecodeFilename;
    *browseInfoFilename = testItemBrowseInfoFilename;
    *pseFilename = testItemPSEFilename;
    return true;
}

bool Cache::getUniqueFilenames(string barcode, long sourceId,
    string* mxfFilename, string* browseFilename, string* browseTimecodeFilename, 
    string* browseInfoFilename, string* pseFilename)
{
    // base filename is the barcode with spaces replaced with '_'
    string baseFilename = barcode;
    size_t index = 0;
    while ((index = baseFilename.find(" ", index)) != string::npos)
    {
        baseFilename.at(index) = '_';
    }

    // find a instance number which creates a filenames
    string testMXFFilename;
    string testBrowseFilename;
    string testBrowseTimecodeFilename;
    string testBrowseInfoFilename;
    string testPSEFilename;
    long firstInstanceNumber = -1;
    long lastInstanceNumber = -1;
    long testInstanceNumber;
    int numTries = 0;
    const int maxTries = 20;
    while (numTries < maxTries)
    {
        testInstanceNumber = RecorderDatabase::getInstance()->getNewSourceRecordingInstance(sourceId);

        testMXFFilename = getSingleItemFilename(baseFilename, testInstanceNumber, ".mxf");
        testBrowseFilename = getSingleItemFilename(baseFilename, testInstanceNumber, ".mpg");
        testBrowseTimecodeFilename = getSingleItemFilename(baseFilename, testInstanceNumber, ".tc");
        testBrowseInfoFilename = getSingleItemFilename(baseFilename, testInstanceNumber, ".txt");
        testPSEFilename = getSingleItemFilename(baseFilename, testInstanceNumber, ".PSEreport.html");
        
        if (!creatingFileExists(testMXFFilename) &&
            !fileExists(testMXFFilename) &&        
            !browseFileExists(testBrowseFilename) &&
            !browseFileExists(testBrowseTimecodeFilename) &&
            !browseFileExists(testBrowseInfoFilename) &&
            !pseFileExists(testPSEFilename))
        {
            break;
        }
        
        if (firstInstanceNumber < 0)
        {
            firstInstanceNumber = testInstanceNumber;
        }
        else
        {
            lastInstanceNumber = testInstanceNumber;
        }
        
        numTries++;
    }
    if (numTries >= maxTries)
    {
        // reset the instance number
        RecorderDatabase::getInstance()->resetSourceRecordingInstance(sourceId, firstInstanceNumber, lastInstanceNumber);
        return false;
    }
    if (numTries > 1)
    {
        Logging::warning("The cache contained files which caused the source instance number to be incremented by more than 1\n");
    }
    
    
    *mxfFilename = testMXFFilename;
    *browseFilename = testBrowseFilename;
    *browseTimecodeFilename = testBrowseTimecodeFilename;
    *browseInfoFilename = testBrowseInfoFilename;
    *pseFilename = testPSEFilename;
    return true;
}

int64_t Cache::getDiskSpace()
{
    struct statvfs statvfsBuf;
    
    if (statvfs(_directory.c_str(), &statvfsBuf) == 0)
    {
        return statvfsBuf.f_bfree * statvfsBuf.f_bsize;
    }
    return -1; // unknown
}

string Cache::getPath()
{
    return _directory;
}

string Cache::getBrowsePath()
{
    return _browseDirectory;
}

string Cache::getPSEPath()
{
    return _pseDirectory;
}

string Cache::getCompleteFilename(string filename)
{
    return join_path(_directory, filename);
}

string Cache::getCompleteCreatingFilename(string filename)
{
    return join_path(_directory, g_creatingDirectory, filename);
}

string Cache::getCompleteBrowseFilename(string filename)
{
    REC_ASSERT(_browseDirectory.size() > 0);
    
    return join_path(_browseDirectory, filename);
}

string Cache::getCompletePSEFilename(string filename)
{
    REC_ASSERT(_pseDirectory.size() > 0);
    
    return join_path(_pseDirectory, filename);
}

int64_t Cache::getFileSize(string filename)
{
    struct stat statBuf;
    
    if (stat(getCompleteFilename(filename).c_str(), &statBuf) == 0)
    {
        return statBuf.st_size;
    }
    
    return -1;
}

int64_t Cache::getCreatingFileSize(string filename)
{
    struct stat statBuf;
    
    if (stat(getCompleteCreatingFilename(filename).c_str(), &statBuf) == 0)
    {
        return statBuf.st_size;
    }
    
    return -1;
}

int64_t Cache::getBrowseFileSize(string filename)
{
    struct stat statBuf;
    
    if (stat(getCompleteBrowseFilename(filename).c_str(), &statBuf) == 0)
    {
        return statBuf.st_size;
    }
    
    return -1;
}

void Cache::registerCreatingItem(HardDiskDestination* dest, RecordingSessionTable* session, D3Source* d3Source, bool isTemp)
{
    // remove item with same name (TODO: is this actually an assertion violation?)
    CreatingCacheItem* existingItem = eraseCreatingItem(dest->name);
    if (existingItem != 0)
    {
        Logging::warning("Attempting to register duplicate cache creating item '%s'; original has been removed\n", dest->name.c_str());
        delete existingItem;
    }
    
    // touch the file in the creating directory
    if (isPageFilename(dest->name))
    {
        string firstPageFile = getPageFilename(dest->name, 0); 
        FILE* file = fopen(getCompleteCreatingFilename(firstPageFile).c_str(), "wb");
        if (file == NULL)
        {
            REC_LOGTHROW(("Failed to open new mxf page file '%s' in cache creating directory: %s", 
                firstPageFile.c_str(), strerror(errno)));
        }
        fclose(file);
    }
    else
    {
        FILE* file = fopen(getCompleteCreatingFilename(dest->name).c_str(), "wb");
        if (file == NULL)
        {
            REC_LOGTHROW(("Failed to open new file '%s' in cache creating directory: %s", 
                dest->name.c_str(), strerror(errno)));
        }
        fclose(file);
    }

    // create the creating item
    {
        CacheStatusUpdateHelper updater(this);
        LOCK_SECTION(_itemsMutex);
        
        // add new item to list
        _creatingItems.push_back(new CreatingCacheItem());
        CreatingCacheItem* creatingItem = _creatingItems.back();
        creatingItem->hdDestinationId = dest->databaseId;
        creatingItem->name = dest->name;
        creatingItem->browseName = dest->browseName;
        creatingItem->pseName = dest->pseName;
        creatingItem->pseResult = dest->pseResult;
        creatingItem->size = dest->size;
        creatingItem->duration = dest->duration;
        creatingItem->sessionId = session->databaseId;
        creatingItem->sessionCreation = session->creation;
        creatingItem->sessionComments = session->comments;
        creatingItem->sessionStatus = session->status;
        creatingItem->sourceSpoolNo = d3Source->spoolNo;
        creatingItem->sourceItemNo = d3Source->itemNo;
        creatingItem->sourceProgNo = d3Source->progNo;
        creatingItem->sourceMagPrefix = d3Source->magPrefix;
        creatingItem->sourceProdCode = d3Source->prodCode;
        creatingItem->isTemp = isTemp;
        
        updater.enable();
    }
}

void Cache::updateCreatingItem(HardDiskDestination* dest, RecordingSessionTable* session)
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);

    CacheItem* creatingItem = getCreatingItem(dest->name);    
    REC_ASSERT(creatingItem != 0);
    
    // update the cache item
    creatingItem->sessionStatus = session->status;
    creatingItem->duration = dest->duration;
    creatingItem->size = dest->size;
    creatingItem->pseResult = dest->pseResult;

    updater.enable();
}

void Cache::finaliseCreatingItem(HardDiskDestination* dest)
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);
    
    CreatingCacheItem* creatingItem = getCreatingItem(dest->name);    
    REC_ASSERT(creatingItem != 0);
    
    // move the file
    if (rename(getCompleteCreatingFilename(creatingItem->name).c_str(), 
        getCompleteFilename(creatingItem->name).c_str()) != 0)
    {
        REC_LOGTHROW(("Failed to move cache creating file '%s' into cache", 
            creatingItem->name.c_str()));
    }
    
    _items.push_back(creatingItem);
    REC_ASSERT(eraseCreatingItem(creatingItem->name) != 0);
    updater.enable();
}

void Cache::removeCreatingItems()
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);
    
    vector<CreatingCacheItem*>::iterator iter = _creatingItems.begin();
    while (iter != _creatingItems.end())
    {
        CreatingCacheItem* creatingItem = *iter;
        
        // remove link in database
        RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache(creatingItem->hdDestinationId);
        
        // remove file on disk in creating directory
        if (isPageFilename(creatingItem->name))
        {
            int index = 0;
            while (removeCreatingFile(getPageFilename(creatingItem->name, index)))
            {
                index++;
            }
            if (index != 0)
            {
                Logging::info("Removed page file '%s' from the cache creating directory\n", creatingItem->name.c_str());
            }
        }
        else
        {
            if (removeCreatingFile(creatingItem->name))
            {
                Logging::info("Removed file '%s' from the cache creating directory\n", creatingItem->name.c_str());
            }
        }
        
        // Note: we never remove browse or pse files - if the full quality is lost for whatever
        // reason then at least we will still have the browse copy
        
        SAFE_DELETE(creatingItem);
        iter = _creatingItems.erase(iter);
        
        updater.enable();
    }
}

void Cache::removeCreatingItem(string filename)
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);
    
    CreatingCacheItem* creatingItem = getCreatingItem(filename);    
    REC_ASSERT(creatingItem != 0);
    

    // remove link in database
    RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache(creatingItem->hdDestinationId);
        

    // remove file on disk in creating directory
    if (isPageFilename(creatingItem->name))
    {
        int index = 0;
        while (removeCreatingFile(getPageFilename(creatingItem->name, index)))
        {
            index++;
        }
        if (index != 0)
        {
            Logging::info("Removed page file '%s' from the cache creating directory\n", creatingItem->name.c_str());
        }
    }
    else
    {
        if (removeCreatingFile(creatingItem->name))
        {
            Logging::info("Removed file '%s' from the cache creating directory\n", creatingItem->name.c_str());
        }
    }
    
    // Note: we never remove browse or pse files - if the full quality is lost for whatever
    // reason then at least we will still have the browse copy
        
    eraseCreatingItem(creatingItem->name);
    SAFE_DELETE(creatingItem);
    
    updater.enable();
}

bool Cache::removeItem(long hdDestinationId)
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);
    
    bool result;
    vector<CacheItem*>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if ((*iter)->hdDestinationId == hdDestinationId)
        {
            // remove link in database
            RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache(hdDestinationId);
            
            // remove file on disk
            result = removeFile((*iter)->name);
            
            // Note: we never remove browse or pse files - if the full quality is lost for whatever
            // reason then at least we will still have the browse copy

            delete *iter;
            _items.erase(iter);
            
            updater.enable();
            return result;
        }
    }
    
    return false;
}

bool Cache::removeItem(string name)
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);
    
    bool result;
    vector<CacheItem*>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if ((*iter)->name.compare(name) == 0)
        {
            // remove link in database
            RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache((*iter)->hdDestinationId);
            
            // remove file on disk
            result = removeFile((*iter)->name);
            
            // Note: we never remove browse or pse files - if the full quality is lost for whatever
            // reason then at least we will still have the browse copy

            delete *iter;
            _items.erase(iter);
            
            updater.enable();
            return result;
        }
    }
    
    return false;
}

bool Cache::processItemRemoved(string name)
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);
    
    vector<CacheItem*>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if ((*iter)->name.compare(name) == 0)
        {
            // Note: the database will only be updated here if the item was deleted outside the recorder
            // or tape transfer system. The recorder and tape transfer systems will remove the database link 
            // before deleting the file from disk
            RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache((*iter)->hdDestinationId);

            delete *iter;
            _items.erase(iter);
            
            updater.enable();
            return true;
        }
    }
    
    return false;
}

bool Cache::itemExists(string name)
{
    LOCK_SECTION(_itemsMutex);

    bool haveItem = false;
    vector<CacheItem*>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if ((*iter)->name.compare(name) == 0)
        {
            haveItem = true;
            break;
        }
    }
    
    if (!haveItem)
    {
        // either the file no longer exists or the cache is momentarily out of sync with whats on disk.
        return false;
    }
    
    return fileExists(name);
}

string Cache::getItemName(long hdDestinationId)
{
    LOCK_SECTION(_itemsMutex);

    vector<CacheItem*>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if ((*iter)->hdDestinationId == hdDestinationId)
        {
            return (*iter)->name;
        }
    }
    
    return "";
}

bool Cache::itemsAreKnown(vector<long>& itemIds)
{
    LOCK_SECTION(_itemsMutex);

    set<long> itemIdsSet(itemIds.begin(), itemIds.end());

    size_t matchCount = 0;    
    vector<CacheItem*>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if (itemIdsSet.find((*iter)->hdDestinationId) != itemIdsSet.end())
        {
            matchCount++;
        }
    }
    
    return matchCount == itemIdsSet.size();
}

int64_t Cache::getTotalSize(std::vector<long>& itemIds)
{
    LOCK_SECTION(_itemsMutex);

    set<long> itemIdsSet(itemIds.begin(), itemIds.end());

    int64_t totalSize = 0;    
    vector<CacheItem*>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if (itemIdsSet.find((*iter)->hdDestinationId) != itemIdsSet.end())
        {
            totalSize += (*iter)->size;
        }
    }
    
    return totalSize;
}

CacheStatus Cache::getStatus()
{
    LOCK_SECTION(_statusMutex);
    
    return _status;
}

void Cache::signalCacheUpdate()
{
    _status.statusChangeCount++;    
}


static bool orderCacheItems(const CacheContentItem* l, const CacheContentItem* r)
{
    // descending creation date and otherwise descending item no
    return l->sessionCreation > r->sessionCreation ||
        (l->sessionCreation == r->sessionCreation && l->sourceItemNo > r->sourceItemNo);
}

CacheContents* Cache::getContents()
{
    LOCK_SECTION(_itemsMutex);
    
    auto_ptr<CacheContents> contents(new CacheContents());
    contents->path = _cacheTable->path;

    CacheContentItem* ccItem;    
    vector<CacheItem*>::const_iterator iter1;
    for (iter1 = _items.begin(); iter1 != _items.end(); iter1++)
    {
        CacheItem* item = *iter1;
        
        contents->items.push_back(new CacheContentItem());
        ccItem = contents->items.back();
        ccItem->identifier = item->hdDestinationId;
        ccItem->sourceSpoolNo = item->sourceSpoolNo;
        ccItem->sourceItemNo = item->sourceItemNo;
        ccItem->sourceProgNo = item->sourceProgNo;
        ccItem->sourceMagPrefix = item->sourceMagPrefix;
        ccItem->sourceProdCode = item->sourceProdCode;
        ccItem->sessionId = item->sessionId;
        ccItem->sessionCreation = item->sessionCreation;
        ccItem->sessionComments = item->sessionComments;
        ccItem->sessionStatus = item->sessionStatus;
        ccItem->name = item->name;
        ccItem->size = item->size;
        ccItem->duration = item->duration;
        ccItem->pseName = item->pseName;
        ccItem->pseResult = item->pseResult;
    }
    
    // include the non-temporary creating items as well
    vector<CreatingCacheItem*>::const_iterator iter2;
    for (iter2 = _creatingItems.begin(); iter2 != _creatingItems.end(); iter2++)
    {
        CreatingCacheItem* item = *iter2;
        
        if (item->isTemp)
        {
            continue;
        }
        
        contents->items.push_back(new CacheContentItem());
        ccItem = contents->items.back();
        ccItem->identifier = item->hdDestinationId;
        ccItem->sourceSpoolNo = item->sourceSpoolNo;
        ccItem->sourceItemNo = item->sourceItemNo;
        ccItem->sourceProgNo = item->sourceProgNo;
        ccItem->sourceMagPrefix = item->sourceMagPrefix;
        ccItem->sourceProdCode = item->sourceProdCode;
        ccItem->sessionId = item->sessionId;
        ccItem->sessionCreation = item->sessionCreation;
        ccItem->sessionComments = item->sessionComments;
        ccItem->sessionStatus = item->sessionStatus;
        ccItem->name = item->name;
        ccItem->size = item->size;
        ccItem->duration = item->duration;
        ccItem->pseName = item->pseName;
        ccItem->pseResult = item->pseResult;
    }
    
    
    sort(contents->items.begin(), contents->items.end(), orderCacheItems);
    
    return contents.release();
}

long Cache::getDatabaseId()
{
    return _cacheTable->databaseId;
}

bool Cache::fileExists(string filename)
{
    struct stat statBuf;
    if (stat(getCompleteFilename(filename).c_str(), &statBuf) == 0)
    {
        return true;
    }
    
    return false;
}

bool Cache::creatingFileExists(string filename)
{
    if (filename.find("%d") != string::npos)
    {
        struct stat statBuf;
        if (stat(getCompleteCreatingFilename(getPageFilename(filename, 0)).c_str(), &statBuf) == 0)
        {
            return true;
        }
    }
    else
    {
        struct stat statBuf;
        if (stat(getCompleteCreatingFilename(filename).c_str(), &statBuf) == 0)
        {
            return true;
        }
    }
    
    return false;
}

bool Cache::browseFileExists(string filename)
{
    REC_ASSERT(_browseDirectory.size() > 0);
    
    struct stat statBuf;
    if (stat(getCompleteBrowseFilename(filename).c_str(), &statBuf) == 0)
    {
        return true;
    }
    
    return false;
}

bool Cache::pseFileExists(string filename)
{
    REC_ASSERT(_pseDirectory.size() > 0);
    
    struct stat statBuf;
    if (stat(getCompletePSEFilename(filename).c_str(), &statBuf) == 0)
    {
        return true;
    }
    
    return false;
}

bool Cache::removeFile(string filename)
{
    if (fileExists(filename))
    {
        if (remove(getCompleteFilename(filename).c_str()) == 0)
        {
            Logging::info("Removed file '%s' from the cache\n", filename.c_str());
            return true;
        }
    }
    
    return false;
}

bool Cache::removeCreatingFile(string filename)
{
    if (creatingFileExists(filename))
    {
        if (remove(getCompleteCreatingFilename(filename).c_str()) == 0)
        {
            return true;
        }
    }
    
    return false;
}

void Cache::updateStatus()
{
    LOCK_SECTION(_statusMutex);
    LOCK_SECTION_2(_itemsMutex);
    
    _status.numItems = _items.size();
    _status.totalSize = 0;
    vector<CacheItem*>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        _status.totalSize += (*iter)->size;
    }
    _status.statusChangeCount++;
}


void Cache::initialise(long recorderId, bool forRecorder)
{
    if (forRecorder)
    {
        // create the 'creating' directory and remove any files present
        if (!fileExists(g_creatingDirectory))
        {
            if (mkdir(join_path(_directory, g_creatingDirectory).c_str(), 0777) != 0)
            {
                REC_LOGTHROW(("Failed to create cache creating ('%s') directory: %s", 
                    g_creatingDirectory, strerror(errno)));
            }
            
            Logging::info("Created cache creating directory '%s'\n", g_creatingDirectory);
        }
        else
        {
            // remove any files present in the creating directory
            
            DIR* creatingDirStream = NULL;
            struct dirent* cacheCreatingDirent;
            if ((creatingDirStream = opendir(join_path(_directory, g_creatingDirectory).c_str())) == NULL)
            {
                REC_LOGTHROW(("Failed to open the cache creating directory ('%s') for reading: %s", 
                    g_creatingDirectory, strerror(errno)));
            }
            
            while ((cacheCreatingDirent = readdir(creatingDirStream)) != NULL)
            {
                if (strcmp(cacheCreatingDirent->d_name, ".") == 0 ||
                    strcmp(cacheCreatingDirent->d_name, "..") == 0)
                {
                    continue;
                }
                
                if (remove(getCompleteCreatingFilename(cacheCreatingDirent->d_name).c_str()) != 0)
                {
                    Logging::warning("Failed to remove '%s' from the cache creating directory ('%s'): %s\n",
                        cacheCreatingDirent->d_name, g_creatingDirectory, strerror(errno));
                }
            }
            
            closedir(creatingDirStream);
        }
    }
    
    // create or load cache data from the database
    _cacheTable = RecorderDatabase::getInstance()->loadCache(recorderId, _directory);
    if (_cacheTable)
    {
        Logging::info("Loaded cache '%s' data from database\n", _directory.c_str());

        _items = RecorderDatabase::getInstance()->loadCacheItems(_cacheTable->databaseId);
        Logging::info("Loaded %d cache items from the database\n", _items.size());
    }
    else
    {
        if (!forRecorder)
        {
            // we expect a cache table to be present when the recorder is registered in the database
            REC_LOGTHROW(("No cache table found in database for recorder %ld", recorderId));
        }
        
        auto_ptr<CacheTable> newCacheTable(new CacheTable());
        newCacheTable->recorderId = recorderId;
        newCacheTable->path = _directory;
        RecorderDatabase::getInstance()->saveCache(newCacheTable.get());
        _cacheTable = newCacheTable.release();

        Logging::info("Created and saved cache '%s' data to database\n", _directory.c_str());
    }
    
        
    // remove items with duplicate names
    // we assume that duplicate files created earlier have been overwritten and
    // have effectively being removed from the cache
    
    map<string, CacheItem*> itemsMap;
    vector<CacheItem*>::iterator itemIter;
    pair<map<string, CacheItem*>::iterator, bool> itemsMapResult;
    CacheItem* removedItem;
    
    for (itemIter = _items.begin(); itemIter != _items.end(); itemIter++)
    {
        itemsMapResult = itemsMap.insert(pair<string, CacheItem*>((*itemIter)->name, *itemIter));
        if (!itemsMapResult.second)
        {
            if ((*(itemsMapResult.first)).second->sessionCreation < (*itemIter)->sessionCreation)
            {
                // item in map has same name but is older - remove and insert the newer item
                removedItem = (*itemsMapResult.first).second;
                if (forRecorder)
                {
                    RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache(removedItem->hdDestinationId);
                }
                itemsMap.erase(itemsMapResult.first);
                itemsMap.insert(pair<string, CacheItem*>((*itemIter)->name, *itemIter));
            }
            else
            {
                removedItem = *itemIter;
                if (forRecorder)
                {
                    RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache(removedItem->hdDestinationId);
                }
            }
            
            if (forRecorder)
            {
                Logging::warning("Removed cache link of item with duplicate name: '%s'\n", removedItem->name.c_str());
            }
            else
            {
                Logging::warning("Removed in-memory cache link of item with duplicate name: '%s'\n", removedItem->name.c_str());
            }
            delete removedItem;
        }
    }
    _items.clear(); // will be refilled with items with files associated
    
    
    // match files on disk to the items registered in the database

    vector<string> filesInCache;
    DIR* cacheDirStream = NULL;
    struct dirent* cacheDirent;
    map<string, CacheItem*>::iterator itemsMapIter;
    
    if ((cacheDirStream = opendir(_directory.c_str())) == NULL)
    {
        REC_LOGTHROW(("Failed to open the cache directory for reading: %s", strerror(errno)));
    }
    
    while ((cacheDirent = readdir(cacheDirStream)) != NULL)
    {
        if (strcmp(cacheDirent->d_name, ".") == 0 ||
            strcmp(cacheDirent->d_name, "..") == 0 ||
            strcmp(cacheDirent->d_name, g_creatingDirectory) == 0)
        {
            continue;
        }
        
        itemsMapIter = itemsMap.find(cacheDirent->d_name);
        if (itemsMapIter != itemsMap.end())
        {
            _items.push_back((*itemsMapIter).second);
            itemsMap.erase(itemsMapIter);
        }
        else
        {
            Logging::warning("Unknown file found in the cache: '%s'\n", cacheDirent->d_name);
        }
        
        filesInCache.push_back(cacheDirent->d_name);
    }
    
    closedir(cacheDirStream);

    
    // delete items that do not have an associated file on disk
    for (itemsMapIter = itemsMap.begin(); itemsMapIter != itemsMap.end(); itemsMapIter++)
    {
        if (forRecorder)
        {
            RecorderDatabase::getInstance()->removeHardDiskDestinationFromCache((*itemsMapIter).second->hdDestinationId);
            Logging::warning("Removed cache link of item '%s' with no associated file on disk\n", 
                (*itemsMapIter).second->name.c_str());
        }
        else
        {
            Logging::warning("Removed in-memory cache link of item '%s' with no associated file on disk\n", 
                (*itemsMapIter).second->name.c_str());
        }
        delete (*itemsMapIter).second;
    }
}

bool Cache::processItemAdded(string name)
{
    CacheStatusUpdateHelper updater(this);
    LOCK_SECTION(_itemsMutex);
    
    // check if we already have it
    vector<CacheItem*>::iterator itemIter;
    for (itemIter = _items.begin(); itemIter != _items.end(); itemIter++)
    {
        if ((*itemIter)->name.compare(name) == 0)
        {
            return true;
        }
    }
    
    CacheItem* item = RecorderDatabase::getInstance()->loadCacheItem(_cacheTable->databaseId, name);
    if (item == 0)
    {
        return false;
    }
    
    updater.enable();
    _items.push_back(item);
    return true;
}

bool Cache::isPageFilename(string filename)
{
    return filename.find("%d") != string::npos;
}

string Cache::getPageFilenameTemplate(string baseFilename)
{
    return baseFilename + "__%d" + g_mxfPageFileSuffix;
}

string Cache::getPageFilename(string filename, int index)
{
    size_t indexFormatPos = filename.find("%d");
    if (indexFormatPos != string::npos)
    {
        // replace %d with the index as string
        return filename.substr(0, indexFormatPos).append(int_to_string(index)).append(&filename.c_str()[indexFormatPos + 2]);
    }
    
    return filename;
}

string Cache::getSingleItemFilename(string baseFilename, long instanceNumber, string suffix)
{
    return baseFilename + get_filename_instance(instanceNumber) + suffix;
}

string Cache::getMultiItemFilename(string baseFilename, int itemNumber, string suffix)
{
    return baseFilename + get_filename_instance(itemNumber) + suffix;
}

CreatingCacheItem* Cache::getCreatingItem(string name)
{
    vector<CreatingCacheItem*>::const_iterator iter;
    for (iter = _creatingItems.begin(); iter != _creatingItems.end(); iter++)
    {
        if ((*iter)->name.compare(name) == 0)
        {
            return *iter;
        }
    }
    
    return 0;
}

CreatingCacheItem* Cache::eraseCreatingItem(string name)
{
    CreatingCacheItem* creatingItem = 0;
    vector<CreatingCacheItem*>::iterator iter;
    for (iter = _creatingItems.begin(); iter != _creatingItems.end(); iter++)
    {
        if ((*iter)->name.compare(name) == 0)
        {
            creatingItem = *iter;
            _creatingItems.erase(iter);
            return creatingItem;
        }
    }
    
    return creatingItem;
}

