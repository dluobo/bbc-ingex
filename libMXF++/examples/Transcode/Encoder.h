#ifndef __ENCODER_H__
#define __ENCODER_H__


typedef enum
{
    UYVY_25I_ENCODER_INPUT
} EncoderInputType;


class EncoderOutput
{
public:
    virtual ~EncoderOutput() {}
    
    virtual void allocateBuffer(int size, int padding) = 0;
    virtual unsigned char* getBuffer() const = 0;
    virtual int getBufferSize() const = 0;
};

class SimpleEncoderOutput : public EncoderOutput
{
public:
    SimpleEncoderOutput();
    virtual ~SimpleEncoderOutput();

    virtual void allocateBuffer(int size, int padding);
    virtual unsigned char* getBuffer() const;
    virtual int getBufferSize() const;

    unsigned char* takeBuffer();
    
private:
    unsigned char* _buffer;
    int _bufferSize;
    int _allocBufferSize;
};


class Encoder
{
public: 
    virtual ~Encoder() {}
    
    virtual void encode(const unsigned char* inData, int inDataSize, EncoderOutput* output) = 0;
};



#endif

