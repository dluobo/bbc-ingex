/*
 * $Id: ConfigReader.cpp,v 1.1 2009/04/16 17:41:37 john_f Exp $
 *
 * Class to read XML options for create_aaf
 *
 * Copyright (C) 2009, BBC, Nicholas Pinks <npinks@users.sourceforge.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <errno.h>
#include <list>
#include <XmlTools.h>
#include <Utilities.h>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>

#include "ConfigReader.h"

using namespace std;
using namespace prodauto;
using namespace xercesc;


/** 
 *  Constructor initiates xerces-c libararies.
 *  The XML tags and attributes are defined
 *  The xerces-C DOM parser infastructor is initialized
 */

ConfigReader::ConfigReader()
{
    try
    {
        XMLPlatformUtils::Initialize();  //Initialize Xerces infastructure
    }
    catch (...)
    {
        fprintf(stderr, "XML toolkit initialization error\n  ");    
        // throw exception here to return ERROR_XERCES_INIT
    }
    
    // Tags and attributes used in XML file.
    // Can't call transcode till after Xerces Initialize()
    TAG_root                    = XMLString::transcode("root");
    TAG_ApplicationSettings     = XMLString::transcode("ApplicationSettings");
    TAG_ClipSettings            = XMLString::transcode("ClipSettings");
    TAG_RunSettings             = XMLString::transcode("RunSettings");
    
    ATTR_Verbose                = XMLString::transcode("Verbose");   
    ATTR_GroupOnly              = XMLString::transcode("GroupOnly");
    ATTR_Group                  = XMLString::transcode("Group");
    ATTR_MultiCam               = XMLString::transcode("MultiCam");
    ATTR_NTSC                   = XMLString::transcode("NTSC");
    ATTR_DNS                    = XMLString::transcode("DNS");
    ATTR_User                   = XMLString::transcode("User");
    ATTR_Password               = XMLString::transcode("Password");
    
    ATTR_PkgID                  = XMLString::transcode("PkgID");
    
    ATTR_Prefix                 = XMLString::transcode("Prefix");
    ATTR_FCP                    = XMLString::transcode("FCP");
    ATTR_EditPath               = XMLString::transcode("EditPath");
    ATTR_DirCut                 = XMLString::transcode("DirCut");
    ATTR_DirSource              = XMLString::transcode("DirSource");
    ATTR_AudioEdit              = XMLString::transcode("AudioEdit");
    
    
    m_ConfigFileParser = new XercesDOMParser;
}


/** 
 *  This function:
 *  - Tests the access and availability of the XML configuration file.
 *  - Configures the xerces-c DOM parser.
 *  - Reads and extracts the pertinent infomation from the XML configuration file.
 *
 *  @param in configFile The text string of the HLA configuration file.
 */

void ConfigReader::readConfigFile(const std::string & configFile)
        throw(std::runtime_error)
{
    // Test to see if the file is OK.
    
    struct stat fileStatus;
    
    int iretStat = stat(configFile.c_str(), &fileStatus);
    if (iretStat == ENOENT)
            throw (std::runtime_error("Path file_name does not exist, or path is an empty string."));
    else if (iretStat == ENOTDIR)
            throw (std::runtime_error("A component of the path is not a directory"));
    else if (iretStat == ELOOP)
            throw (std::runtime_error("Too many symbolic links while traversing the path"));
    else if (iretStat == EACCES)
            throw (std::runtime_error("Permission denied"));
    else if (iretStat == ENAMETOOLONG)
            throw (std::runtime_error("File can not be read\n"));
    
    // Configure DOM parser
    m_ConfigFileParser->setValidationScheme( XercesDOMParser::Val_Never);
    m_ConfigFileParser->setDoNamespaces( false );
    m_ConfigFileParser->setDoSchema( false );
    m_ConfigFileParser->setLoadExternalDTD( false );
    
    try
    {
        m_ConfigFileParser->parse( configFile.c_str() );
  
        DOMDocument* xmlDoc = m_ConfigFileParser->getDocument();
        
        // Get the top-level element: Name is "root". No Attrbutes for "root"
        DOMElement* elementRoot = xmlDoc->getDocumentElement();
        if ( !elementRoot ) 
                throw(std::runtime_error( "empty XML document" ));
        
        //Parse XML file for tags of interest: "ApplicationSettings"
        //Look one level nested within "root".
        
        DOMNodeList* children = elementRoot->getChildNodes();
        const XMLSize_t nodeCount = children->getLength();
        
        //For all nodes, children of "root" in the XML tree.
        for( XMLSize_t xx = 0; xx < nodeCount; ++xx)
        {
            DOMNode* currentNode = children->item(xx);
            if( currentNode->getNodeType() &&  currentNode->getNodeType() == DOMNode::ELEMENT_NODE)
            {
                //Found node which is an Element. Re-cast node as element
                DOMElement* currentElement = dynamic_cast< xercesc::DOMElement* >(currentNode);
                
                //ApplicationSettings
                if( XMLString::equals(currentElement->getTagName(), TAG_ApplicationSettings))
                {
                    //Already tested node as type element and of name "Application Settings"
                    //Read attributes of element "ApplicationSettings"
                        
                    const XMLCh* xmlch_Prefix = currentElement->getAttribute(ATTR_Prefix);
                    m_Prefix = XMLString::transcode(xmlch_Prefix);
                    
                    const XMLCh* xmlch_FCP = currentElement->getAttribute(ATTR_FCP);
                    m_FCP = XMLString::transcode(xmlch_FCP);
                    
                    const XMLCh* xmlch_EditPath = currentElement->getAttribute(ATTR_EditPath);
                    m_EditPath = XMLString::transcode(xmlch_EditPath);
                    
                    const XMLCh* xmlch_DirCut = currentElement->getAttribute(ATTR_DirCut);
                    m_DirCut = XMLString::transcode(xmlch_DirCut);
                    
                    const XMLCh* xmlch_DirSource = currentElement->getAttribute(ATTR_DirSource);
                    m_DirSource = XMLString::transcode(xmlch_DirSource);
                    
                    const XMLCh* xmlch_AudioEdit = currentElement->getAttribute(ATTR_AudioEdit);
                    m_AudioEdit = XMLString::transcode(xmlch_AudioEdit);
                }
                
                //ClipSettings
                if( XMLString::equals(currentElement->getTagName(), TAG_ClipSettings))
                {
                    //Already tested node as type element and of name "Clip Settings"
                    //Read attributes of element "ClipSettings"
                    const XMLCh* xmlch_PkgID = currentElement->getAttribute(ATTR_PkgID);
                    m_PkgID = XMLString::transcode(xmlch_PkgID);
                    PKG_ID.push_back(m_PkgID);
                }
                
                //RunSettings
                if( XMLString::equals(currentElement->getTagName(), TAG_RunSettings))
                {
                    //Already tested node as type element and of name "Run Settings"
                    //Read attributes of element "RunSettings"
                    const XMLCh* xmlch_Verbose = currentElement->getAttribute(ATTR_Verbose);
                    m_Verbose = XMLString::transcode(xmlch_Verbose);
                    
                    const XMLCh* xmlch_GroupOnly = currentElement->getAttribute(ATTR_GroupOnly);
                    m_GroupOnly = XMLString::transcode(xmlch_GroupOnly);
                    
                    const XMLCh* xmlch_Group = currentElement->getAttribute(ATTR_Group);
                    m_Group = XMLString::transcode(xmlch_Group);

                    const XMLCh* xmlch_MultiCam = currentElement->getAttribute(ATTR_MultiCam); 
                    m_MultiCam = XMLString::transcode(xmlch_MultiCam);
                    
                    const XMLCh* xmlch_NTSC = currentElement->getAttribute(ATTR_NTSC);
                    m_NTSC = XMLString::transcode(xmlch_NTSC);
                    
                    const XMLCh* xmlch_DNS = currentElement->getAttribute(ATTR_DNS);
                    m_DNS = XMLString::transcode(xmlch_DNS);
                    
                    const XMLCh* xmlch_User = currentElement->getAttribute(ATTR_User);
                    m_User = XMLString::transcode(xmlch_User);
                    
                    const XMLCh* xmlch_Password = currentElement->getAttribute(ATTR_Password);
                    m_Password = XMLString::transcode(xmlch_Password);
                    

                }
            }
        }
    }
    catch( xercesc::XMLException& e)
    {
        char* message = xercesc::XMLString::transcode( e.getMessage() );
        ostringstream errBuf;
        errBuf << "Error parsing file: " << message << flush;
        XMLString::release ( &message );
    }
}

