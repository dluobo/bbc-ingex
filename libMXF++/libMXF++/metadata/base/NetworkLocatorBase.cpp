/*
 * $Id: NetworkLocatorBase.cpp,v 1.1 2009/02/02 05:14:35 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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

#include <libMXF++/MXF.h>


using namespace std;
using namespace mxfpp;


const mxfKey NetworkLocatorBase::setKey = MXF_SET_K(NetworkLocator);


NetworkLocatorBase::NetworkLocatorBase(HeaderMetadata* headerMetadata)
: Locator(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

NetworkLocatorBase::NetworkLocatorBase(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: Locator(headerMetadata, cMetadataSet)
{}

NetworkLocatorBase::~NetworkLocatorBase()
{}


std::string NetworkLocatorBase::getURLString() const
{
    return getStringItem(&MXF_ITEM_K(NetworkLocator, URLString));
}

void NetworkLocatorBase::setURLString(std::string value)
{
    setStringItem(&MXF_ITEM_K(NetworkLocator, URLString), value);
}

