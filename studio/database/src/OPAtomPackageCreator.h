/*
 * $Id: OPAtomPackageCreator.h,v 1.1 2009/10/12 15:54:33 philipn Exp $
 *
 * OP-Atom package group creator
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

#ifndef __PRODAUTO_OPATOM_PACKAGE_CREATOR_H__
#define __PRODAUTO_OPATOM_PACKAGE_CREATOR_H__

#include "RecorderPackageCreator.h"


namespace prodauto
{


class OPAtomPackageCreator : public RecorderPackageCreator
{
public:
    static std::string CreateFileLocation(std::string prefix, std::string suffix,
                                          int data_def, uint32_t track_id, uint32_t track_number);

public:
    OPAtomPackageCreator(bool is_pal_project);
    virtual ~OPAtomPackageCreator();

public:
    // from RecorderPackageCreator
    virtual void CreatePackageGroup(SourceConfig *source_config, std::vector<bool> enabled_tracks,
                                    uint32_t umid_gen_offset);

private:
    std::string CreateFileLocation(int data_def, uint32_t track_id, uint32_t track_number);
};



};



#endif

