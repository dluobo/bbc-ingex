/*
 * $Id: Cache.h,v 1.1 2008/07/08 16:22:00 philipn Exp $
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
 
#ifndef __RECORDER_CACHE_H__
#define __RECORDER_CACHE_H__


#include <string>
#include <map>

#include "Types.h"
#include "DatabaseThings.h"
#include "Threads.h"
#include "DirectoryWatch.h"


namespace rec
{

class CacheStatus
{
public:
    CacheStatus() : numItems(0), statusChangeCount(0) {}
    
    int numItems;
    int64_t totalSize;
    int statusChangeCount;
};
 

class CacheContentItem
{
public:
    CacheContentItem();
    ~CacheContentItem();

    long identifier;
    std::string sourceSpoolNo;
    std::string sourceProgNo;
    uint32_t sourceItemNo;
    std::string sourceMagPrefix;
    std::string sourceProdCode;
    long sessionId;
    Timestamp sessionCreation;
    std::string sessionComments;
    int sessionStatus;
    std::string name;
    int64_t size;
    int64_t duration;
    std::string pseName;
    int pseResult;
};

class CacheContents
{
public:
    CacheContents();
    ~CacheContents();
    
    std::string path;
    std::vector<CacheContentItem*> items;
};


class CreatingCacheItem : public CacheItem
{
public:
    CreatingCacheItem();
    virtual ~CreatingCacheItem();
    
    bool isTemp;
};

class Cache : public DirectoryWatchListener
{
public:
    friend class CacheDeleteManager;
    
public:
    static bool directoryExists(std::string directory);
    
public:
    // the recorder use this constructor 
    Cache(long recorderId, std::string directory, std::string browseDirectory, std::string pseDirectory);
    // the tape export use this constructor
    Cache(std::string recorderName, std::string directory);
    virtual ~Cache();
    
    // from DirectoryWatchListener
    virtual void thisDirectoryDeletion(std::string directory);
    virtual void deletion(std::string directory, std::string name);
    virtual void creation(std::string directory, std::string name);
    virtual void movedFrom(std::string directory, std::string name);
    virtual void movedTo(std::string directory, std::string name);
    
    
    bool checkMultiItemMXF(std::string barcode, int numItems);
    
    bool getMultiItemTemplateFilename(std::string barcode, int numItems, std::string* mxfFilenameTemplate);
    bool getMultiItemFilenames(std::string barcode, int itemNumber,
        std::string* mxfFilename, 
        std::string* browseFilename, std::string* browseTimecodeFilename, std::string* browseInfoFilename, 
        std::string* pseFilename);
    bool getUniqueFilenames(std::string barcode, long sourceId, 
        std::string* mxfFilename, 
        std::string* browseFilename, std::string* browseTimecodeFilename, std::string* browseInfoFilename, 
        std::string* pseFilename);
    
    int64_t getDiskSpace();
    
    std::string getPath();
    std::string getBrowsePath();
    std::string getPSEPath();
    std::string getCompleteCreatingFilename(std::string filename);
    std::string getCompleteFilename(std::string filename);
    std::string getCompleteBrowseFilename(std::string filename);
    std::string getCompletePSEFilename(std::string filename);

    int64_t getFileSize(std::string filename);
    int64_t getCreatingFileSize(std::string filename);
    int64_t getBrowseFileSize(std::string filename);
    
    void registerCreatingItem(HardDiskDestination* dest, RecordingSessionTable* session, D3Source* d3Source, bool isTemp);
    void updateCreatingItem(HardDiskDestination* dest, RecordingSessionTable* session);
    void finaliseCreatingItem(HardDiskDestination* dest);
    void removeCreatingItems();
    void removeCreatingItem(std::string filename);
    
    bool removeItem(long hdDestinationId);
    bool removeItem(std::string name);
    bool processItemRemoved(std::string name);
    bool itemExists(std::string name);
    std::string getItemName(long hdDestinationId);

    bool itemsAreKnown(std::vector<long>& itemIds);
    int64_t getTotalSize(std::vector<long>& itemIds);
    
    CacheStatus getStatus();
    CacheContents* getContents();

    // used to trigger the cache to be reloaded by clients
    void signalCacheUpdate();
    
    long getDatabaseId();    

public:
    // internal use only
    void updateStatus();

private:
    void initialise(long recorderId, bool forRecorder);
    
    bool processItemAdded(std::string name);
    
    bool fileExists(std::string filename);
    bool creatingFileExists(std::string filename);
    bool browseFileExists(std::string filename);
    bool pseFileExists(std::string filename);
    bool removeFile(std::string filename);
    bool removeCreatingFile(std::string filename);

    bool isPageFilename(std::string filename);
    std::string getPageFilenameTemplate(std::string baseFilename);
    std::string getPageFilename(std::string templateFilename, int index);
    std::string getPageFilename(std::string baseFilename, long instanceNumber, int index);
    std::string getMultiItemFilename(std::string baseFilename, int itemNumber, std::string suffix);
    std::string getSingleItemFilename(std::string baseFilename, long instanceNumber, std::string suffix);
    
    CreatingCacheItem* getCreatingItem(std::string name);
    CreatingCacheItem* eraseCreatingItem(std::string name);
    
    std::string _directory;
    std::string _browseDirectory;
    std::string _pseDirectory;
    CacheTable* _cacheTable;
    
    std::vector<CacheItem*> _items;
    std::vector<CreatingCacheItem*> _creatingItems;
    Mutex _itemsMutex;
    
    CacheStatus _status;
    Mutex _statusMutex;
    
    DirectoryWatch* _directoryWatch;
};





};





#endif



