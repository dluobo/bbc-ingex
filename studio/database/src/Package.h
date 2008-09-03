/*
 * $Id: Package.h,v 1.4 2008/09/03 14:27:23 john_f Exp $
 *
 * A MXF/AAF Package
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
 
#ifndef __PRODAUTO_PACKAGE_H__
#define __PRODAUTO_PACKAGE_H__


#include <string>
#include <vector>
#include <set>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "Track.h"
#include "DatabaseEnums.h"


// standard names for Avid user comments (tagged values)
#define AVID_UC_COMMENTS_NAME       "Comments"
#define AVID_UC_DESCRIPTION_NAME    "Descript"
#define AVID_UC_SHOOT_DATE_NAME     "Shoot Date"

#define STATIC_COMMENT_POSITION     (-1)
#define POSITIONED_COMMENT_NAME     "Comment" 


namespace prodauto
{

typedef enum 
{ 
    MATERIAL_PACKAGE, 
    SOURCE_PACKAGE 
} PackageType;


class ProjectName : public DatabaseObject
{
public:
    ProjectName();
    ProjectName(std::string name_);
    ProjectName(const ProjectName& projectName);
    ~ProjectName();

    ProjectName& operator=(const std::string& nm);
    ProjectName& operator=(const ProjectName& nm);
    
    bool operator==(const ProjectName& pn) const;
    bool operator<(const ProjectName& pn) const;
    
    std::string name;
};

class UserComment : public DatabaseObject
{
public:
    UserComment();
    UserComment(std::string name, std::string value, int64_t position, int colour);
    UserComment(const UserComment& tv);
    virtual ~UserComment();
    
    std::string name;
    std::string value;
    int64_t position;
    int colour;
};

class Package : public DatabaseObject
{
public:
    friend class Database; // allow access to _taggedValueLinks
    
public:
    Package();
    virtual ~Package();
    
    virtual std::string toString();

    Track* getTrack(uint32_t id);
    
    virtual PackageType getType() = 0;

    void addUserComment(std::string, std::string value, int64_t position, int colour);
    std::vector<UserComment> getUserComments(std::string name); 
    std::vector<UserComment> getUserComments();
    void clearUserComments();

    virtual void cloneInPlace(UMID newUID, bool resetLengths);
    
    UMID uid;
    std::string name;
    Timestamp creationDate;
    ProjectName projectName;
    std::vector<Track*> tracks;
    
private:
    std::vector<UserComment> _userComments;
};



class MaterialPackage : public Package
{
public:
    MaterialPackage();
    virtual ~MaterialPackage();
    
    virtual PackageType getType() { return MATERIAL_PACKAGE; }

    virtual std::string toString();
};



class EssenceDescriptor : public DatabaseObject
{
public:
    EssenceDescriptor();
    virtual ~EssenceDescriptor() {};
    
    virtual int getType() = 0;
};

class SourcePackage : public Package
{
public:
    SourcePackage();
    virtual ~SourcePackage();
    
    virtual PackageType getType() { return SOURCE_PACKAGE; }

    virtual std::string toString();

    virtual void cloneInPlace(UMID newUID, bool resetLengths);
    
    
    EssenceDescriptor* descriptor;
    std::string sourceConfigName; // file SourcePackage only
};



class FileEssenceDescriptor : public EssenceDescriptor
{
public:
    FileEssenceDescriptor();
    
    virtual int getType() { return FILE_ESSENCE_DESC_TYPE; };
    
    std::string fileLocation;
    int fileFormat;
    int videoResolutionID;
    Rational imageAspectRatio;
    uint32_t audioQuantizationBits;
};

class TapeEssenceDescriptor : public EssenceDescriptor
{
public:
    TapeEssenceDescriptor();
    
    virtual int getType() { return TAPE_ESSENCE_DESC_TYPE; };
    
    std::string spoolNumber;
};

class LiveEssenceDescriptor : public EssenceDescriptor
{
public:
    LiveEssenceDescriptor();
    
    virtual int getType() { return LIVE_ESSENCE_DESC_TYPE; };
    
    long recordingLocation;
};


// typedefs for sets of pointers to Package, MaterialPackage and SourcePackage

struct PackageComp
{
    bool operator()(const Package* left, const Package* right) const
    {
        return memcmp(&left->uid, &right->uid, sizeof(UMID)) < 0;
    }
};

typedef std::set<Package*, PackageComp> PackageSet;

struct MaterialPackageComp
{
    bool operator()(const MaterialPackage* left, const MaterialPackage* right) const
    {
        return memcmp(&left->uid, &right->uid, sizeof(UMID)) < 0;
    }
};

typedef std::set<MaterialPackage*, MaterialPackageComp> MaterialPackageSet;

struct SourcePackageComp
{
    bool operator()(const SourcePackage* left, const SourcePackage* right) const
    {
        return memcmp(&left->uid, &right->uid, sizeof(UMID)) < 0;
    }
};

typedef std::set<SourcePackage*, SourcePackageComp> SourcePackageSet;


};



#endif


