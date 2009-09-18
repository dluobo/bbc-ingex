/*
 * $Id: IngexMXFInfo.h,v 1.1 2009/09/18 17:29:30 john_f Exp $
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

#include <Package.h>

#include <mxf/mxf_types.h>

namespace mxfpp {
    class DataModel;
    class File;
    class Partition;
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
    } ReadResult;
    
public:
    static ReadResult read(std::string filename, IngexMXFInfo **info);

public:
    ~IngexMXFInfo();

    prodauto::MaterialPackage* getMaterialPackage() { return _material_package; }
    prodauto::SourcePackage* getFileSourcePackage() { return _file_source_package; }
    prodauto::SourcePackage* getTapeSourcePackage() { return _tape_source_package; }
    
    prodauto::Rational getProjectEditRate() { return _project_edit_rate; }
    std::string getProjectName() { return _project_name; }
    
private:
    IngexMXFInfo(std::string filename, mxfpp::File *mxf_file, mxfpp::Partition *header_partition);
    
    void extractPackageInfo(mxfpp::GenericPackage *mxf_package, mxfUL *essence_container_label);
    void extractMaterialPackageInfo(mxfpp::MaterialPackage *mxf_package);
    void extractFileSourcePackageInfo(mxfpp::SourcePackage *mxf_package, mxfUL *essence_container_label);
    void extractTapeSourcePackageInfo(mxfpp::SourcePackage *mxf_package);

    std::vector<TaggedValue*> getPackageAttributes(mxfpp::GenericPackage *mxf_package);
    std::vector<TaggedValue*> getPackageComments(mxfpp::GenericPackage *mxf_package);
    std::string getStringTaggedValue(std::vector<TaggedValue*>& tagged_values, std::string name);

    int getVideoResolutionId(mxfUL *container_label, mxfUL *picture_essence_coding, int32_t avid_resolution_id);

private:
    std::string _filename;
    
    mxfpp::DataModel *_data_model;
    
    prodauto::MaterialPackage *_material_package;
    prodauto::SourcePackage *_file_source_package;
    prodauto::SourcePackage *_tape_source_package;
    
    prodauto::Rational _project_edit_rate;
    std::string _project_name;
};



#endif
