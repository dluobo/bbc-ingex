/*
 * $Id: XMCFile.cpp,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Parse Multi-Camera cuts file.
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

#define __STDC_FORMAT_MACROS 1 // for PRId64 macro

#include <memory>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>

#include <XmlTools.h>

#include <DatabaseEnums.h>

#include "XMCFile.h"
#include "ICException.h"

using namespace std;
using namespace prodauto;
using namespace xercesc;



string XMCFile::ParseStringAttr(const DOMElement *element, string attr_name)
{
    const XMLCh *x_attr = element->getAttribute(Xml(attr_name.c_str()));
    if (!x_attr || x_attr[0] == (XMLCh)0)
        throw ICException("Missing attribute '%s'", attr_name.c_str());
 
    return StrXml(x_attr).localForm();
}

int XMCFile::ParseIntAttr(const DOMElement *element, string attr_name)
{
    string attr_str;
    int value;
    
    attr_str = ParseStringAttr(element, attr_name);
    
    if (sscanf(attr_str.c_str(), "%d", &value) != 1)
        throw ICException("Invalid int value: %s", attr_str.c_str());
    
    return value;
}

Date XMCFile::ParseDateAttr(const DOMElement *element, string attr_name)
{
    string attr_str;
    Date value;
    int year, month, day;
    
    attr_str = ParseStringAttr(element, attr_name);
    
    if (sscanf(attr_str.c_str(), "%d-%d-%d", &year, &month, &day) != 3)
        throw ICException("Invalid Date value: %s", attr_str.c_str());
    
    value.year = year;
    value.month = month;
    value.day = day;
    
    return value;
}

int64_t XMCFile::ParseInt64Attr(const DOMElement *element, string attr_name)
{
    string attr_str;
    int64_t value;
    
    attr_str = ParseStringAttr(element, attr_name);
    
    if (sscanf(attr_str.c_str(), "%"PRId64, &value) != 1)
        throw ICException("Invalid int64 value: %s", attr_str.c_str());
    
    return value;
}

Rational XMCFile::ParseRationalAttr(const DOMElement *element, string attr_name)
{
    string attr_str;
    Rational value;
    int num, den;
    
    attr_str = ParseStringAttr(element, attr_name);
    
    if (sscanf(attr_str.c_str(), "%d/%d", &num, &den) != 2)
        throw ICException("Invalid Rational value: %s", attr_str.c_str());
    
    value.numerator = num;
    value.denominator = den;
    
    return value;
}

int XMCFile::ParseDataDefAttr(const DOMElement *element, string attr_name)
{
    string attr_str;
    
    attr_str = ParseStringAttr(element, attr_name);
    
    if (attr_str.compare(PICTURE_DATA_DEFINITION_NAME) == 0)
        return PICTURE_DATA_DEFINITION;
    else if (attr_str.compare(SOUND_DATA_DEFINITION_NAME) == 0)
        return SOUND_DATA_DEFINITION;
    else
        throw ICException("Invalid data definition name: %s", attr_str.c_str());
}


XMCFile::XMCFile()
{
    mParser = 0;
    
    try
    {
        XMLPlatformUtils::Initialize();
    }
    catch (...)
    {
        throw ICException("XML toolkit initialization error");
    }
}

XMCFile::~XMCFile()
{
    Clear();
    
    // not calling XMLPlatformUtils::Terminate() to allow reuse of initialized parser
    // from the docs: "The termination call is currently optional, to aid those dynamically loading the parser to clean
    //                 up before exit, or to avoid spurious reports from leak detectors."
}

bool XMCFile::Parse(string filename)
{
    Clear();

    try 
    {
        // create parser
        mParser = new XercesDOMParser();
        mParser->setValidationScheme(XercesDOMParser::Val_Never);
        mParser->setDoNamespaces(false);
        mParser->setDoSchema(false);
        mParser->setLoadExternalDTD(false);
    
        // xml parse file
        // TODO: handle parse errors, e.g. file not exist
        mParser->parse(filename.c_str());

        DOMDocument *doc = mParser->getDocument();
        if (!doc)
            throw ICException("File does not exist or is not an XML file");
        
        // parse root element
        DOMElement *root = doc->getDocumentElement();
        if (!root)
            throw ICException("Empty XML document");
        if (!XMLString::equals(root->getTagName(), Xml("root")))
            throw ICException("Root element is not <root>");
        
        // parse root children
        DOMNodeList *root_children = root->getChildNodes();
        const XMLSize_t root_children_count = root_children->getLength();
        XMLSize_t i;
        for (i = 0; i < root_children_count; i++) {
            DOMNode *node = root_children->item(i);
            
            if (node->getNodeType() && node->getNodeType() == DOMNode::ELEMENT_NODE) {
                DOMElement *element = dynamic_cast<xercesc::DOMElement*>(node);
                
                if (XMLString::equals(element->getTagName(), Xml("Location"))) {
                    auto_ptr<XLocation> location(new XLocation());
                    location->Parse(element);
                    
                    if (!mLocations.insert(location.get()).second)
                        throw ICException("Location Name is not unique");
                    location.release();
                } else if (XMLString::equals(element->getTagName(), Xml("SourceConfig"))) {
                    auto_ptr<XSourceConfig> src_config(new XSourceConfig());
                    src_config->Parse(element);
                    
                    if (!mSourceConfigs.insert(make_pair(src_config->Name(), src_config.get())).second)
                        throw ICException("SourceConfig Name is not unique");
                    src_config.release();
                } else if (XMLString::equals(element->getTagName(), Xml("McClip"))) {
                    auto_ptr<XMCClip> mc_clip(new XMCClip());
                    mc_clip->Parse(element);
                    
                    if (!mClips.insert(make_pair(mc_clip->Name(), mc_clip.get())).second)
                        throw ICException("McClip Name is not unique");
                    mc_clip.release();
                } else if (XMLString::equals(element->getTagName(), Xml("McCut"))) {
                    auto_ptr<XMCCut> mc_cut(new XMCCut());
                    mc_cut->Parse(element);
                    
                    mCuts.push_back(mc_cut.release());
                }
            }
        }
        
        return true;
    }
    catch (const ICException &ex)
    {
        fprintf(stderr, "Parse error: %s\n", ex.what());
        Clear();
        return false;
    }
    catch (...)
    {
        fprintf(stderr, "Parse error:\n");
        Clear();
        return false;
    }
}

void XMCFile::Clear()
{
    delete mParser;
    mParser = 0;
    
    map<string, XSourceConfig*>::iterator src_config_iter;
    for (src_config_iter = mSourceConfigs.begin(); src_config_iter != mSourceConfigs.end(); src_config_iter++)
        delete src_config_iter->second;
    mSourceConfigs.clear();

    map<string, XMCClip*>::iterator clip_iter;
    for (clip_iter = mClips.begin(); clip_iter != mClips.end(); clip_iter++)
        delete clip_iter->second;
    mClips.clear();

    vector<XMCCut*>::iterator cuts_iter;
    for (cuts_iter = mCuts.begin(); cuts_iter != mCuts.end(); cuts_iter++)
        delete (*cuts_iter);
    mCuts.clear();
}
