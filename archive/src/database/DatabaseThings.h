/*
 * $Id: DatabaseThings.h,v 1.1 2008/07/08 16:23:03 philipn Exp $
 *
 * Database enums and classes matching the things stored in the database
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
 
#ifndef __RECORDER_DATABASE_THINGS_H__
#define __RECORDER_DATABASE_THINGS_H__


#include <string>
#include <vector>

#include "Types.h"


// Database enums

#define D3_TAPE_SOURCE_TYPE             1

#define HARD_DISK_DEST_TYPE             1
#define DIGIBETA_DEST_TYPE              2

#define RECORDING_SESSION_STARTED       1
#define RECORDING_SESSION_COMPLETED     2
#define RECORDING_SESSION_ABORTED       3

#define USER_ABORT_SESSION              1
#define SYSTEM_ABORT_SESSION            2

#define PSE_RESULT_PASSED               1
#define PSE_RESULT_FAILED               2

#define LTO_TRANSFER_SESSION_STARTED    1
#define LTO_TRANSFER_SESSION_COMPLETED  2
#define LTO_TRANSFER_SESSION_ABORTED    3

#define LTO_FILE_TRANSFER_STATUS_NOT_STARTED     1
#define LTO_FILE_TRANSFER_STATUS_STARTED         2
#define LTO_FILE_TRANSFER_STATUS_COMPLETED       3
#define LTO_FILE_TRANSFER_STATUS_FAILED          4


// AssetProperty types;
extern const char* g_stringPropertyType;
extern const char* g_datePropertyType;
extern const char* g_lengthPropertyType;
    

namespace rec
{

class AssetProperty
{
public:
    AssetProperty();
    AssetProperty(int id_, std::string name_, std::string title_, std::string value_, 
        std::string type_, int maxSize_, bool editable_);
    AssetProperty(const AssetProperty& a);
    ~AssetProperty();
    
    int id;
    std::string name;
    std::string title;
    std::string value;
    std::string type;
    int maxSize; // relevant for the string type (not including null terminator)
    bool editable;
};
    

class AssetPropertySource
{
public:
    virtual ~AssetPropertySource() {}
    
    virtual bool haveValue(int assetId, std::string assetName) = 0;
    virtual std::string getValue(int assetId, std::string assetName) = 0;
};



class DatabaseTable
{
public:
    DatabaseTable();
    virtual ~DatabaseTable();
    
    long databaseId;    
};

class ConcreteSource : public DatabaseTable
{
public:
    virtual ~ConcreteSource() {}
    
    virtual int getTypeId() = 0;
    virtual std::vector<AssetProperty> getSourceProperties() = 0;
    virtual bool parseSourceProperties(AssetPropertySource* assetSource, std::string* errMessage) = 0; 
};

class D3Source;

class Source : public DatabaseTable
{
public:
    Source();
    virtual ~Source();

    D3Source* getD3Source(uint32_t itemNo);
    
    std::vector<ConcreteSource*> concreteSources;
    std::string barcode;
    long recInstance;
};


class ConcreteDestination : public DatabaseTable
{
public:
    virtual ~ConcreteDestination() {}
    
    virtual int getTypeId() = 0;
    virtual std::vector<AssetProperty> getDestinationProperties() = 0;
};

class Destination : public DatabaseTable
{
public:
    Destination();
    virtual ~Destination();

    ConcreteDestination* concreteDestination;
    std::string barcode;
    long sourceId;
    long d3SourceId;
    int ingestItemNo;
};


class D3Source : public ConcreteSource
{
public:    
    D3Source();
    virtual ~D3Source();
    
    virtual int getTypeId() { return D3_TAPE_SOURCE_TYPE; }
    virtual std::vector<AssetProperty> getSourceProperties();
    virtual bool parseSourceProperties(AssetPropertySource* assetSource, std::string* errMessage); 
    
    std::string format;
    std::string progTitle;
    std::string episodeTitle;
    Date txDate;
    std::string magPrefix;
    std::string progNo;
    std::string prodCode;
    std::string spoolStatus;
    Date stockDate;
    std::string spoolDescr;
    std::string memo;
    int64_t duration;
    std::string spoolNo;
    std::string accNo;
    std::string catDetail;
    uint32_t itemNo;
};


class HardDiskDestination : public ConcreteDestination
{
public:
    HardDiskDestination();
    virtual ~HardDiskDestination();
    
    virtual int getTypeId() { return HARD_DISK_DEST_TYPE; }
    virtual std::vector<AssetProperty> getDestinationProperties();
    
    std::string hostName;
    std::string path;
    std::string name;
    UMID materialPackageUID;
    UMID filePackageUID;
    UMID tapePackageUID;
    std::string browsePath;
    std::string browseName;
    std::string psePath;
    std::string pseName;
    int pseResult;
    int64_t size;
    int64_t duration;
    int64_t browseSize;
    long cacheId;
};


class DigibetaDestination : public ConcreteDestination
{
public:
    DigibetaDestination();
    virtual ~DigibetaDestination();
    
    virtual int getTypeId() { return DIGIBETA_DEST_TYPE; }
    virtual std::vector<AssetProperty> getDestinationProperties();
};


class RecorderTable : public DatabaseTable
{
public:
    RecorderTable();
    virtual ~RecorderTable();
    
    std::string name;
};

class RecordingSessionSourceTable : public DatabaseTable
{
public:
    RecordingSessionSourceTable();
    virtual ~RecordingSessionSourceTable();
    
    Source* source; // not owned
};

class RecordingSessionDestinationTable : public DatabaseTable
{
public:
    RecordingSessionDestinationTable();
    virtual ~RecordingSessionDestinationTable();
    
    Destination* destination; // not owned
};


class RecordingSessionTable : public DatabaseTable
{
public:
    RecordingSessionTable();
    virtual ~RecordingSessionTable();
    
    RecorderTable* recorder; // not owned
    Timestamp creation;
    Timestamp updated; // only set when loading from the database
    int status;
    int abortInitiator;
    std::string comments;
    long totalD3Errors;
    RecordingSessionSourceTable* source;
    std::vector<RecordingSessionDestinationTable*> destinations;
};


class CacheTable : public DatabaseTable
{
public:
    CacheTable();
    virtual ~CacheTable();
    
    long recorderId;
    std::string path;
};


class CacheItem
{
public:
    CacheItem();
    virtual ~CacheItem();
    
    long hdDestinationId;
    std::string name;
    std::string browseName;
    std::string pseName;
    int pseResult;
    int64_t size;
    int64_t duration;
    long sessionId;
    Timestamp sessionCreation;
    std::string sessionComments;
    int sessionStatus;
    std::string sourceSpoolNo;
    uint32_t sourceItemNo;
    std::string sourceProgNo;
    std::string sourceMagPrefix;
    std::string sourceProdCode;
};
    


class InfaxExportTable : public DatabaseTable
{
public:    
    InfaxExportTable();
    virtual ~InfaxExportTable();

    Timestamp transferDate;
    std::string d3SpoolNo;
    uint32_t d3ItemNo;
    std::string progTitle;
    std::string episodeTitle;
    std::string magPrefix;
    std::string progNo;
    std::string prodCode;
    std::string mxfName;
    std::string digibetaSpoolNo;
    std::string browseName;
    std::string ltoSpoolNo;
};


class LTOFileTable : public DatabaseTable
{
public:    
    LTOFileTable();
    virtual ~LTOFileTable();
    
    int status;
    
    std::string name;
    int64_t size;
    int64_t realDuration;
    
    std::string format;
    std::string progTitle;
    std::string episodeTitle;
    Date txDate;
    std::string magPrefix;
    std::string progNo;
    std::string prodCode;
    std::string spoolStatus;
    Date stockDate;
    std::string spoolDescr;
    std::string memo;
    int64_t duration;
    std::string spoolNo;
    std::string accNo;
    std::string catDetail;
    uint32_t itemNo;
    
    long recordingSessionId;
    
    std::string hddHostName;
    std::string hddPath;
    std::string hddName;
    UMID materialPackageUID;
    UMID filePackageUID;
    UMID tapePackageUID;
    
    std::string sourceSpoolNo;
    uint32_t sourceItemNo;
    std::string sourceProgNo;
    std::string sourceMagPrefix;
    std::string sourceProdCode;

    // stored in InfaxExport table
    InfaxExportTable infaxExport;
    
    // not stored in database
    Timestamp transferStarted;
    Timestamp transferEnded;
};


class LTOTable : public DatabaseTable
{
public:    
    LTOTable();
    virtual ~LTOTable();
    
    std::string spoolNo;
    std::vector<LTOFileTable*> ltoFiles;
};


class LTOTransferSessionTable : public DatabaseTable
{
public:
    LTOTransferSessionTable();
    virtual ~LTOTransferSessionTable();
    
    long recorderId;
    Timestamp creation;
    Timestamp updated;
    int status;
    int abortInitiator;
    std::string comments;
    std::string tapeDeviceVendor;
    std::string tapeDeviceModel;
    std::string tapeDeviceRevision;
    LTOTable* lto;
};



};




#endif


