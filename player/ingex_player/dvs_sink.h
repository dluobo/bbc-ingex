#ifndef __DVS_SINK_H__
#define __DVS_SINK_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"


/* minimum number of DVS fifo buffers (must be > 4) */
#define MIN_NUM_DVS_FIFO_BUFFERS        8


typedef enum
{
    VITC_AS_SDI_VITC = 1,   /* allow 0 to be used in int type to indicate value is null */
    LTC_AS_SDI_VITC,
    COUNT_AS_SDI_VITC
} SDIVITCSource;


typedef struct DVSSink DVSSink;



int dvs_card_is_available();

int dvs_open(int dvsCard, int dvsChannel, SDIVITCSource sdiVITCSource, int extraSDIVITCSource, 
    int numBuffers, int disableOSD, int fitVideo, DVSSink** sink);

MediaSink* dvs_get_media_sink(DVSSink* sink);

/* only closes the DVS card - use when handling interrupt signals */ 
void dvs_close_card(DVSSink* sink);


#ifdef __cplusplus
}
#endif


#endif


