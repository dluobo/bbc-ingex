/*
 * $Id: media_sink_frame.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#ifndef __MEDIA_SINK_FRAME_H__
#define __MEDIA_SINK_FRAME_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "frame_info.h"
#include "on_screen_display.h"



typedef struct
{
    void* data;

    int (*register_stream)(void* data, int streamId, const StreamInfo* streamInfo);

    void (*reset)(void* data);

    int (*accept_stream_frame)(void* data, int streamId, const FrameInfo* frameInfo);
    int (*allocate_stream_buffer)(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer);
    int (*set_is_present)(void* data, int streamId);
    int (*complete_frame)(void* data, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo);

    int (*get_stream_buffer)(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer);

    void (*free)(void* data);
} MediaSinkFrame;



/* utility functions for calling MediaSinkFrame functions */

int msf_register_stream(MediaSinkFrame* sinkFrame, int streamId, const StreamInfo* streamInfo);
void msf_reset(MediaSinkFrame* sinkFrame);
int msf_accept_stream_frame(MediaSinkFrame* sinkFrame, int streamId, const FrameInfo* frameInfo);
int msf_allocate_stream_buffer(MediaSinkFrame* sinkFrame, int streamId, unsigned int bufferSize, unsigned char** buffer);
int msf_set_is_present(MediaSinkFrame* sinkFrame, int streamId);
int msf_complete_frame(MediaSinkFrame* sinkFrame, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo);
void msf_free(MediaSinkFrame* sinkFrame);



#ifdef __cplusplus
}
#endif


#endif


