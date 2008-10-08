#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>   /* gettimeofday() */
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>


#include "x11_display_sink.h"
#include "x11_common.h"
#include "keyboard_input_connect.h"
#include "on_screen_display.h"
#include "video_conversion.h"
#include "YUV_frame.h"
#include "YUV_small_pic.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


                    
#define MAX_TIMECODES               64

/* frame rate control if the display get behind by 1/2 frame (40 * 1000 / 2)*/
#define MAX_FRAME_RATE_SLIPPAGE     20000


typedef struct 
{
    struct X11DisplaySink* sink;
    
    MediaSinkFrame sinkFrame;
    
    unsigned char* inputBuffer;
    unsigned int inputBufferSize;
    StreamFormat videoFormat;

    unsigned char* scaleBuffer;
    unsigned int scaleBufferSize;
    unsigned char* scaleWorkspace;
    
    unsigned char* rgbBuffer;
    unsigned int rgbBufferSize;
    
    XImage* xImage;
    XShmSegmentInfo shminfo;

    int streamId;
    int videoIsPresent;

    OnScreenDisplayState* osdState;
    
    FrameInfo frameInfo;
} X11DisplayFrame;

struct X11DisplaySink
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
    int depth;
    Visual* visual;
    int useSharedMemory;

    /* image characteristics */
    int inputWidth;
    int inputHeight;
    int width;
    int height;
    Rational aspectRatio;
    Rational pixelAspectRatio;
    Rational monitorAspectRatio;
    int displayWidth;
    int displayHeight;
    float scale;
    int swScale;
    StreamFormat videoFormat;
    formats yuvFormat;
    
    /* video input data */
    X11DisplayFrame* frame;
    
    /* used for rate control */
    struct timeval lastFrameTime;
    
    /* set if sink was reset */
    int haveReset;
    
    /* lookup tables for yuv -> rgb colour transform */
    short fY[256];
    short fVR[256];
    short fUG[256];
    short fVG[256];
    short fUB[256];
};


static int xsk_allocate_frame(void* data, MediaSinkFrame** frame);
static int xsk_accept_stream(void* data, const StreamInfo* streamInfo);


/*
R = Y + 1.13983 x V
G = Y − 0.39466 x U − 0.58060 x V
B = Y + 2.03211 x U
*/
static void init_lookup_tables(X11DisplaySink* sink)
{
    int i;
    
    /* Y values range from 16 to 235 */ 
    for (i = 0; i < 16; i++)
    {
        /* clip */
        sink->fY[i] = (short)((16 - 16) * 255 / 219.0 + 0.5);
    }
    for (i = 16; i < 236; i++)
    {
        sink->fY[i] = (short)((i - 16) * 255 / 219.0 + 0.5);
    }
    for (i = 236; i < 256; i++)
    {
        /* clip */
        sink->fY[i] = (short)((235 - 16) * 255 / 219.0 + 0.5);
    }
    
    /* U, V values range from 16 to 240 */ 
    for (i = 0; i < 16; i++)
    {
        /* clip */
        sink->fVR[i] = (short)(0.701 * (16 - 128) * 255 / 112.0 + 0.5);
        sink->fUG[i] = (short)(-0.114 * 0.886 * (16 - 128) * 255 / (112.0 * 0.587) + 0.5);
        sink->fVG[i] = (short)(-0.299 * 0.701 * (16 - 128) * 255 / (112.0 * 0.587) + 0.5);
        sink->fUB[i] = (short)(0.886 * (16 - 128) * 255 / 112.0 + 0.5);
    }
    for (i = 16; i < 241; i++)
    {
        sink->fVR[i] = (short)(0.701 * (i - 128) * 255 / 112.0 + 0.5);
        sink->fUG[i] = (short)(-0.114 * 0.886 * (i - 128) * 255 / (112.0 * 0.587) + 0.5);
        sink->fVG[i] = (short)(-0.299 * 0.701 * (i - 128) * 255 / (112.0 * 0.587) + 0.5);
        sink->fUB[i] = (short)(0.886 * (i - 128) * 255 / 112.0 + 0.5);
    }
    for (i = 241; i < 256; i++)
    {
        /* clip */
        sink->fVR[i] = (short)(0.701 * (240 - 128) * 255 / 112.0 + 0.5);
        sink->fUG[i] = (short)(-0.114 * 0.886 * (240 - 128) * 255 / (112.0 * 0.587) + 0.5);
        sink->fVG[i] = (short)(-0.299 * 0.701 * (240 - 128) * 255 / (112.0 * 0.587) + 0.5);
        sink->fUB[i] = (short)(0.886 * (240 - 128) * 255 / 112.0 + 0.5);
    }

}

#define UYV2RGB() \
    r = sink->fY[y] + sink->fVR[v]; \
    g = sink->fY[y] + sink->fUG[u] + sink->fVG[v]; \
    b = sink->fY[y] + sink->fUB[u]; \
    \
    r = (r < 0) ? 0 : r; \
    r = (r > 255) ? 255 : r; \
    g = (g < 0) ? 0 : g; \
    g = (g > 255) ? 255 : g; \
    b = (b < 0) ? 0 : b; \
    b = (b > 255) ? 255 : b; \
    \
    r *= rRatio; \
    g *= gRatio; \
    b *= bRatio; \
    \
    r &= sink->visual->red_mask; \
    g &= sink->visual->green_mask; \
    b &= sink->visual->blue_mask; \
    \
    if (sink->depth >= 24) \
    { \
        *rgb32 = r | g | b; \
        rgb32++; \
    } \
    else \
    { \
        *rgb16 = r | g | b; \
        rgb16++; \
    }

    
static int convertToRGB(X11DisplaySink* sink, StreamFormat inputFormat, const unsigned char* input, 
    unsigned char* rgb, int width, int height) 
{
    int h;
    int w;
    uint32_t* rgb32 = (uint32_t*)rgb;
    uint16_t* rgb16 = (uint16_t*)rgb;
    const unsigned char* yInput;
    const unsigned char* uInput;
    const unsigned char* vInput;
	double rRatio;
	double gRatio;
	double bRatio;
    unsigned char u;
    unsigned char y;
    unsigned char v;
    int r;
    int g;
    int b;

    rRatio = sink->visual->red_mask / 255.0;
    gRatio = sink->visual->green_mask / 255.0;
    bRatio = sink->visual->blue_mask / 255.0;

    if (inputFormat == UYVY_FORMAT)
    {
        uInput = input;
        yInput = input + 1;
        vInput = input + 2;
        
        for (h = 0; h < height; h++) 
        {
            for (w = 0; w < width / 2; w++)
            {
                y = *yInput;
                u = *uInput;
                v = *vInput;
    
                UYV2RGB();
                    
                yInput += 2;
                uInput += 4;
                vInput += 4;

                
                y = *yInput;
          
                UYV2RGB();

                yInput += 2;
            }
        }
    }
    else if (inputFormat == YUV422_FORMAT)
    {
        yInput = input;
        uInput = input + width * height;
        vInput = input + width * height * 3 / 2;
        
        for (h = 0; h < height; h++) 
        {
            for (w = 0; w < width / 2; w++)
            {
                y = *yInput;
                u = *uInput;
                v = *vInput;
    
                UYV2RGB();
                    
                yInput++;
                uInput++;
                vInput++;

                
                y = *yInput;
          
                UYV2RGB();

                yInput++;
            }
        }
    }
    else /* inputFormat == YUV420_FORMAT */
    {
        yInput = input;
        uInput = input + width * height;
        vInput = input + width * height * 5 / 4;
        
        for (h = 0; h < height; h++) 
        {
            for (w = 0; w < width / 2; w++)
            {
                y = *yInput;
                u = *uInput;
                v = *vInput;
    
                UYV2RGB();
                    
                yInput++;
                uInput++;
                vInput++;

                
                y = *yInput;
          
                UYV2RGB();

                yInput++;
            }
            
            if (h % 2 == 0)
            {
                uInput -= width / 2;
                vInput -= width / 2;
            }
        }
    }

    return 1;
}



static void reset_streams(X11DisplayFrame* frame)
{
    frame->videoIsPresent = 0;
}

static int init_display(X11DisplaySink* sink, const StreamInfo* streamInfo)
{
    if (sink->displayInitialised)
    {
        return 1;
    }
    
    sink->inputWidth = streamInfo->width;   
    sink->inputHeight = streamInfo->height;

    sink->width = sink->inputWidth / sink->swScale;   
    sink->height = sink->inputHeight / sink->swScale;

    sink->aspectRatio.num = 4;
    sink->aspectRatio.den = 3;
    sink->displayWidth = sink->width;
    sink->displayHeight = sink->height;
        
    if (streamInfo->format == UYVY_FORMAT)
    {
        sink->yuvFormat = UYVY;
    }
    else if (streamInfo->format == YUV422_FORMAT)
    {
        sink->yuvFormat = YUV422;
    }
    else /* streamInfo->format == YUV420_FORMAT */
    {
        sink->yuvFormat = I420;
    }
    
    CHK_OFAIL(x11c_open_display(&sink->x11Common));

    /* check if we can use shared memory */
    sink->useSharedMemory = x11c_shared_memory_available(&sink->x11Common);
    
    CHK_OFAIL(x11c_create_window(&sink->x11Common, sink->displayWidth, sink->displayHeight,
        sink->width, sink->height));

    sink->displayInitialised = 1;
    sink->displayInitFailed = 0;
    return 1;
    
fail:
    sink->displayInitialised = 0;
    sink->displayInitFailed = 1;
    return 0;
}

static int display_frame(X11DisplaySink* sink, X11DisplayFrame* frame, const FrameInfo* frameInfo)
{
    struct timeval timeNow;
    long requiredUsec;
    long durationSlept;
    YUV_frame inputFrame;
    YUV_frame outputFrame;
    unsigned char* rgbInputBuffer;

    if (frame->videoIsPresent)
    {
        /* scale image */
        if (sink->swScale != 1)
        {
            YUV_frame_from_buffer(&inputFrame, (void*)frame->inputBuffer, 
                sink->inputWidth, sink->inputHeight, sink->yuvFormat);
                
            YUV_frame_from_buffer(&outputFrame, (void*)frame->scaleBuffer, 
                sink->width, sink->height, sink->yuvFormat);
                
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
                
            rgbInputBuffer = frame->scaleBuffer;
        }
        else
        {
            rgbInputBuffer = frame->inputBuffer;
        }
        
        /* add OSD to frame */
        if (sink->osd != NULL && sink->osdInitialised)
        {
            if (!osd_add_to_image(sink->osd, frameInfo, rgbInputBuffer, sink->width, sink->height))
            {
                ml_log_error("Failed to add OSD to frame\n");
                /* continue anyway */
            }
        }
        
        /* convert frame to RGB */
        convertToRGB(sink, frame->videoFormat, rgbInputBuffer, frame->rgbBuffer, sink->width, sink->height);
        

        /* wait until it is time to display this frame (@ 25 fps) */
        gettimeofday(&timeNow, NULL);
        durationSlept = 0;
        if (frameInfo->rateControl)
        {
            durationSlept = sleep_diff(40 * 1000, &timeNow, &sink->lastFrameTime);
        }

        
        if (sink->useSharedMemory)
        {
            XShmPutImage(sink->x11Common.display, sink->x11Common.window, sink->x11Common.gc, 
                frame->xImage,
                0, 0, 0, 0,
                sink->width, sink->height,
                False);
        }
        else
        {
            XPutImage(sink->x11Common.display, sink->x11Common.window, sink->x11Common.gc, 
                frame->xImage,
                0, 0, 0, 0,
                sink->width, sink->height);
        }

        x11c_process_events(&sink->x11Common, 1);
        
        
        /* report that a new frame has been displayed */
        msl_frame_displayed(sink->listener, frameInfo);

        
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
        
    
    reset_streams(frame);
    
    return 1;
}


static int init_frame(X11DisplayFrame* frame)
{
    X11DisplaySink* sink = (X11DisplaySink*)frame->sink;
    
    sink->depth = DefaultDepth(sink->x11Common.display, DefaultScreen(sink->x11Common.display));
    sink->visual = DefaultVisual(sink->x11Common.display, DefaultScreen(sink->x11Common.display));

    if (sink->depth < 15) 
    {
        ml_log_error("Unknown display depth %d\n", sink->depth);
        return 0;
    }
    
    frame->videoFormat = sink->videoFormat;

    if (frame->videoFormat == UYVY_FORMAT ||
        frame->videoFormat == YUV422_FORMAT)
    {
        frame->inputBufferSize = sink->inputWidth * sink->inputHeight * 2;
        if (sink->swScale != 1)
        {
            frame->scaleBufferSize = sink->width * sink->height * 2;
        }
    }
    else /* YUV420_FORMAT */
    {
        frame->inputBufferSize = sink->inputWidth * sink->inputHeight * 3 / 2;
        if (sink->swScale != 1)
        {
            frame->scaleBufferSize = sink->width * sink->height * 3 / 2;
        }
    }
    frame->rgbBufferSize = sink->width * sink->height * 4; /* max 32-bit */
    
    
    MALLOC_ORET(frame->inputBuffer, unsigned char, frame->inputBufferSize);
    if (sink->swScale != 1)
    {
        MALLOC_ORET(frame->scaleBuffer, unsigned char, frame->scaleBufferSize);
        MALLOC_ORET(frame->scaleWorkspace, unsigned char, sink->inputWidth * 3);
    }    
    
    PTHREAD_MUTEX_LOCK(&sink->x11Common.eventMutex)

    if (sink->useSharedMemory)
    {
        CHK_OFAIL((frame->xImage = 
            XShmCreateImage(sink->x11Common.display, 
                CopyFromParent,
                sink->depth, 
                ZPixmap, 
                NULL, 
                &frame->shminfo, 
                sink->width, 
                sink->height)) != NULL);
                
        if (sink->depth >= 24)
        {
            if (frame->xImage->bytes_per_line * sink->height != (int)sink->width * sink->height * 4)
            {
                ml_log_error("X11 image frame size %d does not match expected size %d\n", 
                    frame->xImage->bytes_per_line * sink->height, sink->width * sink->height * 4);
                XFree(frame->xImage);
                goto fail;
            }
        }
        else /* sink->depth >= 15 */
        {
            if (frame->xImage->bytes_per_line * sink->height != (int)sink->width * sink->height * 2)
            {
                ml_log_error("X11 image frame size %d does not match expected size %d\n", 
                    frame->xImage->bytes_per_line * sink->height, sink->width * sink->height * 2);
                XFree(frame->xImage);
                goto fail;
            }
        }
        
        frame->shminfo.shmid = shmget(IPC_PRIVATE, frame->xImage->bytes_per_line * sink->height,
                                    IPC_CREAT | 0777);
        frame->xImage->data = (char*)shmat(frame->shminfo.shmid, 0, 0);
        frame->shminfo.shmaddr = frame->xImage->data;
        frame->shminfo.readOnly = False;
    
        if (XShmAttach(sink->x11Common.display, &frame->shminfo))
        {
            XSync(sink->x11Common.display, False);
            shmctl(frame->shminfo.shmid, IPC_RMID, 0);
        }
        else
        {
            ml_log_error("Failed to attached shared memory to display\n");
            shmdt(frame->shminfo.shmaddr);
            XFree(frame->xImage);
            goto fail;
        }
        
        frame->rgbBuffer = (unsigned char*)frame->xImage->data;
    }
    else
    {
        MALLOC_OFAIL(frame->rgbBuffer, unsigned char, frame->rgbBufferSize);
        
        if (sink->depth >= 24) 
        {
            CHK_OFAIL((frame->xImage = 
                XCreateImage(sink->x11Common.display, 
                    CopyFromParent,
                    sink->depth, 
                    ZPixmap, 
                    0, 
                    (char*)frame->rgbBuffer, 
                    sink->width, 
                    sink->height,
                    32, 
                    0)) != NULL); 
        } 
        else /* sink->depth >= 15 */ 
        {
            CHK_OFAIL((frame->xImage = 
                XCreateImage(sink->x11Common.display, 
                    CopyFromParent,
                    sink->depth, 
                    ZPixmap, 
                    0, 
                    (char*)frame->rgbBuffer, 
                    sink->width, 
                    sink->height,
                    16, 
                    0)) != NULL); 
        } 
    }
    XInitImage(frame->xImage);

    /* set image byte orders */
    uint16_t twoBytes = 0x0001;
    if (((uint8_t*)&twoBytes)[0] == 0x01)
    {
        frame->xImage->byte_order = LSBFirst;
    }
    else
    {
        frame->xImage->byte_order = MSBFirst;
    }
    frame->xImage->bitmap_bit_order = MSBFirst;
    

    PTHREAD_MUTEX_UNLOCK(&sink->x11Common.eventMutex)

    
    return 1;

fail:
    if (!sink->useSharedMemory)
    {
        SAFE_FREE(&frame->rgbBuffer);
    }
    PTHREAD_MUTEX_UNLOCK(&sink->x11Common.eventMutex)
    return 0;
}

static int xskf_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;
    X11DisplaySink* sink = (X11DisplaySink*)frame->sink;
    StreamInfo outputStreamInfo;

    /* this should've been checked, but we do it here anyway */
    if (!xsk_accept_stream(sink, streamInfo))
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

        /* TODO: user option to set aspect ratio */
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
            /* sw scale changes the output dimensions */
            outputStreamInfo = *streamInfo;
            outputStreamInfo.width = sink->width;
            outputStreamInfo.height = sink->height;
            
            CHK_ORET(osd_initialise(sink->osd, &outputStreamInfo, &sink->aspectRatio));
            sink->osdInitialised = 1;
        }
        
        if (frame->inputBuffer == NULL)
        {
            sink->videoFormat = streamInfo->format;
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

static void xskf_reset(void* data)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;
    
    reset_streams(frame);
}

static int xskf_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;
    
    if (streamId == frame->streamId)
    {
        return 1;
    }
    
    return 0;
}

static int xskf_allocate_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
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
    
    *buffer = (unsigned char*)frame->inputBuffer;
    
    return 1;
}

static int xskf_set_is_present(void* data, int streamId)
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

static int xskf_complete_frame(void* data, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;
    
    osd_set_state(osds_get_osd(frame->osdState), osdState);
    frame->frameInfo = *frameInfo;
    return 1;
}

static void xskf_free(void* data)
{
    X11DisplayFrame* frame = (X11DisplayFrame*)data;
    
    if (frame == NULL)
    {
        return;
    }

    if (frame->sink->useSharedMemory)
    {
        if (frame->xImage)
        {
            XShmDetach(frame->sink->x11Common.display, &frame->shminfo);
            shmdt(frame->shminfo.shmaddr);
            XFree(frame->xImage);
        }
    }
    else
    {
        if (frame->xImage)
        {
            XDestroyImage(frame->xImage);
        }
        else
        {
            SAFE_FREE(&frame->rgbBuffer);
        }
    }
    SAFE_FREE(&frame->inputBuffer);
    SAFE_FREE(&frame->scaleBuffer);

    osds_free(&frame->osdState);
    
    SAFE_FREE(&frame);
}

static int allocate_frame(X11DisplaySink* sink, X11DisplayFrame** frame)
{
    X11DisplayFrame* newFrame = NULL;
    
    CALLOC_ORET(newFrame, X11DisplayFrame, 1);

    CHK_OFAIL(osds_create(&newFrame->osdState));
    
    newFrame->sink = sink;
    newFrame->streamId = -1;
    
    newFrame->sinkFrame.data = newFrame;
    newFrame->sinkFrame.register_stream = xskf_register_stream;
    newFrame->sinkFrame.reset = xskf_reset;
    newFrame->sinkFrame.accept_stream_frame = xskf_accept_stream_frame;
    newFrame->sinkFrame.allocate_stream_buffer = xskf_allocate_stream_buffer;
    newFrame->sinkFrame.set_is_present = xskf_set_is_present;
    newFrame->sinkFrame.complete_frame = xskf_complete_frame;
    newFrame->sinkFrame.free = xskf_free;
    
    *frame = newFrame;
    return 1;
    
fail:
    xskf_free(newFrame);
    return 0;
}




static int xsk_register_listener(void* data, MediaSinkListener* listener)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    sink->listener = listener;    
    sink->x11Common.sinkListener = listener;
    return 1;
}

static void xsk_unregister_listener(void* data, MediaSinkListener* listener)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    if (sink->listener == listener)
    {
        sink->listener = NULL;
        sink->x11Common.sinkListener = NULL;
    }
}


static int xsk_accept_stream(void* data, const StreamInfo* streamInfo)
{
    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        streamInfo->width > 0 &&
        streamInfo->height > 0 &&
        (streamInfo->format == UYVY_FORMAT ||
            streamInfo->format == YUV422_FORMAT ||
            streamInfo->format == YUV420_FORMAT))
    {
        return 1;
    }
    else if (streamInfo->type == TIMECODE_STREAM_TYPE)
    {
        return 1;
    }
    return 0;
}

static int xsk_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    if (sink->frame == NULL)
    {
        CHK_ORET(allocate_frame(sink, &sink->frame));
    }
    
    return xskf_register_stream(sink->frame, streamId, streamInfo);
}

static int xsk_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    return sink->displayInitialised && xskf_accept_stream_frame(sink->frame, streamId, frameInfo);
}

static int xsk_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    return xskf_allocate_stream_buffer(sink->frame, streamId, bufferSize, buffer);
}

static int xsk_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    return xskf_set_is_present(sink->frame, streamId);
}

static int xsk_complete_frame(void* data, const FrameInfo* frameInfo)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    return display_frame(sink, sink->frame, frameInfo);
}

static void xsk_cancel_frame(void* data)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;

    /* do nothing? */

    /* set the current frame's (if there was one) display time */
    gettimeofday(&sink->lastFrameTime, NULL);

    reset_streams(sink->frame);
}

static OnScreenDisplay* xsk_get_osd(void* data)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;
    
    return sink->osd;
}

static int xsk_allocate_frame(void* data, MediaSinkFrame** frame)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;
    X11DisplayFrame* x11Frame = NULL;
    
    CHK_ORET(allocate_frame(sink, &x11Frame));
    
    *frame = &x11Frame->sinkFrame;
    return 1;
}

static int xsk_complete_sink_frame(void* data, const MediaSinkFrame* frame)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;
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

static void xsk_close(void* data)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    /* free before x11c_clear */    
    xskf_free(sink->frame);
    
    if (sink->osd != NULL)
    {
        osd_free(sink->osd);
        sink->osd = NULL;
    }
    
    x11c_clear(&sink->x11Common);
    
    SAFE_FREE(&sink);
}

static int xsk_reset_or_close(void* data)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;
    X11DisplayFrame* blankFrame = NULL;
    FrameInfo blankFrameInfo;

    /* free before x11c_reset */    
    xskf_free(sink->frame);
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
                /* fill buffer with black */
                fill_black(blankFrame->videoFormat, sink->inputWidth, sink->inputHeight, blankFrame->inputBuffer);
                blankFrame->videoIsPresent = 1;
                
                memset(&blankFrameInfo, 0, sizeof(FrameInfo));
                if (!display_frame(sink, blankFrame, &blankFrameInfo))
                {
                    ml_log_warn("Failed to display blank frame at reset\n");
                }
            }
            
            xskf_free(blankFrame);
            blankFrame = NULL;
        }
    }
    
    return 1;
    
fail:
    xsk_close(data);
    return 2;
}


static void xsk_refresh_required(void* data)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;
    
    msl_refresh_required(sink->listener);
}

static void xsk_osd_screen_changed(void* data, OSDScreen screen)
{
    X11DisplaySink* sink = (X11DisplaySink*)data;
    
    msl_osd_screen_changed(sink->listener, screen);
}




int xsk_open(int reviewDuration, int disableOSD, const Rational* pixelAspectRatio, 
    const Rational* monitorAspectRatio, float scale, int swScale, X11PluginWindowInfo *pluginInfo, X11DisplaySink** sink)
{
    X11DisplaySink* newSink;
    
    CALLOC_ORET(newSink, X11DisplaySink, 1);
    
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
    newSink->mediaSink.register_listener = xsk_register_listener;
    newSink->mediaSink.unregister_listener = xsk_unregister_listener;
    newSink->mediaSink.accept_stream = xsk_accept_stream;
    newSink->mediaSink.register_stream = xsk_register_stream;
    newSink->mediaSink.accept_stream_frame = xsk_accept_stream_frame;
    newSink->mediaSink.get_stream_buffer = xsk_get_stream_buffer;
    newSink->mediaSink.receive_stream_frame = xsk_receive_stream_frame;
    newSink->mediaSink.complete_frame = xsk_complete_frame;
    newSink->mediaSink.cancel_frame = xsk_cancel_frame;
    newSink->mediaSink.get_osd = xsk_get_osd;
    newSink->mediaSink.allocate_frame = xsk_allocate_frame;
    newSink->mediaSink.complete_sink_frame = xsk_complete_sink_frame;
    newSink->mediaSink.reset_or_close = xsk_reset_or_close;
    newSink->mediaSink.close = xsk_close;

    
    if (!disableOSD)
    {
        CHK_OFAIL(osdd_create(&newSink->osd));
        
        newSink->osdListener.data = newSink;
        newSink->osdListener.refresh_required = xsk_refresh_required;
        newSink->osdListener.osd_screen_changed = xsk_osd_screen_changed;
        
        osd_set_listener(newSink->osd, &newSink->osdListener);
    }

    CHK_OFAIL(x11c_initialise(&newSink->x11Common, reviewDuration, newSink->osd, pluginInfo));
    
    
    init_lookup_tables(newSink);
    
    gettimeofday(&newSink->lastFrameTime, NULL);
    
    *sink = newSink;
    return 1;
    
fail:
    xsk_close(newSink);
    return 0;
    
}

void xsk_set_media_control(X11DisplaySink* sink, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control)
{
    x11c_set_media_control(&sink->x11Common, mapping, videoSwitch, control);
}

void xsk_unset_media_control(X11DisplaySink* sink)
{
    x11c_unset_media_control(&sink->x11Common);
}

MediaSink* xsk_get_media_sink(X11DisplaySink* sink)
{
    return &sink->mediaSink;
}

void xsk_set_window_name(X11DisplaySink* sink, const char* name)
{
    x11c_set_window_name(&sink->x11Common, name);
}

void xsk_register_window_listener(X11DisplaySink* sink, X11WindowListener* listener)
{
    x11c_register_window_listener(&sink->x11Common, listener);
}

void xsk_unregister_window_listener(X11DisplaySink* sink, X11WindowListener* listener)
{
    x11c_unregister_window_listener(&sink->x11Common, listener);
}

void xsk_register_keyboard_listener(X11DisplaySink* sink, KeyboardInputListener* listener)
{
    x11c_register_keyboard_listener(&sink->x11Common, listener);
}

void xsk_unregister_keyboard_listener(X11DisplaySink* sink, KeyboardInputListener* listener)
{
    x11c_unregister_keyboard_listener(&sink->x11Common, listener);
}

void xsk_register_progress_bar_listener(X11DisplaySink* sink, ProgressBarInputListener* listener)
{
    x11c_register_progress_bar_listener(&sink->x11Common, listener);
}

void xsk_unregister_progress_bar_listener(X11DisplaySink* sink, ProgressBarInputListener* listener)
{
    x11c_unregister_progress_bar_listener(&sink->x11Common, listener);
}

void xsk_register_mouse_listener(X11DisplaySink* sink, MouseInputListener* listener)
{
    x11c_register_mouse_listener(&sink->x11Common, listener);
}

void xsk_unregister_mouse_listener(X11DisplaySink* sink, MouseInputListener* listener)
{
    x11c_unregister_mouse_listener(&sink->x11Common, listener);
}



