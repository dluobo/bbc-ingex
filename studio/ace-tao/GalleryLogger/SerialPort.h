/*
 * $Id: SerialPort.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
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

#ifndef SerialPort_h
#define SerialPort_h

#include <afxwin.h>         // MFC core and standard components

class SerialPort
{
public:
    SerialPort();
    ~SerialPort();
    void Open(const char * port);
    int Write(const char * data, int size);
    int Read(char * buf, int size);
private:
    HANDLE mHandle;
};

#endif //#ifndef SerialPort_h
