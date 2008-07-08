/*
 * $Id: SonyProtocol.cpp,v 1.1 2008/07/08 16:27:06 philipn Exp $
 *
 * Implements the Sony VTR protocol
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
 
#include "SonyProtocol.h"
#include "RecorderException.h"
#include "Logging.h"


using namespace std;
using namespace rec;


CommandBlock::CommandBlock()
: _blockSize(MIN_COMMAND_BLOCK_SIZE)
{
    memset(_block, 0, sizeof(_block));
}

CommandBlock::~CommandBlock()
{
}

void CommandBlock::setCMD1(unsigned char cmd1)
{
    _block[0] = (_block[0] & 0x0f) | (cmd1 & 0xf0);
    setChecksum();
}

void CommandBlock::setCMD2(unsigned char cmd2)
{
    _block[1] = cmd2;
    setChecksum();
}

void CommandBlock::setCMDs(unsigned char cmd1, unsigned char cmd2)
{
    _block[0] = (_block[0] & 0x0f) | (cmd1 & 0xf0);
    _block[1] = cmd2;
    setChecksum();
}

void CommandBlock::setData(const unsigned char* data, int dataCount)
{
    REC_ASSERT(dataCount <= 15);
    
    memcpy(&_block[2], data, dataCount);
    _block[0] = (_block[0] & 0xf0) | (((unsigned char)dataCount) && 0x0f);
    
    _blockSize = dataCount + MIN_COMMAND_BLOCK_SIZE;
    setChecksum();
}

void CommandBlock::setAck()
{
    reset();
    setCMDs(0x10, 0x01);
}

void CommandBlock::setNAck()
{
    // TODO
    REC_ASSERT(false);
}

void CommandBlock::reset()
{
    _blockSize = MIN_COMMAND_BLOCK_SIZE;
    memset(_block, 0, sizeof(_block));
}

unsigned char CommandBlock::getCMD1() const
{
    return _block[0] & 0xf0;
}

int CommandBlock::getDataCount() const
{
    return _block[0] & 0x0f;
}

unsigned char CommandBlock::getCMD2() const
{
    return _block[1];
}

const unsigned char* CommandBlock::getData() const
{
    return &_block[2];
}

bool CommandBlock::checkCMDs(unsigned char cmd1, unsigned char cmd2)
{
    return (_block[0] & 0xf0) == cmd1 && _block[1] == cmd2;
}

bool CommandBlock::isAck() const
{
    return _blockSize == MIN_COMMAND_BLOCK_SIZE && _block[0] == 0x10 && _block[1] == 0x01 && _block[2] == 0x11;
}

bool CommandBlock::isNAck() const
{
    // TODO
    REC_ASSERT(false);
    return false;
}

void CommandBlock::setBlock(unsigned char* block, int blockSize)
{
    REC_ASSERT(blockSize <= (int)sizeof(_block));
    
    if (blockSize <= 0)
    {
        memset(_block, 0, sizeof(_block));
    }
    else
    {
        memcpy(_block, block, blockSize);
    }
    _blockSize = blockSize;
}

const unsigned char* CommandBlock::getBlock() const
{
    return _block;
}

int CommandBlock::getBlockSize() const
{
    return _blockSize;
}

bool CommandBlock::validate() const
{
    // check the block size
    if (_blockSize < MIN_COMMAND_BLOCK_SIZE)
    {
        return false;
    }
    if ((_block[0] & 0xf) + MIN_COMMAND_BLOCK_SIZE != _blockSize)
    {
        return false;
    }

    // checksum
    int i;
    unsigned char checksum = 0;
    for (i = 0; i < _blockSize - 1; i++)
    {
        checksum += _block[i];
    }
    if (checksum != _block[_blockSize - 1])
    {
        return false;
    }
    
    return true;
}

void CommandBlock::setChecksum()
{
    if (_blockSize > 0)
    {
        unsigned char* checkSum = _block + _blockSize - 1;
    
        int i;
        *checkSum = 0;
        for (i = 0; i < _blockSize - 1; i++)
        {
            *checkSum += _block[i];
        }
    }
}





SonyProtocol* SonyProtocol::open(string serialDeviceName)
{
    SerialPort* serialPort = SerialPort::open(serialDeviceName, BAUD_38400, CHAR_SIZE_8,
        STOP_BITS_1, PARITY_ODD);
    if (serialPort == 0)
    {
        return 0;
    }
    
    return new SonyProtocol(serialPort);
}


SonyProtocol::SonyProtocol(SerialPort* serialPort)
: _serialPort(serialPort)
{
}

SonyProtocol::~SonyProtocol()
{
    delete _serialPort;
}

int SonyProtocol::readCommandBlock(CommandBlock* block, long timeoutMSec)
{
    unsigned char buffer[MAX_COMMAND_BLOCK_SIZE];
    
    // read the command block data size in the first byte
    int dataSize;
    int result = _serialPort->read(buffer, 1, timeoutMSec);
    if (result != 0)
    {
        return result;
    }
    dataSize = (buffer[0] & 0x0f);

    // read the rest of the command block
    // TODO: shouldn't the timeout now be a inter symbol timeout?
    result = _serialPort->read(&buffer[1], dataSize + 2, timeoutMSec);
    if (result != 0)
    {
        return result;
    }
    
    // checksum
    int i;
    unsigned char checksum = 0;
    for (i = 0; i < dataSize + 2; i++)
    {
        checksum += buffer[i];
    }
    if (checksum != buffer[dataSize + 2])
    {
        Logging::error("Command block checksum failure\n");
        return -1;
    }
    
    // return ok
    block->setBlock(buffer, dataSize + MIN_COMMAND_BLOCK_SIZE);
    return 0;
}

int SonyProtocol::writeCommandBlock(const CommandBlock* block)
{
    REC_ASSERT(block->validate());
    
    return _serialPort->write(block->getBlock(), block->getBlockSize());
}

void SonyProtocol::flush()
{
    _serialPort->flush(true, true);
}

int SonyProtocol::issueCommand(const CommandBlock* request, CommandBlock* response, string description)
{
    LOCK_SECTION(_issueCommandMutex);
    
    // send request    
    int result = writeCommandBlock(request);
    if (result != 0)
    {
        Logging::error("Failed to issue '%s' request to VTR\n", description.c_str());
        return -1;
    }
    
    // read response
    // TODO: how high should it be set? Can we guarantee delivery with x ms?
    // what are the i/o latencies in linux?
    result = readCommandBlock(response, 100);
    if (result != 0)
    {
        if (result == -2)
        {
            // timed out
            return -2;
        }
        
        Logging::error("Failed to read response to '%s' request\n", description.c_str());
        flush();
        return -1;
    }
    
    return 0;
}

int SonyProtocol::simpleCommand(const CommandBlock* request, string description)
{
    CommandBlock response;
    
    // issue command
    int result = issueCommand(request, &response, description);
    if (result != 0)
    {
        return result;
    }
    
    // check response
    if (!response.isAck())
    {
        if (response.isNAck())
        {
            return 1;
        }
        
        Logging::error("Unexpected response to '%s' request\n", description.c_str());
        return -1;
    }

    // Ack
    return 0;
}

int SonyProtocol::simpleCommand(unsigned char cmd1, unsigned char cmd2, string description)
{
    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(cmd1, cmd2);
    int result = issueCommand(&request, &response, description);
    if (result != 0)
    {
        return result;
    }
    
    // check response
    if (!response.isAck())
    {
        if (response.isNAck())
        {
            return 1;
        }
        
        Logging::error("Unexpected response to '%s' request\n", description.c_str());
        return -1;
    }

    // Ack
    return 0;
}

