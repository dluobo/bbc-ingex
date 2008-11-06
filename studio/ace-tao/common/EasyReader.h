// EasyReader.h

#ifndef EasyReader_h
#define EasyReader_h

#include "CommunicationPort.h"

//#include <ace/DEV_Addr.h>
//#include <ace/DEV_Connector.h>
//#include <ace/DEV_IO.h>
//#include <ace/TTY_IO.h>
#include <ace/Thread_Mutex.h>
#include <string>

#include "TimecodeReader.h"

const int bufsize = 1024;
const int chunksize = 64;

/**
Interface to external timecode reader
*/
class EasyReader : public TimecodeReader
{
public:
// methods
	EasyReader();
	~EasyReader();
    bool Init(const std::string & port, Transport::EnumType transport);
    void Stop();
    std::string Timecode();

private:
// methods
	void ReadData();
// members
	//SerialPort mSerialPort;
	//ACE_TTY_IO mSerialDevice;
	//ACE_DEV_Connector mDeviceConnector;
    CommunicationPort * mpCommunicationPort;

	char mBuffer[bufsize];
    ACE_Thread_Mutex mBufferMutex;
	std::string mTimecode;
	char * mWritePtr;

    bool mActivated;
    ACE_thread_t mThreadId;

// friends
    friend ACE_THR_FUNC_RETURN read_data(void * p);
};

#endif //#ifndef EasyReader_h

