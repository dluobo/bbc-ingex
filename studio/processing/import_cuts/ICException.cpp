#include <stdarg.h>

#include "ICException.h"

using namespace std;



ICException::ICException()
{
}

ICException::ICException(const char *format, ...)
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

    mMessage = message;
}


ICException::~ICException() throw()
{
}

const char* ICException::what() const throw()
{
    return mMessage.c_str();
}

