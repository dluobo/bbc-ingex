#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>


#include "buffered_media_source.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


//#define DEBUG_BUFFERED_SINK 1


/* maximum number of positions in frame buffer to check for client requested frame */ 
#define MAX_SEARCH_SIZE             50

#define IS_EOF(bufSource, position) \
    (bufSource->eofPosition >= 0 && position >= bufSource->eofPosition)

/* reads and seek will timeout after this number of seconds */
#define TIMEOUT_SEC                 1    
    

/* Linux  */
#if !defined(ETIMEDOUT)
    #if defined(__linux__)
        #define ETIMEDOUT                   110
    #else
        #error Unknown value for pthread ETIMEDOUT error value
    #endif
#endif


typedef struct BufferedStream
{
    int streamId;
    int isDisabled;
    int isPresent;
    int isEOF;
    
    unsigned char* buffer;
    unsigned int bufferSize; /* size allocated */
    unsigned int dataSize; /* <= bufferSize */
} BufferedStream;

typedef struct BufferedFrame
{
    int isReady;
    int64_t position;
    BufferedStream* streams;
} BufferedFrame;

struct BufferedMediaSource
{
    int blocking;
    float byteRateLimit;
    
    MediaPlayer* player;
    MediaSource mediaSource;
    MediaSource* targetSource;
    MediaSourceListener targetSourceListener;
    
    BufferedFrame* frames;
    int frameBufferSize;
    int numStreams;
    int64_t frameSize;
    
    pthread_mutex_t stateMutex;
    pthread_cond_t frameReadCond;
    pthread_cond_t clientFrameReadCond;
    
    int clientWaiting;
    int64_t clientPosition;
    
    int waiting;
    int64_t lastPosition;
    int64_t eofPosition;
    
    int positionInBuffer; /* used by the target source listener to fill in the frame data */

	pthread_t readThreadId;
    
    int stopped; /* set when the read thread is stopped */
};



static BufferedStream* get_stream(BufferedMediaSource* bufSource, int streamId)
{
    BufferedFrame* frame = &bufSource->frames[bufSource->positionInBuffer];
    int i;
    
    for (i = 0; i < bufSource->numStreams; i++)
    {
        if (frame->streams[i].streamId == streamId)
        {
            return &frame->streams[i];
        }
    }
    
    return NULL;
}

static int bmsrc_accept_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    BufferedStream* stream = get_stream(bufSource, streamId);
    
    if (stream == NULL)
    {
        ml_log_error("Unknown stream %d\n", streamId);
        return 0;
    }
    
    return !stream->isDisabled;
}
    
static int bmsrc_allocate_buffer(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    BufferedStream* stream = get_stream(bufSource, streamId);
    
    if (stream == NULL)
    {
        ml_log_error("Unknown stream %d\n", streamId);
        return 0;
    }
    
    if (stream->bufferSize < bufferSize)
    {
        MALLOC_ORET(stream->buffer, unsigned char, bufferSize);
        stream->bufferSize = bufferSize;
    }

    stream->dataSize = bufferSize;
    
    *buffer = stream->buffer;
    return 1;
}

static void bmsrc_deallocate_buffer(void* data, int streamId, unsigned char** buffer)
{
    /* do nothing because buffer is reused */
}

static int bmsrc_receive_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    BufferedStream* stream = get_stream(bufSource, streamId);
    
    if (stream == NULL)
    {
        ml_log_error("Unknown stream %d\n", streamId);
        return 0;
    }
    
    stream->isPresent = 1;
    
    return 1;
}

static void* read_thread(void* arg)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)arg;
    int doneWaiting;
    int64_t position = 0;
    int readResult;
    int doReadFrame;
    int isEOF = 0;
    int haveReadFrame;
    int status;
    int i;
    int64_t lastPosition = 0;
    int seekToFrame;
    int haveClientFrame;
    int haveSeeked;
    int clientPositionInBuffer;
    int positionInBuffer = 0;
    int readEOF;
    FrameInfo dummyFrameInfo;
    long timeDiff;
    long targetTimeDiff;
    struct timeval now = {0, 0};
    struct timeval prevRead = now;

    memset(&dummyFrameInfo, 0, sizeof(FrameInfo));
    
    
    while (!bufSource->stopped)
    {
        /* wait until a new frame can be read */
        doneWaiting = 0;
        haveClientFrame = 0;
        while (!doneWaiting && !bufSource->stopped)
        {
            PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
            
            bufSource->waiting = 1;
            
            clientPositionInBuffer = bufSource->clientPosition % bufSource->frameBufferSize;
            
            if (bufSource->frames[clientPositionInBuffer].isReady && 
                bufSource->frames[clientPositionInBuffer].position == bufSource->clientPosition)
            {
#ifdef DEBUG_BUFFERED_SINK                
                printf("READ THREAD: client frame is in buffer (%lld)\n", bufSource->clientPosition); fflush(stdout);
#endif                

                position = bufSource->lastPosition + 1;
                positionInBuffer = position % bufSource->frameBufferSize;
                if (positionInBuffer == clientPositionInBuffer)
                {
                    /* buffer is full. Wait for client to read the frame */
                    if (bufSource->clientWaiting)
                    {
                        /* wake up client */
                        status = pthread_cond_signal(&bufSource->frameReadCond);
                        if (status != 0)
                        {
                            ml_log_error("Failed to wake up client\n");
                        }
                    }
                    
#ifdef DEBUG_BUFFERED_SINK                
                    printf("READ THREAD: buffer full; waiting for client\n"); fflush(stdout);
#endif                
                    status = pthread_cond_wait(&bufSource->clientFrameReadCond, &bufSource->stateMutex);
                    if (status != 0)
                    {
                        ml_log_error("buffered source read thread failed to wait for condition\n");
                    }
                }
                else
                {
                    /* clients frame is ready and we can read a new as well */
                    doneWaiting = 1;
                }
            }
            else
            {
                /* frame at clientPosition must be read */
                position = bufSource->clientPosition;
                doneWaiting = 1;
            }
            
            if (doneWaiting)
            {
                isEOF = IS_EOF(bufSource, position);
                positionInBuffer = position % bufSource->frameBufferSize;
                lastPosition = bufSource->lastPosition;
                bufSource->waiting = 0;
            }
            
            PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
        }
        
        /* read thread has stopped */
        if (bufSource->stopped)
        {
            break;
        }
        

        /* read the next frame */
        
        haveReadFrame = 0;
        readEOF = 0;
        if (!isEOF)
        {
            doReadFrame = 1;
            haveSeeked = 0;
            
            /* seek to position if required */
            if (position != lastPosition + 1)
            {
                seekToFrame = 1;
                while (seekToFrame)
                {
                    if (msc_seek(bufSource->targetSource, position) != 0)
                    {
                        /* failed to seek to position */
                        doReadFrame = 0;
                        seekToFrame = 0;
                    }
                    else
                    {
                        seekToFrame = 0;
                        haveSeeked = 1;
                    }
                }
            }

            /* read the frame */
            while (doReadFrame)
            {
                /* initialise */
                PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
                bufSource->frames[positionInBuffer].isReady = 0;
                bufSource->frames[positionInBuffer].position = -1;
                bufSource->positionInBuffer = positionInBuffer;
                for (i = 0; i < bufSource->numStreams; i++)
                {
                    bufSource->frames[positionInBuffer].streams[i].isPresent = 0;
                }
                PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);

                /* delay if there is a byte rate limit */
                if (bufSource->byteRateLimit > 0.0 && bufSource->frameSize > 0)
                {
                    gettimeofday(&now, NULL);
                    timeDiff = (now.tv_sec - prevRead.tv_sec) * 1000000 + now.tv_usec - prevRead.tv_usec;
                    targetTimeDiff = (long)(bufSource->frameSize / bufSource->byteRateLimit * 1000000.0);
                    
                    if (timeDiff > 0 && timeDiff < targetTimeDiff)
                    {
                        usleep(targetTimeDiff - timeDiff);
                    }
                    
                    gettimeofday(&prevRead, NULL);
                }
                
                /* read the frame */
                readResult = msc_read_frame(bufSource->targetSource, &dummyFrameInfo,
                    &bufSource->targetSourceListener);
                if (readResult == 0)
                {
#ifdef DEBUG_BUFFERED_SINK                
                    printf("READ THREAD: have read frame\n"); fflush(stdout);
#endif                
                    haveReadFrame = 1;                
                    doReadFrame = 0;

                    /* re-calculate the frame size */
                    bufSource->frameSize = 0;
                    for (i = 0; i < bufSource->numStreams; i++)
                    {
                        if (bufSource->frames[positionInBuffer].streams[i].isPresent)
                        {
                            bufSource->frameSize += bufSource->frames[positionInBuffer].streams[i].dataSize;
                        }
                    }
                }
                else if (readResult == -2)
                {
                    /* timed out */
                    doReadFrame = 0;
                }
                else
                {
                    /* can't read the requested frame */
                    readEOF = msc_eof(bufSource->targetSource);
                    doReadFrame = 0;
                }
            }
            
            /* return to after last position if have failed to read */
            if (!haveReadFrame)
            {
                if (haveSeeked)
                {
                    /* seek to the start of the next frame after the last frame read */
                    if (msc_seek(bufSource->targetSource, lastPosition + 1) != 0)
                    {
                        ml_log_error("Failed to recover from failed read - "
                            "failed to seek to the start of the next frame after the last frame read\n");
                    }
                }
                else
                {
                    /* The mxf_source (plus libMXFReader) doesn't (yet) recover from partial read failures and 
                    this results in the file not being position at the start of the next frame.
                    The libMXFReader assumes the file is positioned at a start of the next frame 
                    and ignores any seek to the start of the next frame. However, a failed frame 
                    read will have moved the file position past the start of the next frame.
                    To avoid this problem we seek back to the start of the _last_ frame and then seek
                    to the start of the next frame */
                    
                    /* seek to the start of the last frame read */
                    if (msc_seek(bufSource->targetSource, lastPosition) != 0)
                    {
                        ml_log_error("Failed to recover from failed read - "
                            "failed to seek to the start of the last frame read\n");
                    }
                    /* seek to the start of the next frame */
                    else if (msc_seek(bufSource->targetSource, lastPosition + 1) != 0)
                    {
                        ml_log_error("Failed to recover from failed read - "
                            "failed to seek to the start of the next frame after the last frame read\n");
                    }
                }
            }
        }            

        
        PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
        
        if (haveReadFrame)
        {
            /* complete the read */
#ifdef DEBUG_BUFFERED_SINK                
            printf("READ THREAD: read frame (%lld)\n", position); fflush(stdout);
#endif                
            bufSource->frames[positionInBuffer].isReady = 1;
            bufSource->frames[positionInBuffer].position = position;
            bufSource->lastPosition = position;
        }
        
        /* update the eof if we tried to read and the target source said it was eof */ 
        if (readEOF)
        {
            bufSource->eofPosition = position;
        }
        
        /* note: we signal even if we failed to read the frame */
        if (bufSource->clientWaiting)
        {
            status = pthread_cond_signal(&bufSource->frameReadCond);
            if (status != 0)
            {
                ml_log_error("Failed to signal that frame has been read\n");
            }
        }
        
        PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);

#if 0
        /* TODO: remove this */
        static int d = 0;
        d++;
        if (d % 20 == 0)
        {
            sleep(3);
        }
#endif    
    
    }

    pthread_exit((void*) 0);
}




static int bmsrc_is_complete(void* data)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_is_complete(bufSource->targetSource);
}

static int bmsrc_post_complete(void* data, MediaSource* rootSource, MediaControl* mediaControl)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_post_complete(bufSource->targetSource, rootSource, mediaControl);
}

static int bmsrc_get_num_streams(void* data)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_get_num_streams(bufSource->targetSource);
}

static int bmsrc_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_get_stream_info(bufSource->targetSource, streamIndex, streamInfo);
}

static int bmsrc_disable_stream(void* data, int streamIndex)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    BufferedStream* stream = get_stream(bufSource, streamIndex);
    
    stream->isDisabled = msc_disable_stream(bufSource->targetSource, streamIndex);

    return stream->isDisabled;
}

static void bmsrc_disable_audio(void* data)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    int i;
    int numTargetStreams;
    BufferedStream* stream;
    
    msc_disable_audio(bufSource->targetSource);
    
    /* synchronize the local stream disabled to take into account disabled target streams */
    numTargetStreams = msc_get_num_streams(bufSource->targetSource);
    for (i = 0; i < numTargetStreams; i++)
    {
        stream = get_stream(bufSource, i);
        stream->isDisabled = msc_stream_is_disabled(bufSource->targetSource, i);
    }
}

static int bmsrc_stream_is_disabled(void* data, int streamIndex)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_stream_is_disabled(bufSource->targetSource, streamIndex);
}

static int bmsrc_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    int status;
    int doneWaiting;
    int i;
    int failedToSendFrame;
    int clientIsEOF;
    int waitCount;
    int clientBufferPosition = 0;
    int readFailed;
    int timedOut = 0;
    struct timeval now;
    struct timespec timeout;

    /* wait for the frame to become available or it cannot be read */
    waitCount = 0;
    clientIsEOF = 0;
    doneWaiting = 0;
    readFailed = 0;
    while (!doneWaiting && !bufSource->stopped)
    {
        PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);

        bufSource->clientWaiting = 1;
        
        clientBufferPosition = bufSource->clientPosition % bufSource->frameBufferSize;
        
        if (IS_EOF(bufSource, bufSource->clientPosition))
        {
            /* client is at end of file  */
            clientIsEOF = 1;
            doneWaiting = 1;
        }
        else if (bufSource->frames[clientBufferPosition].isReady &&
            bufSource->frames[clientBufferPosition].position == bufSource->clientPosition)
        {
            /* frame is ready */
#ifdef DEBUG_BUFFERED_SINK                
            printf("CLIENT: frame is ready (%lld)\n", bufSource->clientPosition); fflush(stdout);
#endif                
            doneWaiting = 1;
        }
        else
        {
            if (waitCount >= 2)
            {
                /* the first wait (waitCount == 0) wake up could have been for another frame. 
                The second (waitCount == 1) should result in the requested frame, 
                otherwise a third attempt (waitCount == 2) means it failed */
                readFailed = 1;
                doneWaiting = 1;
            }
            else
            {
                if (bufSource->waiting)
                {
                    /* wake up the reading thread */
#ifdef DEBUG_BUFFERED_SINK                
                    printf("CLIENT: signal reading thread to wake up\n"); fflush(stdout);
#endif                
                    status = pthread_cond_signal(&bufSource->clientFrameReadCond);
                    if (status != 0)
                    {
                        ml_log_error("Failed to signal that frame has been read by buffer client\n");
                        /* TODO: what now? */
                    }
                }
                
                /* wait for read thread to make next frame available */
                gettimeofday(&now, NULL);
                timeout.tv_sec = now.tv_sec + 1;
                timeout.tv_nsec = now.tv_usec * 1000;
#ifdef DEBUG_BUFFERED_SINK                
                printf("CLIENT: wait for read thread to read frame (%lld)\n", bufSource->clientPosition); fflush(stdout);
#endif                
                status = pthread_cond_timedwait(&bufSource->frameReadCond, &bufSource->stateMutex, &timeout);
                if (status == ETIMEDOUT)
                {
                    doneWaiting = 1;
                    timedOut = 1;
                }
                else if (status != 0)
                {
                    ml_log_error("Failed to wait for frame read conditional variable: %d\n", status);
                    /* TODO: what now? */ 
                }

                waitCount++;
            }
        }

        if (doneWaiting && !timedOut)
        {
            clientBufferPosition = bufSource->clientPosition % bufSource->frameBufferSize;
            bufSource->clientWaiting = 0;
        }

        PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
    }
        
    /* no more reading */
    if (bufSource->stopped)
    {
        return -1;
    }
    
    if (timedOut)
    {
        return -2;
    }
    
    
    if (clientIsEOF || readFailed)
    {
        return -1;
    }

    
    /* send frame to source listener */
    failedToSendFrame = 0;
    for (i = 0; i < bufSource->numStreams; i++)
    {
        BufferedStream* stream = &bufSource->frames[clientBufferPosition].streams[i];
        
        if (stream->isDisabled || !stream->isPresent)
        {
            continue;
        }
        
        if (!sdl_accept_frame(listener, stream->streamId, frameInfo))
        {
            continue;
        }
        
        if (!sdl_receive_frame_const(listener, stream->streamId, stream->buffer, stream->dataSize))
        {
            failedToSendFrame = 1;
            break;
        }
    }
    
    
    if (!failedToSendFrame)
    {
        PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);

#ifdef DEBUG_BUFFERED_SINK                
        printf("CLIENT: signal the client has read frame (%lld)\n", bufSource->clientPosition); fflush(stdout);
#endif                

        /* update client frame read and position */ 
        bufSource->clientPosition += 1;

        /* signal that the client has read the frame */
        status = pthread_cond_signal(&bufSource->clientFrameReadCond);
        if (status != 0)
        {
            ml_log_error("Failed to signal that frame has been read by buffer client\n");
            /* TODO: what now? */
        }

        PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
    }
    
    
    return !failedToSendFrame ? 0 : -1;
}

static int bmsrc_is_seekable(void* data)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_is_seekable(bufSource->targetSource);
}

static int bmsrc_seek(void* data, int64_t position)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    int status;
    int clientIsEOF;
    int64_t origClientPosition = 0;
    int doneWaiting;
    int waitCount;
    int clientBufferPosition = 0;
    int seekFailed;
    int timedOut = 0;
    struct timeval now;
    struct timespec timeout;

    if (position < 0)
    {
        /* note: if we tried seeking then clientBufferPosition below would have been negative! */ 
        return -1;
    }
    
    /* wait for the frame to become available or it cannot be read */
    clientIsEOF = 0;
    doneWaiting = 0;
    waitCount = 0;
    seekFailed = 0;
    while (!doneWaiting && !bufSource->stopped)
    {
        PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);

        if (!bufSource->clientWaiting)
        {
            /* initialise first time */
            origClientPosition = bufSource->clientPosition; /* remember if need to restore when fail to seek */
        }
        bufSource->clientWaiting = 1;

        bufSource->clientPosition = position; /* seek to position */
        clientBufferPosition = bufSource->clientPosition % bufSource->frameBufferSize;

        if (IS_EOF(bufSource, bufSource->clientPosition))
        {
            /* client is at end of file  */
            clientIsEOF = 1;
            doneWaiting = 1;
        }
        else if (bufSource->frames[clientBufferPosition].isReady &&
            bufSource->frames[clientBufferPosition].position == bufSource->clientPosition)
        {
            /* frame is ready */
            doneWaiting = 1;
        }
        else
        {
            if (waitCount >= 2)
            {
                /* the first wait (waitCount == 0) wake up could have been for another frame. 
                The second (waitCount == 1) should result in the requested frame, 
                otherwise a third attempt (waitCount == 2) means it failed */
                seekFailed = 1;
                doneWaiting = 1;
            }
            else
            {
                if (bufSource->waiting)
                {
                    /* wake up the reading thread */
#ifdef DEBUG_BUFFERED_SINK                
                    printf("CLIENT: signal reading thread to wake up\n"); fflush(stdout);
#endif                
                    status = pthread_cond_signal(&bufSource->clientFrameReadCond);
                    if (status != 0)
                    {
                        ml_log_error("Failed to signal that frame has been read by buffer client\n");
                        /* TODO: what now? */
                    }
                }
                
                /* wait for read thread to make next frame available */
                gettimeofday(&now, NULL);
                timeout.tv_sec = now.tv_sec + 1;
                timeout.tv_nsec = now.tv_usec * 1000;
#ifdef DEBUG_BUFFERED_SINK                
                printf("CLIENT: wait for read thread to seek and read frame\n"); fflush(stdout);
#endif                
                status = pthread_cond_timedwait(&bufSource->frameReadCond, &bufSource->stateMutex, &timeout);
                if (status == ETIMEDOUT)
                {
                    doneWaiting = 1;
                    timedOut = 1;
                }
                else if (status != 0)
                {
                    ml_log_error("Failed to wait for frame read conditional variable: %d\n", status);
                    /* TODO: what now? */ 
                }

                waitCount++;
            }
        }
        
        if (doneWaiting && !timedOut)
        {
            clientBufferPosition = bufSource->clientPosition % bufSource->frameBufferSize;
            bufSource->clientWaiting = 0;
        }

        PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
    }
    
    /* no more reading */
    if (bufSource->stopped)
    {
        return -1;
    }
    
    if (timedOut)
    {
        return -2;
    }
    
    if (clientIsEOF)
    {
        PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
        if (bufSource->clientPosition > bufSource->eofPosition)
        {
            /* restore position if it is beyond the eof */
            bufSource->clientPosition = origClientPosition;
        }
        PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
        return -1;
    }
    else if (seekFailed)
    {
        /* failed to seek to position */
        PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
        bufSource->clientPosition = origClientPosition;
        PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
        return -1;
    }

    return 0;
}

static int bmsrc_seek_timecode(void* data, const Timecode* timecode, TimecodeType type, TimecodeSubType subType)
{
    //BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    /* TODO: reset client is EOF */
    ml_log_error("Seeking on timecode in a buffered media source not yet implemented\n");
    return -1;
    // if (bufSource->stopped)
    // {
        // return -1;
    // }
//    return msc_seek_timecode(bufSource->targetSource, timecode, type, subType);
}

static int bmsrc_get_length(void* data, int64_t* length)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_get_length(bufSource->targetSource, length);
}

static int bmsrc_get_position(void* data, int64_t* position)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return bufSource->clientPosition;
}

static int bmsrc_get_available_length(void* data, int64_t* length)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    return msc_get_available_length(bufSource->targetSource, length);
}

static int bmsrc_eof(void* data)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    int isEOF;

    PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
    isEOF = IS_EOF(bufSource, bufSource->clientPosition);
    PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
    
    return isEOF;
}

static void bmsrc_close(void* data)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    int i, j;
    
    if (data == NULL)
    {
        return;
    }
    
    bufSource->stopped = 1;

    /* wake up the threads - this is to avoid valgrind saying the mutx is 
    still in use when pthread_mutex_destroy is called below */
    PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
    pthread_cond_broadcast(&bufSource->frameReadCond);
    pthread_cond_broadcast(&bufSource->clientFrameReadCond);
    PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
        
    join_thread(&bufSource->readThreadId, NULL, NULL);
    
    msc_close(bufSource->targetSource);
    
    for (i = 0; i < bufSource->frameBufferSize; i++)
    {
        BufferedFrame* frame = &bufSource->frames[i];
        for (j = 0; j < bufSource->numStreams; j++)
        {
            BufferedStream* stream = &frame->streams[j];
            SAFE_FREE(&stream->buffer);
        }
        SAFE_FREE(&frame->streams);
    }
    SAFE_FREE(&bufSource->frames);
    
    destroy_cond_var(&bufSource->frameReadCond);
    destroy_cond_var(&bufSource->clientFrameReadCond);
	destroy_mutex(&bufSource->stateMutex);
    
    SAFE_FREE(&bufSource);
}

/* Note: the state returned assumes the player is playing forwards one frame at a time */
static int bmsrc_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    PTHREAD_MUTEX_LOCK(&bufSource->stateMutex);
    
    *numBuffers = bufSource->frameBufferSize;
    if (bufSource->lastPosition + 1 < bufSource->clientPosition)
    {
        /* out of order so assume empty */
        *numBuffersFilled = 0;
    }
    else if (bufSource->lastPosition + 1 > bufSource->clientPosition + bufSource->frameBufferSize)
    {
        /* last too far ahead so assume empty */
        *numBuffersFilled = 0;
    }
    else
    {
        *numBuffersFilled = bufSource->lastPosition + 1 - bufSource->clientPosition;
    }
    
    PTHREAD_MUTEX_UNLOCK(&bufSource->stateMutex);
    
    return 1;
}

static int64_t bmsrc_convert_position(void* data, int64_t position, MediaSource* childSource)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    if (childSource != &bufSource->mediaSource)
    {
        return msc_convert_position(bufSource->targetSource, position, childSource);
    }
    
    return position;
}

static void bmsrc_set_source_name(void* data, const char* name)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    msc_set_source_name(bufSource->targetSource, name);
}

static void bmsrc_set_clip_id(void* data, const char* id)
{
    BufferedMediaSource* bufSource = (BufferedMediaSource*)data;
    
    msc_set_clip_id(bufSource->targetSource, id);
}



int bmsrc_create(MediaSource* targetSource, int size, int blocking, float byteRateLimit,
    BufferedMediaSource** bufSource)
{
    BufferedMediaSource* newBufSource = NULL;
    int i, j;
    const StreamInfo* streamInfo;
    
    CALLOC_ORET(newBufSource, BufferedMediaSource, 1);
    
    newBufSource->targetSource = targetSource;
    newBufSource->blocking = blocking;
    newBufSource->frameBufferSize = size;
    newBufSource->byteRateLimit = byteRateLimit;
    
    newBufSource->lastPosition = -1;
    newBufSource->eofPosition = -1;
    
    newBufSource->mediaSource.data = newBufSource;
    newBufSource->mediaSource.is_complete = bmsrc_is_complete;
    newBufSource->mediaSource.post_complete = bmsrc_post_complete;
    newBufSource->mediaSource.get_num_streams = bmsrc_get_num_streams;
    newBufSource->mediaSource.get_stream_info = bmsrc_get_stream_info;
    newBufSource->mediaSource.disable_stream = bmsrc_disable_stream;
    newBufSource->mediaSource.disable_audio = bmsrc_disable_audio;
    newBufSource->mediaSource.stream_is_disabled = bmsrc_stream_is_disabled;
    newBufSource->mediaSource.read_frame = bmsrc_read_frame;
    newBufSource->mediaSource.is_seekable = bmsrc_is_seekable;
    newBufSource->mediaSource.seek = bmsrc_seek;
    newBufSource->mediaSource.seek_timecode = bmsrc_seek_timecode;
    newBufSource->mediaSource.get_length = bmsrc_get_length;
    newBufSource->mediaSource.get_position = bmsrc_get_position;
    newBufSource->mediaSource.get_available_length = bmsrc_get_available_length;
    newBufSource->mediaSource.eof = bmsrc_eof;
    newBufSource->mediaSource.close = bmsrc_close;
    newBufSource->mediaSource.get_buffer_state = bmsrc_get_buffer_state;
    newBufSource->mediaSource.convert_position = bmsrc_convert_position;
    newBufSource->mediaSource.set_source_name = bmsrc_set_source_name;
    newBufSource->mediaSource.set_clip_id = bmsrc_set_clip_id;
    
    newBufSource->targetSourceListener.data = newBufSource;
    newBufSource->targetSourceListener.accept_frame = bmsrc_accept_frame;
    newBufSource->targetSourceListener.allocate_buffer = bmsrc_allocate_buffer;
    newBufSource->targetSourceListener.deallocate_buffer = bmsrc_deallocate_buffer;
    newBufSource->targetSourceListener.receive_frame = bmsrc_receive_frame;
    
    
    newBufSource->numStreams = msc_get_num_streams(targetSource);
    CALLOC_OFAIL(newBufSource->frames, BufferedFrame, size);
    for (i = 0; i < size; i++)
    {
        BufferedFrame* frame = &newBufSource->frames[i];
        if (newBufSource->numStreams > 0)
        {
            CALLOC_OFAIL(frame->streams, BufferedStream, newBufSource->numStreams);
            for (j = 0; j < newBufSource->numStreams; j++)
            {
                BufferedStream* stream = &frame->streams[j];
                CHK_OFAIL(msc_get_stream_info(newBufSource->targetSource, j, &streamInfo));
                stream->streamId = j;
            }
        }
    }
    
    CHK_OFAIL(init_mutex(&newBufSource->stateMutex));
    CHK_OFAIL(init_cond_var(&newBufSource->frameReadCond));
    CHK_OFAIL(init_cond_var(&newBufSource->clientFrameReadCond));
    
    CHK_OFAIL(create_joinable_thread(&newBufSource->readThreadId, read_thread, newBufSource));
    
    
    *bufSource = newBufSource;
    return 1;
    
fail:
    bmsrc_close(newBufSource);
    return 0;
}

MediaSource* bmsrc_get_source(BufferedMediaSource* bufSource)
{
    return &bufSource->mediaSource;
}

void bmsrc_set_media_player(BufferedMediaSource* bufSource, MediaPlayer* player)
{
    bufSource->player = player;
}

