/*
 * $Id: AvidHeaderMetadata.cpp,v 1.2 2009/09/18 15:33:41 philipn Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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

#include <libMXF++/MXF.h>

#include <mxf/mxf_avid.h>

using namespace std;
using namespace mxfpp;



AvidHeaderMetadata::AvidHeaderMetadata(DataModel* dataModel)
: HeaderMetadata(dataModel)
{
    MXFPP_CHECK(mxf_avid_load_extensions(dataModel->getCDataModel()));
    dataModel->finalise();
}

AvidHeaderMetadata::~AvidHeaderMetadata()
{
}

void AvidHeaderMetadata::createDefaultMetaDictionary()
{
    ::MXFMetadataSet* metaDictSet;
    MXFPP_CHECK(mxf_avid_create_default_metadictionary(getCHeaderMetadata(), &metaDictSet));
}

void AvidHeaderMetadata::createDefaultDictionary(Preface* preface)
{
    ::MXFMetadataSet* dictSet;
    MXFPP_CHECK(mxf_avid_create_default_dictionary(getCHeaderMetadata(), &dictSet));
    
    MXFPP_CHECK(mxf_set_strongref_item(preface->getCMetadataSet(), &MXF_ITEM_K(Preface, Dictionary), dictSet));
}

void AvidHeaderMetadata::read(File* file, Partition* partition, const mxfKey* key, uint8_t llen, uint64_t len)
{
    MXFPP_CHECK(mxf_avid_read_filtered_header_metadata(file->getCFile(), 0, getCHeaderMetadata(), 
        partition->getCPartition()->headerByteCount, key, llen, len));
}

void AvidHeaderMetadata::write(File* file, Partition* partition, FillerWriter* filler)
{
    partition->markHeaderStart(file);
    
    MXFPP_CHECK(mxf_avid_write_header_metadata(file->getCFile(), getCHeaderMetadata(), partition->getCPartition()));
    if (filler)
    {
        filler->write(file);
    }

    partition->markHeaderEnd(file);
}

