/*
 * $Id: recplay.c,v 1.2 2010/01/14 14:05:36 john_f Exp $
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
#include <ncurses.h>
#include <sys/stat.h>

#include <pthread.h>

#include "audio_utils.h"

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
sv_handle       *a_sv[4];
int             element_size = 0, audio_offset = 0;
const char      *video_file = NULL, *audio12_file = NULL, *audio34_file = NULL;
char            video_path[FILENAME_MAX] = {0};
char            audio12_path[FILENAME_MAX] = {0};
char            audio34_path[FILENAME_MAX] = {0};
FILE            *fp_video = NULL, *fp_audio12 = NULL, *fp_audio34 = NULL;
FILE            *fp_pbvideo = NULL, *fp_pbaudio12 = NULL, *fp_pbaudio34 = NULL;

static int audio_size = 1920*2*4;

static int verbose = 1;
static int no_audio = 0;
static off_t frame_inpoint = 0;
static off_t frame_outpoint = 0;
static off_t frames_to_capture = 25 * 60 * 90;      // maximum is 90 minutes
static int video_size = 0;

typedef struct thread_arg_t {
    sv_handle           *sv;
    sv_fifo             *pfifo;
    int                 x;
    int                 y;
} thread_arg_t;


static inline void write_bcd(unsigned char* target, int val)
{
    *target = ((val / 10) << 4) + (val % 10);
}

static char *framesToStr(int tc, char *s)
{
        int frames = tc % 25;
        int hours = (int)(tc / (60 * 60 * 25));
        int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
        int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

        sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
        return s;
}

// Table to quickly convert DVS style timecodes to integer timecodes
static int dvs_to_int[128] = {
 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0, 
10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0, 
20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0, 
30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0, 
40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0, 
50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
60, 61, 62, 63, 64, 65, 66, 67, 68, 69,  0,  0,  0,  0,  0,  0,
70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  0,  0,  0,  0,  0,  0
};

static int dvs_tc_to_int(int tc)
{
    // E.g. 0x09595924
    // E.g. 0x10000000
    int hours = dvs_to_int[(tc & 0x7f000000) >> 24];
    int mins  = dvs_to_int[(tc & 0x007f0000) >> 16];
    int secs  = dvs_to_int[(tc & 0x00007f00) >> 8];
    int frames= dvs_to_int[(tc & 0x0000007f)];

    int tc_as_total =   hours * (25*60*60) +
                        mins * (25*60) +
                        secs * (25) +
                        frames;

    return tc_as_total;
}

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

static pthread_mutex_t m_last_frame_written;
static pthread_mutex_t m_last_frame_captured;
static int g_last_frame_written = -1;
static int g_last_frame_captured = -1;
static int g_record_start_frame = -1;
static int g_record_start = 0;
static int g_record_stop_frame = -1;
static int g_frames_written = 0;

static pthread_mutex_t m_last_frame_read;
static pthread_mutex_t m_last_frame_displayed;
static int g_playout_running = 0;
static int64_t g_disk_frames_read = 0;
static int64_t g_disk_frames_displayed = 0;
static int64_t g_disk_frame_position = 0;

#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");

static uint8_t *rec_ring = NULL;
static uint8_t *play_ring = NULL;
#define RING_SIZE 125

static uint8_t *rec_ring_frame(int frame)
{
    return rec_ring + (frame % RING_SIZE) * element_size;
}
static int ring_element_size(void)
{
    return element_size;
}
static int ring_timecode_offset(void)
{
    return element_size - sizeof(int);
}

static pthread_mutex_t m_screen;
#define MT_mvprintw(...) \
    PTHREAD_MUTEX_LOCK(&m_screen) \
    mvprintw(__VA_ARGS__); \
    refresh(); \
    PTHREAD_MUTEX_UNLOCK(&m_screen)

// Read from ring memory buffer, write to disk file
static void *write_to_disk(void *arg)
{
    while (1)
    {
        int last_frame_written;
        int last_frame_captured;
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_written)
        last_frame_written = g_last_frame_written;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
        last_frame_captured = g_last_frame_captured;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)

        // g_record_start signals start of write-to-disk
        // -1 indicates nothing available yet (empty buffer) so nothing to save
        if (last_frame_captured == -1 || g_record_start == 0) {
            usleep(20 * 1000);
            continue;
        }

        // stop recording if reached last frame
        if (g_record_start && g_record_stop_frame != -1 &&
                last_frame_written == g_record_stop_frame) {
            // signal recording has stopped
            MT_mvprintw(7,0,"disk record: STOP   lf=%d rec_stop_fr=%d", last_frame_written, g_record_stop_frame);
            g_record_start_frame = -1;
            g_record_start = 0;
            g_record_stop_frame = -1;
            usleep(20 * 1000);
            continue;
        }

        // cannot write frames from the buffer after the last safely captured frame
        if (last_frame_written >= last_frame_captured) {
            usleep(20 * 1000);
            continue;
        }
    
        uint8_t *video_buf = rec_ring_frame(last_frame_written + 1);
        uint8_t *audio_buf = video_buf + video_size;
    
        if ( fwrite(video_buf, video_size, 1, fp_video) != 1 )
        {
            perror("fwrite to video file");
            exit(1);
        }

        if (! no_audio) {
            if ( fwrite(audio_buf, audio_size, 1, fp_audio12) != 1 )
            {
                perror("fwrite to audio 12 file");
            }
            if ( fwrite(audio_buf + 0x4000, audio_size, 1, fp_audio34) != 1 )
            {
                perror("fwrite to audio 34 file");
            }
        }
        g_frames_written++;
    
        MT_mvprintw(8,0,"write_to_disk: last_frame_written=%6d filesize=%8.2fMB\n", last_frame_written + 1, (long long)g_frames_written * video_size / 1024.0 / 1024.0);
    
        PTHREAD_MUTEX_LOCK(&m_last_frame_written)
        g_last_frame_written = last_frame_written + 1;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)
    }
}

static int capture_sv_fifo(sv_handle *sv, sv_fifo *pinput)
{
    int lastdropped = 0;
    int lastres = SV_ERROR_INPUT_VIDEO_NOSIGNAL;

    while (1) {
        PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
        int last_frame_captured = g_last_frame_captured;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)

        // Get sv memmory buffer
        sv_fifo_buffer      *pbuffer;
        int                 res;
        if ((res = sv_fifo_getbuffer(sv, pinput, &pbuffer, NULL, 0)) != SV_OK)
        {
            if (res == SV_ERROR_INPUT_VIDEO_NOSIGNAL) {
                MT_mvprintw(5,0,"NO VIDEO                 ");
            }
            else {
                MT_mvprintw(5,0,sv_geterrortext(res));
            }

            // If no audio, display status but continue to capture
            if (res == SV_ERROR_INPUT_AUDIO_NOAIV || res == SV_ERROR_INPUT_AUDIO_NOAESEBU) {
                MT_mvprintw(5,0,"Video only");
            }
            else {
                usleep(40*1000);
                lastres = res;
                continue;
            }
        }
        if (res == SV_OK && lastres != SV_OK) {
            MT_mvprintw(5,0,"Video+Audio(4ch)");
        }
        lastres = res;

        // store new frame into next element in ring buffer
        last_frame_captured++;
        char *addr = (char *)rec_ring_frame(last_frame_captured);
        pbuffer->dma.addr = addr;
        pbuffer->dma.size = ring_element_size();

        // ask the hardware to capture
        SV_CHECK(sv_fifo_putbuffer(sv, pinput, pbuffer, NULL));

        // save timecode in ring buffer
        int tc_as_int = dvs_tc_to_int(pbuffer->timecode.vitc_tc);
        memcpy(addr + ring_timecode_offset(), &tc_as_int, sizeof(int));

        // get status such as dropped frames
        sv_fifo_info info = {0};
        SV_CHECK(sv_fifo_status(sv, pinput, &info));

        // update shared variable to signal to disk writing thread
        PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
        g_last_frame_captured = last_frame_captured;
        PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)

        if (verbose) {
            char str[64] = {0};
            MT_mvprintw(6,0,"fr=%7d tcfr=%6d tc=%s drop=%d",
                        last_frame_captured,
                        dvs_tc_to_int(pbuffer->timecode.vitc_tc),
                        framesToStr( dvs_tc_to_int(pbuffer->timecode.vitc_tc), str ),
                        info.dropped);
        }
        lastdropped = info.dropped;
    }
    return 1;
}

static void *record_thread(void *arg)
{
    // allocate ring buffer
    sv_handle *sv = ((thread_arg_t*)arg)->sv;
    sv_fifo *pinput = ((thread_arg_t*)arg)->pfifo;

    // sv internals need suitably aligned memory, so use valloc not malloc
    rec_ring = (uint8_t*)valloc(element_size * RING_SIZE);
    if (rec_ring == NULL) {
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

    return NULL;
}

static void playback_seek(int64_t frame)
{
    struct stat buf;
    int64_t cur_size = 0;
    if (stat(video_file, &buf) != 0) {
        fprintf(stderr, "stat error on %s\n", video_file);
    }
    else {
        cur_size = buf.st_size;
    }
    if ((int64_t)frame * video_size > cur_size)
        frame = cur_size / video_size - 25;
    if (frame < 0)
        frame = 0;
    fseeko(fp_pbvideo, (int64_t)frame * video_size, SEEK_SET);
    fseeko(fp_pbaudio12, 44 + (int64_t)frame * audio_size, SEEK_SET);
    fseeko(fp_pbaudio34, 44 + (int64_t)frame * audio_size, SEEK_SET);
    g_disk_frame_position = frame;
}

static int display_on_sv_fifo(sv_handle *sv, sv_fifo *poutput)
{
    int lastdropped = 0;
    sv_fifo_buffer      *pbuffer;
    int                 res;

    char *video_buf = (char *)valloc(ring_element_size());

    while (1) {
        // Read from disk file
        char *audio_buf = video_buf + video_size;

        if (g_playout_running) {
            int input_exhausted = 0;
    
            if ( fread(video_buf, video_size, 1, fp_pbvideo) != 1 )
            {
                if (feof(fp_pbvideo)) 
                    input_exhausted = 1;
                else
                    perror("fread from video file");
            }
            else {
                //off_t pos = ftello(fp_pbvideo);
                g_disk_frame_position++;
                g_disk_frames_read++;
                MT_mvprintw(12,0,"frames read=%6"PFi64"  pos=%6"PFi64"", g_disk_frames_read, g_disk_frame_position);
            }
    
            if (no_audio) {
                memset(audio_buf, 0, audio_size);
                memset(audio_buf + 0x4000, 0, audio_size);
            }
            else
            {
                if ( fread(audio_buf, audio_size, 1, fp_pbaudio12) != 1 )
                {
                    if (feof(fp_pbaudio12))
                        input_exhausted = 1;
                    else
                        perror("fread from audio 12 file");
                }
                if ( fread(audio_buf + 0x4000, audio_size, 1, fp_pbaudio34) != 1 )
                {
                    if (feof(fp_pbaudio34))
                        input_exhausted = 1;
                    else
                        perror("fread from audio 34 file");
                }
            }
            if (input_exhausted) {
                g_playout_running = 0;
                MT_mvprintw(11,0,"Playback stopped   ");
            }
        }
        else {
            // repeat last frame, but silence audio
            memset(audio_buf, 0, audio_size);
            memset(audio_buf + 0x4000, 0, audio_size);
        }

        // write to output fifo
        if ((res = sv_fifo_getbuffer(sv, poutput, &pbuffer, NULL, 0)) != SV_OK)
        {
            fprintf(stderr, "failed to sv_fifo_getbuffer(): %d %s\n", res, sv_geterrortext(res));
            exit(1);
        }

        pbuffer->dma.addr = video_buf;
        pbuffer->dma.size = ring_element_size();

        // send the filled in buffer to the hardware
        SV_CHECK(sv_fifo_putbuffer(sv, poutput, pbuffer, NULL));
        g_disk_frames_displayed++;

        sv_fifo_info info = {0};
        SV_CHECK(sv_fifo_status(sv, poutput, &info));
        MT_mvprintw(12,40,"frames displayed=%6d (drop=%d)", g_disk_frames_displayed, info.dropped);
        lastdropped = info.dropped;
    }
    return 1;
}

static void *playout_thread(void *arg)
{
    // allocate ring buffer
    sv_handle *sv = ((thread_arg_t*)arg)->sv;
    sv_fifo *poutput = ((thread_arg_t*)arg)->pfifo;

    // sv internals need suitably aligned memory, so use valloc not malloc
    play_ring = (uint8_t*)valloc(element_size * RING_SIZE);
    if (play_ring == NULL) {
        fprintf(stderr, "valloc failed\n");
        exit(1);
    }

    // initialise mutexes
    pthread_mutex_init(&m_last_frame_read, NULL);
    pthread_mutex_init(&m_last_frame_displayed, NULL);

    // wait until something is saved to disk (wait for 5 frames)
    while (g_frames_written <= 25) {
        usleep(40*1000);
        continue;
    }

    // Start the fifo.
    SV_CHECK( sv_fifo_start(sv, poutput) );

    // Start playback
    playback_seek(0);
    g_playout_running = 1;
    MT_mvprintw(11,0,"Playback started   ");

    // Loop reading frames until input exhausted
    // Signal to display thread ready frames using m_last_frame_read
    display_on_sv_fifo(sv, poutput);

    return NULL;
}

static void init_terminal(void)
{
    initscr();              // start curses mode
    cbreak();               // turn off line buffering
    keypad(stdscr, TRUE);   // turn on F1, arrow keys etc
    noecho();
    pthread_mutex_init(&m_screen, NULL);
}

// card number passed as void * (using cast)
static void * sdi_monitor(void *arg)
{
    long                card = (long)arg;
    sv_handle           *sv = a_sv[card];
    sv_fifo             *pinput, *poutput;
    sv_info             status;
    sv_storageinfo      storage_info;
    printf("card = %ld\n", card);

    SV_CHECK( sv_status( sv, &status) );

    SV_CHECK( sv_storage_status(sv,
                            0,
                            NULL,
                            &storage_info,
                            sizeof(storage_info),
                            0) );

    // record FIFO
    SV_CHECK( sv_fifo_init( sv,
                            &pinput,        // FIFO handle
                            TRUE,           // bInput (TRUE for record, FALSE for playback)
                            FALSE,          // bShared (TRUE for input/output share memory)
                            TRUE,           // bDMA
                            FALSE,          // reserved
                            0) );           // nFrames (0 means use maximum)
    // playout FIFO
    SV_CHECK( sv_fifo_init( sv,
                            &poutput,       // FIFO handle
                            FALSE,          // bInput (TRUE for record, FALSE for playback)
                            FALSE,          // bShared (TRUE for input/output share memory)
                            TRUE,           // bDMA
                            FALSE,          // reserved
                            0) );           // nFrames (0 means use maximum)

    int err;
    // start record thread which waits for control
    pthread_t   record_thread_id;
    thread_arg_t rec_args = {sv, pinput, status.xsize, status.ysize};
    if ((err = pthread_create(&record_thread_id, NULL, record_thread, &rec_args)) != 0) {
        fprintf(stderr, "Failed to create record thread: %s\n", strerror(err));
        exit(1);
    }

    // start playout thread which waits for control
    pthread_t   playout_thread_id;
    thread_arg_t play_args = {sv, poutput, status.xsize, status.ysize};
    if ((err = pthread_create(&playout_thread_id, NULL, playout_thread, &play_args)) != 0) {
        fprintf(stderr, "Failed to create playout thread: %s\n", strerror(err));
        exit(1);
    }

    init_terminal();
    attron(A_BOLD);
    mvprintw(0,0,"Uncompressed SDI record & playout");
    attroff(A_BOLD);
    mvprintw(1,0,"card %ld: %dx%d", card, status.xsize, status.ysize);

    attron(A_BOLD);
    mvprintw(4,0,"INPUT");
    mvprintw(10,0,"OUTPUT");
    attroff(A_BOLD);

    mvprintw(7,0,"disk record: STOP                         ");
    refresh();

    while (1) {
        int c = getch();
        MT_mvprintw(15,0,"                              ");
        switch (c) {
            case 'q': case 'Q': case 27:    // ESC also quit
                endwin();
                exit(0);
                break;
            case 'r': case 'R':
                // start record
                PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
                g_record_start_frame = g_last_frame_captured;
                PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)
                g_record_start = 1;
                // set last frame written to be just before last captured so
                // that we don't try to save any previous stuff
                PTHREAD_MUTEX_LOCK(&m_last_frame_written)
                g_last_frame_written = g_record_start_frame - 1;
                PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)
                MT_mvprintw(7,0,"disk record: START fr=%d                   ", g_record_start_frame);
                break;
            case 's': case 'S':
                // stop record
                if (g_record_start) {
                    PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
                    g_record_stop_frame = g_last_frame_captured;
                    PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)
                    MT_mvprintw(7,0,"disk record: signalled STOP stop_frame=%d", g_record_stop_frame);
                }
                break;
            case 'p': case 'P': case 32:        // P, p, <spacebar>
                // toggle playout
                if (g_playout_running) {
                    MT_mvprintw(11,0,"Playback stopped   ");
                    g_playout_running = 0;      // stop playback
                }
                else {
                    MT_mvprintw(11,0,"Playback started   ");
                    g_playout_running = 1;      // start playback
                }
                break;
            case 262:   // Home key - seek to beginning
                playback_seek(0);
                MT_mvprintw(11,20,"seek to 0         ");
                break;
            case 360:   // End key - seek to end
                playback_seek(0xffffffff);
                MT_mvprintw(11,20,"seek to end       ");
                break;
            case 260:   // left arrow
                playback_seek(g_disk_frame_position - 5*25);
                MT_mvprintw(11,20,"seek -5 sec");
                break;
            case 261:   // right arrow
                playback_seek(g_disk_frame_position + 5*25);
                MT_mvprintw(11,20,"seek +5 sec");
                break;
            case 258:   // down arrow
                playback_seek(g_disk_frame_position - 60*25);
                MT_mvprintw(11,20,"seek -1 min");
                break;
            case 259:   // up arrow
                playback_seek(g_disk_frame_position + 60*25);
                MT_mvprintw(11,20,"seek +1 min");
                break;
            default:
                MT_mvprintw(15,0,"Unknown command key (code=%d)", c);
                break;
        }
    }
}



static void usage_exit(void)
{
    fprintf(stderr, "Usage: record [options] video.uyvy [audio12.wav audio34.wav]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "E.g.   record video.uyvy audio12.wav audio34.wav\n");
    fprintf(stderr, "E.g.   record -n video.uyvy\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -f <int>     number of frames to capture\n");
    fprintf(stderr, "    -n           no audio\n");
    fprintf(stderr, "    -in          in VITC timecode to capture from\n");
    fprintf(stderr, "    -out         out VITC timecode to capture up to (not including)\n");
    fprintf(stderr, "    -q           quiet (no status messages)\n");
    fprintf(stderr, "    -v           increment verbosity\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main (int argc, char ** argv)
{
    int n, max_cards = 1;

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

    if (!video_file)
        usage_exit();

    // If audio is set to ON but no audio file args are provided
    // treat video filepath as basename and add suffixes
    if (!no_audio && (!audio12_file || !audio34_file)) {
        sprintf(video_path, "%s.uyvy", video_file);
        sprintf(audio12_path, "%s_12.wav", video_file);
        sprintf(audio34_path, "%s_34.wav", video_file);
        video_file = video_path;
        audio12_file = audio12_path;
        audio34_file = audio34_path;
    }

    //////////////////////////////////////////////////////
    // Attempt to open all sv cards
    //
    // card specified by string of form "PCI,card=n" where n = 0,1,2,3
    //
    int card;
    for (card = 0; card < max_cards; card++)
    {
        sv_info             status_info;
        char card_str[20] = {0};

        snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);

        int res = sv_openex(&a_sv[card],
                            card_str,
                            SV_OPENPROGRAM_DEMOPROGRAM,
                            SV_OPENTYPE_MASK_DEFAULT,
                            0,
                            0);
        if (res != SV_OK)
        {
            // typical reasons include:
            //  SV_ERROR_DEVICENOTFOUND
            //  SV_ERROR_DEVICEINUSE
            fprintf(stderr, "card %d: %s\n", card, sv_geterrortext(res));
            continue;
        }
        sv_status( a_sv[card], &status_info);
        printf("card[%d] frame is %dx%d, assuming input UYVY matches\n", card, status_info.xsize, status_info.ysize);
        video_size = status_info.xsize * status_info.ysize *2;
    }

    // FIXME - this could be calculated correctly by using a struct for example
    element_size =  video_size      // video frame
                    + 0x8000;       // size of internal structure of dvs dma transfer
                                    // for 4 channels.  This is greater than 1920 * 4 * 4
                                    // due to padding at the end of the space


    audio_offset = video_size;

    // Open output video and audio files
    if ((fp_video = fopen(video_file, "wb")) == NULL) {
        perror("fopen output video file");
        return 0;
    }
    // Open same file for read-only
    if ((fp_pbvideo = fopen(video_file, "rb")) == NULL) {
        perror("fopen input video file");
        return 0;
    }


    if (! no_audio) {
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

        // open for read-only
        fp_pbaudio12 = fopen(audio12_file, "rb");
        fp_pbaudio34 = fopen(audio34_file, "rb");
    }

    // Loop forever capturing and writing to disk
    card = 0;
    sdi_monitor((void *)card);

    return 0;
}
