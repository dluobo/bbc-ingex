/*
 * $Id: testgen.c,v 1.5 2009/10/12 15:11:46 john_f Exp $
 *
 * Dummy SDI input for testing shared memory video & audio interface.
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

#define _XOPEN_SOURCE 600       // for posix_memalign
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>

#include <signal.h>
#include <unistd.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/time.h>
#ifdef __MMX__
#include <mmintrin.h>
#endif

#include "nexus_control.h"
#include "video_conversion.h"
#include "video_test_signals.h"

// Globals
pthread_t       sdi_thread[8];
int             num_sdi_threads = 0;
NexusControl    *p_control = NULL;
unsigned char   *ring[8] = {0};
int             timecode[8] = {0};
int             control_id, ring_id[8];
int             g_timecode = 0x10000000;        // 10:00:00:00
char            *video_file = 0;
char            *audio_file = 0;
int             use_random_video = 0;
int             width = 720, height = 576;
int     element_size = 0, dma_video_size = 0, dma_total_size = 0;
int     audio_offset = 0, audio_size = 0;
/*
int     ltc_offset = 0, vitc_offset = 0, tick_offset = 0, signal_ok_offset = 0;
*/
int     frame_data_offset = 0;
CaptureFormat   video_format = Format422PlanarYUV;
CaptureFormat   video_secondary_format = FormatNone;

static int verbose = 1;

static void cleanup_shared_mem(void)
{
    int             i, id;
    struct shmid_ds shm_desc;

    for (i = 0; i < (1 + num_sdi_threads); i++)
    {
        id = shmget(9 + i, sizeof(*p_control), 0444);
        if (id == -1)
        {
            fprintf(stderr, "No shmem id for key %d\n", 9 + i);
            continue;
        }
        if (shmctl(id, IPC_RMID, &shm_desc) == -1)
        {
            perror("shmctl(id, IPC_RMID):");
        }
    }
}

static void cleanup_exit(int res)
{
    printf("cleaning up\n");

    // delete shared memory
    cleanup_shared_mem();

    usleep(100 * 1000);     // 0.1 seconds

    printf("done - exiting\n");
    exit(res);
}

static void catch_sigusr1(int sig_number)
{
    // toggle a flag
}

static void catch_sigint(int sig_number)
{
    printf("\nReceived signal %d - ", sig_number);
    cleanup_exit(0);
}

// Read available physical memory stats and
// allocate shared memory ring buffers accordingly
static int allocate_shared_buffers(int num_channels, long long max_memory)
{
    long long   k_shmmax = 0;
    int         ring_len, i;
    FILE        *procfp;

    // Linux specific way to read shmmax value
    if ((procfp = fopen("/proc/sys/kernel/shmmax", "r")) == NULL)
    {
        perror("fopen(/proc/sys/kernel/shmmax)");
        return 0;
    }
    fscanf(procfp, "%lld", &k_shmmax);
    fclose(procfp);

    if (max_memory > 0) {
        // Limit to specified maximum memory usage
        k_shmmax = max_memory;
    }
    else {
        // Reduce maximum to 1GiB to avoid misleading shmmat errors since
        // Documentation/sysctl/kernel.txt says "memory segments up to 1Gb are now supported"
        if (k_shmmax > 0x40000000) {
            printf("shmmax=%lld (%.3fMiB) probably too big, reducing to 1024MiB\n", k_shmmax, k_shmmax / (1024*1024.0));
            k_shmmax = 0x40000000;  // 1GiB
        }
    }

    // calculate reasonable ring buffer length
    // reduce by small number 5 to leave a little room for other shared mem
    ring_len = k_shmmax / num_channels / element_size - 5;
    printf("shmmax=%lld (%.3fMiB) calculated per channel ring_len=%d\n", k_shmmax, k_shmmax / (1024*1024.0), ring_len);

    printf("element_size=%d ring_len=%d (%.2f secs) (total=%lld)\n", element_size, ring_len, ring_len / 25.0, (long long)element_size * ring_len);
    if (ring_len < 10)
    {
        printf("ring_len=%d too small (< 10) - try increasing shmmax:\n", ring_len);
        printf("  echo 1073741824 >> /proc/sys/kernel/shmmax\n");
        return 0;
    }

    // Allocate memory for control structure which is fixed size
    if ((control_id = shmget(9, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "shmget: shm segment exists, deleting all related segments\n");
            cleanup_shared_mem();
            if ((control_id = shmget(9, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
            {
                perror("shmget control key 9");
                return 0;
            }
        }
        else
        {
            perror("shmget control key 9");
            return 0;
        }
    }
    p_control = (NexusControl*)shmat(control_id, NULL, 0);

    p_control->channels = num_channels;
    p_control->ringlen = ring_len;
    p_control->elementsize = element_size;

    p_control->width = width;
    p_control->height = height;
    p_control->frame_rate_numer = 25;
    p_control->frame_rate_denom = 1;
    p_control->pri_video_format = video_format;
    p_control->sec_video_format = video_secondary_format;

    p_control->audio12_offset = audio_offset;
    p_control->audio34_offset = audio_offset + audio_size / 2;
    p_control->audio_size = audio_size;

    p_control->frame_data_offset = frame_data_offset;
    /*
    p_control->vitc_offset = vitc_offset;
    p_control->ltc_offset = ltc_offset;
    p_control->signal_ok_offset = signal_ok_offset;
    */
    p_control->sec_video_offset = dma_video_size + audio_size;

    p_control->source_name_update = 0;
    if (pthread_mutex_init(&p_control->m_source_name_update, NULL) != 0)
        fprintf(stderr, "Mutex init error\n");

    // Allocate multiple element ring buffers containing video + audio + tc
    //
    // by default 32MB is available (40 PAL frames), or change it with:
    //  root# echo  104857600 >> /proc/sys/kernel/shmmax  # 120 frms (100MB)
    //  root# echo  201326592 >> /proc/sys/kernel/shmmax  # 240 frms (192MB)
    //  root# echo 1073741824 >> /proc/sys/kernel/shmmax  # 1294 frms (1GB)

    // key for variable number of ring buffers can be 10, 11, 12, 13, 14, 15, 16, 17
    for (i = 0; i < num_channels; i++)
    {
        int key = i + 10;
        ring_id[i] = shmget(key, element_size * ring_len, IPC_CREAT | IPC_EXCL | 0666);
        if (ring_id[i] == -1)   /* shm error */
        {
            int save_errno = errno;
            fprintf(stderr, "Attemp to shmget for key %d: ", key);
            perror("shmget");
            if (save_errno == EEXIST)
                fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
            if (save_errno == ENOMEM)
                fprintf(stderr, "Perhaps you could increase shmmax: e.g.\n  echo 1073741824 >> /proc/sys/kernel/shmmax\n");
            if (save_errno == EINVAL)
                fprintf(stderr, "You asked for too much?\n");
    
            return 0;
        }

        ring[i] = (uint8_t *)shmat(ring_id[i], NULL, 0);
        if (ring[i] == (void *)-1) {
            fprintf(stderr, "shmat failed for channel[%d], ring_id[%d] = %d\n", i, i, ring_id[i]);
            perror("shmat");
            return 0;
        }
        p_control->channel[i].lastframe = -1;
        p_control->channel[i].hwdrop = 0;
        sprintf(p_control->channel[i].source_name, "ch%d", i);
        p_control->channel[i].source_name[sizeof(p_control->channel[i].source_name) - 1] = '\0';

        // initialise mutex in shared memory
        if (pthread_mutex_init(&p_control->channel[i].m_lastframe, NULL) != 0)
            fprintf(stderr, "Mutex init error\n");

        // Set ring frames to black - too slow!
        // memset(ring[i], 0x80, element_size * ring_len);
    }

    return 1;
}

static int fill_buffers(int num_channels, int shift)
{
    FILE *fp_video = NULL;
    FILE *fp_audio = NULL;
    int video_uyvy_size = width*height*2;
    uint8_t *video_uyvy = (uint8_t*)malloc(video_uyvy_size);
    int video_yuv422_size = width*height*2;
    uint8_t *video_yuv422 = (uint8_t*)malloc(video_yuv422_size);
    int video_yuv_size = width*height*3/2;
    uint8_t *video_yuv = (uint8_t*)malloc(video_yuv_size);
    int audio_buf_size = 1920*2*4;
    uint8_t *audio_buf = (uint8_t*)malloc(audio_buf_size);
    int video_read_ok = 0;

    if (use_random_video) {
        uyvy_random_frame(width, height, video_uyvy);
        video_read_ok = 1;
    }
    else {
        // Open sample video and audio files
        if (video_file && (fp_video = fopen(video_file, "rb")) == NULL) {
            perror("fopen input video file");
            return 0;
        }
        if (audio_file && (fp_audio = fopen(audio_file, "rb")) == NULL) {
            perror("fopen input video file");
            return 0;
        }
        if (fp_audio) {
            fseek(fp_audio, 44, SEEK_SET);      // skip WAV header
        }
    }
        
    // Read from sample files storing video + audio in ring buffer
    printf("Filling buffers...\n");
    int frame_num;
    for (frame_num = 0; p_control && frame_num < p_control->ringlen; ++ frame_num)
    {
        if (fp_video && fread(video_uyvy, video_uyvy_size, 1, fp_video) == 1) {
            video_read_ok = 1;
        }
        else {
            // Use last video frame, otherwise gray frame
            if (!video_read_ok)
                memset(video_uyvy, 0x80, video_uyvy_size);
        }
        if (fp_audio && fread(audio_buf, audio_buf_size, 1, fp_audio) == 1) {
            // Audio frame successfully read
        }
        else {
            memset(audio_buf, 0, audio_buf_size);
        }

        // create YUV buffer
        uyvy_to_yuv420(width, height, shift, video_uyvy, video_yuv);

        // convert UYVY to planar 4:2:2
        uyvy_to_yuv422(width, height, shift, video_uyvy, video_yuv422);

        // Copy video and audio across all channels
        int i;
        for (i = 0; i < num_channels; i++)
        {
            memcpy(ring[i] + element_size * frame_num, video_yuv422, video_yuv422_size);
            memcpy(ring[i] + element_size * frame_num + video_yuv422_size, audio_buf, audio_buf_size);
            memcpy(ring[i] + element_size * frame_num + p_control->audio12_offset + p_control->audio_size,
                video_yuv, video_yuv_size);
        }
    }

    // NB. Should report how many frames loaded from files.
    printf("Loaded %d frames of test video & audio into ring buffer\n", frame_num);

    if (fp_video)
    {
        fclose(fp_video);
    }
    if (fp_audio)
    {
        fclose(fp_audio);
    }

    free(video_uyvy);
    free(video_yuv422);
    free(video_yuv);
    free(audio_buf);

    return 1;
}

static char * framesToStr(int tc, char *s)
{
    int frames = tc % 25;
    int hours = (int)(tc / (60 * 60 * 25));
    int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
    int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

    sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    return s;
}

//
// write_picture()
//
// Gets a frame from the SDI FIFO and stores it to memory ring buffer
// (also can write to disk as a debugging option)
static int write_picture(int channel)
{
    int                 ring_len = p_control->ringlen;
    NexusBufCtl         *pc = &(p_control->channel[channel]);

    // Copy frame data such as timecodes to the end of the ring element.
    // This is a bit sneaky but saves maintaining a separate ring buffer
    NexusFrameData * nfd = (NexusFrameData *)(ring[channel] + element_size * ((pc->lastframe + 1) % ring_len) + frame_data_offset);

    // LTC and VITC
    nfd->vitc = timecode[channel];
    nfd->ltc = timecode[channel];

    /*
    memcpy(ring[channel] + element_size * ((pc->lastframe+1) % ring_len)
            + p_control->vitc_offset,
            &timecode[channel], sizeof(int));
    // And same in LTC position
    memcpy(ring[channel] + element_size * ((pc->lastframe+1) % ring_len)
            + p_control->vitc_offset - 4,
            &timecode[channel], sizeof(int));
    */

    // Write signal_ok flag into buffer
    nfd->signal_ok = 1;
    /*
    *(int *)(ring[channel] + element_size * ((pc->lastframe+1) % ring_len) + signal_ok_offset) = 1;
    */

    // Increment timecode by 1 for each frame for testing
    timecode[channel] = (timecode[channel] + 1) % (24 * 60 * 60 * 25);

    // TODO: Update a burnt-in timecode in the test picture residing in the ring buffer

    if (verbose > 1)
    {
        // timecode:
        //   vitc_tc=268441105 vitc_tc2=-1879042543   for "10:00:16:11"
        char tcstr[32];

        printf("channel %d: Wrote frame %5d  size=%d vitc_tc=%s\n",
                    channel, pc->lastframe, element_size,
                    framesToStr(timecode[channel], tcstr)
                    );
        fflush(stdout);
    }

    // signal frame is now ready
    PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
    pc->lastframe++;
    PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

    return 0;
}

static int64_t tv_diff_microsec(const struct timeval* a, const struct timeval* b)
{
    int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
    return diff;
}

// channel number passed as void * (using cast)
static void * sdi_monitor(void * arg)
{
    int channel = (long)arg;

    // record start time as reference for frame timing
    struct timeval   start_time;
    gettimeofday(&start_time, NULL);

    int framecount = 0;
    while (1)
    {
        write_picture(channel);

        // compute time to sleep based on starting reference and framecount
        struct timeval now_time;
        gettimeofday(&now_time, NULL);
        int64_t elapsed = tv_diff_microsec(&start_time, &now_time);
        int64_t expected = (framecount+1) * 40*1000;
        int64_t diff = expected - elapsed;
        if (diff > 0)
            usleep(diff);

        framecount++;
    }

    return NULL;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: testgen [-v] [-q] [-h] [-c channels] [video_file audio_file]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "e.g.   testgen -c 4 video.uyvy audio.wav\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -r             random video data (no video or audio files required)\n");
    fprintf(stderr, "    -c channels    number of channels to simulate [default 4 for SD, 2 for HD]\n");
    fprintf(stderr, "    -t <type>      video frame type SD/HD1080/HD720 [default SD]\n");
    fprintf(stderr, "    -m memory(MB)  maximum meory to use in MB\n");
    fprintf(stderr, "    -q             quiet operation\n");
    fprintf(stderr, "    -h             help message\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main (int argc, char ** argv)
{
    int n;
    int max_channels = 4;
    int shift = 1; // Shift field order for DV
    long long opt_max_memory = 0;

    time_t now;
    struct tm * tm_now;

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
        else if (strcmp(argv[n], "-h") == 0)
        {
            usage_exit();
        }
        else if (strcmp(argv[n], "-r") == 0)
        {
            use_random_video = 1;
        }
        else if (strcmp(argv[n], "-m") == 0)
        {
            if (sscanf(argv[n+1], "%lld", &opt_max_memory) != 1) {
                fprintf(stderr, "-m requires integer maximum memory in MB\n");
                return 1;
            }
            opt_max_memory = opt_max_memory * 1024 * 1024;
            n++;
        }
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (sscanf(argv[n+1], "%d", &max_channels) != 1 ||
                max_channels > 8 || max_channels <= 0)
            {
                fprintf(stderr, "-c requires integer maximum channel number <= 8\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-t") == 0)
        {
            if (strcmp(argv[n+1], "HD1080") == 0) {
                width = 1920; height = 1080;
                if (max_channels > 2)
                    max_channels = 2;
            }
            else if (strcmp(argv[n+1], "HD720") == 0) {
                width = 1280; height = 720;
                if (max_channels > 2)
                    max_channels = 2;
            }
            else if (strcmp(argv[n+1], "SD") == 0) {
                width = 720; height = 576;
            }
            else {
                fprintf(stderr, "-t requires video type [SD/HD1080/HD720]\n");
                return 1;
            }
            n++;
        }
        // Set video and audio files
        else
        {
            if (video_file == 0)
            {
                video_file = argv[n];
            }
            else if (audio_file == 0)
            {
                audio_file = argv[n];
            }
        }
    }

    // Install signal handlers to do clean exit
    if (signal(SIGINT, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGINT)");
        return 1;
    }
    if (signal(SIGTERM, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
        return 1;
    }
    if (signal(SIGHUP, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
        return 1;
    }
    if (signal(SIGUSR1, catch_sigusr1) == SIG_ERR)
    {
        perror("signal(SIGUSR1)");
        return 1;
    }

    //////////////////////////////////////////////////////
    // Attempt to open all sv cards
    //
    // card specified by string of form "PCI,card=n" where n = 0,1,2,3
    //
    int channel;
    for (channel = 0; channel < max_channels; channel++)
    {
        num_sdi_threads++;
    }

    if (num_sdi_threads < 1)
    {
        fprintf(stderr, "Error: No SDI monitor threads created.  Exiting\n");
        return 1;
    }

    // Values worked out by experiment to match DVS values
    dma_video_size = width*height*2;
    audio_size = 0x8000;
    audio_offset = dma_video_size;

    // audio_size is greater than 1920 * 4 * 4
    // so we use the spare bytes at end for timecode etc.
    frame_data_offset = audio_offset + audio_size - sizeof(NexusFrameData);
    /*
    vitc_offset         = audio_offset + audio_size - sizeof(int);
    ltc_offset          = audio_offset + audio_size - 2 * sizeof(int);
    tick_offset         = audio_offset + audio_size - 3 * sizeof(int);
    signal_ok_offset    = audio_offset + audio_size - 4 * sizeof(int);
    */

    // An element in the ring buffer contains: video(4:2:2) + audio + video(4:2:0)
    dma_total_size = dma_video_size     // video frame as captured by dma transfer
                    + audio_size;       // DVS internal structure for 4 audio channels
                                        

    element_size = dma_total_size
                    + width*height*3/2; // YUV 4:2:0 video buffer

    if (! allocate_shared_buffers(num_sdi_threads, opt_max_memory))
    {
        return 1;
    }

    if (! fill_buffers(num_sdi_threads, shift))
    {
        return 1;
    }

    for (channel = 0; channel < num_sdi_threads; channel++)
    {
        int err;
        fprintf(stderr, "channel %d: starting capture thread\n", channel);

        if ((err = pthread_create(&sdi_thread[channel], NULL, sdi_monitor, (void *)channel)) != 0)
        {
            fprintf(stderr, "Failed to create sdi_monitor thread: %s\n", strerror(err));
            return 1;
        }
    }

    // SDI monitor threads never terminate.
    // Loop forever monitoring status of threads for logging purposes
    while (1)
    {
        usleep( 2 * 1000 * 1000);
    }
}
