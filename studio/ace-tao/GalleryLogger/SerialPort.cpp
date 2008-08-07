/*
 * $Id: SerialPort.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to manage communication over a serial port.
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

#include "SerialPort.h"

#include <iostream>

SerialPort::SerialPort()
{
}

SerialPort::~SerialPort()
{
    CloseHandle(mHandle);
}

void SerialPort::Open(const char * port)
{
    mHandle = CreateFile(port, 
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);
    if(mHandle == INVALID_HANDLE_VALUE)
    {
        //ACE_DEBUG(( LM_ERROR, "Failed to open port \"%C\"\n", port ));
        return;
    }

    //SetupComm(portHandle, 4096, 4096));

    DCB dcb;
    GetCommState(mHandle, &dcb);  // get existing settings
    BuildCommDCB("baud=19200 parity=N data=8 stop=1", &dcb);  // set baud rate etc.

    if(0 == SetCommState(mHandle, &dcb))  // apply modified settings
    {
        //ACE_DEBUG(( LM_ERROR, "SetCommState failed\n" ));
        return;
    }


    COMMTIMEOUTS cts;
    cts.ReadIntervalTimeout = MAXDWORD;
    cts.ReadTotalTimeoutConstant = 0;
    cts.ReadTotalTimeoutMultiplier = 0;
    cts.WriteTotalTimeoutConstant = 0;
    cts.WriteTotalTimeoutMultiplier = 0;

    if(0 == SetCommTimeouts(mHandle, &cts))
    {
        //ACE_DEBUG(( LM_ERROR, "SetCommTimeouts failed\n" ));
        return;
    }

    std::cerr << "Open ok" << std::endl;
}

int SerialPort::Read(char * buf, int size)
{
    DWORD bytes_read;
    int result = ReadFile(mHandle, buf, size, &bytes_read, NULL);
    return bytes_read;
}

int SerialPort::Write(const char * data, int size)
{
    DWORD bytes_written;
    int result = WriteFile(mHandle, data, size, &bytes_written, NULL);
    return result;
}


