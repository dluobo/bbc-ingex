/*
 * $Id: FCPFile.cpp,v 1.1 2008/10/08 10:16:06 john_f Exp $
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
 
#include <cassert>

#include "FCPFile.h"
#include "CreateAAFException.h"

#include <Logging.h>

#include "XmlTools.h"
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>


using namespace std;
using namespace prodauto;
using namespace xercesc;



FCPFile::FCPFile(string filename)
: _filename(filename), _impl(0), _doc(0), _rootElem(0)
{
    XmlTools::Initialise();
    
    _impl =  DOMImplementationRegistry::getDOMImplementation(X("Core"));
    _doc = _impl->createDocument(
            0,                  // root element namespace URI.
            X("xmeml"),         // root element name
            0);                 // document type object (DTD).

    _rootElem = _doc->getDocumentElement();
    _rootElem->setAttribute(X("version"), X("2"));
}

FCPFile::~FCPFile()
{
    _doc->release();
    
    XmlTools::Terminate();
}

void FCPFile::save()
{
    XmlTools::DomToFile(_doc, _filename.c_str());
}

void FCPFile::addClip(MaterialPackage* materialPackage, PackageSet& packages)
{
    // single camera clip
}

void FCPFile::addMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, 
    PackageSet& packages, vector<CutInfo> sequence)
{
    // multi-camera clip and director's cut sequence
}



