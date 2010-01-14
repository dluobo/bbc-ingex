/*
 * $Id: record.c,v 1.3 2010/01/14 14:05:36 john_f Exp $
 *
 * Record uncompressed SDI video and audio to disk.
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

#include <signal.h>
#include <unistd.h> 
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <pthread.h>

#include "dvs_clib.h"
#include "dvs_fifo.h"

#include "audio_utils.h"
#include "video_VITC.h"

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
const char      *video_file = NULL, *audio12_file = NULL, *audio34_file = NULL;
FILE            *fp_video = NULL, *fp_audio12 = NULL, *fp_audio34 = NULL;
FILE            *fp_audio1 = NULL, *fp_audio2 = NULL, *fp_audio3 = NULL, *fp_audio4 = NULL;

static int verbose = 1;
static int no_audio = 0;
static int audio24 = 0;
static int video_bit_format = -1;
static const char *video_mode_name = "unknown";
static const char *mode_name_8B = "8 bits";
static const char *mode_name_10B = "10B";
static const char *mode_name_10BDVS = "10BDVS";
static int frame_inpoint = -1;
static int frame_outpoint = -1;
static off_t frames_to_capture = 0xffffffff;
static int video_size = 0;
static int video_width = 0;
static int video_height = 0;
static int audio12_offset = 0;
static int audio34_offset = 0;
static int exit_on_error = 0;
static int RING_SIZE = 125;

static void cleanup_exit(int res, sv_handle *sv)
{
    printf("cleaning up\n");
    // attempt to avoid lockup when stopping playback
    if (sv != NULL)
    {
        // sv_fifo_wait does nothing for record, only useful for playback
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

static pthread_mutex_t m_last_frame_written;
static pthread_mutex_t m_last_frame_captured;
static int g_last_frame_written = -1;
static int g_last_frame_captured = -1;
static int g_finished_capture = 0;

#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");

static uint8_t *ring = NULL;

static uint8_t *ring_frame(int frame)
{
    return ring + (frame % RING_SIZE) * element_size;
}
static int ring_element_size(void)
{
    return element_size;
}

static void audio_32_to_24bit(int channel, uint8_t *buf32, uint8_t *buf24)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    int channel_offset = 0;
    if (channel == 1)
        channel_offset = 4;

    // Skip every other channel, copying 24 most significant bits of 32 bits
    // from little-endian DVS format to little-endian 24bits
    for (i = channel_offset; i < 1920*4*2; i += 8) {
        *buf24++ = buf32[i+1];
        *buf24++ = buf32[i+2];
        *buf24++ = buf32[i+3];
    }
}


// Read from ring memory buffer, write to disk file
static void *write_to_disk(void *arg)
{
    while (1)
    {
        int audio_size = 1920*2*4;
        int last_frame_written;
        int last_frame_captured;
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_written)
        last_frame_written = g_last_frame_written;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
        last_frame_captured = g_last_frame_captured;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)
    
        // -1 indicates nothing available yet (empty buffer) so nothing to save
        if (last_frame_captured == -1) {
            usleep(20 * 1000);
            continue;
        }

        // cannot write frames after the last safely captured frame
        if (last_frame_written >= last_frame_captured) {
            if (last_frame_written == last_frame_captured && g_finished_capture) {
                printf("exiting write_to_disk\n");
                pthread_exit(NULL);
            }

            usleep(20 * 1000);
            continue;
        }
    
        uint8_t *video_buf = ring_frame(last_frame_written + 1);
    
        if ( fwrite(video_buf, video_size, 1, fp_video) != 1 ) {
            perror("fwrite to video file");
            exit(1);
        }

        if (! no_audio) {
            if (audio24) {
                int audio24size = 1920 * 3;
                uint8_t *audio24buf = (uint8_t*)malloc(1920 * 3);

                audio_32_to_24bit(0, video_buf + audio12_offset, audio24buf);
                if ( fwrite(audio24buf, audio24size, 1, fp_audio1) != 1 ) {
                    perror("fwrite to audio 1 file");
                }

                audio_32_to_24bit(1, video_buf + audio12_offset, audio24buf);
                if ( fwrite(audio24buf, audio24size, 1, fp_audio2) != 1 ) {
                    perror("fwrite to audio 2 file");
                }

                audio_32_to_24bit(0, video_buf + audio34_offset, audio24buf);
                if ( fwrite(audio24buf, audio24size, 1, fp_audio3) != 1 ) {
                    perror("fwrite to audio 3 file");
                }

                audio_32_to_24bit(1, video_buf + audio34_offset, audio24buf);
                if ( fwrite(audio24buf, audio24size, 1, fp_audio4) != 1 ) {
                    perror("fwrite to audio 4 file");
                }
                free(audio24buf);
            }
            else {
                if ( fwrite(video_buf + audio12_offset, audio_size, 1, fp_audio12) != 1 ) {
                    perror("fwrite to audio 12 file");
                }
                if ( fwrite(video_buf + audio34_offset, audio_size, 1, fp_audio34) != 1 ) {
                    perror("fwrite to audio 34 file");
                }
            }
        }
    
        if (verbose > 1)
            printf("write_to_disk: last_frame_written=%d\n", last_frame_written + 1);
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_written)
        g_last_frame_written = last_frame_written + 1;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)
    }
}

static int capture_sv_fifo(sv_handle *sv, sv_fifo *pinput)
{
    int lastdropped = 0;

    while (1) {
        int last_frame_captured;
        int last_frame_written;

        PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
        last_frame_captured = g_last_frame_captured;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)

        PTHREAD_MUTEX_LOCK(&m_last_frame_written)
        last_frame_written = g_last_frame_written;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)

        // Make sure we don't capture too quickly and wrap around the ring buffer
        if (last_frame_captured - last_frame_written >= RING_SIZE - 1) {
            usleep(40 * 1000);
            continue;
        }

        // write frame to sv fifo
        sv_fifo_buffer      *pbuffer;
        int                 res;

        // Get sv memmory buffer
        if ((res = sv_fifo_getbuffer(sv, pinput, &pbuffer, NULL, 0)) != SV_OK)
        {
            fprintf(stderr, "failed to sv_fifo_getbuffer(): %d %s\n", res, sv_geterrortext(res));
            exit(1);
        }

        pbuffer->dma.addr = (char *)ring_frame(last_frame_captured + 1);
        pbuffer->dma.size = ring_element_size();
        if (audio12_offset == 0) {
            audio12_offset = (unsigned long)pbuffer->audio[0].addr[0];
            audio34_offset = (unsigned long)pbuffer->audio[0].addr[1];
        }

        // ask the hardware to capture
        SV_CHECK(sv_fifo_putbuffer(sv, pinput, pbuffer, NULL));

        // Read timecode from PALFF (fullframe)
        if (video_height == 592) {
            int stride = 720*2;
            unsigned hh, mm, ss, ff;

            if (readVITC(ring_frame(last_frame_captured + 1) + stride * 0, &hh, &mm, &ss, &ff))
                printf("readVITC: %02u:%02u:%02u:%02u", hh, mm, ss, ff);
            else
                printf("readVITC failed");

            if (readVITC(ring_frame(last_frame_captured + 1) + stride * 2, &hh, &mm, &ss, &ff))
                printf("  LTC: %02u:%02u:%02u:%02u\n", hh, mm, ss, ff);
            else
                printf("  LTC failed\n");
        }

        // If scanning for start timecode, wait until timecode matches
        if (frame_inpoint != -1) {
            int current_tc_frames = dvs_tc_to_int(pbuffer->timecode.vitc_tc);
            if (frame_inpoint != current_tc_frames) {
                printf("\rwaiting for in point: fr=%6d current=%d (0x%08x)",
                    frame_inpoint, current_tc_frames, pbuffer->timecode.vitc_tc);
                fflush(stdout);
                usleep(20 * 1000);          // wait for next frame
                continue;
            }
            // frame inpoint found so turn of frame in point scanning
            frame_inpoint = -1;
        }

        if (verbose && 0)
            printf("fr=%6d tc=0x%08x\n", last_frame_captured + 1,
                        pbuffer->timecode.vitc_tc);

        sv_fifo_info info = {0};
        SV_CHECK(sv_fifo_status(sv, pinput, &info));

        if (verbose) {
            char str[64] = {0};
            printf("\rframe=%6d tc=%s (tc=0x%08x tc2=0x%08x) drop=%d",
                        last_frame_captured + 1,
                        framesToStr( dvs_tc_to_int(pbuffer->timecode.vitc_tc), str ),
                        pbuffer->timecode.vitc_tc, pbuffer->timecode.vitc_tc2,
                        info.dropped);
            fflush(stdout);
        }

        if (exit_on_error && info.dropped > 0) {
            printf("\nexiting due to dropped frame\n");
            exit(1);
        }
        lastdropped = info.dropped;

        // update shared variable to signal to disk writing thread
        last_frame_captured++;
        PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
        g_last_frame_captured = last_frame_captured;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)

        // finished intended capture?
        // subtract 1 since last_frame_captured starts counting from 0
        if (last_frame_captured >= frames_to_capture - 1) {
            g_finished_capture = 1;
            return 1;
        }
    }
    return 1;
}

// card number passed as void * (using cast)
static void * sdi_monitor(void *arg)
{
    long                card = (long)arg;
    sv_fifo             *pinput;
    sv_info             status_info;
    sv_storageinfo      storage_info;
    printf("card = %ld\n", card);

    SV_CHECK( sv_status( sv, &status_info) );

    SV_CHECK( sv_storage_status(sv,
                            0,
                            NULL,
                            &storage_info,
                            sizeof(storage_info),
                            0) );

    SV_CHECK( sv_fifo_init( sv,
                            &pinput,        // FIFO handle
                            TRUE,           // bInput (TRUE for record, FALSE for playback)
                            FALSE,          // bShared (TRUE for input/output share memory)
                            TRUE,           // bDMA
                            FALSE,          // reserved
                            0) );           // nFrames (0 means use maximum)

    // allocate ring buffer
    printf("Allocating memory %d (video+audio size) * %d frames = %"PFi64"\n", element_size, RING_SIZE, (int64_t)element_size * RING_SIZE);

    // sv internals need suitably aligned memory for DMA transfer,
    // so use valloc not malloc to get memory aligned to PAGESIZE (4096) bytes.
    // valloc is obsolete so you can use posix_memalign instead, e.g.
    // if (posix_memalign(&ring, 4096, element_size * RING_SIZE) != 0) ...
    ring = (uint8_t*)valloc(element_size * RING_SIZE);
    if (ring == NULL) {
        fprintf(stderr, "valloc failed\n");
        exit(1);
    }

    // initialise mutexes
    pthread_mutex_init(&m_last_frame_written, NULL);
    pthread_mutex_init(&m_last_frame_captured, NULL);

    // Start disk writing thread
    pthread_t   write_thread;
    int err;
    if ((err = pthread_create(&write_thread, NULL, write_to_disk, NULL)) != 0) {
        fprintf(stderr, "Failed to create write thread: %s\n", strerror(err));
        exit(1);
    }

    // Start the fifo.  The dropped counter starts counting dropped frame from now.
    SV_CHECK( sv_fifo_start(sv, pinput) );

    // Loop capturing frames
    capture_sv_fifo(sv, pinput);

    printf("Finished capturing %"PFu64" frames.  Waiting for buffer to be written\n", frames_to_capture);
    if ((err = pthread_join(write_thread, NULL)) != 0) {
        fprintf(stderr, "Failed to join write thread: %s\n", strerror(err));
        exit(1);
    }

    printf("Finished.  last_frame_written=%d last_frame_captured=%d\n", g_last_frame_written, g_last_frame_captured);
    exit(0);

    return NULL;
}

int tcstr_to_frames(const char* tc_str, int *frames)
{
    unsigned h,m,s,f;
    if (sscanf(tc_str, "%u:%u:%u:%u", &h, &m, &s, &f) == 4)
    {
        *frames = f + 25*s + 25*60*m + 25*60*60*h;
        return 1;
    }
    return 0;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: record [options] video.uyvy [audio12.wav audio34.wav]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "E.g.   record video.uyvy audio12.wav audio34.wav\n");
    fprintf(stderr, "E.g.   record -n video.uyvy\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c {0-3}         card number (default 0)\n");
    fprintf(stderr, "    -f <num frames>  number of frames to capture\n");
    fprintf(stderr, "    -n               no audio\n");
    fprintf(stderr, "    -r <ringsize>    size of ring buffer in number of frames cached\n");
    fprintf(stderr, "    --exit           exit on error such as frame drop\n");
    fprintf(stderr, "    --audio24        write 24bit audio in mono WAV files instead of 32bit stereo files\n");
    fprintf(stderr, "    --8b             record video in 8bit format (changes card videomode)\n");
    fprintf(stderr, "    --10b            record video in 10bit (10B) format (changes card videomode)\n");
    fprintf(stderr, "    --10bdvs         record video in 10bdvs (10BDVS) format (changes card videomode)\n");
    fprintf(stderr, "    -in <timecode>   in VITC timecode to capture from e.g. 10:00:00:00 or 900000\n");
    fprintf(stderr, "    -out <timecode>  out VITC timecode to capture up to (but not including)\n");
    fprintf(stderr, "    -q               quiet (no status messages)\n");
    fprintf(stderr, "    -v               increment verbosity\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main (int argc, char ** argv)
{
    int n;
    int card = 0;

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
        else if (strcmp(argv[n], "--exit") == 0)
        {
            exit_on_error = 1;
        }
        else if (strcmp(argv[n], "-r") == 0)
        {
            if (sscanf(argv[n+1], "%u", &RING_SIZE) != 1)
            {
                fprintf(stderr, "-r requires integer ring size in number of frames\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "--8b") == 0)
        {
            video_bit_format = 0;
            video_mode_name = mode_name_8B;
        }
        else if (strcmp(argv[n], "--10b") == 0)
        {
            video_bit_format = SV_MODE_NBIT_10B;
            video_mode_name = mode_name_10B;
        }
        else if (strcmp(argv[n], "--10bdvs") == 0)
        {
            video_bit_format = SV_MODE_NBIT_10BDVS;
            video_mode_name = mode_name_10BDVS;
        }
        else if (strcmp(argv[n], "--audio24") == 0)
        {
            audio24 = 1;
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
        else if (strcmp(argv[n], "-f") == 0)
        {
            if (sscanf(argv[n+1], "%"PFu64"", &frames_to_capture) != 1)
            {
                fprintf(stderr, "-f requires integer number of frames\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-in") == 0)
        {
            if (! tcstr_to_frames(argv[n+1], &frame_inpoint) &&
                (sscanf(argv[n+1], "%d", &frame_inpoint) != 1))
            {
                fprintf(stderr, "-in requires timecode string or integer number of frames\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-out") == 0)
        {
            if (frame_inpoint == -1) {
                fprintf(stderr, "must specify -in first before -out\n");
                return 1;
            }
            if (! tcstr_to_frames(argv[n+1], &frame_outpoint) &&
                (sscanf(argv[n+1], "%d", &frame_outpoint) != 1))
            {
                fprintf(stderr, "-out requires timecode string or integer number of frames\n");
                return 1;
            }
            frames_to_capture = frame_outpoint - frame_inpoint + 1;
            n++;
        }
        else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
        {
            usage_exit();
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
                        SV_OPENTYPE_INPUT,
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

    int voptmode = 0;
    if (video_bit_format != -1) {
        // Switch to video mode specified on command line
        // if not already in that video mode
        SV_CHECK( sv_option_get(sv, SV_OPTION_VIDEOMODE, &voptmode) );
        if ((voptmode & SV_MODE_NBIT_MASK) == video_bit_format) {
            printf("video mode %s already set\n", video_mode_name);
        }
        else {
            printf("setting %s video mode\n", video_mode_name);
            voptmode &= ~SV_MODE_NBIT_MASK;
            voptmode |= video_bit_format;           // e.g. SV_MODE_NBIT_10B
            SV_CHECK( sv_videomode(sv, voptmode) );
        }
    }

    sv_status(sv, &status_info);
    video_width = status_info.xsize;
    video_height = status_info.ysize;
    video_size = video_width * video_height *2;
    int videomode = status_info.config;

    if ((videomode & SV_MODE_NBIT_MASK) != SV_MODE_NBIT_8B) {
        switch (videomode & SV_MODE_NBIT_MASK) {
        case SV_MODE_NBIT_10B:
        case SV_MODE_NBIT_10BDVS:
        case SV_MODE_NBIT_10BDPX:
            video_size = 4 * video_size / 3;
            break;
        }
    }
    printf("card[%d] frame is %dx%d (%d bits) %d bytes, assuming input UYVY matches\n", card, video_width, video_height, status_info.nbit, video_size);

    // FIXME - this could be calculated correctly by using a struct for example
    element_size =  video_size      // video frame
                    + 0x8000;       // size of internal structure of dvs dma transfer
                                    // for 4 channels.  This is greater than 1920 * 4 * 4
                                    // so we use the last 4 bytes for timecode


    audio_offset = video_size;

    // Open output video and audio files
    if ((fp_video = fopen(video_file, "wb")) == NULL)
    {
        perror("fopen output video file");
        return 0;
    }
    if (! no_audio) {
        if (audio24) {
            char audio1_file[] = "audio1.wav";
            char audio2_file[] = "audio2.wav";
            char audio3_file[] = "audio3.wav";
            char audio4_file[] = "audio4.wav";
            if ((fp_audio1 = fopen(audio1_file, "wb")) == NULL) {
                perror("fopen");
                return 0;
            }
            if ((fp_audio2 = fopen(audio2_file, "wb")) == NULL) {
                perror("fopen");
                return 0;
            }
            if ((fp_audio3 = fopen(audio3_file, "wb")) == NULL) {
                perror("fopen");
                return 0;
            }
            if ((fp_audio4 = fopen(audio4_file, "wb")) == NULL) {
                perror("fopen");
                return 0;
            }
            writeWavHeader(fp_audio1, 24, 1);
            writeWavHeader(fp_audio2, 24, 1);
            writeWavHeader(fp_audio3, 24, 1);
            writeWavHeader(fp_audio4, 24, 1);
        }
        else {
            if ((fp_audio12 = fopen(audio12_file, "wb")) == NULL) {
                perror("fopen output audio 1,2 file");
                return 0;
            }
            if ((fp_audio34 = fopen(audio34_file, "wb")) == NULL) {
                perror("fopen output audio 3,4 file");
                return 0;
            }
            writeWavHeader(fp_audio12, 32, 2);
            writeWavHeader(fp_audio34, 32, 2);
        }
    }

    // Loop forever capturing and writing to disk
    card = 0;
    sdi_monitor((void *)card);

    return 0;
}
