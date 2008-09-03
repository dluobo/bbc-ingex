/*
 * $Id: DataSourceImpl.cpp,v 1.2 2008/09/03 13:43:33 john_f Exp $
 *
 * Implementation of DataSource for use in servant.
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

#include "DataSourceImpl.h"
#include "CorbaUtil.h"
#include "StatusClientC.h"

// Implementation skeleton constructor
DataSourceImpl::DataSourceImpl (void)
{
}

// Implementation skeleton destructor
DataSourceImpl::~DataSourceImpl (void)
{
}

::ProdAuto::DataSource::ConnectionState DataSourceImpl::AddStatusClient (
    ::ProdAuto::StatusClient_ptr client
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("DataSourceImpl::AddStatusClient - %C\n"),
        CorbaUtil::Instance()->ObjectToText(client).c_str()));

    return mStatusDist.AddClient(client);
}

void DataSourceImpl::RemoveStatusClient (
    ::ProdAuto::StatusClient_ptr client
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    mStatusDist.RemoveClient(client);
}

char * DataSourceImpl::ReturnStatus (
    const char * name
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
	return CORBA::string_dup("");
}


