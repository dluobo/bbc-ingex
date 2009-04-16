/*
 * $Id: ConfigReader.h,v 1.1 2009/04/16 17:41:37 john_f Exp $
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
 
#ifndef ConfigReader_h
#define ConfigReader_h

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMDocumentType.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMNodeIterator.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMText.hpp>

#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XMLUni.hpp>

#include <string>
#include <stdexcept>
#include <vector>
 
// Error Codes

enum 
{
    Error_ARGS = 1,
    ERROR_XERCES_INIT,
    ERROR_PARSE,
    ERROR_EMPTY_DOCUMENT
};

class ConfigReader
{
public:
    ConfigReader();
    void readConfigFile(const std::string & configFile) throw(std::runtime_error);

    std::string getPrefix()              { return m_Prefix;  }
    std::string getVerbose()             { return m_Verbose; }
    std::string getGroupOnly()           { return m_GroupOnly; }
    std::string getGroup()               { return m_Group; }
    std::string getMultiCam()            { return m_MultiCam; }
    std::string getNTSC()                { return m_NTSC; }
    std::string getDNS()                 { return m_DNS; }
    std::string getUser()                { return m_User; }
    std::string getPassword()            { return m_Password; }
    
    std::vector<std::string> getPkgID()  { return PKG_ID;   }
    
    std::string getFCP()                 { return m_FCP; }
    std::string getEditPath()            { return m_EditPath; }
    std::string getDirCut()              { return m_DirCut; }
    std::string getDirSource()           { return m_DirSource; }
    std::string getAudioEdit()           { return m_AudioEdit; }
    
    std::vector<std::string> PKG_ID;
    
private:
    xercesc::XercesDOMParser *m_ConfigFileParser;
    std::string m_Prefix;
    std::string m_Verbose;
    std::string m_GroupOnly;
    std::string m_Group;
    std::string m_MultiCam;
    std::string m_NTSC;
    std::string m_DNS;
    std::string m_User;
    std::string m_Password;
    std::string m_PkgID;
    
    std::string m_FCP;
    std::string m_EditPath;
    std::string m_DirCut;
    std::string m_DirSource;
    std::string m_AudioEdit;
    
    //Internal class use only. Hold Xerces data in UTF-16 SMLCh type.
    
    XMLCh* TAG_root;
    
    //App Settings
    XMLCh* TAG_ApplicationSettings;
    XMLCh* ATTR_Prefix;
    XMLCh* ATTR_FCP;
    XMLCh* ATTR_EditPath;
    XMLCh* ATTR_DirCut;
    XMLCh* ATTR_DirSource;
    XMLCh* ATTR_AudioEdit;
    
    //Clip Settings
    XMLCh* TAG_ClipSettings;
    XMLCh* ATTR_PkgID;
    
    //Run Settings
    XMLCh* TAG_RunSettings;
    XMLCh* ATTR_Verbose;
    XMLCh* ATTR_GroupOnly;
    XMLCh* ATTR_Group;
    XMLCh* ATTR_MultiCam;
    XMLCh* ATTR_NTSC;
    XMLCh* ATTR_DNS;
    XMLCh* ATTR_User;
    XMLCh* ATTR_Password;
};
 
 #endif //ifndef ConfigReader_h

