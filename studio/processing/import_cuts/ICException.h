#ifndef __IC_EXCEPTION_H__
#define __IC_EXCEPTION_H__


#include <string>
#include <exception>


class ICException : public std::exception
{
public:
    ICException();
    ICException(const char *format, ...);
    virtual ~ICException() throw();

    const char* what() const throw();

protected:
    std::string mMessage;
};



#endif

