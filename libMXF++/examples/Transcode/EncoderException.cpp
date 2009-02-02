#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

#include "EncoderException.h"

using namespace std;


EncoderException::EncoderException()
{}

EncoderException::EncoderException(const char* format, ...)
{
    char message[512];
    
    va_list varg;
    va_start(varg, format);
#if defined(_MSC_VER)
    _vsnprintf(message, 512, format, varg);
#else
    vsnprintf(message, 512, format, varg);
#endif
    va_end(varg);
    
    _message = message;
}


EncoderException::~EncoderException()
{}

string EncoderException::getMessage() const
{
    return _message;
}

