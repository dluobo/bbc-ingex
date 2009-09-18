/*
 * $Id: dvs_sdi.c,v 1.24 2009/09/18 16:35:05 philipn Exp $
 *
 * Record multiple SDI inputs to shared memory buffers.
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

#define _XOPEN_SOURCE 600           // for posix_memalign

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include <signal.h>
#include <unistd.h> 
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include <sys/shm.h>
#ifdef __MMX__
#include <mmintrin.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include "dvs_clib.h"
#include "dvs_fifo.h"

#include "nexus_control.h"
#include "logF.h"
#include "video_VITC.h"
#include "video_conversion.h"
#include "video_test_signals.h"
#include "avsync_analysis.h"

//#define MAX_CHANNELS 8  (defined in nexus_control.h)
// each DVS card can have 2 channels, so 4 cards gives 8 channels

// Each thread uses the following thread-specific data
typedef struct {
    struct SwsContext* scale_context;
    AVFrame *inFrame;
    AVFrame *outFrame;
} SDIThreadData;

// Globals
pthread_t       sdi_thread[MAX_CHANNELS] = {0};
sv_handle       *a_sv[MAX_CHANNELS] = {0};
SDIThreadData   td[MAX_CHANNELS];
int             num_sdi_threads = 0;
NexusControl    *p_control = NULL;
uint8_t         *ring[MAX_CHANNELS] = {0};
int             control_id, ring_id[MAX_CHANNELS];
int             width = 0, height = 0;
int             sec_width = 0, sec_height = 0;
int             frame_rate_numer = 0, frame_rate_denom = 0;
int             element_size = 0, dma_video_size = 0, dma_total_size = 0;
static int      audio_offset = 0, audio_size = 0, audio_pair_size = 0;
int             ltc_offset = 0, vitc_offset = 0;
int             sys_time_offset = 0, tick_offset = 0, signal_ok_offset = 0;
int             num_aud_samp_offset = 0;
int             frame_number_offset = 0;
CaptureFormat   video_format = Format422PlanarYUV;
CaptureFormat   video_secondary_format = FormatNone;
static int verbose = 0;
static int verbose_channel = -1;    // which channel to show when verbose is on
static int audio8ch = 0;
static int anc_tc = 0;              // read timecodes from RP188/RP196 ANC data
static int test_avsync = 0;
static uint8_t *video_work_area[MAX_CHANNELS];
static int benchmark = 0;
static uint8_t *benchmark_video = NULL;
static char *video_sample_file = NULL;
static int aes_audio = 0;
static int aes_routing = 0;

pthread_mutex_t m_log = PTHREAD_MUTEX_INITIALIZER;      // logging mutex to prevent intermixing logs

// Recover timecode type is the type of timecode to use to generate dummy frames
typedef enum {
    RecoverVITC,
    RecoverLTC
} RecoverTimecodeType;

static NexusTimecode master_type = NexusTC_None;    // by default do not use master timecode
static int master_channel = 0;                      // channel with master timecode source (default channel 0)

// The mutex m_master_tc guards all access to master_tc and master_tod variables
pthread_mutex_t m_master_tc = PTHREAD_MUTEX_INITIALIZER;
static int master_tc = 0;               // timecode as integer
static int64_t master_tod = 0;          // gettimeofday value corresponding to master_tc

// When generating dummy frames, use LTC differences to calculate how many frames
static RecoverTimecodeType recover_timecode_type = RecoverLTC;

static uint8_t *no_video_frame = NULL;              // captioned black frame saying "NO VIDEO"
static uint8_t *no_video_secondary_frame = NULL;    // captioned black frame saying "NO VIDEO"

#define SV_CHECK(x) {int res = x; if (res != SV_OK) { fprintf(stderr, "sv call failed=%d  %s line %d\n", res, __FILE__, __LINE__); sv_errorprint(sv,res); cleanup_exit(1, sv); } }

// Define missing macros for older SDKs
#ifndef SV_OPTION_MULTICHANNEL
#define SV_OPTION_MULTICHANNEL      184
#endif
#ifndef SV_OPTION_AUDIOAESROUTING
#define SV_OPTION_AUDIOAESROUTING   198
#endif
#ifndef SV_AUDIOAESROUTING_DEFAULT
#define SV_AUDIOAESROUTING_DEFAULT  0
#endif
#ifndef SV_AUDIOAESROUTING_8_8
#define SV_AUDIOAESROUTING_8_8      2
#endif
#ifndef SV_AUDIOAESROUTING_4_4
#define SV_AUDIOAESROUTING_4_4      3
#endif

// Returns time-of-day as microseconds
static int64_t gettimeofday64(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t tod = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec ;
    return tod;
}

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

static void cleanup_exit(int res, sv_handle *sv)
{
    printf("cleaning up\n");

    // Cancel all threads (other than main thread)
    int i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (sdi_thread[i] != 0) {
            logTF("cancelling thread %d (id %lu)\n", i, sdi_thread[i]);
            pthread_cancel(sdi_thread[i]);
            pthread_join(sdi_thread[i], NULL);
            //logTF("thread %d joined ok\n", i);
        }
    }

    // Close all sv handles
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (a_sv[i] != 0) {
            printf("closing sv_handle index %d\n", i);
            sv_close(a_sv[i]);
        }
    }

    // delete shared memory
    cleanup_shared_mem();

    usleep(100 * 1000);     // 0.1 seconds

    printf("done - exiting\n");
    exit(res);
}

static int check_sdk_version3(void)
{
    sv_handle *sv;
    sv_version version;
    int device = 0;
    int result = 0;

    SV_CHECK( sv_openex(&sv, (char*)"PCI,card=0", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_QUERY, 0, 0) );
    SV_CHECK( sv_version_status(sv, &version, sizeof(version), device, 1, 0) );
    printf("%s version = (%d,%d,%d,%d)\n", version.module, version.release.v.major, version.release.v.minor, version.release.v.patch, version.release.v.fix);
    if (version.release.v.major >= 3)
        result = 1;

    SV_CHECK( sv_close(sv) );

    return result;
}

static void framerate_for_videomode(int videomode, int *p_numer, int *p_denom)
{
    int video = videomode & SV_MODE_MASK;       // mask off everything except video

    if (video == SV_MODE_PAL || video == SV_MODE_SMPTE274_25I) {
        *p_numer = 25;
        *p_denom = 1;
    }
    else if (video == SV_MODE_NTSC || video == SV_MODE_SMPTE274_30I) {
        *p_numer = 30000;
        *p_denom = 1001;
    }
    else if (video == SV_MODE_SMPTE296_50P) {
        *p_numer = 50;
        *p_denom = 1;
    }
    else if (video == SV_MODE_SMPTE296_60P) {
        *p_numer = 60000;
        *p_denom = 1001;
    }
    else {
        *p_numer = 25;
        *p_denom = 1;
    }
}

static int set_videomode_on_all_channels(int max_channels, int opt_video_mode)
{
    // Assume SDK multichannel capability (> major version 3)
    int card = 0;
    int channel = 0;
    for (card = 0; card < max_channels && channel < max_channels; card++)
    {
        char card_str[64] = {0};

        snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=0", card);

        int res = sv_openex(&a_sv[channel],
                            card_str, SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
        if (res == SV_ERROR_WRONGMODE)
        {
            // Multichannel mode not on so doesn't like "channel=0"
            snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
            res = sv_openex(&a_sv[channel], card_str, SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
        }
        if (res == SV_OK)
        {
            res = sv_videomode(a_sv[channel], opt_video_mode);
            if (res != SV_OK) {
                fprintf(stderr, "sv_videomode(channel=%d, 0x%08x) failed: ", channel, opt_video_mode);
                sv_errorprint(a_sv[channel], res);
                sv_close(a_sv[channel]);
                return 0;
            }

            // check for multichannel mode
            int multichannel_mode = 0;
            if ((res = sv_option_get(a_sv[channel], SV_OPTION_MULTICHANNEL, &multichannel_mode)) != SV_OK) {
                logTF("card %d: sv_option_get(SV_OPTION_MULTICHANNEL) failed: %s\n", card, sv_geterrortext(res));
            }

            channel++;

            // If card has a multichannel mode on, open second channel
            if (multichannel_mode)
            {
                if (channel < max_channels)
                {
                    snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=1", card);
                    int res = sv_openex(&a_sv[channel], card_str, SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
                    if (res == SV_OK)
                    {
                        res = sv_videomode(a_sv[channel], opt_video_mode);
                        if (res != SV_OK) {
                            logTF("card %d: channel=1, sv_videomode(0x%08x) failed, trying without AUDIO\n", card, opt_video_mode);
                            opt_video_mode = opt_video_mode & (~SV_MODE_AUDIO_MASK);
                            res = sv_videomode(a_sv[channel], opt_video_mode);
                            if (res != SV_OK) {
                                fprintf(stderr, "sv_videomode(channel=%d, 0x%08x) failed: ", channel, opt_video_mode);
                                sv_errorprint(a_sv[channel], res);
                                sv_close(a_sv[channel]);
                                return 0;
                            }
                            logTF("card %d: channel=1, sv_videomode(0x%08x) succeeded without AUDIO\n", card, opt_video_mode);
                        }

                        sv_close(a_sv[channel]);

                        channel++;
                    }
                    else
                    {
                        logTF("card %d: failed opening second channel: %s\n", card, sv_geterrortext(res));
                    }
                    sv_close(a_sv[channel-2]);
                }
                else
                {
                    sv_close(a_sv[channel-1]);
                }
            }
            else
            {
                logTF("card %d: multichannel mode off\n", card);
                sv_close(a_sv[channel-1]);
            }
        }
        else
        {
            // SV_ERROR_DEVICENOTFOUND is returned when there is no more hardware
            if (res == SV_ERROR_DEVICENOTFOUND)
                return 1;

            logTF("card %d: sv_openex(%s) %s\n", card, card_str, sv_geterrortext(res));
            return 0;
        }
    }

    return 1;
}

void set_aes_option(int channel, int enable_aes)
{
    int opt_value = (enable_aes ? SV_AUDIOINPUT_AESEBU : SV_AUDIOINPUT_AIV);
    int res = sv_option_set(a_sv[channel], SV_OPTION_AUDIOINPUT, opt_value);
    if (SV_OK != res)
    {
        logTF("channel %d: sv_option_set(SV_OPTION_AUDIOINPUT) failed: %s\n", channel, sv_geterrortext(res));
    }
}

void set_sync_option(int channel, int sync_type, int width)
{
    if (sync_type == -1) {
        // default to bilevel for SD or trilevel for HD
        if (width <= 720)
            sync_type = SV_SYNC_BILEVEL;
        else
            sync_type = SV_SYNC_TRILEVEL;
    }

    int res = sv_sync(a_sv[channel], sync_type);
    if (res != SV_OK) {
        logTF("channel %d: sv_sync(0x%08x) failed: %s\n", channel, sync_type, sv_geterrortext(res));
    }
}

static void catch_sigusr1(int sig_number)
{
    // toggle a flag
}

static void catch_sigint(int sig_number)
{
    printf("\nReceived signal %d - ", sig_number);
    cleanup_exit(0, NULL);
}

static void log_avsync_analysis(int chan, int lastframe, const uint8_t *addr, unsigned long audio12_offset, unsigned long audio34_offset)
{
    int line_size = width*2;
    int click1 = 0, click1_off = -1;
    int click2 = 0, click2_off = -1;
    int click3 = 0, click3_off = -1;
    int click4 = 0, click4_off = -1;

    int flash = find_red_flash_uyvy(addr, line_size);
    find_audio_click_32bit_stereo(addr + audio12_offset,
                                                &click1, &click1_off,
                                                &click2, &click2_off);
    find_audio_click_32bit_stereo(addr + audio34_offset,
                                                &click3, &click3_off,
                                                &click4, &click4_off);

    if (flash || click1 || click2 || click3 || click4) {
        if (flash)
            logTF("chan %d: %5d  red-flash      \n", chan, lastframe + 1);
        if (click1)
            logTF("chan %d: %5d  a1off=%d %.1fms%s\n", chan, lastframe + 1, click1_off, click1_off / 48.0 , (click1_off > 480 ? " A/V sync error!" : ""));
        if (click2)
            logTF("chan %d: %5d  a2off=%d %.1fms\n", chan, lastframe + 1, click2_off, click2_off / 48.0);
        if (click3)
            logTF("chan %d: %5d  a3off=%d %.1fms\n", chan, lastframe + 1, click3_off, click3_off / 48.0);
        if (click4)
            logFF("chan %d: %5d  a4off=%d %.1fms\n", chan, lastframe + 1, click4_off, click4_off / 48.0);
    }
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
        const long long shm_limit = 0x40000000; // 1GiB

        if (k_shmmax > shm_limit) {
            printf("shmmax=%lld (%.3f MiB) probably too big, reducing to %lld MiB\n", k_shmmax, k_shmmax / (1024 * 1024.0), shm_limit / (1024 * 1024) );
            k_shmmax = shm_limit;
        }
    }

    // calculate reasonable ring buffer length
    // reduce by small number 5 to leave a little room for other shared mem
    ring_len = k_shmmax / num_channels / element_size - 5;
    printf("shmmax=%lld (%.3f MiB) calculated per channel ring_len=%d\n", k_shmmax, k_shmmax / (1024*1024.0), ring_len);

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
                fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
                return 0;
            }
        }
        else
        {
            perror("shmget control key 9");
            fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
            return 0;
        }
    }
    p_control = (NexusControl*)shmat(control_id, NULL, 0);

    p_control->channels = num_channels;
    p_control->ringlen = ring_len;
    p_control->elementsize = element_size;

    p_control->width = width;
    p_control->height = height;
    p_control->frame_rate_numer = frame_rate_numer;
    p_control->frame_rate_denom = frame_rate_denom;
    p_control->pri_video_format = video_format;
    p_control->sec_video_format = video_secondary_format;
    p_control->sec_width = sec_width;
    p_control->sec_height = sec_height;
    p_control->master_tc_type = master_type;
    p_control->master_tc_channel = master_channel;

    p_control->audio12_offset = audio_offset;
    p_control->audio34_offset = audio_offset + audio_pair_size;
    if (audio8ch) {
        p_control->audio56_offset = audio_offset + audio_pair_size * 2;
        p_control->audio78_offset = audio_offset + audio_pair_size * 3;
    }
    else {
        p_control->audio56_offset = 0;
        p_control->audio78_offset = 0;
    }
    p_control->audio_size = audio_size;

    p_control->vitc_offset = vitc_offset;
    p_control->ltc_offset = ltc_offset;
    p_control->sys_time_offset = sys_time_offset;
    p_control->tick_offset = tick_offset;
    p_control->signal_ok_offset = signal_ok_offset;
    p_control->num_aud_samp_offset = num_aud_samp_offset;
    p_control->frame_number_offset = frame_number_offset;
    p_control->sec_video_offset = dma_video_size + audio_size;

    p_control->source_name_update = 0;
    if (pthread_mutex_init(&p_control->m_source_name_update, NULL) != 0) {
        fprintf(stderr, "Mutex init error\n");
        return 0;
    }

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

// Return number of seconds between 1 Jan 1970 and last midnight
// where midnight is localtime not GMT.
static time_t today_midnight_time(void)
{
    struct tm now_tm;

    time_t now = time(NULL);
    localtime_r(&now, &now_tm);
    now_tm.tm_sec = 0;
    now_tm.tm_min = 0;
    now_tm.tm_hour = 0;
    return mktime(&now_tm);
}

// Given a 64bit time-of-day corresponding to when a frame was captured,
// calculate a derived timecode from the global master timecode.
static int derive_timecode_from_master(int64_t tod_rec, int64_t *p_diff_to_master)
{
    int derived_tc = 0;
    int64_t diff_to_master = 0;

    PTHREAD_MUTEX_LOCK( &m_master_tc )

    // if master_tod is not set, derived_tc is also left as 0
    if (master_tod) {
        // compute offset between this chan's recorded frame's time-of-day and master's
        diff_to_master = tod_rec - master_tod;
        int int_diff = diff_to_master;
        int div;
        if (int_diff < 0)   // -ve range is from -20000 to -1
            div = (int_diff - 19999) / 40000;
        else                // +ve range is from 0 to 19999
            div = (int_diff + 20000) / 40000;
        derived_tc = master_tc + div;
    }

    PTHREAD_MUTEX_UNLOCK( &m_master_tc )

    // return diff_to_master for logging purposes
    if (p_diff_to_master)
        *p_diff_to_master = diff_to_master;

    return derived_tc;
}

//
// write_picture()
//
// Gets a frame from the SDI FIFO and stores it to memory ring buffer
// (also can write to disk as a debugging option)
static int write_picture(int chan, sv_handle *sv, sv_fifo *poutput, int recover_from_video_loss)
{
    sv_fifo_buffer      *pbuffer;
    sv_fifo_bufferinfo  bufferinfo;
    int                 get_res;
    int                 put_res;
    int                 ring_len = p_control->ringlen;
    NexusBufCtl         *pc = &(p_control->channel[chan]);

    // Get sv memmory buffer
    // Ignore problems with missing audio, and carry on with good video but zeroed audio
    int flags = 0;
    if (recover_from_video_loss)
    {
        flags |= SV_FIFO_FLAG_FLUSH;
        logTF("chan %d: Setting SV_FIFO_FLAG_FLUSH\n", chan);
    }
    //logTF("chan %d: calling sv_fifo_getbuffer()...\n", chan);
    get_res = sv_fifo_getbuffer(sv, poutput, &pbuffer, NULL, flags);
    //logTF("chan %d: sv_fifo_getbuffer() returned\n", chan);
    if (get_res != SV_OK && get_res != SV_ERROR_INPUT_AUDIO_NOAIV
            && get_res != SV_ERROR_INPUT_AUDIO_NOAESEBU)
    {
        return get_res;
    }

    // dma buffer is structured as follows
    //
    // 0x0000 video frame                   offset= 0
    // 0x0000 audio ch. 0, 1 interleaved    offset= pbuffer->audio[0].addr[0]
    // 0x0000 audio ch. 2, 3 interleaved    offset= pbuffer->audio[0].addr[1]
    //

    // check audio offset
    /*fprintf(stderr, "chan = %d, audio_offset = %d, video[0].addr = %p, audio[0].addr[0-4] = %p %p %p %p\n",
        chan,
        audio_offset,
        pbuffer->video[0].addr,
        pbuffer->audio[0].addr[0],
        pbuffer->audio[0].addr[1],
        pbuffer->audio[0].addr[2],
        pbuffer->audio[0].addr[3]);*/

    // Clear frame number.
    // Do this first in order to mark video as changed before we start to change it.
    // We can't actually set the frame number now because the audio DMA will overwrite it.
    int frame_number = 0;
    memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + frame_number_offset, &frame_number, sizeof(int));

    uint8_t *vid_dest = ring[chan] + element_size * ((pc->lastframe+1) % ring_len);
    uint8_t *dma_dest = vid_dest;
    if (video_format == Format422PlanarYUV || video_format == Format422PlanarYUVShifted) {
#if 0
        // Store frame at pc->lastframe+2 to allow space for UYVY->YUV422 conversion
        dma_dest = ring[chan] + element_size * ((pc->lastframe+2) % ring_len);
#else
        // Store frame in video work area (it is faster)
        dma_dest = video_work_area[chan];
#endif
    }
    pbuffer->dma.addr = (char *)dma_dest;
    pbuffer->dma.size = dma_total_size;         // video + audio

    // read frame from DVS chan
    // reception of a SIGUSR1 can sometimes cause this to fail
    // If it fails we should restart fifo.
    //logTF("chan %d: calling sv_fifo_putbuffer()...\n", chan);
    put_res = sv_fifo_putbuffer(sv, poutput, pbuffer, &bufferinfo);
    if (put_res != SV_OK)
    {
        sv_errorprint(sv, put_res);
        fprintf(stderr, "sv_fifo_putbuffer failed, restarting fifo\n");
        SV_CHECK( sv_fifo_reset(sv, poutput) );
        SV_CHECK( sv_fifo_start(sv, poutput) );
    }
    //logTF("chan %d: sv_fifo_putbuffer() returned ok\n", chan);

    // set flag so we can zero audio if not present
    int no_audio = (SV_ERROR_INPUT_AUDIO_NOAIV == get_res
                || SV_ERROR_INPUT_AUDIO_NOAESEBU == get_res
                || 0 == pbuffer->audio[0].addr[0]);

    if (test_avsync && !no_audio)
    {
        log_avsync_analysis(chan, pc->lastframe,
            dma_dest, (unsigned long)pbuffer->audio[0].addr[0], (unsigned long)pbuffer->audio[0].addr[1]);
    }

    // Get current time-of-day time and this chan's current hw clock.
    int current_tick;
    unsigned int h_clock, l_clock;
    SV_CHECK( sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock) );
    int64_t tod = gettimeofday64();

    // Use the hw-clock time the putbuffer'd frame was recorded,
    // to calculate the time-of-day the frame was recorded.
    // (pbuffer->control.clock_high/low appears identical to bufferinfo.clock_high/low)
    // Comment from Jim:
    // I found that as well, except for the 10th or 11th frame (when the FIFO
    // first wraps round?), when it jumps back in time a lot. Using pbuffer
    // means I don't have to pass a sv_fifo_bufferinfo to putbuffer.
    int64_t cur_clock = (int64_t)h_clock * 0x100000000LL + l_clock;
    int64_t rec_clock = (int64_t)bufferinfo.clock_high * 0x100000000LL + bufferinfo.clock_low;
    int64_t clock_diff = cur_clock - rec_clock;
    int64_t tod_rec = tod - clock_diff;

    if (video_format == Format422PlanarYUV || video_format == Format422PlanarYUVShifted)
    {
        uint8_t *vid_input = dma_dest;

        // Use hard-to-code picture when benchmarking
        if (benchmark)
            vid_input = benchmark_video;

        // Time this
        //int64_t start = gettimeofday64();

        // Repack to planar YUV 4:2:2
        uyvy_to_yuv422(     width, height,
                            video_format == Format422PlanarYUVShifted,  // do DV50 line shift?
                            vid_input,                      // input
                            vid_dest);                      // output

        // End of timing
        //int64_t end = gettimeofday64();
        //fprintf(stderr, "uyvy_to_yuv422() took %6"PRIi64" microseconds\n", end - start);

        // copy audio to match
        if (!no_audio)
        {
            memcpy( vid_dest + audio_offset,    // dest
                    dma_dest + audio_offset,    // src
                    audio_size);
        }
    }

    if (no_audio)
    {
        memset( vid_dest + audio_offset, 0, audio_size);
    }

    // Convert primary video into secondary video buffer
    if (video_secondary_format != FormatNone) {
        if (width > 720) {
            // HD primary video, scale and reformat to SD secondary
            PixelFormat in_pixfmt = (video_format == Format422UYVY) ? PIX_FMT_UYVY422 : PIX_FMT_YUV422P;
            PixelFormat out_pixfmt = PIX_FMT_YUV420P;
            if (video_secondary_format == Format422PlanarYUV ||
                video_secondary_format == Format422PlanarYUVShifted) {
                out_pixfmt = PIX_FMT_YUV422P;
            }

            td[chan].scale_context = sws_getCachedContext(td[chan].scale_context,
                    width, height,          // input WxH
                    in_pixfmt,
                    sec_width, sec_height,  // output WxH
                    out_pixfmt,
                    SWS_FAST_BILINEAR,
                    NULL, NULL, NULL);
                    // other flags include: SWS_FAST_BILINEAR, SWS_BILINEAR, SWS_BICUBIC, SWS_POINT
                    // SWS_AREA, SWS_BICUBLIN, SWS_GAUSS, ... SWS_PRINT_INFO (debug)
            avpicture_fill( (AVPicture*)td[chan].inFrame,
                            vid_dest,                                   // captured video
                            in_pixfmt,
                            width, height);
            avpicture_fill( (AVPicture*)td[chan].outFrame,
                            vid_dest + (audio_offset + audio_size),     // output to secondary video
                            out_pixfmt,
                            sec_width, sec_height);
            sws_scale(td[chan].scale_context,
                            td[chan].inFrame->data, td[chan].inFrame->linesize,
                            0, height,
                            td[chan].outFrame->data, td[chan].outFrame->linesize);
        }
        else {
            // SD primary video, reformat for SD secondary
            if (video_secondary_format == Format422PlanarYUV || video_secondary_format == Format422PlanarYUVShifted) {
                // Repack to planar YUV 4:2:2
                uyvy_to_yuv422( width, height,
                                video_secondary_format == Format422PlanarYUVShifted,    // do DV50 line shift?
                                dma_dest,                                   // input
                                vid_dest + (audio_offset + audio_size));    // output
            }
            else {
                // Downconvert to 4:2:0 YUV buffer located just after audio
                if (video_secondary_format == Format420PlanarYUVShifted) {  // do DV25 style subsampling?
                    uyvy_to_yuv420_DV_sampling( width, height,
                                1,                                          // do DV25 line shift?
                                dma_dest,                                   // input
                                vid_dest + (audio_offset + audio_size));    // output
                }
                else {
                    uyvy_to_yuv420( width, height,
                                0,                                          // do DV25 line shift?
                                dma_dest,                                   // input
                                vid_dest + (audio_offset + audio_size));    // output
                }
            }
        }
    }

    // Handle buggy field order (can happen with misconfigured camera)
    // Incorrect field order causes vitc_tc and vitc2 to be swapped.
    // If the high bit is set on vitc use vitc2 instead.
    int vitc_to_use = anc_tc ? pbuffer->anctimecode.dvitc_tc[0] : pbuffer->timecode.vitc_tc;
    if ((unsigned)vitc_to_use >= 0x80000000) {
        vitc_to_use = anc_tc ? pbuffer->anctimecode.dvitc_tc[1] : pbuffer->timecode.vitc_tc2;
        if (verbose) {
            PTHREAD_MUTEX_LOCK( &m_log )
            logFF("chan %d: 1st vitc value >= 0x80000000 (0x%08x), using 2nd vitc (0x%08x)\n", chan, anc_tc ? pbuffer->anctimecode.dvitc_tc[0] : pbuffer->timecode.vitc_tc, vitc_to_use);
            PTHREAD_MUTEX_UNLOCK( &m_log )
        }
    }
    int vitc_as_int = dvs_tc_to_int(vitc_to_use);
    int orig_vitc_as_int = vitc_as_int;

    // A similar check must be done for LTC since the field
    // flag is occasionally set when the fifo call returns
    int ltc_to_use = anc_tc ? pbuffer->anctimecode.dltc_tc : pbuffer->timecode.ltc_tc;
    if ((unsigned)ltc_to_use >= 0x80000000) {
        ltc_to_use = (unsigned)(anc_tc ? pbuffer->anctimecode.dltc_tc : pbuffer->timecode.ltc_tc) & 0x7fffffff;
        if (verbose) {
            PTHREAD_MUTEX_LOCK( &m_log )
            logFF("chan %d: ltc tc >= 0x80000000 (0x%08x), masking high bit (0x%08x)\n", chan, anc_tc ? pbuffer->anctimecode.dltc_tc : pbuffer->timecode.ltc_tc, ltc_to_use);
            PTHREAD_MUTEX_UNLOCK( &m_log )
        }
    }
    int ltc_as_int = dvs_tc_to_int(ltc_to_use);
    int orig_ltc_as_int = ltc_as_int;

    // If enabled, handle master timecode distribution
    int derived_tc = 0;
    int64_t diff_to_master = 0;
    if (master_type != NexusTC_None) {
        if (chan == master_channel) {
            // Store timecode and time-of-day the frame was recorded in shared variable
            PTHREAD_MUTEX_LOCK( &m_master_tc )
            master_tc = (master_type == NexusTC_LTC) ? ltc_as_int : vitc_as_int;
            master_tod = tod_rec;
            PTHREAD_MUTEX_UNLOCK( &m_master_tc )
        }
        else {
            derived_tc = derive_timecode_from_master(tod_rec, &diff_to_master);

            // Save calculated timecode
            if (master_type == NexusTC_LTC)
                ltc_as_int = derived_tc;
            else
                vitc_as_int = derived_tc;
        }
    }

    // Lookup last frame's timecodes to calculate timecode discontinuity
    int last_vitc = 0;
    int last_ltc = 0;
    if (pc->lastframe > -1) {       // Only process discontinuity when not the first frame
        last_vitc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + vitc_offset);
        last_ltc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + ltc_offset);

        // Check number of frames to recover if any
        // A maximum of 50 frames will be recovered.
        int recover_diff = (recover_timecode_type == RecoverVITC) ? vitc_as_int - last_vitc : ltc_as_int - last_ltc;
        if (recover_from_video_loss && recover_diff > 1 && recover_diff < 52) {
            logTF("chan %d: Need to recover %d frames\n", chan, recover_diff);

            vitc_as_int += recover_diff - 1;
            ltc_as_int += recover_diff - 1;

            // Increment pc->lastframe by amount to avoid discontinuity
            pc->lastframe += recover_diff - 1;

            // Ideally we would copy the dma transferred frame into its correct position
            // but this needs testing.
            //uint8_t *tmp_frame = (uint8_t*)malloc(width*height*2);
            //memcpy(tmp_frame, vid_dest, width*height*2);
            //free(tmp_frame);
            logTF("chan %d: Recovered.  lastframe=%d ltc=%d\n", chan, pc->lastframe, ltc_as_int);
        }
    }

    // Copy timecode into last 4 bytes of element_size
    // This is a bit sneaky but saves maintaining a separate ring buffer
    // Also copy LTC just before VITC timecode as int.
    memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + vitc_offset, &vitc_as_int, sizeof(int));
    memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + ltc_offset, &ltc_as_int, sizeof(int));

    // Copy "frame" tick (tick / 2) into unused area at end of element_size
    int frame_tick = pbuffer->control.tick / 2;
    memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + tick_offset, &frame_tick, sizeof(int));

    // Set signal_ok flag
    *(int *)(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + signal_ok_offset) = 1;

    // Set number of audio samples captured into ring element (not constant for NTSC)
    // audio[0].size is total size in bytes of 2 channels of 32bit samples
    int num_audio_samples = pbuffer->audio[0].size / 2 / 4;
    memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + num_aud_samp_offset, &num_audio_samples, sizeof(int));

    // Get info structure for statistics
    sv_fifo_info info;
    SV_CHECK(sv_fifo_status(sv, poutput, &info));

    // sys_time uses system clock as timecode source (tod_rec is in microsecs since 1970)
    int64_t sys_time_microsec = tod_rec - (today_midnight_time() * 1000000LL);
    // compute sys_time timecode as int number of frames since midnight
    int sys_time = (int)(sys_time_microsec * frame_rate_numer / (1000000LL * frame_rate_denom));
    memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + sys_time_offset, &sys_time, sizeof(int));

    // Timecode error occurs when difference is not exactly 1
    // or (around midnight) not exactly -2159999
    // (ignore the first frame captured when lastframe is -1)
    int vitc_diff = vitc_as_int - last_vitc;
    int ltc_diff = ltc_as_int - last_ltc;
    int tc_err = (vitc_diff != 1 && vitc_diff != -2159999
        && ltc_diff != 1 && ltc_diff != -2159999) && pc->lastframe != -1;

    // log timecode discontinuity if any, or give verbose log of specified chan
    if (tc_err || (verbose && chan == verbose_channel)) {
        PTHREAD_MUTEX_LOCK( &m_log )
        logTF("chan %d: lastframe=%6d tick/2=%7d hwdrop=%5d vitc_tc=%8x vitc_tc2=%8x ltc=%8x tc_int=%d ltc_int=%d last_vitc=%d vitc_diff=%d %s ltc_diff=%d %s\n",
        chan, pc->lastframe, pbuffer->control.tick / 2,
        info.dropped,
        anc_tc ? pbuffer->anctimecode.dvitc_tc[0] : pbuffer->timecode.vitc_tc,
        anc_tc ? pbuffer->anctimecode.dvitc_tc[1] : pbuffer->timecode.vitc_tc2,
        anc_tc ? pbuffer->anctimecode.dltc_tc : pbuffer->timecode.ltc_tc,
        vitc_as_int,
        ltc_as_int,
        last_vitc,
        vitc_diff,
        vitc_diff != 1 ? "!" : "",
        ltc_diff,
        ltc_diff != 1 ? "!" : "");
        PTHREAD_MUTEX_UNLOCK( &m_log )
    }

    if (verbose) {
        PTHREAD_MUTEX_LOCK( &m_log )        // guard logging with mutex to avoid intermixing

        if (info.dropped != pc->hwdrop)
            logTF("chan %d: lf=%7d vitc=%8d ltc=%8d dropped=%d\n", chan, pc->lastframe+1, vitc_as_int, ltc_as_int, info.dropped);

        logFF("chan[%d]: tick=%d hc=%u,%u diff_to_mast=%12"PRIi64"  lf=%7d vitc=%8d ltc=%8d (orig v=%8d l=%8d] drop=%d\n", chan,
            pbuffer->control.tick,
            pbuffer->control.clock_high, pbuffer->control.clock_low,
            diff_to_master,
            pc->lastframe+1, vitc_as_int, ltc_as_int,
            orig_vitc_as_int, orig_ltc_as_int,
            info.dropped);

        PTHREAD_MUTEX_UNLOCK( &m_log )      // end logging guard
    }

    // Query hardware health (once a second to reduce excessive sv_query calls)
    if ((pc->lastframe+1) % 25 == 0) {
        int temp;
        if (sv_query(sv, SV_QUERY_TEMPERATURE, 0, &temp) == SV_OK) {
            pc->hwtemperature = ((double)temp) / 65536.0;
        }
    }

    // Set frame number
    frame_number = pc->lastframe + 1;
    memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + frame_number_offset, &frame_number, sizeof(int));

    // signal frame is now ready
    PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
    pc->hwdrop = info.dropped;
    pc->lastframe++;
    PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

    return SV_OK;
}

static int write_dummy_frames(sv_handle *sv, int chan, int current_frame_tick, int tick_last_dummy_frame, int64_t tod_tc_read)
{
    // No video so copy "no video" frame instead
    //
    // Use this chan's internal tick counter to determine when and
    // how many frames to fabricate
    if (verbose > 1)
        logTF("chan: %d frame_tick=%d tick_last_dummy_frame=%d\n", chan, current_frame_tick, tick_last_dummy_frame);

    if (tick_last_dummy_frame == -1 || current_frame_tick - tick_last_dummy_frame > 0) {
        // Work out how many frames to make
        int num_dummy_frames;
        if (tick_last_dummy_frame == -1)
            num_dummy_frames = 1;
        else
            num_dummy_frames = current_frame_tick - tick_last_dummy_frame;

        // first dummy frame will get the closest timecode to the master timecode
        // subsequent dummy frames will increment by 1
        int last_vitc = derive_timecode_from_master(tod_tc_read, NULL);
        int last_ltc = derive_timecode_from_master(tod_tc_read, NULL);

        int i;
        for (i = 0; i < num_dummy_frames; i++) {
            // Read ring buffer info
            int                 ring_len = p_control->ringlen;
            NexusBufCtl         *pc = &(p_control->channel[chan]);

            // Set frame number
            // Do this first in order to mark video as changed before we start to change it.
            int frame_number = pc->lastframe + 1;
            memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + frame_number_offset, &frame_number, sizeof(int));

            // dummy video frame
            uint8_t *vid_dest = ring[chan] + element_size * ((pc->lastframe+1) % ring_len);
            memcpy(vid_dest, no_video_frame, width*height*2);

            // dummy video frame in secondary buffer
            if (no_video_secondary_frame)
            {
                uint8_t *vid_dest_sec = ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + audio_offset + audio_size;
                if (video_secondary_format == Format420PlanarYUV ||
                    video_secondary_format == Format420PlanarYUVShifted)
                {
                    // 4:2:0
                    memcpy(vid_dest_sec, no_video_secondary_frame, sec_width*sec_height*3/2);
                }
                else
                {
                    // 4:2:2
                    memcpy(vid_dest_sec, no_video_secondary_frame, sec_width*sec_height*2);
                }
            }

            // write silent 4 channels of 32bit audio
            memset(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + audio_offset, 0, 1920*4*4);

            // Increment timecode by 1 for dummy frames after the first
            if (i > 0) {
                if (pc->lastframe >= 0) {
                    last_vitc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + vitc_offset);
                    last_ltc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + ltc_offset);
                }
                last_vitc++;
                last_ltc++;
            }
            int last_ftk = 0;
            if (pc->lastframe >= 0) {
                last_ftk = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + tick_offset);
            }
            last_ftk++;
            memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + vitc_offset, &last_vitc, sizeof(int));
            memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + ltc_offset, &last_ltc, sizeof(int));
            memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + tick_offset, &last_ftk, sizeof(int));

            // Indicate we've got a bad video signal
            *(int *)(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + signal_ok_offset) = 0;

            if (verbose)
            {
                logTF("chan: %d i:%2d  cur_frame_tick=%d tick_last_dummy_frame=%d last_ftk=%d tc=%d ltc=%d\n", chan, i, current_frame_tick, tick_last_dummy_frame, last_ftk, last_vitc, last_ltc);
            }

            // signal frame is now ready
            PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
            pc->lastframe++;
            PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

            tick_last_dummy_frame = current_frame_tick - num_dummy_frames + 1;
        }
    }

    return tick_last_dummy_frame;
}

static void wait_for_good_signal(sv_handle *sv, int chan, int required_good_frames)
{
    int good_frames = 0;
    int tick_last_dummy_frame = -1;

    // Poll until we get a good video signal for at least required_good_frames
    while (1) {
        int                 current_tick;
        unsigned int        h_clock, l_clock;
        sv_timecode_info    timecodes;
        int                 sdiA, videoin, audioin, tc_status;

        SV_CHECK( sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock) );
        SV_CHECK( sv_timecode_feedback(sv, &timecodes, NULL) );     // read current input timecodes
        int64_t tod_tc_read = gettimeofday64();

        // Keep master timecode ticking over when video signal is not present
        if (chan == master_channel) {
            int ltc_as_int = dvs_tc_to_int(timecodes.altc_tc);
            int vitc_as_int = dvs_tc_to_int(timecodes.avitc_tc[0]);

            // update time-of-day the timecodes were read
            PTHREAD_MUTEX_LOCK( &m_master_tc )
            master_tc = (master_type == NexusTC_LTC) ? ltc_as_int : vitc_as_int;
            master_tod = tod_tc_read;
            PTHREAD_MUTEX_UNLOCK( &m_master_tc )
        }

        SV_CHECK( sv_query(sv, SV_QUERY_INPUTRASTER_SDIA, 0, &sdiA) );
        SV_CHECK( sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin) );
        SV_CHECK( sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin) );
        SV_CHECK( sv_query(sv, SV_QUERY_VALIDTIMECODE, 0, &tc_status) );

        if (verbose > 1) {
            printf("\r%8d raster=0x%08x video=%s audio=%s tc_status=%s%s%s%s%s%s (0x%x)",
                current_tick,
                sdiA,
                videoin == SV_OK ? "OK" : "error",
                audioin == SV_OK ? "OK" : "error",
                (tc_status & SV_VALIDTIMECODE_LTC) ? "LTC " : "",
                (tc_status & SV_VALIDTIMECODE_DLTC) ? "DLTC " : "",
                (tc_status & SV_VALIDTIMECODE_VITC_F1) ? "VITC/F1 " : "",
                (tc_status & SV_VALIDTIMECODE_VITC_F2) ? "VITC/F2 " : "",
                (tc_status & SV_VALIDTIMECODE_DVITC_F1) ? "DVITC/F1 " : "",
                (tc_status & SV_VALIDTIMECODE_DVITC_F2) ? "DVITC/F2 " : "",
                tc_status
                );
            fflush(stdout);
        }

        if (videoin == SV_OK) {
            good_frames++;
            if (good_frames == required_good_frames) {
                if (verbose)
                    printf("\n");
                break;              // break from while(1) loop
            }
        }

        // tick in terms of frames, ignoring odd ticks representing fields
        int current_frame_tick = current_tick / 2;

        // Write dummy frames to chan's ring buffer and return tick of last frame written
        tick_last_dummy_frame = write_dummy_frames(sv, chan, current_frame_tick, tick_last_dummy_frame, tod_tc_read);

        usleep(18*1000);        // sleep slightly less than one tick (20,000)
    }

    return;
}

// channel number passed as void * (using cast)
static void * sdi_monitor(void *arg)
{
    long                chan = (long)arg;
    sv_handle           *sv = a_sv[chan];
    sv_fifo             *poutput;
    sv_info             status_info;
    sv_storageinfo      storage_info;

    // Setup thread-specific data;
    td[chan].scale_context = NULL;
    td[chan].inFrame = avcodec_alloc_frame();
    td[chan].outFrame = avcodec_alloc_frame();

    // Get info on SDI capture
    SV_CHECK( sv_status( sv, &status_info) );

    SV_CHECK( sv_storage_status(sv,
                            0,
                            NULL,
                            &storage_info,
                            sizeof(storage_info),
                            0) );

    // To aid debugging, delay channels by a small amount of time so that
    // they appear in log files in channel 0..7 order
    if (chan > 0) {
        usleep(10000 + 5000 * chan);                
    }

#ifdef SV_FIFO_FLAG_NO_LIVE
    // No loop-through to avoid clash with player
    // (macro available in sdk3.2.14.0 and later versions)
    const int flagbase = SV_FIFO_FLAG_NO_LIVE;
#else
    const int flagbase = 0;
#endif

    SV_CHECK( sv_fifo_init( sv,
                            &poutput,
                            1,          // Input
                            FALSE,      // bShared (TRUE for input/output share memory)
                            1,          // DMA FIFO
                            flagbase,   // Base SV_FIFO_FLAG_xxxx flags
                            0) );       // nFrames (0 means use maximum)

    SV_CHECK( sv_fifo_start(sv, poutput) );

    logTF("chan %ld: fifo init/start completed\n", chan);

    int recover_from_video_loss = 0;
    int last_res = -1;
    while (1)
    {
        int res;

        // write_picture() returns the result of sv_fifo_getbuffer()
        if ((res = write_picture(chan, sv, poutput, recover_from_video_loss)) != SV_OK)
        {
            // Display error only when things change
            if (res != last_res) {
                logTF("chan %ld: failed to capture video: (%d) %s\n", chan, res,
                    res == SV_ERROR_INPUT_VIDEO_NOSIGNAL ? "INPUT_VIDEO_NOSIGNAL" : sv_geterrortext(res));
            }
            last_res = res;

            SV_CHECK( sv_fifo_stop(sv, poutput, 0) );
            wait_for_good_signal(sv, chan, 25);

            // Note the channel's tick counter appears to pause while
            // the init() and start() take place
            //SV_CHECK( sv_fifo_init(sv, &poutput, TRUE, FALSE, TRUE, FALSE, 0) );
            SV_CHECK( sv_fifo_reset(sv, poutput) );
            SV_CHECK( sv_fifo_start(sv, poutput) );

            // flag that we need to recover from loss in video signal
            recover_from_video_loss = 1;
            continue;
        }

        // res will be SV_OK to reach this point
        if (res != last_res) {
            logTF("chan %ld: Video signal OK\n", chan);
        }
        recover_from_video_loss = 0;
        last_res = res;
    }

    return NULL;
}


static void usage_exit(void)
{
    fprintf(stderr, "Usage: dvs_sdi [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "E.g.   dvs_sdi -c 2\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -f <video format>    uncompressed video format to store in ring buffer:\n");
    fprintf(stderr, "                         YUV422 - Planar YUV 4:2:2 16bpp (default)\n");
    fprintf(stderr, "                         DV50   - Planar YUV 4:2:2 with field order line shift\n");
    fprintf(stderr, "                         UYVY   - UYVY 4:2:2 16bpp\n");
    fprintf(stderr, "    -s <secondary fmt>   uncompressed video format for the secondary video buffer:\n");
    fprintf(stderr, "                         None   - secondary buffer disabled (default)\n");
    fprintf(stderr, "                         YUV422 - secondary buffer is planar YUV 4:2:2 16bpp\n");
    fprintf(stderr, "                         DV50   - secondary buffer is planar YUV 4:2:2 with field order line shift\n");
    fprintf(stderr, "                         MPEG   - secondary buffer is planar YUV 4:2:0\n");
    fprintf(stderr, "                         DV25   - secondary buffer is planar YUV 4:2:0 with field order line shift\n");
    fprintf(stderr, "    -mode vid[:AUDIO8]   set input mode on all DVS cards, vid is one of:\n");
    fprintf(stderr, "                         PAL,NTSC,1920x1080i50,1920x1080i60,1280x720p50,1280x720p60\n");
    fprintf(stderr, "                         AUDIO8 enables 8 audio channels per SDI input\n");
    fprintf(stderr, "                         E.g. -mode 1920x1080i50:AUDIO8\n");
    fprintf(stderr, "    -sync <type>         set input sync type on all DVS cards, sync is one of:\n");
    fprintf(stderr, "                         bilevel   - analog bilevel sync [default for PAL/NTSC mode]\n");
    fprintf(stderr, "                         trilevel  - analog trilevel sync [default for HD modes]\n");
    fprintf(stderr, "                         internal  - freerunning\n");
    fprintf(stderr, "                         external  - sync to incoming SDI signal\n");
    fprintf(stderr, "    -mt <master tc type> type of master channel timecode to use: VITC, LTC, OFF\n");
    fprintf(stderr, "    -mc <master ch>      channel to use as timecode master: 0..7\n");
    fprintf(stderr, "    -rt <recover type>   timecode type to calculate missing frames to recover: VITC, LTC\n");
    fprintf(stderr, "    -anctc               read \"DLTC\" and \"DVITC\" timecodes from RP188/RP196 ANC data\n");
    fprintf(stderr, "                         instead of SMPTE-12M LTC and VITC\n");
    fprintf(stderr, "    -c <max channels>    maximum number of channels to use for capture\n");
    fprintf(stderr, "    -m <max memory MiB>  maximum memory to use for ring buffers in MiB\n");
    fprintf(stderr, "    -a8                  use 8 audio tracks per video channel\n");
    fprintf(stderr, "    -aes                 use AES/EBU audio with default routing\n");
    fprintf(stderr, "    -aes4                use AES/EBU audio with 4,4 routing\n");
    fprintf(stderr, "    -aes8                use AES/EBU audio with 8,8 routing\n");
    fprintf(stderr, "    -avsync              perform avsync analysis - requires clapper-board input video\n");
    fprintf(stderr, "    -b <video_sample>    benchmark using video_sample instead of captured UYVY video frames\n");
    fprintf(stderr, "    -q                   quiet operation (fewer messages)\n");
    fprintf(stderr, "    -v                   increase verbosity\n");
    fprintf(stderr, "    -ld                  logfile directory\n");
    fprintf(stderr, "    -d <channel>         channel number to print verbose debug messages for\n");
    fprintf(stderr, "    -h                   usage\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main (int argc, char ** argv)
{
    int             n, max_channels = MAX_CHANNELS;
    long long       opt_max_memory = 0;
    const char      *logfiledir = ".";
    int             opt_video_mode = -1, current_video_mode = -1;
    int             opt_sync_type = -1;

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
        else if (strcmp(argv[n], "-h") == 0)
        {
            usage_exit();
        }
        else if (strcmp(argv[n], "-avsync") == 0)
        {
            test_avsync = 1;
        }
        else if (strcmp(argv[n], "-b") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            benchmark = 1;
            video_sample_file = argv[n+1];
            n++;
        }
        else if (strcmp(argv[n], "-ld") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            logfiledir = argv[n+1];
            n++;
        }
        else if (strcmp(argv[n], "-mt") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (strcmp(argv[n+1], "VITC") == 0) {
                master_type = NexusTC_VITC;
            }
            else if (strcmp(argv[n+1], "LTC") == 0) {
                master_type = NexusTC_LTC;
            }
            else if (strcmp(argv[n+1], "OFF") == 0 || strcmp(argv[n+1], "None") == 0) {
                master_type = NexusTC_None;
            }
            else {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-mc") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (sscanf(argv[n+1], "%d", &master_channel) != 1 ||
                master_channel > 7 || master_channel < 0)
            {
                fprintf(stderr, "-mc requires channel number from 0..7\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-rt") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (strcmp(argv[n+1], "VITC") == 0) {
                recover_timecode_type = RecoverVITC;
            }
            else if (strcmp(argv[n+1], "LTC") == 0) {
                recover_timecode_type = RecoverLTC;
            }
            else {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-anctc") == 0)
        {
            anc_tc = 1;
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
            if (argc <= n+1)
                usage_exit();

            if (sscanf(argv[n+1], "%d", &max_channels) != 1 ||
                max_channels > MAX_CHANNELS || max_channels <= 0)
            {
                fprintf(stderr, "-c requires integer maximum channel number <= %d\n", MAX_CHANNELS);
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-mode") == 0)
        {
            char vidmode[256] = "", audmode[256] = "";
            if (sscanf(argv[n+1], "%s:%s", vidmode, audmode) != 2) {
                if (sscanf(argv[n+1], "%s", vidmode) != 1) {
                    fprintf(stderr, "-mode requires option of the form videomode[:audiomode]\n");
                    return 1;
                }
            }

            // Default video mode is PAL/YUV422/FRAME/AUDIO8CH/32
            opt_video_mode =    SV_MODE_PAL |
                                SV_MODE_COLOR_YUV422 |
                                SV_MODE_STORAGE_FRAME |
                                SV_MODE_AUDIO_4CHANNEL;
            if (strcmp(vidmode, "PAL") == 0) {
                opt_video_mode = (opt_video_mode & ~SV_MODE_MASK) | SV_MODE_PAL;
            }
            else if (strcmp(vidmode, "NTSC") == 0) {
                opt_video_mode = (opt_video_mode & ~SV_MODE_MASK) | SV_MODE_NTSC;
            }
            else if (strcmp(vidmode, "1920x1080i50") == 0) {    // SMPTE274/25I
                opt_video_mode = (opt_video_mode & ~SV_MODE_MASK) | SV_MODE_SMPTE274_25I;
            }
            else if (strcmp(vidmode, "1920x1080i60") == 0) {    // SMPTE274/30I
                opt_video_mode = (opt_video_mode & ~SV_MODE_MASK) | SV_MODE_SMPTE274_30I;
            }
            else if (strcmp(vidmode, "1280x720p50") == 0) {     // SMPTE296/50P
                opt_video_mode = (opt_video_mode & ~SV_MODE_MASK) | SV_MODE_SMPTE296_50P;
            }
            else if (strcmp(vidmode, "1280x720p60") == 0) {     // SMPTE296/60P
                opt_video_mode = (opt_video_mode & ~SV_MODE_MASK) | SV_MODE_SMPTE296_60P;
            }

            // Default audio mode is 4 channels per SDI input
            if (strcmp(audmode, "AUDIO8") == 0) {
                audio8ch = 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-sync") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (strcmp(argv[n+1], "bilevel") == 0) {
                opt_sync_type = SV_SYNC_BILEVEL;
            }
            else if (strcmp(argv[n+1], "trilevel") == 0) {
                opt_sync_type = SV_SYNC_TRILEVEL;
            }
            else if (strcmp(argv[n+1], "internal") == 0) {
                opt_sync_type = SV_SYNC_INTERNAL;
            }
            else if (strcmp(argv[n+1], "external") == 0) {
                opt_sync_type = SV_SYNC_EXTERNAL;
            }
            else {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-d") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (sscanf(argv[n+1], "%d", &verbose_channel) != 1 ||
                verbose_channel > 7 || verbose_channel < 0)
            {
                fprintf(stderr, "-d requires channel number {0..7}\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-f") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (strcmp(argv[n+1], "UYVY") == 0) {
                video_format = Format422UYVY;
            }
            else if (strcmp(argv[n+1], "YUV422") == 0) {
                video_format = Format422PlanarYUV;
            }
            else if (strcmp(argv[n+1], "DV50") == 0) {
                video_format = Format422PlanarYUVShifted;
            }
            else {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-s") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (strcmp(argv[n+1], "None") == 0) {
                video_secondary_format = FormatNone;
            }
            else if (strcmp(argv[n+1], "YUV422") == 0) {
                video_secondary_format = Format422PlanarYUV;
            }
            else if (strcmp(argv[n+1], "DV50") == 0) {
                video_secondary_format = Format422PlanarYUVShifted;
            }
            else if (strcmp(argv[n+1], "MPEG") == 0) {
                video_secondary_format = Format420PlanarYUV;
            }
            else if (strcmp(argv[n+1], "DV25") == 0) {
                video_secondary_format = Format420PlanarYUVShifted;
            }
            else {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-a8") == 0)
        {
            audio8ch = 1;
        }
        else if (strcmp(argv[n], "-aes") == 0)
        {
            aes_audio = 1;
            aes_routing = 0;
        }
        else if (strcmp(argv[n], "-aes4") == 0)
        {
            aes_audio = 1;
            aes_routing = 4;
        }
        else if (strcmp(argv[n], "-aes8") == 0)
        {
            aes_audio = 1;
            aes_routing = 8;
        }
        else
        {
            fprintf(stderr, "unknown argument \"%s\"\n", argv[n]);
            return 1;
        }
    }

    // Display info on primary video format
    switch (video_format) {
        case Format422UYVY:
                logTF("Capturing video as UYVY (4:2:2)\n"); break;
        case Format422PlanarYUV:
                logTF("Capturing video as YUV 4:2:2 planar\n"); break;
        case Format422PlanarYUVShifted:
                logTF("Capturing video as YUV 4:2:2 planar with picture shifted down by one line\n"); break;
        default:
                logTF("Unsupported video buffer format\n");
                return 1;
    }

    // Display info on secondary video format
    switch (video_secondary_format) {
        case FormatNone:
                logTF("Secondary video buffer is disabled\n"); break;
        case Format422PlanarYUV:
                logTF("Secondary video buffer is YUV 4:2:2 planar\n"); break;
        case Format422PlanarYUVShifted:
                logTF("Secondary video buffer is YUV 4:2:2 planar with picture shifted down by one line\n"); break;
        case Format420PlanarYUV:
                logTF("Secondary video buffer is YUV 4:2:0 planar\n"); break;
        case Format420PlanarYUVShifted:
                logTF("Secondary video buffer is YUV 4:2:0 planar with picture shifted down by one line\n"); break;
        default:
                logTF("Unsupported Secondary video buffer format\n");
                return 1;
    }

    if (master_channel > max_channels-1) {
        logTF("Master channel number (%d) greater than highest channel number (%d)\n", master_channel, max_channels-1);
        return 1;
    }

    if (master_type == NexusTC_None)
        logTF("Master timecode not used\n");
    else
        logTF("Master timecode type is %s using channel %d\n",
                master_type == NexusTC_VITC ? "VITC" : "LTC", master_channel);

    logTF("Using %s to determine number of frames to recover when video re-aquired\n",
            recover_timecode_type == RecoverVITC ? "VITC" : "LTC");

    if (audio8ch)
    {
        logTF("Audio 8 channel mode enabled\n");
    }

    char logfile[FILENAME_MAX];
    strcpy(logfile, logfiledir);
    strcat(logfile, "/dvs_sdi");
    openLogFileWithDate(logfile);

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

    // Set video mode if specified
    if (opt_video_mode != -1) {
        if (! set_videomode_on_all_channels(max_channels, opt_video_mode))
            return 1;
    }

    //////////////////////////////////////////////////////
    // Attempt to open all sv cards
    //
    // card specified by string of form "PCI,card=n" where n = 0,1,2,3
    // For each card try to open the second channel "PCI,card=n,channel=1"
    // If you try "PCI,card=0,channel=0" on an old sdk you get SV_ERROR_SVOPENSTRING
    // If you see SV_ERROR_WRONGMODE, you need to "svram multichannel on"
    //

    // Check SDK multichannel capability
    if (check_sdk_version3())
    {
        sv_info status_info;
        int card = 0;
        int channel = 0;
        for (card = 0; card < max_channels && channel < max_channels; card++)
        {
            char card_str[64] = {0};

            snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=0", card);
    
            int res = sv_openex(&a_sv[channel],
                                card_str,
                                SV_OPENPROGRAM_DEMOPROGRAM,
                                SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                0,
                                0);
            if (res == SV_ERROR_WRONGMODE)
            {
                // Multichannel mode not on so doesn't like "channel=0"
                snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
        
                res = sv_openex(&a_sv[channel],
                                    card_str,
                                    SV_OPENPROGRAM_DEMOPROGRAM,
                                    SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                    0,
                                    0);
            }
            if (res == SV_OK)
            {
                /*
                int alignment = 0;
                sv_query(a_sv[channel], SV_QUERY_DMAALIGNMENT, 0, &alignment);
                fprintf(stderr, "Minimum memory alignment for DMA transfers = %d\n", alignment);
                */

                sv_status(a_sv[channel], &status_info);
                sv_query(a_sv[channel], SV_QUERY_MODE_CURRENT, 0, &current_video_mode);
                framerate_for_videomode(current_video_mode, &frame_rate_numer, &frame_rate_denom);
                logTF("card %d: device present (%dx%d videomode=0x%08X rate=%d/%d)\n", card, status_info.xsize, status_info.ysize, current_video_mode, frame_rate_numer, frame_rate_denom);

                if (width == 0)
                {
                    // Set size from first channel
                    width = status_info.xsize;
                    height = status_info.ysize;
                }
                else if (width != status_info.xsize || height != status_info.ysize)
                {
                    // Warn if other channels different from first
                    logTF("card %d: warning: different video size!\n", card);
                }

                // Set AES input
                set_aes_option(channel, aes_audio);

                set_sync_option(channel, opt_sync_type, status_info.xsize);

                // check for multichannel mode
                int multichannel_mode = 0;
                if ((res = sv_option_get(a_sv[channel], SV_OPTION_MULTICHANNEL, &multichannel_mode)) != SV_OK) {
                    logTF("card %d: sv_option_get(SV_OPTION_MULTICHANNEL) failed: %s\n", card, sv_geterrortext(res));
                }

                channel++;
                num_sdi_threads++;

                // If card has a multichannel mode on, open second channel
                if (multichannel_mode)
                {
                    if (channel < max_channels)
                    {
                        snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=1", card);
                        int res = sv_openex(&a_sv[channel],
                                    card_str,
                                    SV_OPENPROGRAM_DEMOPROGRAM,
                                    SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                    0,
                                    0);
                        if (res == SV_OK)
                        {
                            sv_status(a_sv[channel], &status_info);
                            logTF("card %d: opened second channel (%dx%d)\n", card, status_info.xsize, status_info.ysize);
                            // Set AES audio routing
                            if (aes_audio)
                            {
                                int opt_value;
                                switch (aes_routing)
                                {
                                case 4:
                                    opt_value = SV_AUDIOAESROUTING_4_4;
                                    break;
                                case 8:
                                    opt_value = SV_AUDIOAESROUTING_8_8;
                                    break;
                                case 0:
                                default:
                                    opt_value = SV_AUDIOAESROUTING_DEFAULT;
                                    break;
                                }
                                res = sv_option_set(a_sv[channel-1], SV_OPTION_AUDIOAESROUTING, opt_value);
                                if (SV_OK != res)
                                {
                                    logTF("card %d: sv_option_set(SV_OPTION_AUDIOAESROUTING) failed: %s\n", card, sv_geterrortext(res));
                                }
                            }

                            // Set AES input
                            set_aes_option(channel, aes_audio);

                            // Don't set the sync option for the second channel (set_sync_option())
                            // since you get a SV_ERROR_JACK_INVALID error
                            // (perhaps this is a hardware limitation which will eventually go away).

                            channel++;
                            num_sdi_threads++;
                        }
                        else
                        {
                            logTF("card %d: failed opening second channel: %s\n", card, sv_geterrortext(res));
                        }
                    }
                }
                else
                {
                    logTF("card %d: multichannel mode off\n", card);
                }
    
            }
            else
            {
                // typical reasons include:
                //  SV_ERROR_DEVICENOTFOUND
                //  SV_ERROR_DEVICEINUSE
                logTF("card %d: sv_openex(%s) %s\n", card, card_str, sv_geterrortext(res));
                continue;
            }
        }
    }
    else
    {
        // older SDK
        //
        // card specified by string of form "PCI,card=n" where n = 0,1,2,3
        //
        sv_info status_info;
        int card;
        for (card = 0; card < max_channels; card++)
        {
            char card_str[20] = {0};
    
            snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
    
            int res = sv_openex(&a_sv[card],
                                card_str,
                                SV_OPENPROGRAM_DEMOPROGRAM,
                                SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                0,
                                0);
            if (res == SV_OK)
            {
                sv_status(a_sv[card], &status_info);
                sv_query(a_sv[card], SV_QUERY_MODE_CURRENT, 0, &current_video_mode);
                framerate_for_videomode(current_video_mode, &frame_rate_numer, &frame_rate_denom);
                logTF("card %d: device present (%dx%d videomode=0x%08X rate=%d/%d)\n", card, status_info.xsize, status_info.ysize, current_video_mode, frame_rate_numer, frame_rate_denom);
                num_sdi_threads++;
                if (width == 0)
                {
                    width = status_info.xsize;
                    height = status_info.ysize;
                }
                else if (width != status_info.xsize || height != status_info.ysize)
                {
                    // Warn if other channels different from first
                    logTF("card %d: different video size!\n", card);
                }

                // Set AES input
                set_aes_option(card, aes_audio);

                set_sync_option(card, opt_sync_type, status_info.xsize);
            }
            else
            {
                // typical reasons include:
                //  SV_ERROR_DEVICENOTFOUND
                //  SV_ERROR_DEVICEINUSE
                logTF("card %d: %s\n", card, sv_geterrortext(res));
            }
        }
    }

    if (num_sdi_threads < 1)
    {
        logTF("Error: No SDI monitor threads created.  Exiting\n");
        return 1;
    }

    // Ideally we would get the dma_video_size and audio_size parameters
    // from a fifo's pbuffer structure.  But you must complete a
    // successful sv_fifo_getbuffer() before those parameters are known.
    // Instead use the following values found experimentally since we need
    // to allocate buffers before successful dma transfers occur.
#if DVS_VERSION_MAJOR >= 3
    // TODO: test this for HD-SDI
    dma_video_size = width*height*2 + 6144;
#else
    dma_video_size = width*height*2;
#endif
    audio_pair_size = 0x4000;
    if (audio8ch)
    {
        audio_size = audio_pair_size * 4;
    }
    else
    {
        audio_size = audio_pair_size * 2;
    }
    audio_offset = dma_video_size;

    // Max audio_size (8ch) is 0x10000 = 65536
    // 1920 * 4 * 8 = 61440
    // audio_size is greater than 1920 * 4 * (4 or 8)
    // so we use the spare bytes at end for timecode and other data
    vitc_offset         = audio_offset + audio_size - 1 * sizeof(int);
    ltc_offset          = audio_offset + audio_size - 2 * sizeof(int);
    sys_time_offset     = audio_offset + audio_size - 3 * sizeof(int);
    tick_offset         = audio_offset + audio_size - 4 * sizeof(int);
    signal_ok_offset    = audio_offset + audio_size - 5 * sizeof(int);
    num_aud_samp_offset = audio_offset + audio_size - 6 * sizeof(int);
    frame_number_offset = audio_offset + audio_size - 7 * sizeof(int);

    // An element in the ring buffer contains: video(4:2:2) + audio + video(4:2:0)
    dma_total_size = dma_video_size     // video frame as captured by dma transfer
                    + audio_size;       // DVS internal structure for audio channels

    // Compute size of secondary video (if any)
    int secondary_video_size = 0;
    if (video_secondary_format != FormatNone) {
        // Secondary format is always SD even when primary is HD
        switch (frame_rate_numer) {
            case 25:
            case 50:
                sec_width = 720;
                sec_height = 576;
                break;
            default:
                sec_width = 720;
                sec_height = 480;
                break;
        }

        // Secondary video can be 4:2:2 or 4:2:0 so work out the correct size
        secondary_video_size = sec_width*sec_height*3/2;
        if (video_secondary_format == Format422PlanarYUV || video_secondary_format == Format422PlanarYUVShifted)
            secondary_video_size = sec_width*sec_height*2;
    }

    // Element size made up from DMA transferred buffer plus secondary video (if any)
    element_size = dma_total_size + secondary_video_size;

    // Create "NO VIDEO" frame
    no_video_frame = (uint8_t*)malloc(width*height*2);
    uyvy_no_video_frame(width, height, no_video_frame);
    if (video_format == Format422PlanarYUV || video_format == Format422PlanarYUVShifted)
    {
        uint8_t *tmp_frame = (uint8_t*)malloc(width*height*2);
        uyvy_no_video_frame(width, height, tmp_frame);

        // Repack to planar YUV 4:2:2
        uyvy_to_yuv422( width, height,
                        video_format == Format422PlanarYUVShifted,  // do DV50 line shift?
                        tmp_frame,                      // input
                        no_video_frame);                // output

        free(tmp_frame);
    }

    // Create "NO VIDEO" frame for secondary buffer
    if (video_secondary_format == Format422PlanarYUV || video_secondary_format == Format422PlanarYUVShifted)
    {
        uint8_t *tmp_frame = (uint8_t*)malloc(sec_width*sec_height*2);
        uyvy_no_video_frame(sec_width, sec_height, tmp_frame);

        // Repack to planar YUV 4:2:2
        no_video_secondary_frame = (uint8_t*)malloc(sec_width*sec_height*2);
        uyvy_to_yuv422( sec_width, sec_height,
                        video_secondary_format == Format422PlanarYUVShifted,    // do DV50 line shift?
                        tmp_frame,                      // input
                        no_video_secondary_frame);      // output

        free(tmp_frame);
    }
    else if (video_secondary_format == Format420PlanarYUV || video_secondary_format == Format420PlanarYUVShifted)
    {
        uint8_t *tmp_frame = (uint8_t*)malloc(sec_width*sec_height*2);
        uyvy_no_video_frame(sec_width, sec_height, tmp_frame);

        // Repack to planar YUV 4:2:0
        no_video_secondary_frame = (uint8_t*)malloc(sec_width*sec_height*3/2);
        if (video_secondary_format == Format420PlanarYUVShifted)
        {
            uyvy_to_yuv420_DV_sampling( sec_width, sec_height,
                            1,                              // with DV50 line shift
                            tmp_frame,                      // input
                            no_video_secondary_frame);      // output
        }
        else
        {
            uyvy_to_yuv420( sec_width, sec_height,
                            0,                              // no DV50 line shift
                            tmp_frame,                      // input
                            no_video_secondary_frame);      // output
        }

        free(tmp_frame);
    }

    // Create benchmark video if specified
    if (benchmark) {
        benchmark_video = (uint8_t*)malloc(width*height*2);
        FILE *fp_sample = fopen(video_sample_file, "rb");
        if (! fp_sample) {
            fprintf(stderr, "Error opening benchmark video file \"%s\" ", video_sample_file);
            perror("fopen");
            return 1;
        }
        if (fread(benchmark_video, width*height*2, 1, fp_sample) != 1) {
            printf("Could not read sample video frame\n");
            return 1;
        }
        logTF("Read one frame (%dx%d) for use as benchmark frame from %s\n", width, height, video_sample_file);
    }

    // Allocate working buffers for video conversions (uvyv to 422 etc.)
    // This is faster than doing it in the shared memory.
    int chan;
    for (chan = 0; chan < MAX_CHANNELS; chan++)
    {
        if (chan < num_sdi_threads)
        {
            video_work_area[chan] = (uint8_t *) memalign(16, dma_total_size);
            if (!video_work_area[chan])
            {
                fprintf(stderr, "Failed to allocate video work buffer.\n");
                return 1;
            }
        }
        else
        {
            video_work_area[chan] = NULL;
        }
    }

    // Allocate shared memory buffers
    if (! allocate_shared_buffers(num_sdi_threads, opt_max_memory))
    {
        return 1;
    }

    for (chan = 0; chan < num_sdi_threads; chan++)
    {
        int err;
        logTF("chan %d: starting capture thread\n", chan);

        if ((err = pthread_create(&sdi_thread[chan], NULL, sdi_monitor, (void *)chan)) != 0)
        {
            logTF("chan %d: failed to create sdi_monitor thread: %s\n", chan, strerror(err));
            return 1;
        }
        else
        {
            //logTF("chan %d: started capture thread ok\n", chan);
        }
    }

    // Update the heartbeat 10 times a second
    p_control->owner_pid = getpid();
    while (1) {
        gettimeofday(&p_control->owner_heartbeat, NULL);
        usleep(100 * 1000);
    }
    pthread_join(sdi_thread[0], NULL);

    // Cleanup
    for (chan = 0; chan < MAX_CHANNELS; chan++)
    {
        free (video_work_area[chan]);
    }

    return 0; // silence gcc warning
}
