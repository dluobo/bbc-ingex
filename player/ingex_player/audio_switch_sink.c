/*
 * $Id: audio_switch_sink.c,v 1.3 2008/11/06 11:30:09 john_f Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "audio_switch_sink.h"
#include "video_switch_sink.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"



#define FIRST_OUTPUT_STREAM_ID          1024



typedef struct AudioStreamElement
{
    struct AudioStreamElement* next;
    
    int streamId;
    int outputStreamId;
    StreamInfo streamInfo;
} AudioStreamElement;

typedef struct AudioStreamGroup
{
    struct AudioStreamGroup* next;
    struct AudioStreamGroup* prev;
    
    /* all elements in a group have equal non-null clipId or equal sourceId, i.e.
    they belong to the same clip or same source */
    char clipId[CLIP_ID_SIZE];
    int sourceId;
    
    AudioStreamElement elements;
} AudioStreamGroup;

typedef struct 
{
    MediaSink* targetSink;

    AudioSwitchSink switchSink; 
    MediaSink sink;
    
    /* targetSinkListener listens to target sink and forwards to the switch sink listener */
    MediaSinkListener targetSinkListener;
    MediaSinkListener* switchListener;
    
    AudioStreamGroup groups;
    AudioStreamElement outputElements;

    AudioStreamGroup* currentGroup;
    
    pthread_mutex_t nextCurrentGroupMutex;
    AudioStreamGroup* nextCurrentGroup;
    int snapToVideo;
    int disableSwitching;
    int noAudioGroup;

} DefaultAudioSwitch;



static void clear_stream_elements(AudioStreamElement* root)
{
    AudioStreamElement* element = root;
    AudioStreamElement* nextElement = NULL;
    
    while (element != NULL)
    {
        nextElement = element->next;
        
        clear_stream_info(&element->streamInfo);
        if (element != root)
        {
            SAFE_FREE(&element);
        }
        
        element = nextElement;
    }

    memset(root, 0, sizeof(*root));
    root->streamId = -1;
}

static int append_stream_element(AudioStreamElement* root, AudioStreamElement* newElement)
{
    AudioStreamElement* element;

    assert(newElement->streamId >= 0);
    
    if (root->streamId < 0)
    {
        *root = *newElement;
        return 1;
    }
    
    element = root;
    while (element->next != NULL)
    {
        element = element->next;
    }
    
    CALLOC_ORET(element->next, AudioStreamElement, 1);
    *element->next = *newElement;
    
    return 1;
}


static void clear_stream_groups(AudioStreamGroup* root)
{
    AudioStreamGroup* group = root;
    AudioStreamGroup* nextGroup = NULL;
    
    while (group != NULL)
    {
        nextGroup = group->next;
        
        clear_stream_elements(&group->elements);
        if (group != root)
        {
            SAFE_FREE(&group);
        }
        
        group = nextGroup;
    }
    
    memset(root, 0, sizeof(*root));
    root->elements.streamId = -1;
}

static int append_stream_group(AudioStreamGroup* root, AudioStreamGroup* newGroup)
{
    AudioStreamGroup* group;

    assert(newGroup->elements.streamId >= 0);
    
    if (root->elements.streamId < 0)
    {
        *root = *newGroup;
        return 1;
    }
    
    group = root;
    while (group->next != NULL)
    {
        group = group->next;
    }
    
    CALLOC_ORET(group->next, AudioStreamGroup, 1);
    *group->next = *newGroup;
    group->next->prev = group;
    
    return 1;
}

static AudioStreamElement* get_stream_element(AudioStreamElement* root, int streamId)
{
    AudioStreamElement* element = root;
    
    while (element != NULL)
    {
        if (element->streamId == streamId)
        {
            return element;
        }
        
        element = element->next;
    }
    
    return NULL;
}

static AudioStreamGroup* get_stream_group_with_id(AudioStreamGroup* root, char* clipId, int sourceId)
{
    AudioStreamGroup* group = root;
    
    while (group != NULL)
    {
        if ((group->clipId[0] != '\0' && strcmp(group->clipId, clipId) == 0) || 
            group->sourceId == sourceId)
        {
            return group;
        }
        
        group = group->next;
    }
    
    return NULL;
}

static int count_stream_elements(AudioStreamElement* root)
{
    AudioStreamElement* element = root;
    int count = 0;
    
    if (root->streamId < 0)
    {
        return 0;
    }
    count++;
    
    while (element->next != NULL)
    {
        count++;
        element = element->next;
    }
    
    return count;
}

static int add_stream_to_groups(DefaultAudioSwitch* swtch, int streamId, const StreamInfo* streamInfo)
{
    AudioStreamGroup newGroup;
    AudioStreamElement newElement;
    AudioStreamGroup* group;

    memset(&newGroup, 0, sizeof(newGroup));
    clear_stream_groups(&newGroup);
    memset(&newElement, 0, sizeof(newElement));
    clear_stream_elements(&newElement);

    newElement.streamId = streamId;
    CHK_ORET(duplicate_stream_info(streamInfo, &newElement.streamInfo));
    
    
    /* first element in a new group */    
    if (swtch->groups.elements.streamId < 0)
    {
        strcpy(newGroup.clipId, streamInfo->clipId);
        newGroup.sourceId = streamInfo->sourceId;
        newElement.outputStreamId = FIRST_OUTPUT_STREAM_ID;
        CHK_ORET(append_stream_element(&newGroup.elements, &newElement));
        CHK_ORET(append_stream_group(&swtch->groups, &newGroup));
        
        return 1;
    }
    
    
    /* try add to existing groups */
    group = &swtch->groups;
    while (group != NULL)
    {
        if ((streamInfo->clipId[0] != '\0' && strcmp(streamInfo->clipId, group->clipId) == 0) || 
            streamInfo->sourceId == group->sourceId)
        {
            newElement.outputStreamId = FIRST_OUTPUT_STREAM_ID + count_stream_elements(&group->elements);
            CHK_ORET(append_stream_element(&group->elements, &newElement));
            return 1;
        }
        
        group = group->next;
    }
    
    
    /* add to new group with a new element */
    strcpy(newGroup.clipId, streamInfo->clipId);
    newGroup.sourceId = streamInfo->sourceId;
    newElement.outputStreamId = FIRST_OUTPUT_STREAM_ID;
    CHK_ORET(append_stream_element(&newGroup.elements, &newElement));
    CHK_ORET(append_stream_group(&swtch->groups, &newGroup));
    
    return 1;
}

static int is_new_output_stream(DefaultAudioSwitch* swtch, const StreamInfo* streamInfo)
{
    AudioStreamGroup* group = &swtch->groups;
    
    if (group->elements.streamId < 0)
    {
        return 1;
    }
    
    while (group != NULL)
    {
        if ((streamInfo->clipId[0] != '\0' && strcmp(streamInfo->clipId, group->clipId) == 0) || 
            streamInfo->sourceId == group->sourceId)
        {
            if (count_stream_elements(&group->elements) + 1 > count_stream_elements(&swtch->outputElements))
            {
                return 1;
            }
            
            return 0;
        }
        
        group = group->next;
    }
    
    return 0;
}

static int stream_is_compatible(DefaultAudioSwitch* swtch, const StreamInfo* streamInfo)
{
    AudioStreamElement* element = &swtch->outputElements;
    
    if (element->streamId < 0)
    {
        /* shouldn't be here */
        return 0;
    }
    
    if (element->streamInfo.type == streamInfo->type &&
        element->streamInfo.format == streamInfo->format &&
        memcmp(&element->streamInfo.samplingRate, &streamInfo->samplingRate, sizeof(element->streamInfo.samplingRate)) == 0 &&
        element->streamInfo.numChannels == streamInfo->numChannels &&
        element->streamInfo.bitsPerSample == streamInfo->bitsPerSample)
    {
        return 1;
    }
    
    return 0;
}

static int is_input_stream(DefaultAudioSwitch* swtch, int streamId)
{
    AudioStreamGroup* group = &swtch->groups;
    
    if (group->elements.streamId < 0)
    {
        return 0;
    }
    
    while (group != NULL)
    {
        if (get_stream_element(&group->elements, streamId) != NULL)
        {
            return 1;
        }
        
        group = group->next;
    }
    
    return 0;
}




static void qas_frame_displayed(void* data, const FrameInfo* frameInfo)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    
    msl_frame_displayed(swtch->switchListener, frameInfo);
}

static void qas_frame_dropped(void* data, const FrameInfo* lastFrameInfo)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    msl_frame_dropped(swtch->switchListener, lastFrameInfo);
}

static void qas_refresh_required(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    msl_refresh_required(swtch->switchListener);
}



static int qas_register_listener(void* data, MediaSinkListener* listener)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    swtch->switchListener = listener;

    return msk_register_listener(swtch->targetSink, &swtch->targetSinkListener);
}

static void qas_unregister_listener(void* data, MediaSinkListener* listener)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    if (swtch->switchListener == listener)
    {
        swtch->switchListener = NULL;
    }
    
    msk_unregister_listener(swtch->targetSink, &swtch->targetSinkListener);
}

static int qas_accept_stream(void* data, const StreamInfo* streamInfo)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    
    if (streamInfo->type == SOUND_STREAM_TYPE)
    {
        /* any audio stream we can potentially accept */
        return 1;
    }
    else
    {
        return msk_accept_stream(swtch->targetSink, streamInfo);
    }
}

static int qas_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    AudioStreamElement newElement;
    int nextOutputStreamId = FIRST_OUTPUT_STREAM_ID + count_stream_elements(&swtch->outputElements);

    memset(&newElement, 0, sizeof(newElement));
    clear_stream_elements(&newElement);

    if (streamInfo->type == SOUND_STREAM_TYPE)
    {
        if (is_new_output_stream(swtch, streamInfo))
        {
            if (msk_accept_stream(swtch->targetSink, streamInfo) &&
                msk_register_stream(swtch->targetSink, nextOutputStreamId, streamInfo))
            {
                newElement.streamId = nextOutputStreamId;
                CHK_ORET(duplicate_stream_info(streamInfo, &newElement.streamInfo));
                CHK_ORET(append_stream_element(&swtch->outputElements, &newElement));

                CHK_ORET(add_stream_to_groups(swtch, streamId, streamInfo));

                return 1;
            }
        }
        else
        {
            if (stream_is_compatible(swtch, streamInfo))
            {
                CHK_ORET(add_stream_to_groups(swtch, streamId, streamInfo));
                return 1;
            }
        }
        
        return 0;
    }
    else
    {
        return msk_register_stream(swtch->targetSink, streamId, streamInfo);
    }
}

static int qas_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    AudioStreamElement* inputStream = NULL;
    AudioStreamElement* outputStream = NULL;
    VideoSwitchSink* videoSwitch = NULL;
    char clipId[CLIP_ID_SIZE];
    int sourceId;
    AudioStreamGroup* videoGroup = NULL;
    
    if (!swtch->disableSwitching)
    {
        PTHREAD_MUTEX_LOCK(&swtch->nextCurrentGroupMutex);
        
        swtch->noAudioGroup = 0;
        
        if (swtch->snapToVideo)
        {
            videoSwitch = msk_get_video_switch(swtch->targetSink);
            if (videoSwitch != NULL)
            {
                if (vsw_get_first_active_clip_id(videoSwitch, clipId, &sourceId))
                {
                    videoGroup = get_stream_group_with_id(&swtch->groups, clipId, sourceId);
                    if (videoGroup != NULL)
                    {
                        swtch->currentGroup = videoGroup;
                    }
                    else
                    {
                        swtch->noAudioGroup = 1;
                    }
                }
            }
            swtch->nextCurrentGroup = swtch->currentGroup;
        }
        else
        {
            swtch->currentGroup = swtch->nextCurrentGroup;
        }
        
        swtch->disableSwitching = 1;
        
        PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentGroupMutex);
    }
        
    if (is_input_stream(swtch, streamId))
    {
        if (swtch->noAudioGroup)
        {
            return 0;
        }
        
        inputStream = get_stream_element(&swtch->currentGroup->elements, streamId);
        if (inputStream != NULL)
        {
            outputStream = get_stream_element(&swtch->outputElements, inputStream->outputStreamId);
            if (outputStream != NULL)
            {
                return msk_accept_stream_frame(swtch->targetSink, outputStream->streamId, frameInfo);
            }
        }
        
        return 0;
    }
    else
    {
        return msk_accept_stream_frame(swtch->targetSink, streamId, frameInfo);
    }
}

static int qas_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    AudioStreamElement* inputStream = NULL;
    AudioStreamElement* outputStream = NULL;
    
    if (is_input_stream(swtch, streamId))
    {
        inputStream = get_stream_element(&swtch->currentGroup->elements, streamId);
        if (inputStream != NULL)
        {
            outputStream = get_stream_element(&swtch->outputElements, inputStream->outputStreamId);
            if (outputStream != NULL)
            {
                return msk_get_stream_buffer(swtch->targetSink, outputStream->streamId, bufferSize, buffer);
            }
            
        }
        return 0;
    }
    else
    {
        return msk_get_stream_buffer(swtch->targetSink, streamId, bufferSize, buffer);
    }
}

static int qas_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    AudioStreamElement* inputStream = NULL;
    AudioStreamElement* outputStream = NULL;
    
    if (is_input_stream(swtch, streamId))
    {
        inputStream = get_stream_element(&swtch->currentGroup->elements, streamId);
        if (inputStream != NULL)
        {
            outputStream = get_stream_element(&swtch->outputElements, inputStream->outputStreamId);
            if (outputStream != NULL)
            {
                return msk_receive_stream_frame(swtch->targetSink, outputStream->streamId, buffer, bufferSize);
            }
        }
        
        return 0;
    }
    else
    {
        return msk_receive_stream_frame(swtch->targetSink, streamId, buffer, bufferSize);
    }
}

static int qas_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    AudioStreamElement* inputStream = NULL;
    AudioStreamElement* outputStream = NULL;
    
    if (is_input_stream(swtch, streamId))
    {
        inputStream = get_stream_element(&swtch->currentGroup->elements, streamId);
        if (inputStream != NULL)
        {
            outputStream = get_stream_element(&swtch->outputElements, inputStream->outputStreamId);
            if (outputStream != NULL)
            {
                return msk_receive_stream_frame_const(swtch->targetSink, outputStream->streamId, buffer, bufferSize);
            }
        }
        
        return 0;
    }
    else
    {
        return msk_receive_stream_frame_const(swtch->targetSink, streamId, buffer, bufferSize);
    }
}

static int qas_complete_frame(void* data, const FrameInfo* frameInfo)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    swtch->disableSwitching = 0;
    
    return msk_complete_frame(swtch->targetSink, frameInfo);
}

static void qas_cancel_frame(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    
    swtch->disableSwitching = 0;
    
    msk_cancel_frame(swtch->targetSink);
}

static OnScreenDisplay* qas_get_osd(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    return msk_get_osd(swtch->targetSink);
}
    
static VideoSwitchSink* qas_get_video_switch(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    return msk_get_video_switch(swtch->targetSink);
}
    
static AudioSwitchSink* qas_get_audio_switch(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    return &swtch->switchSink;
}

static HalfSplitSink* qas_get_half_split(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    return msk_get_half_split(swtch->targetSink);
}
    
static FrameSequenceSink* qas_get_frame_sequence(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    return msk_get_frame_sequence(swtch->targetSink);
}
    
static int qas_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    return msk_get_buffer_state(swtch->targetSink, numBuffers, numBuffersFilled);
}
    
static int qas_mute_audio(void* data, int mute)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    return msk_mute_audio(swtch->targetSink, mute);
}

static void qas_close(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;

    if (data == NULL)
    {
        return;
    }
    
    msk_close(swtch->targetSink);
    
    clear_stream_elements(&swtch->outputElements);
    clear_stream_groups(&swtch->groups);
    
    destroy_mutex(&swtch->nextCurrentGroupMutex);
    
    SAFE_FREE(&swtch);
}

static int qas_reset_or_close(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    int result;
    
    clear_stream_elements(&swtch->outputElements);
    clear_stream_groups(&swtch->groups);
    
    swtch->currentGroup = &swtch->groups;
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentGroupMutex);
    swtch->nextCurrentGroup = swtch->currentGroup;
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentGroupMutex);
    
    result = msk_reset_or_close(swtch->targetSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            swtch->targetSink = NULL;
        }
        goto fail;
    }
    
    return 1;
    
fail:
    qas_close(data);
    return 2;
}


static MediaSink* qas_get_media_sink(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    
    return &swtch->sink;
}

static int qas_switch_next_audio_group(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    int haveSwitched = 0;
    
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentGroupMutex);
    if (swtch->nextCurrentGroup->next != NULL)
    {
        swtch->nextCurrentGroup = swtch->nextCurrentGroup->next;
        haveSwitched = 1;
    }
    haveSwitched = (haveSwitched || swtch->snapToVideo != 0);
    swtch->snapToVideo = 0;
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentGroupMutex);
    
    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }
    
    return haveSwitched;
}

static int qas_switch_prev_audio_group(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    int haveSwitched = 0;
    
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentGroupMutex);
    if (swtch->nextCurrentGroup->prev != NULL)
    {
        swtch->nextCurrentGroup = swtch->nextCurrentGroup->prev;
        haveSwitched = 1;
    }
    haveSwitched = (haveSwitched || swtch->snapToVideo != 0);
    swtch->snapToVideo = 0;
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentGroupMutex);
    
    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }
    
    return haveSwitched;
}

static void qas_snap_audio_to_video(void* data)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    int haveSwitched = 0;
    
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentGroupMutex);
    haveSwitched = (swtch->snapToVideo != 1);
    swtch->snapToVideo = 1;
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentGroupMutex);

    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }
}

static int qas_switch_audio_group(void* data, int index)
{
    DefaultAudioSwitch* swtch = (DefaultAudioSwitch*)data;
    AudioStreamGroup* group = &swtch->groups;
    int count = 1;
    int haveSwitched = 0;
    
    if (index == 0)
    {
        qas_snap_audio_to_video(data);
        return 1;
    }
    
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentGroupMutex);
    while (count < index && group != NULL)
    {
        count++;
        group = group->next;
    }
    if (group != NULL)
    {
        swtch->nextCurrentGroup = group;
        haveSwitched = 1;
    }
    haveSwitched = (haveSwitched || swtch->snapToVideo != 0);
    swtch->snapToVideo = 0;
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentGroupMutex);
    
    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }
    
    return haveSwitched;
}



int qas_create_audio_switch(MediaSink* sink,  AudioSwitchSink** swtch)
{
    DefaultAudioSwitch* newSwitch;
    
    CALLOC_ORET(newSwitch, DefaultAudioSwitch, 1);
    
    clear_stream_groups(&newSwitch->groups);
    clear_stream_elements(&newSwitch->outputElements);
    newSwitch->currentGroup = &newSwitch->groups;
    newSwitch->nextCurrentGroup = newSwitch->currentGroup;
    newSwitch->snapToVideo = 1;
    
    newSwitch->targetSink = sink;

    newSwitch->switchSink.data = newSwitch;
    newSwitch->switchSink.get_media_sink = qas_get_media_sink;
    newSwitch->switchSink.switch_next_audio_group = qas_switch_next_audio_group;
    newSwitch->switchSink.switch_prev_audio_group = qas_switch_prev_audio_group;
    newSwitch->switchSink.switch_audio_group = qas_switch_audio_group;
    newSwitch->switchSink.snap_audio_to_video = qas_snap_audio_to_video;
    
    newSwitch->targetSinkListener.data = newSwitch;
    newSwitch->targetSinkListener.frame_displayed = qas_frame_displayed;
    newSwitch->targetSinkListener.frame_dropped = qas_frame_dropped;
    newSwitch->targetSinkListener.refresh_required = qas_refresh_required;
    
    newSwitch->sink.data = newSwitch;
    newSwitch->sink.register_listener = qas_register_listener;
    newSwitch->sink.unregister_listener = qas_unregister_listener;
    newSwitch->sink.accept_stream = qas_accept_stream;
    newSwitch->sink.register_stream = qas_register_stream;
    newSwitch->sink.accept_stream_frame = qas_accept_stream_frame;
    newSwitch->sink.get_stream_buffer = qas_get_stream_buffer;
    newSwitch->sink.receive_stream_frame = qas_receive_stream_frame;
    newSwitch->sink.receive_stream_frame_const = qas_receive_stream_frame_const;
    newSwitch->sink.complete_frame = qas_complete_frame;
    newSwitch->sink.cancel_frame = qas_cancel_frame;
    newSwitch->sink.get_osd = qas_get_osd;
    newSwitch->sink.get_video_switch = qas_get_video_switch;
    newSwitch->sink.get_audio_switch = qas_get_audio_switch;
    newSwitch->sink.get_half_split = qas_get_half_split;
    newSwitch->sink.get_frame_sequence = qas_get_frame_sequence;
    newSwitch->sink.get_buffer_state = qas_get_buffer_state;
    newSwitch->sink.mute_audio = qas_mute_audio;
    newSwitch->sink.reset_or_close = qas_reset_or_close;
    newSwitch->sink.close = qas_close;
    
    CHK_OFAIL(init_mutex(&newSwitch->nextCurrentGroupMutex));
    
    
    *swtch = &newSwitch->switchSink;
    return 1;
    
fail:
    qas_close(newSwitch);
    return 0;
}


MediaSink* asw_get_media_sink(AudioSwitchSink* swtch)
{
    if (swtch && swtch->get_media_sink)
    {
        return swtch->get_media_sink(swtch->data);
    }
    return NULL;
}

int asw_switch_next_audio_group(AudioSwitchSink* swtch)
{
    if (swtch && swtch->switch_next_audio_group)
    {
        return swtch->switch_next_audio_group(swtch->data);
    }
    return 0;
}

int asw_switch_prev_audio_group(AudioSwitchSink* swtch)
{
    if (swtch && swtch->switch_prev_audio_group)
    {
        return swtch->switch_prev_audio_group(swtch->data);
    }
    return 0;
}

int asw_switch_audio_group(AudioSwitchSink* swtch, int index)
{
    if (swtch && swtch->switch_audio_group)
    {
        return swtch->switch_audio_group(swtch->data, index);
    }
    return 0;
}

void asw_snap_audio_to_video(AudioSwitchSink* swtch)
{
    if (swtch && swtch->snap_audio_to_video)
    {
        swtch->snap_audio_to_video(swtch->data);
    }
}


