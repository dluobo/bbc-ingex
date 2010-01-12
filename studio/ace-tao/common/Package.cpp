/*
 * $Id: Package.cpp,v 1.4 2010/01/12 16:57:27 john_f Exp $
 *
 * Container for arbitrary data.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
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

#include "Package.h"

Package::Package() : empty_(false)
{
    ACE_TRACE(ACE_TEXT ("Package::Package()"));
}

Package::Package(bool empty) : empty_(empty)
{
    ACE_TRACE(ACE_TEXT ("Package::Package()"));
}


Package::~Package()
{
    ACE_TRACE(ACE_TEXT ("Package::~Package()"));
}


// default constructor makes empty package
StatusPackage::StatusPackage()
    :Package(true)
{
    ACE_TRACE(ACE_TEXT ("StatusPackage::StatusPackage()"));
}

// constuctor makes package containing status
StatusPackage::StatusPackage(const ProdAuto::StatusItem & status)
    :Package(), status_(status)
{
    ACE_TRACE(ACE_TEXT ("StatusPackage::StatusPackage()"));
}


StatusPackage::~StatusPackage(void)
{
    ACE_TRACE(ACE_TEXT ("StatusPackage::~StatusPackage()"));
}


const ProdAuto::StatusItem & StatusPackage::status(void) const
{
    ACE_TRACE(ACE_TEXT ("StatusPackage::status()"));
    return status_;
}

// FramePackage

// constuctor makes package containing frame data
FramePackage::FramePackage(EncodeFrameTrack & eft)
: Package(), mrVideoData(eft)
{
}

FramePackage::~FramePackage()
{
}

