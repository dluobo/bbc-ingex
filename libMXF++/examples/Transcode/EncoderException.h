#ifndef __ENCODER_EXCEPTION_H__
#define __ENCODER_EXCEPTION_H__


#include <string>


#define ENC_CHECK(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
        throw EncoderException("'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
    }

#define ENC_ASSERT(cmd) \
    ENC_CHECK(cmd);


class EncoderException
{
public:
    EncoderException();
    EncoderException(const char* format, ...);
    virtual ~EncoderException();
    
    std::string getMessage() const;
    
protected:
    std::string _message;

};




#endif

