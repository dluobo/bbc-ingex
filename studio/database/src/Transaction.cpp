/*
 * $Id: Transaction.cpp,v 1.2 2009/04/16 18:13:02 john_f Exp $
 *
 * A database transaction
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
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
 
#include <cassert>
#include <cstring>

#include <odbc++/connection.h>

#include "Transaction.h"
#include "Utilities.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


Transaction::Transaction(Database* database, odbc::Connection* odbcConnection)
: Connection(database, odbcConnection)
{}

Transaction::~Transaction()
{
    try
    {
        rollback();
    }
    catch (...) {}
}

void Transaction::commit()
{
    // do nothing - transaction is only committed when commitTransaction() is called
}

void Transaction::rollback()
{
    Connection::rollback();
}

void Transaction::commitTransaction()
{
    Connection::commit();
}

