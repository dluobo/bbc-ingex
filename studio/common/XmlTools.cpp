/*
 * $Id: XmlTools.cpp,v 1.2 2009/09/18 16:25:48 philipn Exp $
 *
 * Utility class for handling XML.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
 * All Rights Reserved.
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

#include "XmlTools.h"

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/parsers/AbstractDOMParser.hpp>

XERCES_CPP_NAMESPACE_USE

#include <iostream>


void XmlTools::Initialise()
{
    // Initialize the XML4C2 system.
    try
    {
        XMLPlatformUtils::Initialize();
    }
    catch(const XMLException& toCatch)
    {
        char * pMsg = XMLString::transcode(toCatch.getMessage());
        // option to output message here
        XMLString::release(&pMsg);
        return;
    }
}

void XmlTools::Terminate()
{
    XMLPlatformUtils::Terminate();
}

void XmlTools::DomToFile(DOMDocument * doc, const char * filename, bool pretty_print)
{
    try
    {
        // Get a serializer, an instance of DOMWriter.
        XMLCh tempStr[100];
        XMLString::transcode("LS", tempStr, 99);
        DOMImplementation * impl          = DOMImplementationRegistry::getDOMImplementation(tempStr);
        DOMWriter         * theSerializer = ((DOMImplementationLS*)impl)->createDOMWriter();

        // Set pretty print feature if available
        if (pretty_print &&
            theSerializer->canSetFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true))
        {
            theSerializer->setFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true);
        }

#if 0
        // set user specified output encoding
        theSerializer->setEncoding(gOutputEncoding);

        // plug in user's own filter
        if (gUseFilter)
        {
            // even we say to show attribute, but the DOMWriter
            // will not show attribute nodes to the filter as
            // the specs explicitly says that DOMWriter shall
            // NOT show attributes to DOMWriterFilter.
            //
            // so DOMNodeFilter::SHOW_ATTRIBUTE has no effect.
            // same DOMNodeFilter::SHOW_DOCUMENT_TYPE, no effect.
            //
            myFilter = new DOMPrintFilter(DOMNodeFilter::SHOW_ELEMENT   |
                                          DOMNodeFilter::SHOW_ATTRIBUTE |
                                          DOMNodeFilter::SHOW_DOCUMENT_TYPE);
            theSerializer->setFilter(myFilter);
        }

        // plug in user's own error handler
        DOMErrorHandler *myErrorHandler = new DOMPrintErrorHandler();
        theSerializer->setErrorHandler(myErrorHandler);

        // set feature if the serializer supports the feature/mode
        if (theSerializer->canSetFeature(XMLUni::fgDOMWRTSplitCdataSections, gSplitCdataSections))
            theSerializer->setFeature(XMLUni::fgDOMWRTSplitCdataSections, gSplitCdataSections);

        if (theSerializer->canSetFeature(XMLUni::fgDOMWRTDiscardDefaultContent, gDiscardDefaultContent))
            theSerializer->setFeature(XMLUni::fgDOMWRTDiscardDefaultContent, gDiscardDefaultContent);

        if (theSerializer->canSetFeature(XMLUni::fgDOMWRTBOM, gWriteBOM))
            theSerializer->setFeature(XMLUni::fgDOMWRTBOM, gWriteBOM);
#endif

        //
        // Plug in a format target to receive the resultant
        // XML stream from the serializer.
        //
        // StdOutFormatTarget prints the resultant XML stream
        // to stdout once it receives any thing from the serializer.
        //
        XMLFormatTarget * myFormTarget = new LocalFileFormatTarget(filename);

        // get the DOM representation
        //DOMNode                     *doc = parser->getDocument();

        //
        // do the serialization through DOMWriter::writeNode();
        //
        theSerializer->writeNode(myFormTarget, *doc);

        delete theSerializer;

        //
        // Filter, formatTarget and error handler
        // are NOT owned by the serializer.
        //
        delete myFormTarget;
        //delete myErrorHandler;

        //if (gUseFilter)
        //    delete myFilter;

    }
    catch (const OutOfMemoryException &)
    {
        //XERCES_STD_QUALIFIER cerr << "OutOfMemoryException" << XERCES_STD_QUALIFIER endl;
        //retval = 5;
    }
    catch (XMLException &)
    {
        //XERCES_STD_QUALIFIER cerr << "An error occurred during creation of output transcoder. Msg is:"
            //<< XERCES_STD_QUALIFIER endl
            //<< StrXml(e.getMessage()) << XERCES_STD_QUALIFIER endl;
            ;
        //retval = 4;
    }
}

/**
Parse an XML file into a DOM document
*/
void XmlTools::FileToDom(const char * filename, DOMBuilder * & parser, DOMDocument * & doc)
{
    // Default settings
    AbstractDOMParser::ValSchemes valScheme = AbstractDOMParser::Val_Auto;
    bool                       doNamespaces       = false;
    bool                       doSchema           = false;
    bool                       schemaFullChecking = false;

    // Instantiate the DOM parser.
    static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
    DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(gLS);
    //DOMBuilder        *parser = ((DOMImplementationLS*)impl)->createDOMBuilder(DOMImplementationLS::MODE_SYNCHRONOUS, 0);
    parser = ((DOMImplementationLS*)impl)->createDOMBuilder(DOMImplementationLS::MODE_SYNCHRONOUS, 0);

    parser->setFeature(XMLUni::fgDOMNamespaces, doNamespaces);
    parser->setFeature(XMLUni::fgXercesSchema, doSchema);
    parser->setFeature(XMLUni::fgXercesSchemaFullChecking, schemaFullChecking);

    if (valScheme == AbstractDOMParser::Val_Auto)
    {
        parser->setFeature(XMLUni::fgDOMValidateIfSchema, true);
    }
    else if (valScheme == AbstractDOMParser::Val_Never)
    {
        parser->setFeature(XMLUni::fgDOMValidation, false);
    }
    else if (valScheme == AbstractDOMParser::Val_Always)
    {
        parser->setFeature(XMLUni::fgDOMValidation, true);
    }

    // enable datatype normalization - default is off
    parser->setFeature(XMLUni::fgDOMDatatypeNormalization, true);

    // And create our error handler and install it
    //DOMCountErrorHandler errorHandler;
    //parser->setErrorHandler(&errorHandler);

    //
    try
    {
        // reset document pool
        parser->resetDocumentPool();

        // parse the XML file and create DOM doc
        doc = parser->parseURI(filename);
    }

    catch (const XMLException& toCatch)
    {
        std::cerr << "\nError during parsing: '" << filename << "'\n"
             << "Exception message is:  \n"
             << StrXml(toCatch.getMessage()) << "\n" << std::endl;
    }
    catch (const DOMException& toCatch)
    {
        const unsigned int maxChars = 2047;
        XMLCh errText[maxChars + 1];

        std::cerr << "\nDOM Error during parsing: '" << filename << "'\n"
            << "DOMException code is:  " << toCatch.code << std::endl;

        if (DOMImplementation::loadDOMExceptionMsg(toCatch.code, errText, maxChars))
            std::cerr << "Message is: " << StrXml(errText) << std::endl;

    }
    catch (...)
    {
        std::cerr << "\nUnexpected exception during parsing: '" << filename << "'\n";
    }
//
    //  Delete the parser itself.  Must be done prior to calling Terminate.
    //
    //parser->release(); 
}

