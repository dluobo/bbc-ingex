#define __STDC_CONSTANT_MACROS 1
extern "C"
{
#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#endif
}

#include "DVEncoder.h"
#include "EncoderException.h"


using namespace std;


#ifdef __MMX__
#include <mmintrin.h>


void uyvy_to_yuv422(int width, int height, const uint8_t *input, uint8_t *output)
{
	__m64 chroma_mask = _mm_set_pi8(255, 0, 255, 0, 255, 0, 255, 0);
	__m64 luma_mask = _mm_set_pi8(0, 255, 0, 255, 0, 255, 0, 255);
	uint8_t *y_comp = output;
	uint8_t *u_comp = output + width * height;
	uint8_t *v_comp = u_comp + (int)((width * height)/2);	// 4:2:2
	int i, j;

    // Fill top with one line of black
    for (i = 0; i < width; i++) {
        *y_comp++ = 0x80;
    }
    for (i = 0; i < width / 2; i++) {
        *u_comp++ = 0x10;
        *v_comp++ = 0x10;
    }
    
	/* Do the y component */
	const uint8_t *tmp = input;
	for (j = 0; j < height - 1; ++j)
	{
		// Consume 16 bytes of UYVY data per iteration (8 pixels worth)
		for (i = 0; i < width*2;  i += 16)
		{
			//__m64 m1 = _mm_and_si64 (*(__m64 *)input, luma_mask);
			//__m64 m2 = _mm_and_si64 (*(__m64 *)(input+8), luma_mask);
			//__m64 m2 = _mm_set_pi8 (0, 0, 0, 0, 0, 0, 0, 0);
			//*(__m64 *)y_comp = _mm_packs_pu16 (m2, m1);
			__m64 m0 = *(__m64 *)input;
			__m64 m2 = _mm_srli_si64(m0, 8);
			__m64 m3 = _mm_slli_si64(m0, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m0 = m2;
			__m64 m1 = *(__m64 *)(input+8);
			m2 = _mm_srli_si64(m1, 8);
			m3 = _mm_slli_si64(m1, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m1 = m2;
			*(__m64 *)y_comp = _mm_packs_pu16 (m0, m1);

			y_comp += 8;
			input += 16;
		}
	}
	/* Do the chroma components */
	input = tmp;
	for (j = 0; j < height - 1; ++j)
	{
		/* Process every line for yuv 4:2:2 */
		for (i = 0; i < width*2;  i += 16)
		{
			__m64 m1 = _mm_unpacklo_pi8 (*(__m64 *)input, *(__m64 *)(input+8));
			__m64 m2 = _mm_unpackhi_pi8 (*(__m64 *)input, *(__m64 *)(input+8));

			__m64 m3 = _mm_unpacklo_pi8 (m1, m2);
			__m64 m4 = _mm_unpackhi_pi8 (m1, m2);
			//*(__m64 *)u_comp = _mm_unpacklo_pi8 (m1, m2);
			//*(__m64 *)v_comp = _mm_unpackhi_pi8 (m1, m2);
			memcpy (u_comp, &m3, 4);
			memcpy (v_comp, &m4, 4);
			u_comp += 4;
			v_comp += 4;
			input += 16;
		}
	}
    
    _mm_empty();        // Clear aliased fp register state
}

void uyvy_to_yuv420(int width, int height, const uint8_t *input, uint8_t *output)
{
	__m64 chroma_mask = _mm_set_pi8(255, 0, 255, 0, 255, 0, 255, 0);
	__m64 luma_mask = _mm_set_pi8(0, 255, 0, 255, 0, 255, 0, 255);
	uint8_t *y_comp = output;
	uint8_t *u_comp = output + width * height;
	uint8_t *v_comp = u_comp + (int)((width * height)/4);	// 4:2:0
	int i, j;

    // Fill top with one line of black
    for (i = 0; i < width; i++) {
        *y_comp++ = 0x80;
    }
    
	/* Do the y component */
	const uint8_t *tmp = input;
	for (j = 0; j < height - 1; ++j)
	{
		// Consume 16 bytes of UYVY data per iteration (8 pixels worth)
		for (i = 0; i < width*2;  i += 16)
		{
			//__m64 m1 = _mm_and_si64 (*(__m64 *)input, luma_mask);
			//__m64 m2 = _mm_and_si64 (*(__m64 *)(input+8), luma_mask);
			//__m64 m2 = _mm_set_pi8 (0, 0, 0, 0, 0, 0, 0, 0);
			//*(__m64 *)y_comp = _mm_packs_pu16 (m2, m1);
			__m64 m0 = *(__m64 *)input;
			__m64 m2 = _mm_srli_si64(m0, 8);
			__m64 m3 = _mm_slli_si64(m0, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m0 = m2;
			__m64 m1 = *(__m64 *)(input+8);
			m2 = _mm_srli_si64(m1, 8);
			m3 = _mm_slli_si64(m1, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m1 = m2;
			*(__m64 *)y_comp = _mm_packs_pu16 (m0, m1);

			y_comp += 8;
			input += 16;
		}
	}
	/* Do the chroma components */
	input = tmp;
	for (j = 0; j < height - 1; ++j)
	{
		/* Skip every odd line to subsample to yuv 4:2:0 */
		if (j %2)
		{
			input += width*2;
			continue;
		}
		for (i = 0; i < width*2;  i += 16)
		{
			__m64 m1 = _mm_unpacklo_pi8 (*(__m64 *)input, *(__m64 *)(input+8));
			__m64 m2 = _mm_unpackhi_pi8 (*(__m64 *)input, *(__m64 *)(input+8));

			__m64 m3 = _mm_unpacklo_pi8 (m1, m2);
			__m64 m4 = _mm_unpackhi_pi8 (m1, m2);
			//*(__m64 *)u_comp = _mm_unpacklo_pi8 (m1, m2);
			//*(__m64 *)v_comp = _mm_unpackhi_pi8 (m1, m2);
			memcpy (u_comp, &m3, 4);
			memcpy (v_comp, &m4, 4);
			u_comp += 4;
			v_comp += 4;
			input += 16;
		}
	}

    _mm_empty();        // Clear aliased fp register state
}


#endif

class DVEncoderInternal
{
public:
    DVEncoderInternal() : encoder(0), frame(0), conversionBuffer(0)
    {}
    
    ~DVEncoderInternal()
    {
        if (frame)
        {
            av_free(frame);
        }
        if (encoder)
        {
            avcodec_close(encoder);
            av_free(encoder);
        }
        delete [] conversionBuffer;
    }
    
    AVCodecContext* encoder;
    AVFrame* frame;
    int frameSize;
    void (*conversion_func)(int width, int height, const uint8_t *input, uint8_t *output);
    unsigned char* conversionBuffer;
};


DVEncoder::DVEncoder(EncoderInputType inputType, DVEncoderOutputType outputType)
{
    _encoderData = new DVEncoderInternal();
    
    // register all ffmpeg codecs
    av_register_all();
    
    // create the ffmpeg dv encoder
    AVCodec* avEncoder = 0;
    ENC_CHECK((avEncoder = avcodec_find_encoder(CODEC_ID_DVVIDEO)) != 0);
    ENC_CHECK((_encoderData->encoder = avcodec_alloc_context()) != 0);
    avcodec_set_dimensions(_encoderData->encoder, 720, 576);
    if (outputType == DV25_ENCODER_OUTPUT)
    {
        _encoderData->frameSize = 144000;
        _encoderData->encoder->pix_fmt = PIX_FMT_YUV420P;
        _encoderData->conversionBuffer = new unsigned char[720 * 576 * 3 / 2];
        _encoderData->conversion_func = uyvy_to_yuv420;
    }
    else
    {
        _encoderData->frameSize = 288000;
        _encoderData->encoder->pix_fmt = PIX_FMT_YUV422P;
        _encoderData->conversionBuffer = new unsigned char[720 * 576 * 2];
        _encoderData->conversion_func = uyvy_to_yuv422;
    }
    ENC_CHECK(avcodec_open(_encoderData->encoder, avEncoder) >= 0); 

    // allocate a ffmpeg frame for the encoder input
    ENC_CHECK((_encoderData->frame = avcodec_alloc_frame()) != 0);
    
}

DVEncoder::~DVEncoder()
{
    delete _encoderData;
}

void DVEncoder::encode(const unsigned char* inData, int inDataSize, EncoderOutput* output)
{
    // convert to yuv422/yuv420 planar and shift down 1 line
    _encoderData->conversion_func(_encoderData->encoder->width, _encoderData->encoder->height, 
        inData, _encoderData->conversionBuffer);
    
    // assign converted input data to the ffmpeg frame    
    ENC_CHECK(avpicture_fill((AVPicture*)_encoderData->frame, 
        _encoderData->conversionBuffer, _encoderData->encoder->pix_fmt, 
        _encoderData->encoder->width, _encoderData->encoder->height) >= 0);
    
    // allocate the output data
    output->allocateBuffer(_encoderData->frameSize, FF_INPUT_BUFFER_PADDING_SIZE);
    
    // encode
    ENC_CHECK(avcodec_encode_video(_encoderData->encoder, output->getBuffer(), 
        output->getBufferSize(), _encoderData->frame) == output->getBufferSize());
}


