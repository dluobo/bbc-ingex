/*
 * $Id: quartzRouter.cpp,v 1.6 2009/01/29 07:36:59 stuart_hc Exp $
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

#include "quartzRouter.h"
#include "RouterObserver.h"
#include "SerialPort.h"
#include "TcpPort.h"

#include <ace/OS_NS_unistd.h>
#include <string>
#include <sstream>
#include <list>


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
: mpCommunicationPort(0), mConnected(false), mRun(false)
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
bool Router::Init(const std::string & port, Transport::EnumType transport)
{
    bool result = false;
    if (mpCommunicationPort)
    {
        delete mpCommunicationPort;
        mpCommunicationPort = 0;
    }
    if (transport == Transport::TCP)
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

    if (mpCommunicationPort->Connect(port))
    {
        SerialPort * p_serial = dynamic_cast<SerialPort *> (mpCommunicationPort);
        if (p_serial)
        {
            p_serial->BaudRate(38400);
        }

        // Flush any input
        ACE_Time_Value tm(0, 200000);
        const int buf_size = 16;
        char buf[buf_size];
        ssize_t bytes_read = 0;
        do
        {
            bytes_read = mpCommunicationPort->Recv ((void *) &buf[0], buf_size, &tm);
        }
        while (bytes_read == buf_size);

        // Test for presence of router
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

    // Initialise buffer pointers
    mWritePtr = &mBuffer[0];
    mBufferEnd = mBuffer + qbufsize;

    mConnected = result;
    return result;
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
        bytes_read = mpCommunicationPort->Recv ((void *) &charReceived, 1, &READ_TIMEOUT);

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

// read router reply from ".01#(cr)" (is router connected?) command. Should be ".A(cr)"
bool Router::readReply()
{
    char charReceived;
    ssize_t bytes_read;
    char * rd_ptr;
        
    
    std::string RouterString = "";
    char tmp[qbufsize];
    char * buf = &tmp[0];
    char * wr_ptr = buf;
    char * buf_end = buf + qbufsize;


    // expect quartz router reply as .A\r
    bool run = 1; // set running flag
    while (run)
    {
        // timeout
        //ACE_Time_Value tm(0.2);
        ACE_Time_Value tm(0, 200000);
        bytes_read = mpCommunicationPort->Recv ((void *) &charReceived, 1, &tm);

        if (bytes_read == 1)
        {
            if (charReceived == '\r')
            {
                run = 0;
                // convert the bytes received so far into a router command string.
                // First, see if we have received enough bytes to decode

                // find the dot . at the start of the message
                rd_ptr = wr_ptr - 1; // last char received
                while (rd_ptr >= buf && *rd_ptr != '.')
                {
                    rd_ptr--;
                }

                RouterString = "";                  
                if (rd_ptr >= buf)
                {
                    // still in buffer, so . found
                    rd_ptr++; // skip over .
                    // next char should be an A
                    while (rd_ptr < wr_ptr)
                    {
                        RouterString += *(rd_ptr++); // copy update command string
                        //run = 0; // exit
                    }
                }

                // reset write pointer
                wr_ptr = &buf[0];
                //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("routerstring: %s\n"), RouterString.c_str() ));
            }
            else
            {
                // save this byte in the buffer
                // is buffer full?
                if (wr_ptr >= buf_end)
                {
                    wr_ptr = buf; // just start at beginning of buffer
                }

                *wr_ptr = charReceived;
                wr_ptr++;
            }
        } // end of bytes read OK
        else
        {
            run = 0; // exit loop
        }
    } // end of while (run)

    //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("found %s\n"), mBuf ));
    if (::strncmp(buf, ".A", 2) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}


// Query current source connected to router.
// Expect observer to read reply.
// (sends interrogate route command, ".I{level}{dest}(cr)" )
void Router::QuerySrc(unsigned int dest)
{
    //find current source connected to router
    std::ostringstream ss;
    ss << ".IV" << dest << "\r";
    const std::string & command = ss.str();
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Sending %C\n"), command.c_str() ));
    mpCommunicationPort->Send (command.c_str(), command.length());
}

void Router::ProcessMessage(const std::string & message)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Message from router: \"%C\"\n"), message.c_str()));

    unsigned int dest = ACE_OS::atoi(message.substr(2, 3).c_str());
    unsigned int src  = ACE_OS::atoi(message.substr(6).c_str());

    for (std::vector<RouterObserver *>::iterator
        it = mObservers.begin(); it != mObservers.end(); ++it)
    {
        (*it)->Observe(src, dest);
    }
}
