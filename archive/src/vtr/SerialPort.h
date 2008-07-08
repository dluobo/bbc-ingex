/*
 * $Id: SerialPort.h,v 1.1 2008/07/08 16:27:16 philipn Exp $
 *
 * Provides serial port reading and writing
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#ifndef __RECORDER_SERIAL_PORT_H__
#define __RECORDER_SERIAL_PORT_H__


#include <termios.h>
#include <string>

#include "Threads.h"


namespace rec
{

typedef enum
{
    BAUD_0 = B0,
    BAUD_50 = B50,
    BAUD_75 = B75,
    BAUD_110 = B110,
    BAUD_134 = B134,
    BAUD_150 = B150,
    BAUD_200 = B200,
    BAUD_300 = B300,
    BAUD_600 = B600,
    BAUD_1200 = B1200,
    BAUD_1800 = B1800,
    BAUD_2400 = B2400,
    BAUD_4800 = B4800,
    BAUD_9600 = B9600,
    BAUD_19200 = B19200,
    BAUD_38400 = B38400,
    BAUD_57600 = B57600,
    BAUD_115200 = B115200,
    BAUD_230400 = B230400
} BaudRate;

typedef enum
{
    CHAR_SIZE_5 = CS5,
    CHAR_SIZE_6 = CS6,
    CHAR_SIZE_7 = CS7,
    CHAR_SIZE_8 = CS8
} CharacterSize;

typedef enum
{
    STOP_BITS_1,
    STOP_BITS_2
} StopBits;

typedef enum 
{
    PARITY_ODD,
    PARITY_EVEN,
    PARITY_NONE
} Parity;


class SerialPort
{
public:
    static SerialPort* open(std::string deviceName, BaudRate baudRate, CharacterSize charSize,
        StopBits stopBits, Parity parity);

public:
    ~SerialPort();
    
    int read(unsigned char* buffer, int count, long timeoutMSec);
    int write(const unsigned char* buffer, int count);
    void flush(bool read, bool write);


private:
    SerialPort(std::string deviceName, int fd);
    
    std::string _deviceName;
    int _fd;
    Mutex _accessMutex;
};




};




#endif


