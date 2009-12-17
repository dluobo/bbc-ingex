#ifndef __VIDEO_CONVERSION_10BITS_H__
#define __VIDEO_CONVERSION_10BITS_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This routine converts a frame of 10-bit YUV "v210" format data
 * to 8-bit UYVY format. It uses error feedback to minimise visibility
 * of any quantising noise.
 *
 * See http://developer.apple.com/quicktime/icefloe/dispatch019.html#v210
 * for a definition of v210 format.
 * See http://www.fourcc.org/yuv.php#UYVY for a definition of UYVY.
 *
 * pInFrame and pOutFrame are pointers to the first sample of the input
 * and output frame buffers.
 * StrideIn and StrideOut are the width of the input and output buffers,
 * i.e. how many *bytes* separate the start of two adjacent lines.
 * xLen and yLen are the image dimensions in pixels and lines.
 */
void DitherFrame(uint8_t *pOutFrame, uint8_t *pInFrame,
				 const int StrideOut, const int StrideIn,
				 const int xLen, const int yLen);

/*
 * This does the same as above, but with no error feedback to mask
 * quantisation error.
 */
void ConvertFrame10to8(uint8_t *pOutFrame, uint8_t *pInFrame,
                       const int StrideOut, const int StrideIn,
                       const int xLen, const int yLen);

/*
 * This routine converts a frame of 8-bit UYVY data to 10-bit v210 format.
 * Zeros are inserted as required. Converting an 8-bit frame to 10-bit and back
 * should have no overall effect.
*/
void ConvertFrame8to10(uint8_t *pOutFrame, uint8_t *pInFrame,
                       const int StrideOut, const int StrideIn,
                       const int xLen, const int yLen);

#ifdef __cplusplus
}
#endif

#endif // __VIDEO_CONVERSION_10BITS_H__
