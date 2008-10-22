#ifndef __FFMPEG_SOURCE_H__
#define __FFMPEG_SOURCE_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


int fms_open(const char* filename, int threadCount, MediaSource** source);


#ifdef __cplusplus
}
#endif


#endif
