/*
 * $Id: buffered_media_sink.c,v 1.7 2010/02/12 14:00:06 philipn Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#include "buffered_media_sink.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"



struct BufferedMediaSink
{
    MediaSink mediaSink;

    MediaSink* targetSink;

    /* listener for sink events */
    MediaSinkListener* listener;

    int dropFrameWhenFull;

    int stopping;  /* set when buf sink is stopping (when closing or resetting) */

    MediaSinkFrame** frames;
    int numFrames;

    OnScreenDisplay bufOSD;
    OnScreenDisplayState* osdState;

    pthread_mutex_t stateMutex;
    pthread_cond_t frameReadCond;
    pthread_cond_t frameWrittenCond;
    MediaSinkFrame* currentFrameRead;
    MediaSinkFrame* swapFrame;
    int firstFramePosition;
    int framesUsed;
    int writePosition;

    int frameStarted;

	pthread_t readThreadId;
};


static void* read_thread(void* arg)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)arg;
    int status;

    while (!bufSink->stopping)
    {
        PTHREAD_MUTEX_LOCK(&bufSink->stateMutex)

        /* check if frame is ready to be read */
        if (bufSink->framesUsed > 0)
        {
            bufSink->currentFrameRead = bufSink->frames[bufSink->firstFramePosition];
            bufSink->frames[bufSink->firstFramePosition] = bufSink->swapFrame;
            bufSink->swapFrame = NULL;
            bufSink->firstFramePosition = (bufSink->firstFramePosition + 1) % bufSink->numFrames;
            bufSink->framesUsed -= 1;
        }

        PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex)


        /* read frame if one is available */
        if (bufSink->currentFrameRead != NULL)
        {
            if (!msk_complete_sink_frame(bufSink->targetSink, bufSink->currentFrameRead))
            {
                ml_log_warn("failed to complete sink frame\n");
                /* we ignore that the sink failed to process the frame and continue with the next one */
            }

            PTHREAD_MUTEX_LOCK(&bufSink->stateMutex)

            bufSink->swapFrame = bufSink->currentFrameRead;
            bufSink->currentFrameRead = NULL;

            /* signal so that writer wakes up */
            status = pthread_cond_signal(&bufSink->frameReadCond);
            if (status != 0)
            {
                ml_log_warn("Failed to signal buffered sink write thread\n");
            }

            PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex)
        }
        /* else wait for write thread to signal that a new frame is ready */
        else
        {
            PTHREAD_MUTEX_LOCK(&bufSink->stateMutex)

            status = pthread_cond_wait(&bufSink->frameWrittenCond, &bufSink->stateMutex);
            if (status != 0)
            {
                ml_log_warn("Failed to wait for signal from write thread in buffered sink\n");
            }

            PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex)
        }
    }

    pthread_exit((void*) 0);
}


static int bms_register_listener(void* data, MediaSinkListener* listener)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    bufSink->listener = listener;

    return msk_register_listener(bufSink->targetSink, listener);
}

static void bms_unregister_listener(void* data, MediaSinkListener* listener)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    if (bufSink->listener == listener)
    {
        bufSink->listener = NULL;
    }

    msk_unregister_listener(bufSink->targetSink, listener);
}

static int bms_accept_stream(void* data, const StreamInfo* streamInfo)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return msk_accept_stream(bufSink->targetSink, streamInfo);
}

static int bms_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;
    int result;
    int i;

    result = 1;
    for (i = 0; i < bufSink->numFrames && result; i++)
    {
        /* TODO: handle partial failure */
        result = result && msf_register_stream(bufSink->frames[i], streamId, streamInfo);
    }
    result = result && msf_register_stream(bufSink->swapFrame, streamId, streamInfo);

    return result;
}

static int bms_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;
    int doneWaiting = 0;
    int status;
    int readyToWrite = 0;


    if (!bufSink->frameStarted)
    {
        /* wait until we write the frame, or return directly if non-drop frames and buffer is full */
        while (!doneWaiting && !bufSink->stopping)
        {
            PTHREAD_MUTEX_LOCK(&bufSink->stateMutex)

            if (bufSink->framesUsed < bufSink->numFrames)
            {
                bufSink->writePosition = (bufSink->firstFramePosition + bufSink->framesUsed) % bufSink->numFrames;
                readyToWrite = 1;
                doneWaiting = 1;
            }
            else
            {
                if (bufSink->dropFrameWhenFull)
                {
                    /* drop a frame */
                    bufSink->firstFramePosition = (bufSink->firstFramePosition + 1) % bufSink->numFrames;
                    bufSink->framesUsed -= 1;

                    bufSink->writePosition = (bufSink->firstFramePosition + bufSink->framesUsed) % bufSink->numFrames;
                    readyToWrite = 1;
                    doneWaiting = 1;
                }
                else
                {
                    /* wait for read thread */
                    status = pthread_cond_wait(&bufSink->frameReadCond, &bufSink->stateMutex);
                    if (status != 0)
                    {
                        ml_log_warn("buffered sink failed to wait for frame read condition\n");
                    }
                }
            }

            PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex)
        }
    }
    else
    {
        readyToWrite = 1;
    }

    if (bufSink->stopping || !readyToWrite)
    {
        return 0;
    }


    if (!bufSink->frameStarted)
    {
        msf_reset(bufSink->frames[bufSink->writePosition]);
        bufSink->frameStarted = 1;
    }

    return msf_accept_stream_frame(bufSink->frames[bufSink->writePosition], streamId, frameInfo);
}

static int bms_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return msf_allocate_stream_buffer(bufSink->frames[bufSink->writePosition], streamId, bufferSize, buffer);
}

static int bms_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return msf_set_is_present(bufSink->frames[bufSink->writePosition], streamId);
}

static int bms_complete_frame(void* data, const FrameInfo* frameInfo)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;
    int result;
    int status;

    /* stamp frame with current OSD state and frame info */
    osds_complete(bufSink->osdState, frameInfo);
    result = msf_complete_frame(bufSink->frames[bufSink->writePosition], bufSink->osdState, frameInfo);

    osds_reset_screen_state(bufSink->osdState);
    bufSink->frameStarted = 0;

    PTHREAD_MUTEX_LOCK(&bufSink->stateMutex)

    bufSink->framesUsed += 1;

    /* signal so that reader wakes up */
    status = pthread_cond_signal(&bufSink->frameWrittenCond);
    if (status != 0)
    {
        ml_log_warn("Failed to signal buffered sink read thread\n");
    }
    PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex)

    return result;
}

static void bms_cancel_frame(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;
    int status;

    osds_reset_screen_state(bufSink->osdState);
    bufSink->frameStarted = 0;

    PTHREAD_MUTEX_LOCK(&bufSink->stateMutex)

    /* signal so that reader wakes up */
    status = pthread_cond_signal(&bufSink->frameWrittenCond);
    if (status != 0)
    {
        ml_log_warn("Failed to signal buffered sink read thread\n");
    }

    PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex)
}

static int bms_allocate_frame(void* data, MediaSinkFrame** frame)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return msk_allocate_frame(bufSink->targetSink, frame);
}

static int bms_complete_sink_frame(void* data, const MediaSinkFrame* frame)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return msk_complete_sink_frame(bufSink->targetSink, frame);
}

static OnScreenDisplay* bms_get_osd(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* only return the osd proxy if the target sink has an OSD */
    if (!msk_get_osd(bufSink->targetSink))
    {
        return NULL;
    }

    return &bufSink->bufOSD;
}

static int bms_mute_audio(void* data, int mute)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return msk_mute_audio(bufSink->targetSink, mute);
}

static void bms_close(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

    bufSink->stopping = 1;

    PTHREAD_MUTEX_LOCK(&bufSink->stateMutex);
    pthread_cond_broadcast(&bufSink->frameReadCond);
    pthread_cond_broadcast(&bufSink->frameWrittenCond);
    PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex);

    join_thread(&bufSink->readThreadId, bufSink->targetSink->data, NULL);

    for (i = 0; i < bufSink->numFrames; i++)
    {
        msf_free(bufSink->frames[i]);
    }
    SAFE_FREE(&bufSink->frames);

    if (bufSink->currentFrameRead != NULL)
    {
        msf_free(bufSink->currentFrameRead);
        bufSink->currentFrameRead = NULL;
    }
    if (bufSink->swapFrame != NULL)
    {
        msf_free(bufSink->swapFrame);
        bufSink->swapFrame = NULL;
    }

    /* only close after free'ing frames (eg. frames could be using shared memory attached to a display) */
    msk_close(bufSink->targetSink);

    osds_free(&bufSink->osdState);

    destroy_cond_var(&bufSink->frameReadCond);
    destroy_cond_var(&bufSink->frameWrittenCond);
	destroy_mutex(&bufSink->stateMutex);

    SAFE_FREE(&bufSink);
}

static int bms_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    PTHREAD_MUTEX_LOCK(&bufSink->stateMutex)
    *numBuffers = bufSink->numFrames;
    *numBuffersFilled = bufSink->framesUsed;
    PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex)

    return 1;
}

static int bms_reset_or_close(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;
    int i;
    int numFrames = bufSink->numFrames;
    int result;

    bufSink->stopping = 1;

    PTHREAD_MUTEX_LOCK(&bufSink->stateMutex);
    pthread_cond_broadcast(&bufSink->frameReadCond);
    pthread_cond_broadcast(&bufSink->frameWrittenCond);
    PTHREAD_MUTEX_UNLOCK(&bufSink->stateMutex);

    join_thread(&bufSink->readThreadId, bufSink->targetSink->data, NULL);

    for (i = 0; i < bufSink->numFrames; i++)
    {
        msf_free(bufSink->frames[i]);
    }
    SAFE_FREE(&bufSink->frames);
    bufSink->numFrames = 0;

    if (bufSink->currentFrameRead != NULL)
    {
        msf_free(bufSink->currentFrameRead);
        bufSink->currentFrameRead = NULL;
    }
    if (bufSink->swapFrame != NULL)
    {
        msf_free(bufSink->swapFrame);
        bufSink->swapFrame = NULL;
    }

    bufSink->stopping = 0;

    result = msk_reset_or_close(bufSink->targetSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            bufSink->targetSink = NULL;
        }
        goto fail;
    }

    osds_sink_reset(bufSink->osdState);

    CALLOC_OFAIL(bufSink->frames, MediaSinkFrame*, numFrames);
    bufSink->numFrames = numFrames;

    for (i = 0; i < bufSink->numFrames; i++)
    {
        CHK_OFAIL(msk_allocate_frame(bufSink->targetSink, &bufSink->frames[i]));
    }
    CHK_OFAIL(msk_allocate_frame(bufSink->targetSink, &bufSink->swapFrame));

    bufSink->firstFramePosition = 0;
    bufSink->framesUsed = 0;
    bufSink->writePosition = 0;

    bufSink->frameStarted = 0;

    CHK_OFAIL(create_joinable_thread(&bufSink->readThreadId, read_thread, bufSink));

    return 1;

fail:
    bms_close(data);
    return 2;
}

static int bms_osd_create_menu_model(void* data, OSDMenuModel** menu)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and add directly to sink */
    return osd_create_menu_model(msk_get_osd(bufSink->targetSink), menu);
}

static void bms_osd_free_menu_model(void* data, OSDMenuModel** menu)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    return osd_free_menu_model(msk_get_osd(bufSink->targetSink), menu);
}

static void bms_osd_set_active_menu_model(void* data, int updateMask, OSDMenuModel* menu)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and set directly to sink */
    return osd_set_active_menu_model(msk_get_osd(bufSink->targetSink), updateMask, menu);
}

static int bms_osd_set_screen(void* data, OSDScreen screen)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return osd_set_screen(osds_get_osd(bufSink->osdState), screen);
}

static int bms_osd_next_screen(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return osd_next_screen(osds_get_osd(bufSink->osdState));
}

static int bms_osd_get_screen(void* data, OSDScreen* screen)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and get directly from sink */
    return osd_get_screen(msk_get_osd(bufSink->targetSink), screen);
}

static int bms_osd_next_timecode(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return osd_next_timecode(osds_get_osd(bufSink->osdState));
}

static int bms_osd_set_timecode(void* data, int index, int type, int subType)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return osd_set_timecode(osds_get_osd(bufSink->osdState), index, type, subType);
}

static int bms_osd_set_play_state(void* data, OSDPlayState state, int value)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return osd_set_play_state(osds_get_osd(bufSink->osdState), state, value);
}

static int bms_osd_set_state(void* data, const OnScreenDisplayState* state)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return osd_set_state(osds_get_osd(bufSink->osdState), state);
}

static void bms_osd_set_minimum_audio_stream_level(void* data, double level)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_set_minimum_audio_stream_level(osds_get_osd(bufSink->osdState), level);
}

static void bms_osd_set_audio_lineup_level(void* data, float level)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_set_audio_lineup_level(osds_get_osd(bufSink->osdState), level);
}

static void bms_osd_reset_audio_stream_levels(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_reset_audio_stream_levels(osds_get_osd(bufSink->osdState));
}

static int bms_osd_register_audio_stream(void* data, int streamId)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    return osd_register_audio_stream(osds_get_osd(bufSink->osdState), streamId);
}

static void bms_osd_set_audio_stream_level(void* data, int streamId, double level)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_set_audio_stream_level(osds_get_osd(bufSink->osdState), streamId, level);
}

static void bms_osd_set_audio_level_visibility(void* data, int visible)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and get directly from sink */
    return osd_set_audio_level_visibility(msk_get_osd(bufSink->targetSink), visible);
}

static void bms_osd_toggle_audio_level_visibility(void* data)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and get directly from sink */
    return osd_toggle_audio_level_visibility(msk_get_osd(bufSink->targetSink));
}

static void bms_osd_show_field_symbol(void* data, int enable)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_show_field_symbol(osds_get_osd(bufSink->osdState), enable);
}

static void bms_osd_show_vtr_error_level(void* data, int enable)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_show_vtr_error_level(osds_get_osd(bufSink->osdState), enable);
}

static void bms_osd_set_mark_display(void* data, const MarkConfigs* markConfigs)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_set_mark_display(osds_get_osd(bufSink->osdState), markConfigs);
}

static int bms_osd_create_marks_model(void* data, OSDMarksModel** model)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    return osd_create_marks_model(msk_get_osd(bufSink->targetSink), model);
}

static void bms_osd_free_marks_model(void* data, OSDMarksModel** model)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    osd_free_marks_model(msk_get_osd(bufSink->targetSink), model);
}

static void bms_osd_set_marks_model(void* data, int updateMask, OSDMarksModel* model)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    osd_set_marks_model(msk_get_osd(bufSink->targetSink), updateMask, model);
}

static void bms_osd_set_second_marks_model(void* data, int updateMask, OSDMarksModel* model)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    osd_set_second_marks_model(msk_get_osd(bufSink->targetSink), updateMask, model);
}

static void bms_osd_set_progress_bar_visibility(void* data, int visible)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    osd_set_progress_bar_visibility(msk_get_osd(bufSink->targetSink), visible);
}

static float bms_osd_get_position_in_progress_bar(void* data, int x, int y)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    return osd_get_position_in_progress_bar(msk_get_osd(bufSink->targetSink), x, y);
}

static void bms_osd_highlight_progress_bar_pointer(void* data, int on)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    osd_highlight_progress_bar_pointer(msk_get_osd(bufSink->targetSink), on);
}

static void bms_osd_set_active_progress_bar_marks(void* data, int index)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    /* bypass the buffer and remove directly from sink */
    osd_set_active_progress_bar_marks(msk_get_osd(bufSink->targetSink), index);
}

static void bms_osd_set_label(void* data, int xPos, int yPos, int imageWidth, int imageHeight,
    int fontSize, Colour colour, int box, const char* label)
{
    BufferedMediaSink* bufSink = (BufferedMediaSink*)data;

    osd_set_label(osds_get_osd(bufSink->osdState), xPos, yPos, imageWidth, imageHeight, fontSize, colour, box, label);
}


int bms_create(MediaSink** targetSink, int size, int dropFrameWhenFull, BufferedMediaSink** bufSink)
{
    BufferedMediaSink* newBufSink = NULL;
    int i;

    if ((*targetSink)->allocate_frame == NULL ||
        (*targetSink)->complete_sink_frame == NULL)
    {
        ml_log_error("Target sink does not support bufferring\n");
        return 0;
    }
    if (size < 2)
    {
        ml_log_error("Buffered Media Sink must have size >= 2\n");
        return 0;
    }

    CALLOC_ORET(newBufSink, BufferedMediaSink, 1);

    newBufSink->dropFrameWhenFull = dropFrameWhenFull;

    CALLOC_ORET(newBufSink->frames, MediaSinkFrame*, size);
    newBufSink->numFrames = size;

    for (i = 0; i < size; i++)
    {
        CHK_OFAIL(msk_allocate_frame(*targetSink, &newBufSink->frames[i]));
    }
    CHK_OFAIL(msk_allocate_frame(*targetSink, &newBufSink->swapFrame));

    CHK_OFAIL(osds_create(&newBufSink->osdState));

    newBufSink->mediaSink.data = newBufSink;
    newBufSink->mediaSink.register_listener = bms_register_listener;
    newBufSink->mediaSink.unregister_listener = bms_unregister_listener;
    newBufSink->mediaSink.accept_stream = bms_accept_stream;
    newBufSink->mediaSink.register_stream = bms_register_stream;
    newBufSink->mediaSink.accept_stream_frame = bms_accept_stream_frame;
    newBufSink->mediaSink.get_stream_buffer = bms_get_stream_buffer;
    newBufSink->mediaSink.receive_stream_frame = bms_receive_stream_frame;
    newBufSink->mediaSink.complete_frame = bms_complete_frame;
    newBufSink->mediaSink.cancel_frame = bms_cancel_frame;
    newBufSink->mediaSink.allocate_frame = bms_allocate_frame;
    newBufSink->mediaSink.complete_sink_frame = bms_complete_sink_frame;
    newBufSink->mediaSink.get_osd = bms_get_osd;
    newBufSink->mediaSink.get_buffer_state = bms_get_buffer_state;
    newBufSink->mediaSink.mute_audio = bms_mute_audio;
    newBufSink->mediaSink.reset_or_close = bms_reset_or_close;
    newBufSink->mediaSink.close = bms_close;

    newBufSink->bufOSD.data = newBufSink;
    newBufSink->bufOSD.create_menu_model = bms_osd_create_menu_model;
    newBufSink->bufOSD.free_menu_model = bms_osd_free_menu_model;
    newBufSink->bufOSD.set_active_menu_model = bms_osd_set_active_menu_model;
    newBufSink->bufOSD.set_screen = bms_osd_set_screen;
    newBufSink->bufOSD.next_screen = bms_osd_next_screen;
    newBufSink->bufOSD.get_screen = bms_osd_get_screen;
    newBufSink->bufOSD.next_timecode = bms_osd_next_timecode;
    newBufSink->bufOSD.set_timecode = bms_osd_set_timecode;
    newBufSink->bufOSD.set_play_state = bms_osd_set_play_state;
    newBufSink->bufOSD.set_state = bms_osd_set_state;
    newBufSink->bufOSD.set_minimum_audio_stream_level = bms_osd_set_minimum_audio_stream_level;
    newBufSink->bufOSD.set_audio_lineup_level = bms_osd_set_audio_lineup_level;
    newBufSink->bufOSD.reset_audio_stream_levels = bms_osd_reset_audio_stream_levels;
    newBufSink->bufOSD.register_audio_stream = bms_osd_register_audio_stream;
    newBufSink->bufOSD.set_audio_stream_level = bms_osd_set_audio_stream_level;
    newBufSink->bufOSD.set_audio_level_visibility = bms_osd_set_audio_level_visibility;
    newBufSink->bufOSD.toggle_audio_level_visibility = bms_osd_toggle_audio_level_visibility;
    newBufSink->bufOSD.show_field_symbol = bms_osd_show_field_symbol;
    newBufSink->bufOSD.show_vtr_error_level = bms_osd_show_vtr_error_level;
    newBufSink->bufOSD.set_mark_display = bms_osd_set_mark_display;
    newBufSink->bufOSD.create_marks_model = bms_osd_create_marks_model;
    newBufSink->bufOSD.free_marks_model = bms_osd_free_marks_model;
    newBufSink->bufOSD.set_marks_model = bms_osd_set_marks_model;
    newBufSink->bufOSD.set_second_marks_model = bms_osd_set_second_marks_model;
    newBufSink->bufOSD.set_progress_bar_visibility = bms_osd_set_progress_bar_visibility;
    newBufSink->bufOSD.highlight_progress_bar_pointer = bms_osd_highlight_progress_bar_pointer;
    newBufSink->bufOSD.get_position_in_progress_bar = bms_osd_get_position_in_progress_bar;
    newBufSink->bufOSD.set_active_progress_bar_marks = bms_osd_set_active_progress_bar_marks;
    newBufSink->bufOSD.set_label = bms_osd_set_label;


    CHK_OFAIL(init_mutex(&newBufSink->stateMutex));
    CHK_OFAIL(init_cond_var(&newBufSink->frameReadCond));
    CHK_OFAIL(init_cond_var(&newBufSink->frameWrittenCond));

    CHK_OFAIL(create_joinable_thread(&newBufSink->readThreadId, read_thread, newBufSink));


    newBufSink->targetSink = *targetSink;
    *targetSink = NULL;
    *bufSink = newBufSink;
    return 1;

fail:
    bms_close(newBufSink);
    return 0;
}

MediaSink* bms_get_sink(BufferedMediaSink* bufSink)
{
    return &bufSink->mediaSink;
}



