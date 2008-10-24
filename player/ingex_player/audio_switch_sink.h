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


