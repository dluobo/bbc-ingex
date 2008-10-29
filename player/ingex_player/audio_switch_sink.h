/*
 * $Id: audio_switch_sink.h,v 1.2 2008/10/29 17:47:41 john_f Exp $
 *
 *
 *
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

#ifndef __AUDIO_SWITCH_SINK_H__
#define __AUDIO_SWITCH_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"


struct AudioSwitchSink
{
    void* data;
    
    MediaSink* (*get_media_sink)(void* data);
    
    int (*switch_next_audio_group)(void* data);
    int (*switch_prev_audio_group)(void* data);
    int (*switch_audio_group)(void* data, int index);
    void (*snap_audio_to_video)(void* data);
};

/* utility functions for calling AudioSwitchSink functions */

MediaSink* asw_get_media_sink(AudioSwitchSink* swtch);
int asw_switch_next_audio_group(AudioSwitchSink* swtch);
int asw_switch_prev_audio_group(AudioSwitchSink* swtch);
int asw_switch_audio_group(AudioSwitchSink* swtch, int index);
void asw_snap_audio_to_video(AudioSwitchSink* swtch);


/* audio switch */

int qas_create_audio_switch(MediaSink* sink, AudioSwitchSink** swtch);




#ifdef __cplusplus
}
#endif


#endif


