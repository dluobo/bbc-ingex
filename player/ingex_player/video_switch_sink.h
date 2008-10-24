#ifndef __VIDEO_SWITCH_SINK_H__
#define __VIDEO_SWITCH_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"
#include "video_switch_database.h"


struct VideoSwitchSink
{
    void* data;
    
    MediaSink* (*get_media_sink)(void* data);
    
    int (*switch_next_video)(void* data);
    int (*switch_prev_video)(void* data);
    int (*switch_video)(void* data, int index);
    void (*show_source_name)(void* data, int enable);
    void (*toggle_show_source_name)(void* data);
    int (*get_video_index)(void* data, int imageWidth, int imageHeight, int xPos, int yPos, int* index);
    int (*get_first_active_clip_id)(void* data, char* clipId, int* sourceId);
};

/* utility functions for calling VideoSwitchSink functions */

MediaSink* vsw_get_media_sink(VideoSwitchSink* swtch);
int vsw_switch_next_video(VideoSwitchSink* swtch);
int vsw_switch_prev_video(VideoSwitchSink* swtch);
int vsw_switch_video(VideoSwitchSink* swtch, int index);
void vsw_show_source_name(VideoSwitchSink* swtch, int enable);
void vsw_toggle_show_source_name(VideoSwitchSink* swtch);
int vsw_get_video_index(VideoSwitchSink* swtch, int imageWidth, int imageHeight, int xPos, int yPos, int* index);
int vsw_get_first_active_clip_id(VideoSwitchSink* swtch, char* clipId, int* sourceId);


/* video switch */

typedef enum
{
    NO_SPLIT_VIDEO_SWITCH = 1,
    QUAD_SPLIT_VIDEO_SWITCH = 4,
    NONA_SPLIT_VIDEO_SWITCH = 9
} VideoSwitchSplit;

int qvs_create_video_switch(MediaSink* sink, VideoSwitchSplit split, int applySplitFilter, int splitSelect, int prescaledSplit,
    VideoSwitchDatabase* database, int masterTimecodeIndex, int masterTimecodeType, int masterTimecodeSubType,
    VideoSwitchSink** swtch);




#ifdef __cplusplus
}
#endif


#endif


