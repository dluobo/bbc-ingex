/*
 * $Id: playout.c,v 1.2 2010/01/14 14:05:36 john_f Exp $
 *
 * Playout uncompressed video and audio files over SDI.
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include <signal.h>
#include <unistd.h> 
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <pthread.h>

#include "dvs_clib.h"
#include "dvs_fifo.h"

#if defined(__x86_64__)
#define PFi64 "ld"
#define PFu64 "lu"
#define PFoff "ld"
#else
#define PFi64 "lld"
#define PFu64 "llu"
#define PFoff "lld"
#endif

#define SV_CHECK(x) {int res = x; if (res != SV_OK) { fprintf(stderr, "sv call failed=%d  %s line %d\n", res, __FILE__, __LINE__); sv_errorprint(sv,res); cleanup_exit(1, sv); } }


// Globals
sv_handle       *sv;
int             element_size = 0, audio_offset = 0;
int             timecode = 0;
const char      *video_file = NULL, *audio12_file = NULL, *audio34_file = NULL;
FILE *fp_video = NULL, *fp_audio12 = NULL, *fp_audio34 = NULL;

static int verbose = 1;
static int no_audio = 0;
static int loop = 0;
static off_t current_video_frame = 0;
static off_t frame_inpoint = 0;
static off_t frame_outpoint = 0;
static int black_frames = 0;
static int black_frames_displayed = 0;
static int video_size = 0;
static uint8_t *black_frame_buf;

static inline void write_bcd(unsigned char* target, int val)
{
    *target = ((val / 10) << 4) + (val % 10);
}

static int int_to_dvs_tc(int tc)
{
    // e.g. turn 1020219 into hex 0x11200819
    int fr = tc % 25;
    int hr = (int)(tc / (60*60*25));
    int mi = (int)((tc - (hr * 60*60*25)) / (60 * 25));
    int se = (int)((tc - (hr * 60*60*25) - (mi * 60*25)) / 25);

    unsigned char raw_tc[4];
    write_bcd(raw_tc + 0, fr);
    write_bcd(raw_tc + 1, se);
    write_bcd(raw_tc + 2, mi);
    write_bcd(raw_tc + 3, hr);
    int result = *(int *)(raw_tc);
    return result;
}

static void cleanup_exit(int res, sv_handle *sv)
{
    printf("cleaning up\n");
    // attempt to avoid lockup when stopping playback
    if (sv != NULL)
    {
        // sv_fifo_wait does nothing for record, only useful for playback
        //sv_fifo_wait(sv, poutput);
        sv_close(sv);
    }

    printf("done - exiting\n");
    exit(res);
}

typedef struct thread_arg_t {
    int         card;
    sv_handle   *sv;
    sv_fifo     *fifo;
} thread_arg_t;

static pthread_mutex_t m_last_frame_read;
static pthread_mutex_t m_last_frame_displayed;
static int g_last_frame_read = -1;
static int g_last_frame_displayed = -1;
static int g_read_thread_finished = 0;

#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");

static uint8_t *ring = NULL;
#define RING_SIZE 125

static uint8_t *ring_frame(int frame)
{
    assert(frame >= 0);
    return ring + (frame % RING_SIZE) * element_size;
}
static int ring_element_size(void)
{
    return element_size;
}

// Read from disk, write to ring memory buffer
static void *read_from_disk(void *arg)
{
    while (1)
    {
        int audio_size = 1920*2*4;
        int last_frame_read;
        int last_frame_displayed;
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_read)
        last_frame_read = g_last_frame_read;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_read)
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_displayed)
        last_frame_displayed = g_last_frame_displayed;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_displayed)
    
        // make sure we don't read too quickly and wrap around the ring buffer
        // If we use (RING_SIZE) instead of (RING_SIZE-1) we see corrupted frames
        if (last_frame_read - last_frame_displayed >= RING_SIZE - 1) {
            //printf("read_picture: sleeping\n");
            usleep(40 * 1000);
            continue;
        }
    
        uint8_t *video_buf = ring_frame(last_frame_read + 1);
        uint8_t *audio_buf = video_buf + video_size;
    
        // if requested display black frames at start
        if (black_frames > 0 && black_frames_displayed < black_frames) {
            int i;
            for (i = 0; i < video_size; i+= 4) {
                video_buf[i+0] = 0x80;
                video_buf[i+1] = 0x80;
                video_buf[i+2] = 0x80;
                video_buf[i+3] = 0x80;
            }
            memcpy(video_buf, black_frame_buf, video_size);
            memset(audio_buf, 0, audio_size);
            memset(audio_buf + 0x4000, 0, audio_size);
            black_frames_displayed++;
        }
        else
        {
            int input_exhausted;
          READ_FROM_FILE:
            input_exhausted = 0;
    
            if ( fread(video_buf, video_size, 1, fp_video) != 1 )
            {
                if (feof(fp_video))
                    input_exhausted = 1;
                else
                    perror("fread from video file");
            }
            current_video_frame++;
    
            if (no_audio) {
                memset(audio_buf, 0, audio_size);
                memset(audio_buf + 0x4000, 0, audio_size);
            }
            else
            {
                if ( fread(audio_buf, audio_size, 1, fp_audio12) != 1 )
                {
                    if (feof(fp_audio12))
                        input_exhausted = 1;
                    else
                        perror("fread from audio 12 file");
                }
                if ( fread(audio_buf + 0x4000, audio_size, 1, fp_audio34) != 1 )
                {
                    if (feof(fp_audio34))
                        input_exhausted = 1;
                    else
                        perror("fread from audio 34 file");
                }
            }
            if (input_exhausted) {
                if (loop) {
                    if (verbose > 1)
                        printf("Reached end of input, looping\n");
                    fseeko(fp_video, frame_inpoint * video_size, SEEK_SET);
                    current_video_frame = frame_inpoint;
                    if (! no_audio) {
                        fseeko(fp_audio12, frame_inpoint * audio_size, SEEK_SET);
                        fseeko(fp_audio34, frame_inpoint * audio_size, SEEK_SET);
                    }
                    goto READ_FROM_FILE;
                }
    
                printf("Reached end of input - thread exiting\n");
                g_read_thread_finished = 1;
                pthread_exit(NULL);
            }
            if (frame_outpoint > 0 && current_video_frame >= frame_outpoint) {
                fseeko(fp_video, frame_inpoint * video_size, SEEK_SET);
                current_video_frame = frame_inpoint;
                if (! no_audio) {
                    fseeko(fp_audio12, frame_inpoint * audio_size, SEEK_SET);
                    fseeko(fp_audio34, frame_inpoint * audio_size, SEEK_SET);
                }
            }
        }
    
        if (verbose > 1)
            printf("read_picture: last_frame_read=%d\n", last_frame_read + 1);
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_read)
        g_last_frame_read = last_frame_read + 1;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_read)
    }
}

static int display_on_sv_fifo(sv_handle *sv, sv_fifo *poutput)
{
    int last_frame_displayed = -1;
    int last_frame_read = -1;
    int lastdropped = 0;

    while (1) {
        // wait for new frame to be available
        PTHREAD_MUTEX_LOCK(&m_last_frame_read)
        last_frame_read = g_last_frame_read;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_read)

        // -1 indicates nothing read yet (empty buffer) so nothing to show
        if (last_frame_read == -1) {
            usleep(20 * 1000);
            continue;
        }

        // cannot display frames after the last safely read frame
        if (last_frame_displayed >= last_frame_read) {
            if (last_frame_displayed == last_frame_read && g_read_thread_finished) {
                break;
            }
            usleep(20 * 1000);
            continue;
        }

        // write frame to sv fifo
        sv_fifo_buffer      *pbuffer;
        int                 res;

        // Get sv memmory buffer
        if ((res = sv_fifo_getbuffer(sv, poutput, &pbuffer, NULL, 0)) != SV_OK)
        {
            fprintf(stderr, "failed to sv_fifo_getbuffer(): %d %s\n", res, sv_geterrortext(res));
            exit(1);
        }

        pbuffer->dma.addr = (char*)ring_frame(last_frame_displayed + 1);
        pbuffer->dma.size = ring_element_size();

        // set timecode
        // doesn't appear to work i.e. get output over SDI
        int timecode = last_frame_displayed + 1;
        int dvs_timecode = int_to_dvs_tc(timecode);
        pbuffer->timecode.vitc_tc = dvs_timecode;
        pbuffer->timecode.vitc_tc2 = dvs_timecode | 0x80000000;     // set high bit for second field timecode

        // send the filled in buffer to the hardware
        SV_CHECK(sv_fifo_putbuffer(sv, poutput, pbuffer, NULL));

        sv_fifo_info info = {0};
        SV_CHECK(sv_fifo_status(sv, poutput, &info));

        // update shared variable
        last_frame_displayed++;
        PTHREAD_MUTEX_LOCK(&m_last_frame_displayed)
        g_last_frame_displayed = last_frame_displayed;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_displayed)

        if (verbose > 1 || info.dropped != lastdropped)
            printf("last_displayed=%d nbuf=%d tick=%d dropped=%d waitdrop=%d\n", last_frame_displayed, info.nbuffers, info.tick, info.dropped, info.waitdropped);
        lastdropped = info.dropped;
    }
    return 1;
}

// card number passed as void * (using cast)
static void * sdi_monitor(void *arg)
{
    long                card = (long)arg;
    sv_fifo             *poutput;
    sv_info             status_info;
    sv_storageinfo      storage_info;
    printf("card=%ld\n", card);

    SV_CHECK( sv_status( sv, &status_info) );

    SV_CHECK( sv_storage_status(sv,
                            0,
                            NULL,
                            &storage_info,
                            sizeof(storage_info),
                            0) );

    SV_CHECK( sv_fifo_init( sv,
                            &poutput,       // FIFO handle
                            FALSE,          // bInput (FALSE for playback)
                            FALSE,          // bShared (TRUE for input/output share memory)
                            TRUE,           // bDMA
                            FALSE,          // reserved
                            0) );           // nFrames (0 means use maximum)

    // allocate ring buffer
    int w = status_info.xsize;
    int h = status_info.ysize;
    printf("Allocating memory (%dx%dx2=%d) * %d frames = %"PFi64"\n", w, h, w*h*2, RING_SIZE, (int64_t)element_size * RING_SIZE);
    ring = (uint8_t*)valloc(element_size * RING_SIZE);
    if (ring == NULL) {
        fprintf(stderr, "valloc failed\n");
        exit(1);
    }

    // initialise mutexes
    pthread_mutex_init(&m_last_frame_read, NULL);
    pthread_mutex_init(&m_last_frame_displayed, NULL);

    // Start frame reading thread to read from disk into ring memory
    pthread_t   read_thread;
    int err;
    if ((err = pthread_create(&read_thread, NULL, read_from_disk, NULL)) != 0) {
        fprintf(stderr, "Failed to create display thread: %s\n", strerror(err));
        exit(1);
    }

    // ensure a certain amount is buffered before displaying
    int prebuffer_frames = -1;
    while (prebuffer_frames < 25) {
        // wait for new frame to be available
        PTHREAD_MUTEX_LOCK(&m_last_frame_read)
        prebuffer_frames = g_last_frame_read;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_read)

        usleep(40 * 1000);
    }

    // Start the fifo.  The dropped counter starts counting dropped frame from now.
    SV_CHECK( sv_fifo_start(sv, poutput) );


    // Loop reading frames until input exhausted
    // Signal to display thread ready frames using m_last_frame_read
    display_on_sv_fifo(sv, poutput);

    printf("Finished.  last_frame_read=%d last_frame_displayed=%d\n", g_last_frame_read, g_last_frame_displayed);
    exit(0);

    return NULL;
}


static void usage_exit(void)
{
    fprintf(stderr, "Usage: playout [options] video.uyvy [audio12.wav audio34.wav]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "E.g.   playout -black 50 video.uyvy audio12.wav audio34.wav\n");
    fprintf(stderr, "E.g.   playout -n -black 50 video.uyvy\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c {0-3}     card number (default 0)\n");
    fprintf(stderr, "    -l           loop over input forever\n");
    fprintf(stderr, "    -n           no audio\n");
    fprintf(stderr, "    -in          play from this in point in frames\n");
    fprintf(stderr, "    -out         play up to this out point in frames\n");
    fprintf(stderr, "    -black       number of black frames before playing video\n");
    fprintf(stderr, "    -t           initial timecode to use (in frames)\n");
    fprintf(stderr, "    -q           quiet (no status messages)\n");
    fprintf(stderr, "    -v           increment verbosity\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main (int argc, char ** argv)
{
    int         n;
    int         card = 0;

    time_t now;
    struct tm *tm_now;

    now = time ( NULL );
    tm_now = localtime ( &now );

    // process command-line args
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-v") == 0)
        {
            verbose++;
        }
        else if (strcmp(argv[n], "-q") == 0)
        {
            verbose = 0;
        }
        else if (strcmp(argv[n], "-n") == 0)
        {
            no_audio = 1;
        }
        else if (strcmp(argv[n], "-l") == 0)
        {
            loop = 1;
        }
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (sscanf(argv[n+1], "%u", &card) != 1)
            {
                fprintf(stderr, "-c requires integer card number\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-in") == 0)
        {
            if (sscanf(argv[n+1], "%"PFu64"", &frame_inpoint) != 1)
            {
                fprintf(stderr, "-in requires integer number of frames\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-out") == 0)
        {
            if (sscanf(argv[n+1], "%"PFu64"", &frame_outpoint) != 1)
            {
                fprintf(stderr, "-out requires integer number of frames\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-black") == 0)
        {
            if (sscanf(argv[n+1], "%u", &black_frames) != 1)
            {
                fprintf(stderr, "-black requires integer number of frames\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
        {
            usage_exit();
        }
        else if (strcmp(argv[n], "-t") == 0)
        {
            if (sscanf(argv[n+1], "%d", &timecode) != 1)
            {
                fprintf(stderr, "-t requires integer timecode\n");
                return 1;
            }
            n++;
        }
        // Set video and audio files
        else {
            if (! video_file)
                video_file = argv[n];
            else if (! audio12_file)
                audio12_file = argv[n];
            else if (! audio34_file)
                audio34_file = argv[n];
        }
    }

    if (no_audio && !video_file)
        usage_exit();

    if (!no_audio && (!audio12_file || !audio34_file))
        usage_exit();

    //////////////////////////////////////////////////////
    // Attempt to open all sv cards
    //
    // card specified by string of form "PCI,card=n" where n = 0,1,2,3
    //
    sv_info             status_info;
    char card_str[20] = {0};

    snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);

    int res = sv_openex(&sv,
                        card_str,
                        SV_OPENPROGRAM_DEMOPROGRAM,
                        SV_OPENTYPE_OUTPUT,
                        0,
                        0);
    if (res != SV_OK)
    {
        // typical reasons include:
        //  SV_ERROR_DEVICENOTFOUND
        //  SV_ERROR_DEVICEINUSE
        fprintf(stderr, "card %d: %s\n", card, sv_geterrortext(res));
        return 1;
    }
    sv_status(sv, &status_info);
    printf("card[%d] frame is %dx%d, assuming input UYVY matches\n", card, status_info.xsize, status_info.ysize);
    video_size = status_info.xsize * status_info.ysize *2;

    // FIXME - this could be calculated correctly by using a struct for example
    element_size =  video_size      // video frame
                    + 0x8000;       // size of internal structure of dvs dma transfer
                                    // for 4 channels.  This is greater than 1920 * 4 * 4
                                    // so we use the last 4 bytes for timecode


    audio_offset = video_size;

    // Open sample video and audio files
    if ((fp_video = fopen(video_file, "rb")) == NULL)
    {
        perror("fopen input video file");
        return 0;
    }
    if (! no_audio) {
        if ((fp_audio12 = fopen(audio12_file, "rb")) == NULL)
        {
            perror("fopen input audio 1,2 file");
            return 0;
        }
        fseek(fp_audio12, 44, SEEK_SET);        // skip WAV header
        if ((fp_audio34 = fopen(audio34_file, "rb")) == NULL)
        {
            perror("fopen input audio 3,4 file");
            return 0;
        }
        fseek(fp_audio34, 44, SEEK_SET);        // skip WAV header
    }

    // skip input frames if specified
    if (frame_inpoint) {
        fseeko(fp_video, frame_inpoint * video_size, SEEK_SET);
        current_video_frame = frame_inpoint;
        if (! no_audio) {
            int audio_size = 1920*2*4;
            fseeko(fp_audio12, frame_inpoint * audio_size, SEEK_SET);
            fseeko(fp_audio34, frame_inpoint * audio_size, SEEK_SET);
        }
    }

    if (black_frames > 0) {
        black_frame_buf = (uint8_t*)malloc(video_size);

        // initialise black frame buffer with 0x80108010 (UYVY black)
        int i;
        for (i = 0; i < video_size; i+= 4) {
            black_frame_buf[i+0] = 0x80;
            black_frame_buf[i+1] = 0x10;
            black_frame_buf[i+2] = 0x80;
            black_frame_buf[i+3] = 0x10;
        }
    }

    // Loop forever reading from file, writing to sv fifo
    card = 0;
    sdi_monitor((void *)card);

    return 0;
}
