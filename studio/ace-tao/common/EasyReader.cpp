// EasyReader.cpp

#include <ace/Thread.h>
#include <ace/Guard_T.h>
#include <ace/Time_Value.h>
#include <ace/OS_NS_unistd.h>

#include "EasyReader.h"

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
: mActivated(false), mThreadId(0)
{
}

/**
Destructor
*/
EasyReader::~EasyReader()
{
	//mSerialPort.Close();  // SerialPort destructor will do this
}

/**
Open serial port and send set-up string to EasyReader
*/
bool EasyReader::Init(const std::string & port)
{
    // NB. Should allow for re-init
    bool ok = true;

    // Open the serial port
    if (-1 == mDeviceConnector.connect(mSerialDevice, ACE_DEV_Addr(ACE_TEXT(port.c_str()))))
    {
        ok = false;
    }

    // Set baudrate etc.
	ACE_TTY_IO::Serial_Params myparams;
	myparams.baudrate = 19200;
#if ACE_MAJOR_VERSION >= 5 && ACE_MINOR_VERSION >= 6 && ACE_BETA_VERSION >= 6
	myparams.paritymode = "none";
#else
	myparams.parityenb = false;
#endif
	myparams.databits = 8;
	myparams.stopbits = 1;
	myparams.readtimeoutmsec = 100;
	myparams.ctsenb = 0;
	myparams.rcvenb = true;
	mSerialDevice.control(ACE_TTY_IO::SETPARAMS, &myparams);

    // Initialise EasyReader
    mSerialDevice.send((const void *)"L1V1U0", 6);  // select LTC and VITC

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
It's a good idea to call this regularly so that data coming in to the
serial port doesn't pile up.
*/
//void EasyReader::OnTimer()
//{
	//ReadData();
//}

/**
Return the current timecode.
VITC is used if available, otherwise LTC
*/
std::string EasyReader::Timecode()
{
	ReadData();

    ACE_Guard<ACE_Thread_Mutex> guard(mBufferMutex);

	char * read_ptr = mWritePtr - 1;
	while(*read_ptr != '\r' && read_ptr > mBuffer + 18)
	{
		--read_ptr;
	}
	if(*read_ptr == '\r')
	{
		--read_ptr;
		if(*read_ptr == 'E' || *read_ptr == 'e')
		{
			// no VITC, try LTC instead
			read_ptr -= 9;
		}

		if(*read_ptr == 'E' || *read_ptr == 'e')
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
	int bytes_read;
    ACE_Guard<ACE_Thread_Mutex> guard(mBufferMutex);
	do
	{
		// shift data down in the buffer if getting near the end
		if(mBuffer + bufsize < mWritePtr + chunksize)
		{
			memmove(mBuffer, mWritePtr - chunksize, chunksize);
			mWritePtr = mBuffer + chunksize;
		}

		// read available data into buffer
		//bytes_read = mSerialPort.Read(mWritePtr, chunksize);
		bytes_read = mSerialDevice.recv(mWritePtr, chunksize);
        if (bytes_read > 0)
        {
		    mWritePtr += bytes_read;
        }
	}
	while (bytes_read == chunksize);
}


