/*
 * $Id: dvs_sink.h,v 1.5 2008/10/29 17:47:41 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham, <stuart_hc@users.sourceforge.net>
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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



int dvs_card_is_available(int card, int channel);

int dvs_open(int dvsCard, int dvsChannel, SDIVITCSource sdiVITCSource, int extraSDIVITCSource, 
    int numBuffers, int disableOSD, int fitVideo, DVSSink** sink);

MediaSink* dvs_get_media_sink(DVSSink* sink);

/* only closes the DVS card - use when handling interrupt signals */ 
void dvs_close_card(DVSSink* sink);


#ifdef __cplusplus
}
#endif


#endif


