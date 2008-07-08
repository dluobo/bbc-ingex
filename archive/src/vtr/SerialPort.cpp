/*
 * $Id: SerialPort.cpp,v 1.1 2008/07/08 16:27:05 philipn Exp $
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
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "SerialPort.h"
#include "RecorderException.h"
#include "Logging.h"
#include "Config.h"


using namespace std;
using namespace rec;



SerialPort* SerialPort::open(string deviceName, BaudRate baudRate, CharacterSize charSize,
        StopBits stopBits, Parity parity)
{
    // open the serial device
    int fd = ::open(deviceName.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0)
    {
        if (Config::getBoolD("dbg_serial_port_open", false))
        {
            Logging::debug("Failed to open serial device '%s': %s\n", deviceName.c_str(), strerror(errno));
        }
        return 0;
    }
    
    // set the new serial settings
    
    struct termios newTermios;
    bzero(&newTermios, sizeof(newTermios));
    
    // non-canonical read mode
    newTermios.c_lflag = 0;
    
    // ignore modem control lines and enable the receiver
    newTermios.c_cflag |= CLOCAL | CREAD;
    
    // baud rate
    newTermios.c_cflag |= baudRate;

    // character size
    newTermios.c_cflag |= charSize;
    
    // stop bits
    switch (stopBits)
    {
        case STOP_BITS_1:
            // nothing
            break;
        case STOP_BITS_2:
            newTermios.c_cflag |= CSTOPB;
            break;
    }
    
    // parity
    switch (parity)
    {
        case PARITY_EVEN:
            newTermios.c_cflag |= PARENB;
            newTermios.c_iflag |= INPCK;
            break;
        case PARITY_ODD:
            newTermios.c_cflag |= PARENB | PARODD;
            newTermios.c_iflag |= INPCK;
            break;
        case PARITY_NONE:
            newTermios.c_iflag |= IGNPAR;
            break;
    }
    
    // read returns immediately with available characters 
    newTermios.c_cc[VMIN] = 0;
    newTermios.c_cc[VTIME] = 0;

    // flush input and output buffers
    tcflush(fd, TCIOFLUSH);
    
    // set new
    if (tcsetattr(fd, TCSANOW, &newTermios) < 0)
    {
        Logging::error("Failed to set new serial settings for '%s': %s\n", deviceName.c_str(), strerror(errno));
        close(fd);
        return 0;
    }

    // check settings could be completed
    struct termios setTermios;    
    tcgetattr(fd, &setTermios);
    if (memcmp(&newTermios.c_iflag, &setTermios.c_iflag, sizeof(newTermios.c_iflag)) != 0 ||
        memcmp(&newTermios.c_oflag, &setTermios.c_oflag, sizeof(newTermios.c_oflag)) != 0 ||
        memcmp(&newTermios.c_cflag, &setTermios.c_cflag, sizeof(newTermios.c_cflag)) != 0 ||
        memcmp(&newTermios.c_lflag, &setTermios.c_lflag, sizeof(newTermios.c_lflag)) != 0 ||
        memcmp(&newTermios.c_cc, &setTermios.c_cc, sizeof(newTermios.c_cc)) != 0) 
    {
        Logging::error("New serial settings could not be set for '%s'\n", deviceName.c_str());
        close(fd);
        return 0;
    }

    
    return new SerialPort(deviceName, fd);
}

SerialPort::SerialPort(string deviceName, int fd)
: _deviceName(deviceName), _fd(fd)
{
}

SerialPort::~SerialPort()
{
    if (_fd >= 0)
    {
        close(_fd);
    }
}

// TODO: there should be some inter symbol timeout as well after the first
// symbol has been read before the timeoutMSec
int SerialPort::read(unsigned char* buffer, int count, long timeoutMSec)
{
    LOCK_SECTION(_accessMutex);

    fd_set set;
    struct timeval tv;
    int retval;
    ssize_t numRead;
    int totalRead = 0;
    
    while (true)
    {
        FD_ZERO(&set);
        FD_SET(_fd, &set);
        
        tv.tv_sec = timeoutMSec / 1000;
        tv.tv_usec = (timeoutMSec % 1000) * 1000;
        
        retval = select(_fd + 1, &set, NULL, NULL, &tv);
        if (retval < 0)
        {
            if (errno == EINTR)
            {
                // select was interrupted by a signal - retry
                continue;
            }
            
            // select error
            Logging::error("Failed to do a select on the serial port: %s\n", strerror(errno));
            return -1;
        }
        else if (retval == 0)
        {
            // select timeout
            return -2;
        }
        
        numRead = ::read(_fd, &buffer[totalRead], count - totalRead);
        if (numRead < 0)
        {
            Logging::error("Failed to read from serial port: %s\n", strerror(errno));
            return -1;
        }
        
        totalRead += numRead;
        if (totalRead == count)
        {
            return 0;
        }
    }
}

int SerialPort::write(const unsigned char* buffer, int count)
{
    LOCK_SECTION(_accessMutex);

    while (true)
    {
        ssize_t result = ::write(_fd, buffer, count);
        if (result < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                // try again if write was blocked or interrupted by a signal
                continue;
            }
            
            Logging::error("Failed to write to serial port: %s\n", strerror(errno));
            return -1;
        }
        else if (result != count)
        {
            Logging::error("Failed to write %d bytes to serial port; only %d bytes were written\n", count, result);
            return -1;
        }
        
        return 0;
    }
}

void SerialPort::flush(bool read, bool write)
{
    LOCK_SECTION(_accessMutex);
    
    if (read && !write)
    {
        if (tcflush(_fd, TCIFLUSH) != 0)
        {
            Logging::error("Failed to flush serial port '%s' input\n", _deviceName.c_str());
        }
    }
    else if (write && !read)
    {
        if (tcflush(_fd, TCOFLUSH) != 0)
        {
            Logging::error("Failed to flush serial port '%s' output\n", _deviceName.c_str());
        }
    }
    else // read && write
    {
        if (tcflush(_fd, TCIOFLUSH) != 0)
        {
            Logging::error("Failed to flush serial port '%s' input and output\n", _deviceName.c_str());
        }
    }
}




