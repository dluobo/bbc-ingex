/*
 * $Id: SonyProtocol.h,v 1.1 2008/07/08 16:27:17 philipn Exp $
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
 
#ifndef __RECORDER_SONY_PROTOCOL_H__
#define __RECORDER_SONY_PROTOCOL_H__


#include "SerialPort.h"
#include "Threads.h"
#include "ByteArray.h"


// command block = CMD1|DataSize, CMD2, (DATA){0..15}, Checksum
#define MIN_COMMAND_BLOCK_SIZE          3
#define MAX_COMMAND_BLOCK_SIZE          18



namespace rec
{


class CommandBlock
{
public:
    CommandBlock();
    ~CommandBlock();

    // setters for the command block    
    void setCMD1(unsigned char cmd1);
    void setCMD2(unsigned char cmd2);
    void setCMDs(unsigned char cmd1, unsigned char cmd2);
    void setData(const unsigned char* data, int dataCount);
    void setAck();
    void setNAck();
    void reset();
    
    // getters for the command block
    unsigned char getCMD1() const;
    int getDataCount() const;
    unsigned char getCMD2() const;
    const unsigned char* getData() const;
    bool checkCMDs(unsigned char cmd1, unsigned char cmd2);
    bool isAck() const;
    bool isNAck() const;
    
    // set and get command block in one go
    void setBlock(unsigned char* block, int blockSize);
    const unsigned char* getBlock() const;
    int getBlockSize() const;
    
    // validate the command block
    bool validate() const;
    
    
private:
    void setChecksum();

    unsigned char _block[MAX_COMMAND_BLOCK_SIZE];
    int _blockSize;
};
    

class SonyProtocol
{
public:
    static SonyProtocol* open(std::string serialDeviceName);
    
public:
    ~SonyProtocol();
    
    // read and write a command block
    int readCommandBlock(CommandBlock* block, long timeoutMSec);
    int writeCommandBlock(const CommandBlock* block);
    
    // issue a request and return a response
    int issueCommand(const CommandBlock* request, CommandBlock* response, std::string description);
    int simpleCommand(const CommandBlock* request, std::string description);
    int simpleCommand(unsigned char cmd1, unsigned char cmd2, std::string description);

    void flush();
    
private:
    SonyProtocol(SerialPort* serialPort);
    
    SerialPort* _serialPort;
    
    Mutex _issueCommandMutex;
};





};





#endif



