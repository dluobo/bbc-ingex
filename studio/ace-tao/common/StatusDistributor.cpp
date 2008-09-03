/*
 * $Id: StatusDistributor.cpp,v 1.2 2008/09/03 13:43:34 john_f Exp $
 *
 * Send data to a StatusClient.
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

#include "StatusDistributor.h"
#include "IdlEnumText.h"


const int max_clients = 4;
const int queue_depth = 10;
const int queue_timeout = 5;

// constructor
StatusDistributor::StatusDistributor()
: ForwardTable(max_clients), mQueueTimeout(queue_timeout)
{
}

ProdAuto::DataSource::ConnectionState StatusDistributor::AddClient(
        ProdAuto::StatusClient_ptr client)
{
    ProdAuto::DataSource::ConnectionState result;

    if (!CORBA::is_nil(client))
    {
        // Create copy of client reference
        ::ProdAuto::StatusClient_ptr copy = 
                ::ProdAuto::StatusClient::_duplicate(client);

        // Create a new forwarder object
        StatusForwarder *newFwd; 
        ACE_NEW_NORETURN(newFwd, StatusForwarder(copy, this, queue_depth));

        // Try to add forwarder to table
        int response = addForwarder(newFwd);

        if (response == -1) {   // Forwarder can't be added to table
            delete newFwd;
            result = ::ProdAuto::DataSource::REFUSED;
        } 

        if (response == 1) {    // Client already subscribed
            delete newFwd;
            result = ::ProdAuto::DataSource::EXISTS;
        }

        if (response == 0) {    // Forwarder successfully added to table
            ACE_DEBUG((LM_DEBUG, "A new status client has subscribed. There "
                                "are now %d subscribed status clients.\n", 
                                size()));
            newFwd->open(0);    // Activate Forwarder
            result = ::ProdAuto::DataSource::ESTABLISHED;
        }
    }
    else  // nil reference supplied
    {
        result = ::ProdAuto::DataSource::REFUSED;
    }

// For new or refused subscriptions, instigate a clean-up of the forwarders.
// We send out an empty package which will make the forwarders ping their clients.
// This results in non-working clients being removed, although not immediately
// as it will be done in the svc threads.
// In this way, cleaning up takes place even if no data is passing through
// the proxy.
    if(::ProdAuto::DataSource::ESTABLISHED == result
        || ::ProdAuto::DataSource::REFUSED == result)
    {
        ACE_DEBUG((LM_DEBUG, "Instigating a clean-up of the status forwarders.\n"));

        StatusPackage *package;
        ACE_NEW_NORETURN(package, StatusPackage());
        MessageBlock *message;
        ACE_NEW_NORETURN(message, MessageBlock(package));
        queueMessage(message, &mQueueTimeout);
    }

    ACE_DEBUG((LM_DEBUG, "StatusDistributor::AddClient() returning %C\n",
        IdlEnumText::ConnectionState(result) ));

    return result;
}

void StatusDistributor::RemoveClient(ProdAuto::StatusClient_ptr client)
{
#if 0
    // use with -ORBObjRefStyle URL
    extern CORBA::ORB_var orb;
    CORBA::String_var s = orb->object_to_string(client);
    ACE_DEBUG((LM_DEBUG, "\nStatusDistributor::RemoveClient: %C\n", s.in() ));
#endif

    if (removeForwarder(client)) {
        ACE_DEBUG((LM_DEBUG, "A status client has been removed.\n"
                            "There are now %d status clients.\n", 
                            size()));
    } else {
        ACE_DEBUG((LM_DEBUG, "Status client could not be removed.\n"
                            "There are %d status clients.\n", 
                            size()));
    }
}

void StatusDistributor::SendStatus(const char * name, const char * value)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("StatusDistributor::SendStatus (%C, %C)\n"), name, value));

    ProdAuto::StatusItem status;
    status.name = CORBA::string_dup(name);
    status.value = CORBA::string_dup(value);


    // Package status data into a StatusPackage object.
    StatusPackage * package;
    ACE_NEW_NORETURN(package, StatusPackage(status));

    MessageBlock * message;
    ACE_NEW_NORETURN(message, MessageBlock(package));

#if 1
    // Queue message
    ACE_DEBUG((LM_DEBUG, "Queueing status message\n"));
    queueMessage(message, &mQueueTimeout);
#else
    // or don't queue it
    message->release();
#endif
}
