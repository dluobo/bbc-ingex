/*
 * $Id: StatusClientImpl.h,v 1.2 2008/09/03 13:43:34 john_f Exp $
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

#ifndef StatusClient_h
#define StatusClient_h

#include "StatusClientS.h"
#include "StatusObserver.h"

#include <queue>

class  StatusClientImpl
  : public virtual POA_ProdAuto::StatusClient
{
public:
  // Constructor 
  StatusClientImpl (StatusObserver * obs);
  
  // Destructor 
  virtual ~StatusClientImpl (void);
  
  virtual
  void ProcessStatus (
      const ::ProdAuto::StatusItem & status
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  void HandleStatusOust (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  void Ping (
      
    )
    throw (
      ::CORBA::SystemException
    );

  void Destroy();

private:
    StatusObserver * mpObserver;
};


#endif /* StatusClient_h  */

