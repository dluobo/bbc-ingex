/*
 * $Id: RecoverClip.h,v 1.2 2009/12/17 16:54:31 john_f Exp $
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

#ifndef __RECOVER_CLIP_H__
#define __RECOVER_CLIP_H__

#include <vector>
#include <string>

#include <PackageGroup.h>
#include <IngexMXFInfo.h>


class RecoverClip
{
public:
    RecoverClip(const IngexMXFInfo *info);
    ~RecoverClip();
    
    bool IsSameClip(const IngexMXFInfo *info);
    void MergeIn(const IngexMXFInfo *info);
    
    bool IsComplete();
    std::vector<uint32_t> GetMissingTracks();
    
    prodauto::MaterialPackage* GetMaterialPackage() { return mPackageGroup->GetMaterialPackage(); }
    
    bool Recover(std::string create_dir, std::string dest_dir, std::string fail_dir, std::string output_prefix,
                 bool remove_files_on_error, bool simulate, bool simulate_no_ess);
    
private:
    prodauto::PackageGroup *mPackageGroup;
};



#endif
