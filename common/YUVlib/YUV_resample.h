#ifndef __YUVLIB_RESAMPLE__
#define __YUVLIB_RESAMPLE__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resample UV to 444.
 */
int to_444(YUV_frame* in_frame, YUV_frame* out_frame,
           void* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_RESAMPLE__
