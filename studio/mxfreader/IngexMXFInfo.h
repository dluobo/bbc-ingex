/*
 * $Id: IngexMXFInfo.h,v 1.2 2009/10/22 15:15:56 john_f Exp $
 *
 * Extract information from Ingex MXF files.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __INGEX_MXF_INFO_H__
#define __INGEX_MXF_INFO_H__


#include <string>

#include <PackageGroup.h>

#include <mxf/mxf_types.h>

namespace mxfpp {
    class HeaderMetadata;
    class DataModel;
    class GenericPackage;
    class MaterialPackage;
    class SourcePackage;
}

class TaggedValue;


class IngexMXFInfo
{
public:
    typedef enum
    {
        SUCCESS = 0,
        FAILED = -1,
        FILE_OPEN_ERROR = -2,
        NO_HEADER_PARTITION = -3,
        NOT_OP_ATOM = -4,
        HEADER_ERROR = -5,
        UNKNOWN_PROJECT_EDIT_RATE = -6
    } ReadResult;
    
public:
    static ReadResult read(std::string filename, IngexMXFInfo **info);
    static ReadResult read(std::string filename, mxfUL *essence_container_label,
                           mxfpp::HeaderMetadata *header_metadata, mxfpp::DataModel *data_model,
                           IngexMXFInfo **info);
    static std::string errorToString(ReadResult result);

public:
    ~IngexMXFInfo();
    
    std::string getFilename() const { return _filename; }
    
    prodauto::PackageGroup* getPackageGroup() const { return _package_group; }

private:
    IngexMXFInfo(std::string filename, mxfUL *essence_container_label, mxfpp::HeaderMetadata *header_metadata,
                 mxfpp::DataModel *data_model);
    
    
    bool extractProjectEditRate(mxfpp::GenericPackage *mxf_package, prodauto::Rational *project_edit_rate);
    
    void extractPackageInfo(mxfpp::GenericPackage *mxf_package, mxfUL *essence_container_label);
    void extractMaterialPackageInfo(mxfpp::MaterialPackage *mxf_package);
    void extractFileSourcePackageInfo(mxfpp::SourcePackage *mxf_package, mxfUL *essence_container_label);
    void extractTapeSourcePackageInfo(mxfpp::SourcePackage *mxf_package);

    std::vector<TaggedValue*> getPackageAttributes(mxfpp::GenericPackage *mxf_package);
    std::vector<TaggedValue*> getPackageComments(mxfpp::GenericPackage *mxf_package);
    std::string getStringTaggedValue(std::vector<TaggedValue*>& tagged_values, std::string name);

    int getVideoResolutionId(mxfUL *container_label, mxfUL *picture_essence_coding, int32_t avid_resolution_id);
    
    prodauto::SourcePackage* getCurrentFileSourcePackage();

private:
    std::string _filename;
    prodauto::PackageGroup *_package_group;
    
    mxfpp::DataModel *_data_model;
};



#endif
