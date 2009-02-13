/*
 * $Id: x11_xv_display_sink.c,v 1.9 2009/02/13 10:17:44 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Stuart Cunningham
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
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>


#include "x11_xv_display_sink.h"
#include "x11_common.h"
#include "keyboard_input_connect.h"
#include "on_screen_display.h"
#include "YUV_frame.h"
#include "YUV_small_pic.h"
#include "video_conversion.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define VLC_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )

#define X11_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )
#define VLC2X11_FOURCC( i ) \
        X11_FOURCC( ((char *)&i)[0], ((char *)&i)[1], ((char *)&i)[2], \
                    ((char *)&i)[3] )
#define X112VLC_FOURCC( i ) \
        VLC_FOURCC( i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, \
                    (i >> 24) & 0xff )


#define MAX_TIMECODES               64


typedef struct
{
    struct X11XVDisplaySink* sink;

    MediaSinkFrame sinkFrame;

    unsigned char* inputBuffer;
    int inputBufferSize;

    unsigned char* scaleInputBuffer;
    unsigned char* scaleWorkspace;

    XvImage* yuv_image;
    XShmSegmentInfo yuv_shminfo;

    int streamId;
    int videoIsPresent;

    OnScreenDisplayState* osdState;

    FrameInfo frameInfo;
} X11DisplayFrame;

struct X11XVDisplaySink
{
    /* common X11 structure */
    X11Common x11Common;
    int displayInitialised;
    int displayInitFailed;

    /* media sink interface */
    MediaSink mediaSink;

    /* on screen display */
    OnScreenDisplay* osd;
    OSDListener osdListener;
    int osdInitialised;

    /* listener for sink events */
    MediaSinkListener* listener;


    /* display stuff */
    int xvport;
    int useSharedMemory;

    /* image characteristics */
    int32_t frameFormat;
    StreamFormat inputVideoFormat;
    StreamFormat outputVideoFormat;
    formats outputYUVFormat;
    int frameSize;
    int width;
    int height;
    int inputWidth;
    int inputHeight;
    Rational aspectRatio;
    Rational pixelAspectRatio;
    Rational monitorAspectRatio;
    float scale;
    int swScale;
    int initialDisplayWidth;
    int initialDisplayHeight;


    /* video input data */
    X11DisplayFrame* frame;

    /* used for rate control */
    struct timeval lastFrameTime;

    /* set if sink was reset */
    int haveReset;
};


static int xvsk_allocate_frame(void* data, MediaSinkFrame** frame);
static int xvsk_accept_stream(void* data, const StreamInfo* streamInfo);


static int XVideoGetPort( Display *p_display,
                          int32_t i_chroma,
                          int i_requested_adaptor)      // -1 for autoscan
{
    XvAdaptorInfo *p_adaptor;
    unsigned int i;
    unsigned int i_adaptor, i_num_adaptors;
    int i_selected_port;

    switch( XvQueryExtension( p_display, &i, &i, &i, &i, &i ) )
    {
        case Success:
            break;

        case XvBadExtension:
            ml_log_error("XvBadExtension\n" );
            return -1;

        case XvBadAlloc:
            ml_log_error("XvBadAlloc\n" );
            return -1;

        default:
            ml_log_error("XvQueryExtension failed\n" );
            return -1;
    }

    switch( XvQueryAdaptors( p_display,
                             DefaultRootWindow( p_display ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            ml_log_error("XvBadExtension for XvQueryAdaptors\n" );
            return -1;

        case XvBadAlloc:
            ml_log_error("XvBadAlloc for XvQueryAdaptors\n" );
            return -1;

        default:
            ml_log_error("XvQueryAdaptors failed\n" );
            return -1;
    }

    i_selected_port = -1;

    for( i_adaptor = 0; i_adaptor < i_num_adaptors; ++i_adaptor )
    {
        XvImageFormatValues *p_formats;
        int i_format, i_num_formats;
        int i_port;

        /* If we requested an adaptor and it's not this one, we aren't
         * interested */
        if( i_requested_adaptor != -1 && i_adaptor != (unsigned int)i_requested_adaptor )
        {
            continue;
        }

        /* If the adaptor doesn't have the required properties, skip it */
        if( !( p_adaptor[ i_adaptor ].type & XvInputMask ) ||
            !( p_adaptor[ i_adaptor ].type & XvImageMask ) )
        {
            continue;
        }

        /* Check that adaptor supports our requested format... */
        p_formats = XvListImageFormats( p_display,
                                        p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0;
             i_format < i_num_formats && ( i_selected_port == -1 );
             i_format++ )
        {
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;

            /* Matching chroma? */
            if( p_formats[ i_format ].id != i_chroma )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < (int)(p_adaptor[i_adaptor].base_id
                                   + p_adaptor[i_adaptor].num_ports) )
                   && ( i_selected_port == -1 );
                 i_port++ )
            {
                if( XvGrabPort( p_display, i_port, CurrentTime )
                     == Success )
                {
                    i_selected_port = i_port;
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            ml_log_info("X11 display adaptor %i, port %i, format 0x%x (%4.4s) %s\n",
                     i_adaptor, i_selected_port, p_formats[ i_format ].id,
                     (char *)&p_formats[ i_format ].id,
                     ( p_formats[ i_format ].format == XvPacked ) ?
                         "packed" : "planar" );

            /* Make sure XV_AUTOPAINT_COLORKEY is set */
            p_attr = XvQueryPortAttributes( p_display,
                                            i_selected_port,
                                            &i_num_attributes );

            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                if( !strcmp( p_attr[i_attr].name, "XV_AUTOPAINT_COLORKEY" ) )
                {
                    const Atom autopaint =
                        XInternAtom( p_display,
                                     "XV_AUTOPAINT_COLORKEY", False );
                    XvSetPortAttribute( p_display,
                                        i_selected_port, autopaint, 1 );
                    break;
                }
            }

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
        }

        if( p_formats != NULL )
        {
            XFree( p_formats );
        }

    }

    if( i_num_adaptors > 0 )
    {
        XvFreeAdaptorInfo( p_adaptor );
    }

    if( i_selected_port == -1 )
    {
        int i_chroma_tmp = X112VLC_FOURCC( i_chroma );
        if( i_requested_adaptor == -1 )
        {
            ml_log_error("no free XVideo port found for format: "
                      "0x%.8x (%4.4s)\n", i_chroma_tmp, (char*)&i_chroma_tmp );
        }
        else
        {
            ml_log_error("XVideo adaptor %i does not have a free: "
                      "XVideo port for format 0x%.8x (%4.4s)\n",
                      i_requested_adaptor, i_chroma_tmp, (char*)&i_chroma_tmp );
        }
    }

    return i_selected_port;
}

static void XVideoReleasePort( Display *p_display, int i_port )
{
    XvUngrabPort( p_display, i_port, CurrentTime );
    XSync(p_display, False); //required when X is tunnelling through SSH, or immediately calling XvGrabPort() will return XvAlreadyGrabbed
}

static void reset_streams(X11DisplayFrame* frame)
{
    frame->videoIsPresent = 0;
}

static int init_display(X11XVDisplaySink* sink, const StreamInfo* streamInfo)
{
    int screenWidth;
    int screenHeight;
    double wFactor;
    int widthForAspectRatio;
    int heightForAspectRatio;

    if (sink->displayInitialised)
    {
        return 1;
    }

    CHK_OFAIL(x11c_prepare_display(&sink->x11Common));

    sink->inputWidth = streamInfo->width;
    sink->inputHeight = streamInfo->height;

    sink->width = sink->inputWidth / sink->swScale;
    sink->height = sink->inputHeight / sink->swScale;

    if (streamInfo->format == UYVY_FORMAT)
    {
        sink->outputYUVFormat = UYVY;
    }
    else if (streamInfo->format == YUV422_FORMAT)
    {
        /* format is converted to UYVY */
        sink->outputYUVFormat = UYVY;
    }
    else if (streamInfo->format == YUV420_FORMAT)
    {
        sink->outputYUVFormat = I420;
    }
    else /* streamInfo->format == YUV444_FORMAT */
    {
        /* format is converted to UYVY */
        sink->outputYUVFormat = UYVY;
    }

    if (streamInfo->aspectRatio.num > 0 && streamInfo->aspectRatio.den > 0)
    {
        sink->aspectRatio = streamInfo->aspectRatio;

        /* TODO: we shouldn't assume 702 for 720x576/592 */
        if (sink->aspectRatio.num == 4 && sink->aspectRatio.den == 3 &&
            sink->width == 720 &&
            (sink->height == 576 || sink->height == 592))
        {
            widthForAspectRatio = 702;
        }
        else
        {
            widthForAspectRatio = sink->width;
        }

        if (sink->width == 720 && sink->height == 592)
        {
            /* don't include the VBI */
            heightForAspectRatio = 576;
        }
        else
        {
            heightForAspectRatio = sink->height;
        }

        if (sink->pixelAspectRatio.num <= 0 || sink->pixelAspectRatio.den <= 0)
        {
            CHK_OFAIL(x11c_get_screen_dimensions(&sink->x11Common, &screenWidth, &screenHeight));

            /* take into account monitor pixel aspect ratio,
            intended output aspect ratio (sink->aspectRatio)
            and the image dimensions (sink->width and sink->height) */

            if (sink->monitorAspectRatio.num > 0 && sink->monitorAspectRatio.den > 0)
            {
                wFactor = (sink->monitorAspectRatio.den * screenWidth * sink->aspectRatio.num * heightForAspectRatio) /
                        (double)(sink->monitorAspectRatio.num * screenHeight * sink->aspectRatio.den * widthForAspectRatio);
            }
            else
            {
                /* assume 4:3 monitor aspect ratio */
                wFactor = (3 * screenWidth * sink->aspectRatio.num * heightForAspectRatio) /
                        (double)(4 * screenHeight * sink->aspectRatio.den * widthForAspectRatio);
            }
        }
        else
        {
            /* take into account monitor pixel aspect ratio,
            intended output aspect ratio (sink->aspectRatio)
            and the image dimensions (sink->width and sink->height) */
            wFactor = (sink->pixelAspectRatio.den * sink->aspectRatio.num * heightForAspectRatio) /
                    (double)(sink->pixelAspectRatio.num * sink->aspectRatio.den * widthForAspectRatio);
        }


        /* scale in horizontal direction only to avoid dealing with interlacing */
        sink->initialDisplayWidth = (int)(wFactor * sink->width);
        sink->initialDisplayHeight = sink->height;

        sink->initialDisplayWidth *= sink->scale;
        sink->initialDisplayHeight *= sink->scale;
    }
    else
    {
        sink->aspectRatio.num = 4;
        sink->aspectRatio.den = 3;

        sink->initialDisplayWidth = sink->width;
        sink->initialDisplayHeight = sink->height;
    }

    sink->inputVideoFormat = streamInfo->format;

    if (streamInfo->format == UYVY_FORMAT)
    {
        sink->frameFormat = X11_FOURCC('U','Y','V','Y');
        sink->frameSize = sink->width * sink->height * 2;
        sink->outputVideoFormat = streamInfo->format;
    }
    else if (streamInfo->format == YUV422_FORMAT ||
        streamInfo->format == YUV444_FORMAT)
    {
        sink->frameFormat = X11_FOURCC('U','Y','V','Y');
        sink->frameSize = sink->width * sink->height * 2;
        sink->outputVideoFormat = UYVY_FORMAT;
    }
    else /* YUV420_FORMAT */
    {
        sink->frameFormat = X11_FOURCC('I','4','2','0');
        sink->frameSize = sink->width * sink->height * 3 / 2;
        sink->outputVideoFormat = streamInfo->format;
    }


    /* Check that we have access to an XVideo port providing this chroma    */
    /* Commonly supported chromas: YV12, I420, YUY2, YUY2                   */
    sink->xvport = XVideoGetPort(sink->x11Common.windowInfo.display, sink->frameFormat, -1);
    if (sink->xvport < 0)
    {
        ml_log_error("Cannot find an xv port for requested video format\n");
        goto fail;
    }

    /* check if we can use shared memory */
    sink->useSharedMemory = x11c_shared_memory_available(&sink->x11Common);

    CHK_OFAIL(x11c_init_window(&sink->x11Common, sink->initialDisplayWidth, sink->initialDisplayHeight, sink->width, sink->height));

    sink->displayInitialised = 1;
    sink->displayInitFailed = 0;
    return 1;

fail:
    sink->displayInitialised = 0;
    sink->displayInitFailed = 1;
    return 0;
}

static int display_frame(X11XVDisplaySink* sink, X11DisplayFrame* frame, const FrameInfo* frameInfo)
{
    struct timeval timeNow;
    long requiredUsec;
    long durationSlept;
    unsigned int windowWidth;
    unsigned int windowHeight;
    float scaleFactorX;
    float scaleFactorY;
    float scaleFactor;
    YUV_frame inputFrame;
    YUV_frame outputFrame;
    unsigned char* convertOutputBuffer;
    int frameDurationMsec;
    int frameSlippage;

    frameDurationMsec = (int)(1000 * frameInfo->frameRate.den / (double)(frameInfo->frameRate.num));
    frameSlippage = frameDurationMsec * 1000 / 2; /* half a frame */


    if (frame->videoIsPresent)
    {
        /* convert if required */
        if (sink->inputVideoFormat == YUV444_FORMAT)
        {
            if (sink->swScale != 1)
            {
                /* scale afterwards */
                convertOutputBuffer = frame->scaleInputBuffer;
            }
            else
            {
                /* no scale afterwards */
                convertOutputBuffer = (unsigned char*)frame->yuv_image->data;
            }

            yuv444_to_uyvy(sink->inputWidth, sink->inputHeight, frame->inputBuffer, convertOutputBuffer);
        }
        else if (sink->inputVideoFormat == YUV422_FORMAT)
        {
            if (sink->swScale != 1)
            {
                /* scale afterwards */
                convertOutputBuffer = frame->scaleInputBuffer;
            }
            else
            {
                /* no scale afterwards */
                convertOutputBuffer = (unsigned char*)frame->yuv_image->data;
            }

            yuv422_to_uyvy_2(sink->inputWidth, sink->inputHeight, 0, frame->inputBuffer, convertOutputBuffer);
        }
        else
        {
            /* no conversion - scale input if from the frame input buffer */
            convertOutputBuffer = frame->inputBuffer;
        }

        /* scale image */
        if (sink->swScale != 1)
        {
            YUV_frame_from_buffer(&inputFrame, (void*)convertOutputBuffer,
                sink->inputWidth, sink->inputHeight, sink->outputYUVFormat);

            YUV_frame_from_buffer(&outputFrame, (void*)(unsigned char*)frame->yuv_image->data,
                sink->width, sink->height, sink->outputYUVFormat);

            small_pic(&inputFrame,
                &outputFrame,
                0,
                0,
                sink->swScale,
                sink->swScale,
                1,
                1,
                1,
                frame->scaleWorkspace);
        }

        /* add OSD to frame */
        if (sink->osd != NULL && sink->osdInitialised)
        {
            if (!osd_add_to_image(sink->osd, frameInfo, (unsigned char*)frame->yuv_image->data,
                frame->yuv_image->width, frame->yuv_image->height))
            {
                ml_log_error("Failed to add OSD to frame\n");
                /* continue anyway */
            }
        }

        /* wait until it is time to display this frame */
        gettimeofday(&timeNow, NULL);
        durationSlept = 0;
        if (frameInfo->rateControl)
        {
            durationSlept = sleep_diff(frameDurationMsec * 1000, &timeNow, &sink->lastFrameTime);
        }

        /* adjust the display width/height if the window has been resized */
        windowWidth = sink->x11Common.windowWidth;
        windowHeight = sink->x11Common.windowHeight;

        scaleFactorX = windowWidth / (float)(sink->initialDisplayWidth);
        scaleFactorY = windowHeight / (float)(sink->initialDisplayHeight);
        scaleFactor = (scaleFactorX < scaleFactorY) ? scaleFactorX : scaleFactorY;

        sink->x11Common.displayWidth = sink->initialDisplayWidth * scaleFactor;
        sink->x11Common.displayHeight = sink->initialDisplayHeight * scaleFactor;


        if (sink->useSharedMemory)
        {
            XvShmPutImage(sink->x11Common.windowInfo.display, sink->xvport, sink->x11Common.windowInfo.window,
                sink->x11Common.windowInfo.gc, frame->yuv_image,
                0, 0, frame->yuv_image->width, frame->yuv_image->height,
                0, 0, sink->x11Common.displayWidth, sink->x11Common.displayHeight,
                False);
        }
        else
        {
            XvPutImage(sink->x11Common.windowInfo.display, sink->xvport, sink->x11Common.windowInfo.window,
                sink->x11Common.windowInfo.gc, frame->yuv_image,
                0, 0, frame->yuv_image->width, frame->yuv_image->height,
                0, 0, sink->x11Common.displayWidth, sink->x11Common.displayHeight);
        }

        x11c_process_events(&sink->x11Common, 1);


        /* report that a new frame has been displayed */
        msl_frame_displayed(sink->listener, frameInfo);


        /* set the time that this frame was displayed */
        if (frameInfo->rateControl)
        {
            if (durationSlept < - frameSlippage)
            {
                /* reset rate control when slipped by more than frameSlippage */
                sink->lastFrameTime = timeNow;
            }
            else
            {
                /* set what the frame's display time should have been */
                requiredUsec = sink->lastFrameTime.tv_sec * 1000000 + sink->lastFrameTime.tv_usec + frameDurationMsec * 1000;
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


    reset_streams(frame);

    return 1;
}


static int init_frame(X11DisplayFrame* frame)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)frame->sink;
    int result = 1;

    PTHREAD_MUTEX_LOCK(&sink->x11Common.eventMutex)

    if (sink->inputVideoFormat == YUV444_FORMAT)
    {
        /* Conversion required for YUV444 input */
        frame->inputBufferSize = sink->inputWidth * sink->inputHeight * 3;
        MALLOC_ORET(frame->inputBuffer, unsigned char, frame->inputBufferSize);

        /* buffer for software scaling */
        if (sink->swScale != 1)
        {
            MALLOC_ORET(frame->scaleInputBuffer, unsigned char, frame->inputBufferSize);
            MALLOC_ORET(frame->scaleWorkspace, unsigned char, sink->inputWidth * 3);
        }
    }
    else if (sink->inputVideoFormat == YUV422_FORMAT)
    {
        /* Conversion required for YUV422 input */
        frame->inputBufferSize = sink->inputWidth * sink->inputHeight * 2;
        MALLOC_ORET(frame->inputBuffer, unsigned char, frame->inputBufferSize);

        /* buffer for software scaling */
        if (sink->swScale != 1)
        {
            MALLOC_ORET(frame->scaleInputBuffer, unsigned char, frame->inputBufferSize);
            MALLOC_ORET(frame->scaleWorkspace, unsigned char, sink->inputWidth * 3);
        }
    }
    else if (sink->swScale != 1)
    {
        /* buffer for software scaling */

        if (sink->inputVideoFormat == UYVY_FORMAT)
        {
            frame->inputBufferSize = sink->inputWidth * sink->inputHeight * 2;
        }
        else /* sink->inputVideoFormat == YUV420_FORMAT */
        {
            frame->inputBufferSize = sink->inputWidth * sink->inputHeight * 3 / 2;
        }
        MALLOC_ORET(frame->inputBuffer, unsigned char, frame->inputBufferSize);
        MALLOC_ORET(frame->scaleWorkspace, unsigned char, sink->inputWidth * 3);
    }
    /* else frame->inputBuffer = (unsigned char*)frame->yuv_image->data;
    delayed until after creating yuv_image */

    if (sink->useSharedMemory)
    {
        frame->yuv_image = XvShmCreateImage(sink->x11Common.windowInfo.display, sink->xvport,
            sink->frameFormat, NULL, sink->width, sink->height, &frame->yuv_shminfo);
        if (frame->yuv_image->data_size != sink->frameSize)
        {
            ml_log_error("XV frame size %d does not match required frame size %d\n",
                frame->yuv_image->data_size, sink->frameSize);
            result = 0;
        }
        else
        {
            frame->yuv_shminfo.shmid = shmget(IPC_PRIVATE, frame->yuv_image->data_size,
                                        IPC_CREAT | 0777);
            frame->yuv_image->data = (char*)shmat(frame->yuv_shminfo.shmid, 0, 0);
            frame->yuv_shminfo.shmaddr = frame->yuv_image->data;
            frame->yuv_shminfo.readOnly = False;

            result = XShmAttach(sink->x11Common.windowInfo.display, &frame->yuv_shminfo);
            if (result)
            {
                XSync(sink->x11Common.windowInfo.display, False);
                shmctl(frame->yuv_shminfo.shmid, IPC_RMID, 0);
            }
            else
            {
                ml_log_error("Failed to attached shared memory to display\n");
            }
        }
    }
    else
    {
        frame->yuv_image = XvCreateImage(sink->x11Common.windowInfo.display, sink->xvport, sink->frameFormat,
            NULL, sink->width, sink->height);
        if (frame->yuv_image->data_size != sink->frameSize)
        {
            ml_log_error("XV frame size %d does not match required frame size %d\n",
                frame->yuv_image->data_size, sink->frameSize);
            result = 0;
        }
        else
        {
            frame->yuv_image->data = malloc(frame->yuv_image->data_size);
            result = 1;
        }

    }

    /* input buffer == output if no scaling and no conversion */
    if (sink->inputVideoFormat != YUV444_FORMAT &&
        sink->inputVideoFormat != YUV422_FORMAT &&
        sink->swScale == 1)
    {
        frame->inputBufferSize = frame->yuv_image->data_size;
        frame->inputBuffer = (unsigned char*)frame->yuv_image->data;
    }


    PTHREAD_MUTEX_UNLOCK(&sink->x11Common.eventMutex)

    if (!result)
    {
        return 0;
    }

    return 1;
}

static int xvskf_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;
    X11XVDisplaySink* sink = (X11XVDisplaySink*)frame->sink;
    StreamInfo outputStreamInfo;

    /* this should've been checked, but we do it here anyway */
    if (!xvsk_accept_stream(sink, streamInfo))
    {
        ml_log_error("Can't register a stream %d that is not accepted\n", streamId);
        return 0;
    }

    if (streamInfo->type == PICTURE_STREAM_TYPE)
    {
        if (frame->streamId >= 0)
        {
            /* only 1 video stream supported */
            return 0;
        }

        if (sink->displayInitFailed)
        {
            /* if it failed before then don't try agin */
            return 0;
        }

        if (!sink->displayInitialised)
        {
            CHK_ORET(init_display(sink, streamInfo));
        }
        else if (sink->haveReset)
        {
            /* check dimensions */
            /* TODO: allow dimensions to change */
            if (sink->inputWidth != streamInfo->width || sink->inputHeight != streamInfo->height)
            {
                ml_log_error("Image dimensions, %dx%d, does not match previous dimensions %dx%d\n",
                    streamInfo->width, streamInfo->height, sink->inputWidth, sink->inputHeight);
                return 0;
            }
        }

        if (sink->osd != NULL && !sink->osdInitialised)
        {
            outputStreamInfo = *streamInfo;

            /* YUV422 and YUV444 are converted to UYVY */
            if (streamInfo->format == YUV422_FORMAT ||
                streamInfo->format == YUV444_FORMAT)
            {
                outputStreamInfo.format = UYVY_FORMAT;
            }

            /* sw scale changes the output dimensions */
            outputStreamInfo.width = sink->width;
            outputStreamInfo.height = sink->height;

            CHK_ORET(osd_initialise(sink->osd, &outputStreamInfo, &sink->aspectRatio));
            sink->osdInitialised = 1;
        }

        if (frame->yuv_image == NULL)
        {
            CHK_ORET(init_frame(frame));
        }

        frame->streamId = streamId;
    }
    else
    {
        return 0;
    }

    return 1;
}

static void xvskf_reset(void* data)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;

    reset_streams(frame);
}

static int xvskf_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;

    if (streamId == frame->streamId)
    {
        return 1;
    }

    return 0;
}

static int xvskf_allocate_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;

    if (streamId != frame->streamId)
    {
        ml_log_error("Can't return display sink buffer for unknown stream %d \n", streamId);
        return 0;
    }


    if ((unsigned int)frame->inputBufferSize != bufferSize)
    {
        ml_log_error("Buffer size (%d) != image data size (%d)\n", bufferSize, frame->inputBufferSize);
        return 0;
    }
    *buffer = frame->inputBuffer;

    return 1;
}

static int xvskf_set_is_present(void* data, int streamId)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;

    if (streamId != frame->streamId)
    {
        ml_log_error("Can't write frame for display sink buffer for unknown stream %d \n", streamId);
        return 0;
    }

    frame->videoIsPresent = 1;

    return 1;
}

static int xvskf_complete_frame(void* data, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;

    osd_set_state(osds_get_osd(frame->osdState), osdState);
    frame->frameInfo = *frameInfo;
    return 1;
}

static void xvskf_free(void* data)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;

    if (frame == NULL)
    {
        return;
    }

    if (frame->sink != NULL && frame->sink->x11Common.windowInfo.display != NULL)
    {
        if (frame->yuv_image != NULL)
        {
            if (frame->inputBuffer != (unsigned char*)frame->yuv_image->data)
            {
                /* input buffer is not pointing to the output buffer */
                SAFE_FREE(&frame->inputBuffer);
            }
            SAFE_FREE(&frame->scaleInputBuffer);
            SAFE_FREE(&frame->scaleWorkspace);

            if (frame->sink->useSharedMemory)
            {
                XShmDetach(frame->sink->x11Common.windowInfo.display, &frame->yuv_shminfo);
                shmdt(frame->yuv_shminfo.shmaddr);
            }
            else
            {
                SAFE_FREE(&frame->yuv_image->data);
            }
            XFree(frame->yuv_image);
        }
    }

    osds_free(&frame->osdState);

    SAFE_FREE(&frame);
}

static int allocate_frame(X11XVDisplaySink* sink, X11DisplayFrame** frame)
{
    X11DisplayFrame* newFrame = NULL;

    CALLOC_ORET(newFrame, X11DisplayFrame, 1);

    CHK_OFAIL(osds_create(&newFrame->osdState));

    newFrame->sink = sink;
    newFrame->streamId = -1;

    newFrame->sinkFrame.data = newFrame;
    newFrame->sinkFrame.register_stream = xvskf_register_stream;
    newFrame->sinkFrame.reset = xvskf_reset;
    newFrame->sinkFrame.accept_stream_frame = xvskf_accept_stream_frame;
    newFrame->sinkFrame.allocate_stream_buffer = xvskf_allocate_stream_buffer;
    newFrame->sinkFrame.set_is_present = xvskf_set_is_present;
    newFrame->sinkFrame.complete_frame = xvskf_complete_frame;
    newFrame->sinkFrame.free = xvskf_free;

    *frame = newFrame;
    return 1;

fail:
    xvskf_free(newFrame);
    return 0;
}




static int xvsk_register_listener(void* data, MediaSinkListener* listener)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    sink->listener = listener;
    sink->x11Common.sinkListener = listener;
    return 1;
}

static void xvsk_unregister_listener(void* data, MediaSinkListener* listener)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    if (sink->listener == listener)
    {
        sink->listener = NULL;
        sink->x11Common.sinkListener = NULL;
    }
}



static int xvsk_accept_stream(void* data, const StreamInfo* streamInfo)
{
    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        streamInfo->width > 0 &&
        streamInfo->height > 0 &&
        (streamInfo->format == UYVY_FORMAT ||
             streamInfo->format == YUV422_FORMAT ||
             streamInfo->format == YUV420_FORMAT ||
             streamInfo->format == YUV444_FORMAT))
    {
        return 1;
    }
    else if (streamInfo->type == TIMECODE_STREAM_TYPE)
    {
        return 1;
    }
    return 0;
}

static int xvsk_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    if (sink->frame == NULL)
    {
        CHK_ORET(allocate_frame(sink, &sink->frame));
    }

    return xvskf_register_stream(sink->frame, streamId, streamInfo);
}

static int xvsk_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    return sink->displayInitialised && xvskf_accept_stream_frame(sink->frame, streamId, frameInfo);
}

static int xvsk_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    return xvskf_allocate_stream_buffer(sink->frame, streamId, bufferSize, buffer);
}

static int xvsk_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    return xvskf_set_is_present(sink->frame, streamId);
}

static int xvsk_complete_frame(void* data, const FrameInfo* frameInfo)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    return display_frame(sink, sink->frame, frameInfo);
}

static void xvsk_cancel_frame(void* data)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    /* do nothing? */

    /* set the current frame's (if there was one) display time */
    gettimeofday(&sink->lastFrameTime, NULL);

    reset_streams(sink->frame);
}

static OnScreenDisplay* xvsk_get_osd(void* data)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    return sink->osd;
}

static int xvsk_allocate_frame(void* data, MediaSinkFrame** frame)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;
    X11DisplayFrame* x11Frame = NULL;

    CHK_ORET(allocate_frame(sink, &x11Frame));

    *frame = &x11Frame->sinkFrame;
    return 1;
}

static int xvsk_complete_sink_frame(void* data, const MediaSinkFrame* frame)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;
    X11DisplayFrame* x11Frame = (X11DisplayFrame*)frame->data;

    if (sink->osd != NULL)
    {
        if (osd_set_state(sink->osd, x11Frame->osdState))
        {
            msl_refresh_required(sink->listener);
        }
    }

    return display_frame(sink, x11Frame, &x11Frame->frameInfo);
}

static void xvsk_close(void* data)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    if (data == NULL)
    {
        return;
    }

    /* free before x11c_clear */
    xvskf_free(sink->frame);

    if (sink->osd != NULL)
    {
        osd_free(sink->osd);
        sink->osd = NULL;
    }

    if (sink->xvport >= 0 && sink->x11Common.windowInfo.display != NULL)
    {
        XVideoReleasePort(sink->x11Common.windowInfo.display, sink->xvport);
    }
    x11c_clear(&sink->x11Common);

    SAFE_FREE(&sink);
}

static int xvsk_reset_or_close(void* data)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;
    X11DisplayFrame* blankFrame = NULL;
    FrameInfo blankFrameInfo;

    sink->haveReset = 1;

    /* free before reseting x11 */
    xvskf_free(sink->frame);
    sink->frame = NULL;

    if (!x11c_reset(&sink->x11Common))
    {
        goto fail;
    }
    if (sink->osd != NULL)
    {
        if (!osd_reset(sink->osd))
        {
            goto fail;
        }
    }
    sink->osdInitialised = 0;

    gettimeofday(&sink->lastFrameTime, NULL);

    /* display a blank frame */
    if (sink->displayInitialised)
    {
        if (!allocate_frame(sink, &blankFrame))
        {
            ml_log_warn("Failed to allocate blank frame at reset\n");
        }
        else
        {
            if (!init_frame(blankFrame))
            {
                ml_log_warn("Failed to initialise blank frame at reset\n");
            }
            else
            {
                fill_black(sink->inputVideoFormat, sink->inputWidth, sink->inputHeight, blankFrame->inputBuffer);
                blankFrame->videoIsPresent = 1;

                memset(&blankFrameInfo, 0, sizeof(FrameInfo));
                if (!display_frame(sink, blankFrame, &blankFrameInfo))
                {
                    ml_log_warn("Failed to display blank frame at reset\n");
                }
            }

            xvskf_free(blankFrame);
            blankFrame = NULL;
        }
    }

    return 1;

fail:
    xvsk_close(data);
    return 2;
}


static void xvsk_refresh_required(void* data)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    msl_refresh_required(sink->listener);
}

static void xvsk_osd_screen_changed(void* data, OSDScreen screen)
{
    X11XVDisplaySink* sink = (X11XVDisplaySink*)data;

    msl_osd_screen_changed(sink->listener, screen);
}


int xvsk_check_is_available()
{
    Display* display = NULL;

    /* open display */
    if ((display = XOpenDisplay(NULL)) == NULL)
    {
        return 0;
    }

    /* open XV port */
    int port;

    /* try UYVY */
    int32_t frameFormat = X11_FOURCC('U','Y','V','Y');
    if ((port = XVideoGetPort(display, frameFormat, -1)) < 0)
    {
        XCloseDisplay(display);
        return 0;
    }
    XVideoReleasePort(display, port);

    /* try YUV420 */
    frameFormat = X11_FOURCC('I','4','2','0');
    if ((port = XVideoGetPort(display, frameFormat, -1)) < 0)
    {
        XCloseDisplay(display);
        return 0;
    }
    XVideoReleasePort(display, port);

    XCloseDisplay(display);

    return 1;
}


int xvsk_open(int reviewDuration, int disableOSD, const Rational* pixelAspectRatio,
    const Rational* monitorAspectRatio, float scale, int swScale, X11WindowInfo* windowInfo, X11XVDisplaySink** sink)
{
    X11XVDisplaySink* newSink;

    CALLOC_ORET(newSink, X11XVDisplaySink, 1);

    newSink->xvport = -1;
    newSink->pixelAspectRatio = *pixelAspectRatio;
    newSink->monitorAspectRatio = *monitorAspectRatio;
    newSink->swScale = swScale;
    if (scale > 0.0f)
    {
        newSink->scale = scale;
    }
    else
    {
        newSink->scale = 1.0f;
    }

    newSink->mediaSink.data = newSink;
    newSink->mediaSink.register_listener = xvsk_register_listener;
    newSink->mediaSink.unregister_listener = xvsk_unregister_listener;
    newSink->mediaSink.accept_stream = xvsk_accept_stream;
    newSink->mediaSink.register_stream = xvsk_register_stream;
    newSink->mediaSink.accept_stream_frame = xvsk_accept_stream_frame;
    newSink->mediaSink.get_stream_buffer = xvsk_get_stream_buffer;
    newSink->mediaSink.receive_stream_frame = xvsk_receive_stream_frame;
    newSink->mediaSink.complete_frame = xvsk_complete_frame;
    newSink->mediaSink.cancel_frame = xvsk_cancel_frame;
    newSink->mediaSink.get_osd = xvsk_get_osd;
    newSink->mediaSink.allocate_frame = xvsk_allocate_frame;
    newSink->mediaSink.complete_sink_frame = xvsk_complete_sink_frame;
    newSink->mediaSink.reset_or_close = xvsk_reset_or_close;
    newSink->mediaSink.close = xvsk_close;

    if (!disableOSD)
    {
        CHK_OFAIL(osdd_create(&newSink->osd));

        newSink->osdListener.data = newSink;
        newSink->osdListener.refresh_required = xvsk_refresh_required;
        newSink->osdListener.osd_screen_changed = xvsk_osd_screen_changed;

        osd_set_listener(newSink->osd, &newSink->osdListener);
    }

    CHK_OFAIL(x11c_initialise(&newSink->x11Common, reviewDuration, newSink->osd, windowInfo));

    gettimeofday(&newSink->lastFrameTime, NULL);

    *sink = newSink;
    return 1;

fail:
    xvsk_close(newSink);
    return 0;

}

void xvsk_set_media_control(X11XVDisplaySink* sink, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control)
{
    x11c_set_media_control(&sink->x11Common, mapping, videoSwitch, control);
}

void xvsk_unset_media_control(X11XVDisplaySink* sink)
{
    x11c_unset_media_control(&sink->x11Common);
}

MediaSink* xvsk_get_media_sink(X11XVDisplaySink* sink)
{
    return &sink->mediaSink;
}

void xvsk_set_window_name(X11XVDisplaySink* sink, const char* name)
{
    x11c_set_window_name(&sink->x11Common, name);
}

void xvsk_register_window_listener(X11XVDisplaySink* sink, X11WindowListener* listener)
{
    x11c_register_window_listener(&sink->x11Common, listener);
}

void xvsk_unregister_window_listener(X11XVDisplaySink* sink, X11WindowListener* listener)
{
    x11c_unregister_window_listener(&sink->x11Common, listener);
}

void xvsk_register_keyboard_listener(X11XVDisplaySink* sink, KeyboardInputListener* listener)
{
    x11c_register_keyboard_listener(&sink->x11Common, listener);
}

void xvsk_unregister_keyboard_listener(X11XVDisplaySink* sink, KeyboardInputListener* listener)
{
    x11c_unregister_keyboard_listener(&sink->x11Common, listener);
}

void xvsk_register_progress_bar_listener(X11XVDisplaySink* sink, ProgressBarInputListener* listener)
{
    x11c_register_progress_bar_listener(&sink->x11Common, listener);
}

void xvsk_unregister_progress_bar_listener(X11XVDisplaySink* sink, ProgressBarInputListener* listener)
{
    x11c_unregister_progress_bar_listener(&sink->x11Common, listener);
}

void xvsk_register_mouse_listener(X11XVDisplaySink* sink, MouseInputListener* listener)
{
    x11c_register_mouse_listener(&sink->x11Common, listener);
}

void xvsk_unregister_mouse_listener(X11XVDisplaySink* sink, MouseInputListener* listener)
{
    x11c_unregister_mouse_listener(&sink->x11Common, listener);
}


