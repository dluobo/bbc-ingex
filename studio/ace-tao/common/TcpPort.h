/*
 * $Id: TcpPort.h,v 1.1 2008/04/18 16:03:31 john_f Exp $
 *
 * Communicate with remote TCP port.
 *
 * Copyright (C) 2008  British Broadcasting Corporation.
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

#ifndef TcpPort_h
#define TcpPort_h

#include "CommunicationPort.h"

#include <ace/SOCK_Connector.h>
#include <ace/SOCK_Stream.h>


class TcpPort : public CommunicationPort
{
public:
    bool Connect(const std::string &);
    int Send(const void * buf, size_t n, const ACE_Time_Value * timeout = 0);
    int Recv(void * buf, size_t n, const ACE_Time_Value * timeout=0);

    ~TcpPort();
private:
    ACE_SOCK_Connector mConnector;
    ACE_SOCK_Stream mStream;
};

#endif //#ifndef TcpPort_h
