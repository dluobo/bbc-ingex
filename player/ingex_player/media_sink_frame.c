/*
 * $Id: media_sink_frame.c,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "media_sink_frame.h"


int msf_register_stream(MediaSinkFrame* sinkFrame, int streamId, const StreamInfo* streamInfo)
{
    if (sinkFrame && sinkFrame->register_stream)
    {
        return sinkFrame->register_stream(sinkFrame->data, streamId, streamInfo);
    }
    return 0;
}

void msf_reset(MediaSinkFrame* sinkFrame)
{
    if (sinkFrame && sinkFrame->reset)
    {
        sinkFrame->reset(sinkFrame->data);
    }
}

int msf_accept_stream_frame(MediaSinkFrame* sinkFrame, int streamId, const FrameInfo* frameInfo)
{
    if (sinkFrame && sinkFrame->accept_stream_frame)
    {
        return sinkFrame->accept_stream_frame(sinkFrame->data, streamId, frameInfo);
    }
    return 0;
}

int msf_allocate_stream_buffer(MediaSinkFrame* sinkFrame, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    if (sinkFrame && sinkFrame->allocate_stream_buffer)
    {
        return sinkFrame->allocate_stream_buffer(sinkFrame->data, streamId, bufferSize, buffer);
    }
    return 0;
}

int msf_set_is_present(MediaSinkFrame* sinkFrame, int streamId)
{
    if (sinkFrame && sinkFrame->set_is_present)
    {
        return sinkFrame->set_is_present(sinkFrame->data, streamId);
    }
    return 0;
}

int msf_complete_frame(MediaSinkFrame* sinkFrame, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo)
{
    if (sinkFrame && sinkFrame->complete_frame)
    {
        return sinkFrame->complete_frame(sinkFrame->data, osdState, frameInfo);
    }
    return 0;
}

void msf_free(MediaSinkFrame* sinkFrame)
{
    if (sinkFrame && sinkFrame->free)
    {
        sinkFrame->free(sinkFrame->data);
    }
}


