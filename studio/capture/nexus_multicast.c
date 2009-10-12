/*
 * $Id: nexus_multicast.c,v 1.11 2009/10/12 15:11:46 john_f Exp $
 *
 * Utility to multicast video frames from dvs_sdi ring buffer to network
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
#include <string.h>
#include <inttypes.h>

#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <unistd.h>
#include <sys/times.h>

#include "nexus_control.h"
#include "multicast_video.h"
#include "multicast_compressed.h"
#include "video_conversion.h"
#include "video_test_signals.h"
#include "audio_utils.h"

#define TESTING_FLAG        0


int verbose = 1;

static char *framesToStr(int tc, char *s)
{
    int frames = tc % 25;
    int hours = (int)(tc / (60 * 60 * 25));
    int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
    int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

    if (tc < 0 || frames < 0 || hours < 0 || minutes < 0 || seconds < 0
        || hours > 59 || minutes > 59 || seconds > 59 || frames > 24)
        sprintf(s, "             ");
        //sprintf(s, "* INVALID *  ");
    else
        sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    return s;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: nexus_multicast [options] address:port\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c channel    send video from specified SDI channel [default 0]\n");
    fprintf(stderr, "    -s WxH        size of scaled down image to transmit [default 240x192 (1/9 picture)]\n");
    fprintf(stderr, "                  (use 360x288 for 1/4-picture)\n");
    fprintf(stderr, "                  (use 180x144 for 1/16-picture)\n");
    fprintf(stderr, "    -t            send compressed MPEG-TS stream suitable for VLC playback\n");
    fprintf(stderr, "    -b kps        MPEG-2 video bitrate to use for compressed MPEG-TS [default 3500 kbps]\n");
    fprintf(stderr, "    -q            quiet operation (fewer messages)\n");
    exit(1);
}

extern int main(int argc, char *argv[])
{
    int             channelnum = 0;
    int             bitrate = 3500;
    int             opt_size = 0;
    int             mpegts = 0;
    int             out_width = 240, out_height = 192;
    char            *address = NULL;
    int             fd = -1;

    int n;
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-q") == 0)
        {
            verbose = 0;
        }
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (n+1 >= argc ||
                sscanf(argv[n+1], "%d", &channelnum) != 1 ||
                channelnum > 7 || channelnum < 0)
            {
                fprintf(stderr, "-c requires integer channel number {0...7}\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-s") == 0)
        {
            if (n+1 >= argc ||
                sscanf(argv[n+1], "%dx%d", &out_width, &out_height) != 2)
            {
                fprintf(stderr, "-s requires size in the form WxH\n");
                return 1;
            }
            opt_size = 1;
            n++;
        }
        else if (strcmp(argv[n], "-b") == 0)
        {
            if (n+1 >= argc ||
                sscanf(argv[n+1], "%d", &bitrate) != 1)
            {
                fprintf(stderr, "-b requires bitrate in bps\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-t") == 0)
        {
            mpegts = 1;
        }
        else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
        {
            usage_exit();
        }
        else {
            address = argv[n];
        }
    }

    if (address == NULL) {
        usage_exit();
    }

    if (mpegts && opt_size == 0) {
        // default MPEG-TS to fullsize picture
        out_width = 720;
        out_height = 576;
    }

    /* OLD VERSION (v4only) Parse address - udp://@224.1.0.50:1234 or 224.1.0.50:1234 forms */
//  int port;
//  char remote[FILENAME_MAX], *p;
//  if ((p = strchr(address, '@')) != NULL)
//      strcpy(remote, p+1);                /* skip 'udp://@' portion */
//  else
//      if ((p = strrchr(address, '/')) != NULL)
//          strcpy(remote, p+1);            /* skip 'udp://' portion */
//      else
//          strcpy(remote, address);
// 
//  if ((p = strchr(remote, ':')) == NULL) {    /* get the port number */
//      port = 2000;        /* default port to 1234 */
//  }
//  else {
//      port = atol(p+1);
//      *p = '\0';
//  }

    /* NEW VERSION (v4+ v6) Parse address - udp://@224.1.0.50:1234 or 224.1.0.50:1234 forms */
    int port;
    char remote[FILENAME_MAX], *p;
    if ((p = strchr(address, '@')) != NULL)
        strcpy(remote, p+1);                /* skip 'udp://@' portion */
    if ((p = strchr(address, '[')) != NULL)
        strcpy(remote, p+1);                /* skip 'udp://@[]' ipv6 portion */
    else
        if ((p = strrchr(address, '/')) != NULL)
            strcpy(remote, p+1);            /* skip 'udp://' portion */
        if ((p = strrchr(address, '[')) != NULL)
            strcpy(remote, p+1);            /* skip 'udp://[]' ipv6 portion */
        else
            strcpy(remote, address);
    if ((p = strchr(remote, ']')) == NULL)
    {           /* search for [ipv6] end */
        if ((p = strchr(remote, ':')) == NULL) //v4 version
        {   /* get the port number */
            port =1234;             /* default port to 1234 */
        }
    
        else 
        {
            port = atol(p+1);
            *(p) = '\0';
        }
    }
    else//v6 version
    {
        if ((strchr(remote, ']'))-strrchr(remote,':')<= 0) 
        {   /* get the port number */
            port =1234;             /* default port to 1234 */
        }

        p=strrchr(remote,':');
        port =atol(p+1);
        *(p-1) = '\0';
    }
    printf("The Address is: %s and the port is: %d \n",remote,port); // show address and port


    // Connect to NexusControl structure in shared mem
    NexusConnection nc;
    nexus_connect_to_shared_mem(INT_MAX, 1, 1, &nc);

    const NexusControl *pctl = nc.pctl;
    const uint8_t   *const *ring = nc.ring;
    const NexusBufCtl *pc = &pctl->channel[channelnum];
    int last_saved = -1;

    // Choose primary or secondary capture buffer
    int use_primary_video = 0;
    int video_422yuv = 0;

    if (pctl->sec_video_format == Format420PlanarYUV || pctl->sec_video_format == Format420PlanarYUVShifted)
    {
        use_primary_video = 0;
        video_422yuv = 0;
    }
    else if (pctl->sec_video_format == Format422PlanarYUV || pctl->sec_video_format == Format422PlanarYUVShifted)
    {

        use_primary_video = 0;
        video_422yuv = 1;
    }
    else if ((pctl->pri_video_format == Format422PlanarYUV || pctl->pri_video_format == Format422PlanarYUVShifted)
        && pctl->width == 720)
    {
        use_primary_video = 1;
        video_422yuv = 1;
    }
    else
    {
        fprintf(stderr, "No suitable capture format!\n");
        return 1;
    }

    int video_offset = 0;
    int width = 0;
    int height = 0;
    if (use_primary_video)
    {
        video_offset = 0;
        width = pctl->width;
        height = pctl->height;
        printf("Using primary video buffer with format %s\n", nexus_capture_format_name(pctl->pri_video_format));
    }
    else
    {
        video_offset = pctl->sec_video_offset;
        width = pctl->sec_width;
        height = pctl->sec_height;
        printf("Using secondary video buffer with format %s\n", nexus_capture_format_name(pctl->sec_video_format));
    }


    uint8_t *scaled_frame = (uint8_t *)malloc(out_width * out_height * 3/2);

    // setup blank video and audio buffers
    uint8_t *blank_video_uyvy = (uint8_t *)malloc(out_width * out_height * 2);
    uyvy_no_video_frame(out_width, out_height, blank_video_uyvy);
    uint8_t *blank_video = (uint8_t *)malloc(out_width * out_height * 3/2);
    uyvy_to_yuv420_nommx(out_width, out_height, 0, blank_video_uyvy, blank_video);
    free(blank_video_uyvy);
    uint8_t blank_audio[1920*2*2];
    memset(blank_audio, 0, sizeof(blank_audio));

    mpegts_encoder_t *ts = NULL;
    if (mpegts) {
        char url[64];
        // create string suitable for ffmpeg url_fopen() e.g. udp://239.255.1.1:2000
        if (strchr(remote, ':')==NULL)
            sprintf(url, "udp://%s:%d", remote, port);
        else
            sprintf(url, "udp://[%s]:%d", remote, port);

        // setup MPEG-TS encoder
        if ((ts = mpegts_encoder_init(url, out_width, out_height, bitrate, 4)) == NULL) {
            return 1;

        }
    }
    else {
        // open socket for uncompressed multicast
        if ((fd = open_socket_for_streaming(remote, port)) == -1) {
            exit(1);
        }
    }

    int frames_sent = 0;
    while (1)
    {
        const uint8_t *p_video = blank_video, *p_audio = blank_audio;
        int tc = 0, ltc = 0, signal_ok = 0;


        if (! nexus_connection_status(&nc, NULL, NULL)) {
            last_saved = -1;
            if (nexus_connect_to_shared_mem(40000, 1, 1, &nc)) {
                pctl = nc.pctl;
                ring = nc.ring;
                pc = &pctl->channel[channelnum];
                continue;
            }
        }
        else {
            if (last_saved == pc->lastframe) {
                usleep(20 * 1000);      // 0.020 seconds = 50 times a sec
                continue;
            }

            int diff_to_last = pc->lastframe - last_saved;
            if (diff_to_last != 1) {
                printf("\ndiff_to_last = %d\n", diff_to_last);
            }

            NexusFrameData * nfd = (NexusFrameData *)(ring[channelnum] + pctl->elementsize * (pc->lastframe % pctl->ringlen) + pctl->frame_data_offset);

            tc        = nfd->vitc;
            ltc       = nfd->ltc;
            signal_ok = nfd->signal_ok;

            /*
            tc = *(int*)(ring[channelnum] + pctl->elementsize *
                                        (pc->lastframe % pctl->ringlen)
                                + pctl->vitc_offset);
            ltc = *(int*)(ring[channelnum] + pctl->elementsize *
                                        (pc->lastframe % pctl->ringlen)
                                + pctl->ltc_offset);
            signal_ok = *(int*)(ring[channelnum] + pctl->elementsize *
                                        (pc->lastframe % pctl->ringlen)
                                + pctl->signal_ok_offset);
            */

            // get video and audio pointers
            const uint8_t *video_frame = ring[channelnum] + video_offset +
                                    pctl->elementsize * (pc->lastframe % pctl->ringlen);
            const uint8_t *audio_dvs_fmt = ring[channelnum] + pctl->audio12_offset +
                                    pctl->elementsize * (pc->lastframe % pctl->ringlen);

            uint8_t audio[1920*2*2];

            if (signal_ok) {
                if (width != out_width || height != out_height) {
                    // scale down video suitable for multicast
                    if (video_422yuv)
                        scale_video422_for_multicast(width, height, out_width, out_height, video_frame, scaled_frame);
                    else
                        scale_video420_for_multicast(width, height, out_width, out_height, video_frame, scaled_frame);
                    p_video = scaled_frame;
                }
                else {
                    p_video = video_frame;
                }

                if (ts) {
                    // MPEG-2 audio encoder needs 16bit stereo pair
                    dvsaudio32_to_16bitpair(1920, audio_dvs_fmt, audio);
                }
                else {
                    // reformat audio to two mono channels one after the other
                    // i.e. 1920 samples of channel 0, followed by 1920 samples of channel 1
                    dvsaudio32_to_16bitmono(0, audio_dvs_fmt, audio);
                    dvsaudio32_to_16bitmono(1, audio_dvs_fmt, audio + 1920*2);
                }
                p_audio = audio;
            }
        }

        // Send the frame.
        if (ts) {
#if TESTING_FLAG
    printf("Inside nexus_multicast: About to call mpegts_encoder_encode\n");
#endif
            mpegts_encoder_encode(ts,
                                    p_video,
                                    (int16_t*)p_audio,
                                    frames_sent++);
        }
        else {
#if TESTING_FLAG
    printf("Inside nexus_multicast: About to call send_audio_video\n");
#endif
            send_audio_video(fd, out_width, out_height, 2,
                                    p_video,
                                    p_audio,
                                    pc->lastframe, tc, ltc, pc->source_name);
        }

        if (verbose) {
            char tcstr[32], ltcstr[32];

            printf("\rcam%d lastframe=%d %s  tc=%10d  %s   ltc=%11d  %s ",
                    channelnum, pc->lastframe, signal_ok ? "ok" : "--",
                    tc, framesToStr(tc, tcstr), ltc, framesToStr(ltc, ltcstr));
            fflush(stdout);
        }

        last_saved = pc->lastframe;
    }

    return 0;
}
