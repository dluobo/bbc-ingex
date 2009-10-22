/*
 * $Id: MXFOPAtomWriter.h,v 1.2 2009/10/22 13:55:36 john_f Exp $
 *
 * MXF OP-Atom writer
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

#ifndef __PRODAUTO_MXF_OPATOM_WRITER_H__
#define __PRODAUTO_MXF_OPATOM_WRITER_H__


#include <string>
#include <vector>
#include <map>

#include "MXFWriter.h"

typedef struct _PackageDefinitions PackageDefinitions;
typedef struct _AvidClipWriter AvidClipWriter;


namespace prodauto
{

class MXFOPAtomWriter : public MXFWriter
{
public:
    MXFOPAtomWriter();
    virtual ~MXFOPAtomWriter();

public:
    // from MXFWriter
    
    virtual void PrepareToWrite(PackageGroup *package_group, bool take_ownership = false);
    
    virtual void WriteSamples(uint32_t mp_track_id, uint32_t num_samples, const uint8_t *data, uint32_t data_size);
    virtual void StartSampleData(uint32_t mp_track_id);
    virtual void WriteSampleData(uint32_t mp_track_id, const uint8_t *data, uint32_t data_size);
    virtual void EndSampleData(uint32_t mp_track_id, uint32_t num_samples);
    
    virtual void CompleteWriting(bool save_to_database);
    virtual void AbortWriting(bool delete_files);
    
private:
    void CreatePackageDefinitions();
    
    int64_t GetDuration(uint32_t mp_track_id);

    void ResetWriter();

private:
    PackageGroup *mPackageGroup;
    bool mOwnPackageGroup;

    PackageDefinitions *mPackageDefinitions;
    
    AvidClipWriter *mClipWriter;
};


};


#endif

