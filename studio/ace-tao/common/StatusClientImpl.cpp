/*
 * $Id: StatusClientImpl.cpp,v 1.2 2008/09/03 13:43:34 john_f Exp $
 *
 * StatusClient servant.
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

#include <iostream>
#include <sstream>

#include "StatusClientImpl.h"


// Implementation skeleton constructor
StatusClientImpl::StatusClientImpl (StatusObserver * obs)
: mpObserver(obs)
{
}

// Implementation skeleton destructor
StatusClientImpl::~StatusClientImpl (void)
{
}

void StatusClientImpl::ProcessStatus (
    const ::ProdAuto::StatusItem & status
  )
  throw (
    ::CORBA::SystemException
  )
{
    if (mpObserver)
    {
        mpObserver->Observe (status.name.in(), status.value.in());
    }

    std::cout << "Message from recorder: " << status.name.in() << " = " << status.value.in() << std::endl;
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Status Client received: %C = %C\n"),
        status.name.in(), status.value.in()));
}

void StatusClientImpl::HandleStatusOust (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
}

void StatusClientImpl::Ping (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
}

void StatusClientImpl::Destroy()
{
  // Get the POA used when activating the object.
  PortableServer::POA_var poa =
    this->_default_POA ();

  // Get the object ID associated with this servant.
  PortableServer::ObjectId_var oid =
    poa->servant_to_id (this);

  // Now deactivate the object.
  poa->deactivate_object (oid.in ());

  // Decrease the reference count on ourselves.
  this->_remove_ref ();
}

