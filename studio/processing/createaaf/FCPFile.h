/*
 * $Id: FCPFile.h,v 1.2 2008/10/22 09:32:19 john_f Exp $
 *
 * Final Cut Pro XML file for defining clips, multi-camera clips, etc
 *
 * Copyright (C) 2008  British Broadcasting Corporation
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
 
#ifndef __PRODAUTO_FCPFILE_H__
#define __PRODAUTO_FCPFILE_H__



#include <DataTypes.h>
#include <Package.h>
#include <MCClipDef.h>
#include <SourceConfig.h>
#include "CutsDatabase.h"


#include <xercesc/dom/DOM.hpp>



namespace prodauto
{
// utility class to clean-up Package pointers
class MaterialHolder
{
public:
    ~MaterialHolder()
    {
        PackageSet::iterator iter;
        for (iter = packages.begin(); iter != packages.end(); iter++)
        {
            delete *iter;
        }
        
        // topPackages only holds references so don't delete
    }
    
    MaterialPackageSet topPackages; 
    PackageSet packages;
};   

class FCPFile
{
public:
    FCPFile(std::string filename);
    ~FCPFile();

    // must call this to save the data to the file
    void save();
    
    // add a single clip corresponding to the material package
    void addClip(MaterialPackage* materialPackage, PackageSet& packages, int totalClips, std::string fcppath);

    // add a multi-camera clip
    void addMCClip(MCClipDef* itmcP, MaterialHolder &material, int idname);

    // add a multi-camera clip sequence from Directors Cut
    void addMCSequence(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,std::vector<CutInfo> sequence);

    
    
private:
    bool getSource(Track* track, PackageSet& packages, SourcePackage** fileSourcePackage, SourcePackage** sourcePackage, Track** sourceTrack);

    std::string _filename;
    
    xercesc::DOMImplementation* _impl;
    xercesc::DOMDocument* _doc;
    
    xercesc::DOMElement* _rootElem;
    // TODO: Put other elements you need to have beyond each call to addClip, addMCClip, etc.
};


};



#endif

