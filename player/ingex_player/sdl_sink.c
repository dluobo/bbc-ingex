/*
 * $Id: sdl_sink.c,v 1.3 2008/11/06 11:30:09 john_f Exp $
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
#include <unistd.h>


#include "sdl_sink.h"
#include "logging.h"
#include "utils.h"
#include "macros.h"


#if !defined(HAVE_SDL)

int sdls_open(SDLSink** sink)
{
    return 0;
}

MediaSink* sdls_get_media_sink(SDLSink* sink)
{
    return NULL;
}

void sdls_register_window_listener(SDLSink* sink, X11WindowListener* listener)
{
}

void sdls_unregister_window_listener(SDLSink* sink, X11WindowListener* listener)
{
}


#else

#include "SDL/SDL.h"


/* frame rate control if the display get behind by 1/2 frame (40 * 1000 / 2)*/
#define MAX_FRAME_RATE_SLIPPAGE     20000

/* process events at least every 2 x per frame at 25fps */
#define MIN_PROCESS_EVENT_INTERVAL      (20 * 1000)

#define DISPLAY_FRAME_EVENT     (SDL_USEREVENT + 0)


typedef struct
{
    int streamId;
    StreamInfo streamInfo;

    SDL_Overlay* overlay;
    SDL_Surface* screen;

    pthread_mutex_t bufferMutex;
    pthread_cond_t bufferWriteReadyCond;
    unsigned char* buffer;
    unsigned int bufferSize;
    int bufferWriteReady;
    
    int isPresent;
} SDLStream;

struct SDLSink
{
    /* media sink interface */
    MediaSink mediaSink;
    
    /* listener */
    MediaSinkListener* listener;
    X11WindowListener* windowListener;
    
    /* streams */
    SDLStream stream;
    
    /* rate control */
    struct timeval lastFrameTime;
    
    /* events */
    pthread_t eventThread;
    pthread_mutex_t eventMutex;
    int stopped;
    
    int muteAudio;
};



/* make sure we process events regularly, at least every MIN_PROCESS_EVENT_INTERVAL useconds */
static void* process_events_thread(void* arg)
{
    SDLSink* sink = (SDLSink*)arg;
    struct timeval now;
    struct timeval last;
    long diff_usec;
    SDL_Event event;
    int status;
    SDL_Rect rect;
    
    memset(&last, 0, sizeof(last));

    while (!sink->stopped)
    {
        gettimeofday(&now, NULL);
        
        diff_usec = (now.tv_sec - last.tv_sec) * 1000000 + now.tv_usec - last.tv_usec;

        if (diff_usec > MIN_PROCESS_EVENT_INTERVAL)
        {
            last = now;
            
            PTHREAD_MUTEX_LOCK(&sink->eventMutex)
            
            while (!sink->stopped && SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_QUIT:
                        x11wl_close_request(sink->windowListener);
                        break;
                        
                    case DISPLAY_FRAME_EVENT:
                        rect.x = 0;
                        rect.y = 0;
                        rect.w = sink->stream.streamInfo.width;
                        rect.h = sink->stream.streamInfo.height;
                        
                        // TODO: put is separate function and do rate control in there
                        
                        
                        SDL_DisplayYUVOverlay(sink->stream.overlay, &rect);
                        
                        /* signal that the buffer can be written to */
                        PTHREAD_MUTEX_LOCK(&sink->stream.bufferMutex)
                        sink->stream.bufferWriteReady = 1;
                        status = pthread_cond_signal(&sink->stream.bufferWriteReadyCond);
                        if (status != 0)
                        {
                            ml_log_error("Failed to signal that SDL buffer is ready to write\n");
                        }
                        PTHREAD_MUTEX_UNLOCK(&sink->stream.bufferMutex)
                        break;
                        
                    default:
                        break;
                }
            }

            PTHREAD_MUTEX_UNLOCK(&sink->eventMutex)
        }
        else if (diff_usec > 0)
        {
            usleep(MIN_PROCESS_EVENT_INTERVAL - diff_usec);
        }
        
        last = now;
    }
    
    pthread_exit((void*) 0);
}


static void reset_streams(SDLSink* sink)
{
    sink->stream.isPresent = 0;
}

static SDLStream* get_sdl_stream(SDLSink* sink, int streamId)
{
    if (streamId == sink->stream.streamId)
    {
        return &sink->stream;
    }
    
    return NULL;
}


static int sdls_register_listener(void* data, MediaSinkListener* listener)
{
    SDLSink* sink = (SDLSink*)data;
    
    sink->listener = listener;
    
    return 1;
}

static void sdls_unregister_listener(void* data, MediaSinkListener* listener)
{
    SDLSink* sink = (SDLSink*)data;
    
    if (sink->listener == listener)
    {
        sink->listener = NULL;
    }
}

static int sdls_accept_stream(void* data, const StreamInfo* streamInfo)
{
    /* UYVY picture */
    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        (streamInfo->format == UYVY_FORMAT ||
         streamInfo->format == YUV420_FORMAT))
    {
        return 1;
    }
    return 0;
}

static int sdls_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    SDLSink* sink = (SDLSink*)data;
    uint32_t sdlFormat;

    /* this should've been checked, but we do it here anyway */
    if (!sdls_accept_stream(data, streamInfo))
    {
        ml_log_error("Can't register a stream %d that is not accepted\n", streamId);
        return 0;
    }

    if (sink->stream.streamId >= 0)
    {
        /* stream already set */
        return 0;
    }
    
    sink->stream.streamId = streamId;
    sink->stream.streamInfo = *streamInfo;
    
    sink->stream.screen = SDL_SetVideoMode(streamInfo->width, streamInfo->height, 24, 
        SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL);
    SDL_WM_SetCaption("Ingex Player - SDL", "Ingex Player - SDL");
    
    switch (sink->stream.streamInfo.format)
    {
        case UYVY_FORMAT:
            sdlFormat = SDL_UYVY_OVERLAY;
            sink->stream.bufferSize = streamInfo->width * streamInfo->height * 2;
            break;
        default: /* YUV420_FORMAT */
            sdlFormat = SDL_YV12_OVERLAY; /* TODO: should be SDL_IYUV_OVERLAY, but UV are swapped during playback ? */
            sink->stream.bufferSize = streamInfo->width * streamInfo->height * 3 / 2;
            break;
    }
    sink->stream.overlay = SDL_CreateYUVOverlay(streamInfo->width, streamInfo->height, sdlFormat, sink->stream.screen);
    
    CALLOC_ORET(sink->stream.buffer, unsigned char, sink->stream.bufferSize);

    return 1;
}

static int sdls_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    return 1;
}

static int sdls_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    SDLSink* sink = (SDLSink*)data;
    SDLStream* sdlStream;
    int status;
    int doneWaiting;
    
    if ((sdlStream = get_sdl_stream(sink, streamId)) == NULL)
    {
        ml_log_error("Unknown stream %d for sdl sink\n", streamId);
        return 0;
    }
    
    if (sdlStream->bufferSize != bufferSize)
    {
        ml_log_error("Requested buffer size %d differs from available buffer size\n", bufferSize, sdlStream->bufferSize);
        return 0;
    }

    /* wait for buffer to become available */
    doneWaiting = 0;    
    while (!sink->stopped && !doneWaiting)
    {
        PTHREAD_MUTEX_LOCK(&sink->stream.bufferMutex);
        if (!sink->stream.bufferWriteReady)
        {
            status = pthread_cond_wait(&sink->stream.bufferWriteReadyCond, &sink->stream.bufferMutex);
            if (status != 0)
            {
                ml_log_error("SDL sink failed to wait for buffer ready condition\n");
                /* TODO: don't try again? */ 
            }
        }
    
        if (sink->stream.bufferWriteReady)
        {
            doneWaiting = 1;
        }
        PTHREAD_MUTEX_UNLOCK(&sink->stream.bufferMutex);
    }
    
    *buffer = sdlStream->buffer;
    
    return 1;
}

static int sdls_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    SDLSink* sink = (SDLSink*)data;
    SDLStream* sdlStream;
    
    if ((sdlStream = get_sdl_stream(sink, streamId)) == NULL)
    {
        ml_log_error("Unknown stream %d for raw file output\n", streamId);
        return 0;
    }
    if (sdlStream->bufferSize != bufferSize)
    {
        ml_log_error("Buffer size (%d) != sdl stream buffer size (%d)\n", bufferSize, sdlStream->bufferSize);
        return 0;
    }
        
    sdlStream->isPresent = 1;

    return 1;
}

static int sdls_complete_frame(void* data, const FrameInfo* frameInfo)
{
    SDLSink* sink = (SDLSink*)data;
    struct timeval timeNow;
    long requiredUsec;
    long durationSlept;

    if (sink->stream.isPresent)
    {
        SDL_LockYUVOverlay(sink->stream.overlay);
        
        switch (sink->stream.streamInfo.format)
        {
            case UYVY_FORMAT:
                memcpy(sink->stream.overlay->pixels[0], sink->stream.buffer, sink->stream.bufferSize);
                break;
            default: /* YUV420_FORMAT */
                /* Y - V - U planes */
                memcpy(sink->stream.overlay->pixels[0], sink->stream.buffer, 
                    sink->stream.streamInfo.width * sink->stream.streamInfo.height);
                memcpy(sink->stream.overlay->pixels[1], sink->stream.buffer + sink->stream.streamInfo.width * sink->stream.streamInfo.height * 5 / 4, 
                    sink->stream.streamInfo.width * sink->stream.streamInfo.height / 4);
                memcpy(sink->stream.overlay->pixels[2], sink->stream.buffer + sink->stream.streamInfo.width * sink->stream.streamInfo.height, 
                    sink->stream.streamInfo.width * sink->stream.streamInfo.height / 4);
                break;
        }

        SDL_UnlockYUVOverlay(sink->stream.overlay);
        
        
        /* wait until it is time to display this frame (@ 25 fps) */
        
        gettimeofday(&timeNow, NULL);
        durationSlept = 0;
        if (frameInfo->rateControl)
        {
            durationSlept = sleep_diff(40 * 1000, &timeNow, &sink->lastFrameTime);
        }

        
        /* display the frame by sending a SDL event */
        
        PTHREAD_MUTEX_LOCK(&sink->stream.bufferMutex);
        sink->stream.bufferWriteReady = 0;
        PTHREAD_MUTEX_UNLOCK(&sink->stream.bufferMutex);
        
        SDL_UserEvent displayEvent;
        displayEvent.type = DISPLAY_FRAME_EVENT;
        displayEvent.code = 0;
        displayEvent.data1 = NULL;
        displayEvent.data2 = NULL;
        if (SDL_PushEvent((SDL_Event*)&displayEvent) != 0)
        {
            ml_log_error("Failed to push frame display event on SDL event queue: %s\n", SDL_GetError());
        }
        
        
        /* set the time that this frame was displayed */
        
        if (frameInfo->rateControl)
        {
            if (durationSlept < - MAX_FRAME_RATE_SLIPPAGE)
            {
                /* reset rate control when slipped by more than MAX_FRAME_RATE_SLIPPAGE */
                sink->lastFrameTime = timeNow;
            }
            else
            {
                /* set what the frame's display time should have been */
                requiredUsec = sink->lastFrameTime.tv_sec * 1000000 + sink->lastFrameTime.tv_usec + 40 * 1000;
                sink->lastFrameTime.tv_usec = requiredUsec % 1000000;
                sink->lastFrameTime.tv_sec = requiredUsec / 1000000;
            }
        }
        else
        {
            gettimeofday(&sink->lastFrameTime, NULL);
        }
    }
    else
    {
        gettimeofday(&sink->lastFrameTime, NULL);
    }
    
    msl_frame_displayed(sink->listener, frameInfo);

    reset_streams(sink);
    
    return 1;    
}

static void sdls_cancel_frame(void* data)
{
    SDLSink* sink = (SDLSink*)data;

    reset_streams(sink);
}

static int sdls_mute_audio(void* data, int mute)
{
    SDLSink* sink = (SDLSink*)data;

    if (mute < 0)
    {
        sink->muteAudio = !sink->muteAudio;
    }
    else
    {
        sink->muteAudio = mute;
    }
    
    return 1;
}

static void sdls_close(void* data)
{
    SDLSink* sink = (SDLSink*)data;

    if (data == NULL)
    {
        return;
    }

    sink->stopped = 1;
    
    PTHREAD_MUTEX_LOCK(&sink->stream.bufferMutex);
    pthread_cond_broadcast(&sink->stream.bufferWriteReadyCond);
    PTHREAD_MUTEX_UNLOCK(&sink->stream.bufferMutex);

    join_thread(&sink->eventThread, NULL, NULL);
    
    if (sink->stream.streamId >= 0)
    {
        SDL_FreeYUVOverlay(sink->stream.overlay);
        SAFE_FREE(&sink->stream.buffer);
    }
    
    destroy_cond_var(&sink->stream.bufferWriteReadyCond);
    destroy_mutex(&sink->stream.bufferMutex);
    
    destroy_mutex(&sink->eventMutex);
    
    SAFE_FREE(&sink);

    SDL_Quit();
}


int sdls_open(SDLSink** sink)
{
    SDLSink* newSink;

    /* initialise the SDL library */    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        ml_log_error("Failed to initialise the SDL library: %s\n", SDL_GetError());
        return 0;
    }
    
    CALLOC_ORET(newSink, SDLSink, 1);
    newSink->stream.streamId = -1;
    newSink->stream.bufferWriteReady = 1;
    
    newSink->mediaSink.data = newSink;
    newSink->mediaSink.register_listener = sdls_register_listener;
    newSink->mediaSink.unregister_listener = sdls_unregister_listener;
    newSink->mediaSink.accept_stream = sdls_accept_stream;
    newSink->mediaSink.register_stream = sdls_register_stream;
    newSink->mediaSink.accept_stream_frame = sdls_accept_stream_frame;
    newSink->mediaSink.get_stream_buffer = sdls_get_stream_buffer;
    newSink->mediaSink.receive_stream_frame = sdls_receive_stream_frame;
    newSink->mediaSink.mute_audio = sdls_mute_audio;
    newSink->mediaSink.complete_frame = sdls_complete_frame;
    newSink->mediaSink.cancel_frame = sdls_cancel_frame;
    newSink->mediaSink.close = sdls_close;

    
    CHK_OFAIL(init_mutex(&newSink->stream.bufferMutex));
    CHK_OFAIL(init_cond_var(&newSink->stream.bufferWriteReadyCond));
    
    CHK_OFAIL(init_mutex(&newSink->eventMutex));
    CHK_OFAIL(create_joinable_thread(&newSink->eventThread, process_events_thread, newSink));
    
    
    *sink = newSink;
    return 1;

fail:
    sdls_close(newSink);
    return 0;
}

MediaSink* sdls_get_media_sink(SDLSink* sink)
{
    return &sink->mediaSink;
}

void sdls_register_window_listener(SDLSink* sink, X11WindowListener* listener)
{
    PTHREAD_MUTEX_LOCK(&sink->eventMutex) /* wait until events have been processed */
    sink->windowListener = listener;
    PTHREAD_MUTEX_UNLOCK(&sink->eventMutex)
}

void sdls_unregister_window_listener(SDLSink* sink, X11WindowListener* listener)
{
    PTHREAD_MUTEX_LOCK(&sink->eventMutex) /* wait until events have been processed */
    if (sink->windowListener == listener)
    {
        sink->windowListener = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&sink->eventMutex)
}




#endif


