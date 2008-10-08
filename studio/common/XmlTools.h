/*
 * $Id: XmlTools.h,v 1.1 2008/10/08 10:16:06 john_f Exp $
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

#ifndef XmlTools_h
#define XmlTools_h

#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>

#include <iostream>

XERCES_CPP_NAMESPACE_USE

class XmlTools
{
public:
    static void Initialise();
    static void Terminate();
    static void DomToFile(DOMDocument * doc, const char * filename, bool pretty_print = true);
    static void FileToDom(const char * filename, DOMBuilder * & parser, DOMDocument * & doc);
};

// ---------------------------------------------------------------------------
//  This is a simple class that lets us do easy (though not terribly efficient)
//  trancoding of char* data to XMLCh data.
// ---------------------------------------------------------------------------
class XStr
{
public :
    // -----------------------------------------------------------------------
    //  Constructors and Destructor
    // -----------------------------------------------------------------------
    XStr(const char* const toTranscode)
    {
        // Call the private transcoding method
        fUnicodeForm = XMLString::transcode(toTranscode);
    }

    ~XStr()
    {
        XMLString::release(&fUnicodeForm);
    }


    // -----------------------------------------------------------------------
    //  Getter methods
    // -----------------------------------------------------------------------
    const XMLCh* unicodeForm() const
    {
        return fUnicodeForm;
    }

private :
    // -----------------------------------------------------------------------
    //  Private data members
    //
    //  fUnicodeForm
    //      This is the Unicode XMLCh format of the string.
    // -----------------------------------------------------------------------
    XMLCh*   fUnicodeForm;
};

#define X(str) XStr(str).unicodeForm()

// ---------------------------------------------------------------------------
//  This is a simple class that lets us do easy (though not terribly efficient)
//  trancoding of XMLCh data to local code page for display.
// ---------------------------------------------------------------------------
class StrX
{
public :
    // -----------------------------------------------------------------------
    //  Constructors and Destructor
    // -----------------------------------------------------------------------
    StrX(const XMLCh* const toTranscode)
    {
        // Call the private transcoding method
        fLocalForm = XMLString::transcode(toTranscode);
    }

    ~StrX()
    {
        XMLString::release(&fLocalForm);
    }


    // -----------------------------------------------------------------------
    //  Getter methods
    // -----------------------------------------------------------------------
    const char* localForm() const
    {
        return fLocalForm;
    }

private :
    // -----------------------------------------------------------------------
    //  Private data members
    //
    //  fLocalForm
    //      This is the local code page form of the string.
    // -----------------------------------------------------------------------
    char*   fLocalForm;
};

inline std::ostream& operator<<(std::ostream& target, const StrX& toDump)
{
    target << toDump.localForm();
    return target;
}


#endif //#ifndef XmlTools_h

