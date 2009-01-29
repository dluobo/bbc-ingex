// EasyReader.cpp

#include <ace/Thread.h>
#include <ace/Guard_T.h>
#include <ace/Time_Value.h>
//#include <ace/OS_NS_unistd.h>

#include "EasyReader.h"
#include "SerialPort.h"
#include "TcpPort.h"

ACE_THR_FUNC_RETURN read_data(void * p)
{
    EasyReader * p_reader = (EasyReader *) p;
    const ACE_Time_Value READ_INTERVAL(0, 50000);

    while (p_reader->mActivated)
    {
        p_reader->ReadData();
        ACE_OS::sleep(READ_INTERVAL);
    }

    return 0;
}

/**
Constructor
*/
EasyReader::EasyReader()
: mpCommunicationPort(0), mActivated(false), mThreadId(0)
{
}

/**
Destructor
*/
EasyReader::~EasyReader()
{
    this->Stop();
    delete mpCommunicationPort;
}

/**
Open serial port and send set-up string to EasyReader
*/
bool EasyReader::Init(const std::string & port, Transport::EnumType transport)
{
    // NB. Should allow for re-init
    bool ok = true;

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

    if (mpCommunicationPort->Connect(port))
    {
        SerialPort * p_serial = dynamic_cast<SerialPort *> (mpCommunicationPort);
        if (p_serial)
        {
            p_serial->BaudRate(19200);
        }

    // Initialise EasyReader - select LTC and VITC
        mpCommunicationPort->Send ((const void *)"L1V1U0", 6);
    }

    // Initialise local data structures
    mTimecode = "00:00:00:00";
    mWritePtr = &mBuffer[0];

    // Start thread to read incoming data
    mActivated = true;
    ACE_Thread::spawn( read_data, this, THR_NEW_LWP|THR_JOINABLE, &mThreadId );

    return ok;
}

void EasyReader::Stop()
{
    mActivated = false;
    if (mThreadId)
    {
        ACE_Thread::join(mThreadId, 0, 0);
        mThreadId = 0;
    }
}

/**
Return the current timecode.
VITC is used if available, otherwise LTC
*/
std::string EasyReader::Timecode()
{
    ReadData();

    ACE_Guard<ACE_Thread_Mutex> guard(mBufferMutex);

    char * read_ptr = mWritePtr - 1;
    while (*read_ptr != '\r' && read_ptr > mBuffer + 18)
    {
        --read_ptr;
    }
    if (*read_ptr == '\r')
    {
        --read_ptr;
        if (*read_ptr == 'E' || *read_ptr == 'e')
        {
            // no VITC, try LTC instead
            read_ptr -= 9;
        }

        if (*read_ptr == 'E' || *read_ptr == 'e')
        {
            mTimecode = "00:00:00:00";
        }
        else
        {
            read_ptr -= 8; // point to start of timecode string
            mTimecode = "";
            mTimecode += *(read_ptr++);
            mTimecode += *(read_ptr++);
            mTimecode += ':';
            mTimecode += *(read_ptr++);
            mTimecode += *(read_ptr++);
            mTimecode += ':';
            mTimecode += *(read_ptr++);
            mTimecode += *(read_ptr++);
            mTimecode += ':';
            mTimecode += *(read_ptr++);
            mTimecode += *(read_ptr++);
        }
    }

    return mTimecode;
}

/**
Private method to read available data from serial port into a buffer.
*/
void EasyReader::ReadData()
{
    const ACE_Time_Value READ_TIMEOUT(0, 10000);

    int bytes_read;
    ACE_Guard<ACE_Thread_Mutex> guard(mBufferMutex);
    do
    {
        // shift data down in the buffer if getting near the end
        if(mBuffer + bufsize < mWritePtr + chunksize)
        {
            ACE_OS::memmove(mBuffer, mWritePtr - chunksize, chunksize);
            mWritePtr = mBuffer + chunksize;
        }

        // read available data into buffer
        bytes_read = mpCommunicationPort->Recv(mWritePtr, chunksize, &READ_TIMEOUT);
        if (bytes_read > 0)
        {
            mWritePtr += bytes_read;
        }
    }
    while (bytes_read == chunksize);
}


