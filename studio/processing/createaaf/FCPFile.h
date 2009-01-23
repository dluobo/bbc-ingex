/*
 * $Id: FCPFile.h,v 1.3 2009/01/23 19:42:44 john_f Exp $
 *
 * Final Cut Pro XML file for defining clips, multi-camera clips, etc
 *
 * Copyright (C) 2008, BBC, Nicholas Pinks <npinks@users.sourceforge.net>
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



#include "EditorsFile.h"


#include <xercesc/dom/DOM.hpp>



namespace prodauto
{

class FCPFile : public EditorsFile
{
public:
    FCPFile(std::string filename, std::string fcpPath);
    virtual ~FCPFile();

    // must call this to save the data to the file
    virtual void save();
    
    // add a single clip corresponding to the material package
    virtual void addClip(MaterialPackage* materialPackage, PackageSet& packages);

    // add a multi-camera clip and cut sequence
    virtual bool addMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
        std::vector<CutInfo> sequence);
    
    
private:
    bool addMCClipInternal(MCClipDef* itmcP, MaterialPackageSet& materialPackages, PackageSet& packages);
    void addMCSequenceInternal(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,std::vector<CutInfo> sequence);
    bool getSource(Track* track, PackageSet& packages, SourcePackage** fileSourcePackage, SourcePackage** sourcePackage, Track** sourceTrack);

    std::string _filename;
    
    xercesc::DOMImplementation* _impl;
    xercesc::DOMDocument* _doc;
    
    xercesc::DOMElement* _rootElem;

    std::string _fcpPath;
    int _idName;
};


};



#endif

