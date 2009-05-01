/*
 * $Id: EditorsFile.h,v 1.3 2009/05/01 13:34:05 john_f Exp $
 *
 * Editor's file
 *
 * Copyright (C) 2008, BBC, Philip de Nier <philipn@users.sourceforge.net>
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
 
#ifndef __PRODAUTO_EDITORSFILE_H__
#define __PRODAUTO_EDITORSFILE_H__


#include <DataTypes.h>
#include <Package.h>
#include <MCClipDef.h>
#include <SourceConfig.h>
#include "CutsDatabase.h"



namespace prodauto
{

class EditorsFile
{
public:
    virtual ~EditorsFile() {}

    // must call this to save the data to the file
    virtual void save() = 0;
    
    // add a single clip corresponding to the material package
    virtual void addClip(MaterialPackage* materialPackage, PackageSet& packages) = 0;

    // add a multi-camera clip and cut sequence
    virtual bool addMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
        std::vector<CutInfo> sequence) = 0;
};


};


#endif

