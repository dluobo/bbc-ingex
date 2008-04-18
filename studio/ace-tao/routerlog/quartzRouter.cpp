/*
 * $Id: quartzRouter.cpp,v 1.2 2008/04/18 16:56:38 john_f Exp $
 *
 * Class to handle communication with Quartz router.
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

#include <ace/OS_NS_unistd.h>
#include <string>
#include <sstream>
#include <list>

#include "quartzRouter.h"
#include "Observer.h"
#include "SerialPort.h"
#include "TcpPort.h"

#define NEW_SERIAL 1

int Router::svc ()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT ("(%t) router handler running\n"))); 

    readUpdate();

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Exiting router thread\n") )); 

    return 0;
}

/**
Default Constructor
*/
Router::Router()
: mpObserver(0), mpCommunicationPort(0), mConnected(false), mRun(false)
{
}

/**
Destructor
*/
Router::~Router()
{
    delete mpCommunicationPort;
}

/**
Open serial port and send set-up string to router
*/
bool Router::Init(const std::string & port, bool router_tcp)
{
    bool result = false;
    if (mpCommunicationPort)
    {
        delete mpCommunicationPort;
        mpCommunicationPort = 0;
    }
    if (router_tcp)
    {
        mpCommunicationPort = new TcpPort;
    }
    else
    {
        mpCommunicationPort = new SerialPort;
    }

#if 0  // No longer attempting to discover router
    std::list<std::string> port_list;

    if (port.empty())
    {
        // Find available ports
        for (int i = 0; i < 8; i++) // how many should we look for
        {
            char trial_port[20];
            sprintf(trial_port, "COM%d", i);
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("trying %C"), trial_port ));

            //ACE_Time_Value tm(0.2);
            ACE_Time_Value tm(0, 200000);
            ACE_DEV_Connector test_connector;
            ACE_TTY_IO device;
            int ret = test_connector.connect(device, ACE_DEV_Addr(ACE_TEXT(trial_port)), &tm);
            if (0 == ret)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT (" - port exists\n") ));
                port_list.push_back(trial_port);
            }
            else
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT (" - no such port\n") ));
            }
            device.close();
        }
    }
    else
    {
        // Use the supplied port name
        port_list.push_back(port);
    }
#endif

#if NEW_SERIAL
    if (mpCommunicationPort->Connect(port))
    {
        SerialPort * p_serial = dynamic_cast<SerialPort *> (mpCommunicationPort);
        if (p_serial)
        {
            p_serial->BaudRate(38400);
        }

        mpCommunicationPort->Send ((const void *)".#01\r", 5);
        bool tst = readReply();
        if (tst)
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Ack from router!\n") ));
            result = true;
        } 
        else
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("No router connected!\n") ));
        }
    }
    else
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Could not open %C.\n"), port.c_str() ));
    }
#else

    //Now search for connected router
    ACE_TTY_IO::Serial_Params myparams;
    myparams.baudrate = 38400; /* 38400 for router */
    myparams.parityenb = false;
    myparams.databits = 8;
    myparams.stopbits = 1;
    myparams.readtimeoutmsec = 100; //-1;
    myparams.ctsenb = 0;
    myparams.rcvenb = true;

    m_routerConnected = false;
    for (std::list<std::string>::const_iterator it = port_list.begin();
        !m_routerConnected && it != port_list.end(); ++it)
    {
        if (mDeviceConnector.connect(mSerialDevice, ACE_DEV_Addr(ACE_TEXT(it->c_str()))) == -1)
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Could not open %C.\n"), it->c_str() ));
        }
        else
        {
            //check for router
            mSerialDevice.control(ACE_TTY_IO::SETPARAMS, &myparams);

            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Checking for router on %C: Sending .#01\n"), it->c_str() ));
            mSerialDevice.send ((const void *)".#01\r", 5);
            bool tst = readReply();
            if (tst)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Ack from router!\n") ));
                m_routerConnected = true;
            } 
            else
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("No router connected!\n") ));
            }
        }
    }
#endif

    mWritePtr = &mBuffer[0];
    mBufferEnd = mBuffer + qbufsize;

    mConnected = result;
    return result;
}


char Router::readByte()
{
    char charReceived;
    ssize_t bytes_read;
    //char * readPtr;
    bool carryOn = true;
    
    while (carryOn)
    {
        bytes_read = mSerialDevice.recv ((void *) &charReceived, 1);
        if (bytes_read == 1)
        {
            carryOn = false;
        }
    }
    return charReceived;
}

/**
Private method to read available data from serial port into a buffer.
*/
void Router::readUpdate()
{
    char charReceived;
    ssize_t bytes_read;
    char * readPtr;
    //bool carryOn = 1;

    // expect quartz router command as .U{level}ddd,sss\r

    const ACE_Time_Value READ_INTERVAL(0, 10000);
    const ACE_Time_Value READ_TIMEOUT(0, 10000);

    mRun = true; // set running flag
    while (mRun)
    {
#if NEW_SERIAL
        bytes_read = mpCommunicationPort->Recv ((void *) &charReceived, 1, &READ_TIMEOUT);
#else
        bytes_read = mSerialDevice.recv ((void *) &charReceived, 1);
#endif


        if (bytes_read == 1)
        {
            //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("read: %c\n"), charReceived));
            if (charReceived == '\r')
            {
                // convert the bytes received so far into a router command string.
                // First, see if we have received enough bytes to decode

                // find the dot . at the start of the message
                readPtr = mWritePtr - 1; // last char received
                while (readPtr >= mBuffer && *readPtr != '.')
                {
                    readPtr--;
                }

                std::string router_update_string;
                if (readPtr >= mBuffer)
                { // still in buffer, so . found
                    // get timeocde at this moment
                    //timecode = (*tcReader).getTimecode();
                    readPtr++; // skip over .
                    // next char should be a U
    // TO DO: DGK decode U string digits
                    while (readPtr < mWritePtr)
                    {
                        // find destination port number from router update string
                        // .U{level}{dest},{source}\r
                        // {level} is one or more alpha chars, so find first digit in string

                        router_update_string += *(readPtr++); // copy update command string
                    }
                }

                // reset write pointer
                mWritePtr = mBuffer;

                ProcessMessage(router_update_string);
            }
            else
            {
                // save this byte in the buffer
                // is buffer full?
                if (mWritePtr >= mBufferEnd)
                {
                    mWritePtr = mBuffer; // just start at beginning of buffer
                }

                *mWritePtr = charReceived;
                mWritePtr++;

                if (charReceived == 'q')
                {
                    mRun = false;
                }
            }
        } // end of bytes read OK

        ACE_OS::sleep(READ_INTERVAL); // Don't use all the CPU time.
    } //end of while (mRun)
}


void Router::Stop()
{
    mRun = false;
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Stopping router thread\n") )); 
}

#if 0
void Router::setSource(int source)
{
    char command[20];
    SYSTEMTIME st1, st2;

    sprintf(command, ".SV016,%03.3d\r", source);
    //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Sending %s\n"), command ));
    GetSystemTime(&st1);
    mSerialDevice.send (command, 11);
    GetSystemTime(&st2);

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("RC at %ds %d\n"), st1.wSecond, st1.wMilliseconds));
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Send finished at %ds %d\n"), st2.wSecond, st2.wMilliseconds));

}
#endif

// read router reply from ".01#(cr)" (is router connected?) command. Should be ".A(cr)"
bool Router::readReply()
{
    char charReceived;
    ssize_t bytes_read;
    char * readPtr;
        
    
    std::string RouterString = "";
    char * mWPtr, *mBufEnd;
    char mBuf[qbufsize];
    mWPtr = &mBuf[0];
    mBufEnd = mBuf + qbufsize;


    // expect quartz router reply as .A\r
    bool run = 1; // set running flag
    while (run)
    {
        // timeout
        //ACE_Time_Value tm(0.2);
        ACE_Time_Value tm(0, 200000);
#if NEW_SERIAL
        bytes_read = mpCommunicationPort->Recv ((void *) &charReceived, 1, &tm);
#else
        bytes_read = mSerialDevice.recv_n ((void *) &charReceived, 1, &tm);
#endif

        if (bytes_read == 1)
        {
            if (charReceived == '\r')
            {
                run = 0;
                // convert the bytes received so far into a router command string.
                // First, see if we have received enough bytes to decode

                // find the dot . at the start of the message
                readPtr = mWPtr - 1; // last char received
                while (readPtr >= mBuf && *readPtr != '.')
                {
                    readPtr--;
                }

                RouterString = "";                  
                if (readPtr >= mBuf)
                {
                    // still in buffer, so . found
                    readPtr++; // skip over .
                    // next char should be an A
                    while (readPtr < mWPtr)
                    {
                        RouterString += *(readPtr++); // copy update command string
                        //run = 0; // exit
                    }
                }

                // reset write pointer
                mWPtr = mBuf;
                //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("routerstring: %s\n"), RouterString.c_str() ));
            }
            else
            {
                // save this byte in the buffer
                // is buffer full?
                if (mWPtr >= mBufEnd)
                {
                    mWPtr = mBuf; // just start at beginning of buffer
                }

                *mWPtr = charReceived;
                mWPtr++;
            }
        } // end of bytes read OK
        else
        {
            run =0;
        }
    } // end of while (run)

    //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("found %s\n"), mBuf ));
    if (::strncmp (mBuf, ".A", 2) == 0)
    return true;
    else
    return false;
}


// Query current source connected to router.
// Expect observer to read reply.
// (reply from interrogate route command, ".I{level}{dest}(cr)" )
void Router::QuerySrc(unsigned int dest)
{
    //find current source connected to router
    std::ostringstream ss;
    ss << ".IV" << dest << "\r";
    const std::string & command = ss.str();
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Sending %C\n"), command.c_str() ));
#if NEW_SERIAL
    mpCommunicationPort->Send (command.c_str(), command.length());
#else
    mSerialDevice.send (command.c_str(), command.length());
#endif

#if 0
    char charReceived;
    ssize_t bytes_read;
    char * readPtr;
        
    
    std::string router_response;
    char * mWPtr, *mBufEnd;
    char mBuf[qbufsize];
    mWPtr = &mBuf[0];
    mBufEnd = mBuf + qbufsize;

    // expect quartz router reply as .I{level}{dest},{srce}\r
    bool done = false;
    int read_attempts = 0;
    while (!done)
    {
        //ACE_Time_Value tm(0.2);
        ACE_Time_Value tm(0, 200000);
        bytes_read = mSerialDevice.recv_n ((void *) &charReceived, 1, &tm);
        //bytes_read = mSerialDevice.recv ((void *) &charReceived, 1);

        if (bytes_read == 1)
        {
            if (charReceived == '\r')
            {
                // convert the bytes received so far into a router command string.
                // First, see if we have received enough bytes to decode

                // find the dot . at the start of the message
                readPtr = mWPtr - 1; // last char received
                while (readPtr >= mBuf && *readPtr != '.')
                {
                    readPtr--;
                }

                router_response = "";                  
                if (readPtr >= mBuf)
                {
                    // still in buffer, so . found
                    readPtr++; // skip over .
                    // next char should be an I
                    while (readPtr < mWPtr)
                    {
                        router_response += *(readPtr++); // copy update command string
                    }
                }
                else
                {
                    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Empty string from router.\n")));
                }

                // reset write pointer
                mWPtr = mBuf;
                done = true;
            }
            else
            {
                // save this byte in the buffer
                // is buffer full?
                if (mWPtr >= mBufEnd)
                {
                    mWPtr = mBuf; // just start at beginning of buffer
                }

                *mWPtr = charReceived;
                mWPtr++;
            }
        } // end of bytes read OK
        else
        {
            if (++read_attempts > 2)
            {
                done = true;
            }
            else
            {
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Re-trying read from router.\n")));
            }
        }
    } // end of while loop

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("router reply [.{level}{destination},{source}]: %C\n"), router_response.c_str() ));
    //m_CurSrc = router_response.c_str();

    unsigned int src = 0;
    if (router_response.size() > 6)
    {
        src = ACE_OS::atoi(router_response.substr(6).c_str());
    }

    return src;
#endif
}

void Router::ProcessMessage(const std::string & message)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Message from router: \"%C\"\n"), message.c_str()));

    if (mpObserver)
    {
        mpObserver->Observe(message);
    }
}
