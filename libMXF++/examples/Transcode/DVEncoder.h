#ifndef __DV_ENCODER_H__
#define __DV_ENCODER_H__


#include "Encoder.h"


typedef enum
{
    DV25_ENCODER_OUTPUT,
    DV50_ENCODER_OUTPUT
} DVEncoderOutputType;

class DVEncoderInternal;

class DVEncoder : public Encoder
{
public:
    DVEncoder(EncoderInputType inputType, DVEncoderOutputType outputType);
    virtual ~DVEncoder();
    
    virtual void encode(const unsigned char* inData, int inDataSize, EncoderOutput* output);
    
private:
    DVEncoderInternal* _encoderData;
};



#endif

