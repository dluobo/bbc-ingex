#include <string.h>

#include "Encoder.h"


SimpleEncoderOutput::SimpleEncoderOutput()
: _buffer(0), _bufferSize(0), _allocBufferSize(0)
{
}

SimpleEncoderOutput::~SimpleEncoderOutput()
{
    delete [] _buffer;
}

void SimpleEncoderOutput::allocateBuffer(int size, int padding)
{
    if (_allocBufferSize < size + padding)
    {
        delete [] _buffer;
        _buffer = new unsigned char[size + padding];
        memset(_buffer, 0, size + padding);
        _allocBufferSize = size + padding;
    }
    
    _bufferSize = size;
}

unsigned char* SimpleEncoderOutput::getBuffer() const
{
    return _buffer;
}

int SimpleEncoderOutput::getBufferSize() const
{
    return _bufferSize;
}

unsigned char* SimpleEncoderOutput::takeBuffer()
{
    unsigned char* retBuffer = _buffer;
    
    _buffer = 0;
    _bufferSize = 0;
    _allocBufferSize = 0;
    
    return retBuffer;
}

