/*
 * $Id: check_inputs.c,v 1.2 2010/01/14 14:05:35 john_f Exp $
 *
 * Using DVS SDK APIs check input video, audio and timecode status
 *
 * Copyright (C) 2008  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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
#include <unistd.h>

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
#define MAX_CHANNELS 4
sv_handle       *sv;
sv_handle       *sv_array[MAX_CHANNELS] = {NULL,NULL,NULL,NULL};

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

const char *dvs_modestring(int videomode)
{
    switch(videomode & SV_MODE_RASTERMASK) {
        case SV_MODE_PAL:       return "PAL";
        default:                return "unk";
    }
}

int open_card_channel(sv_handle **p_sv, int card, int channel)
{
    sv_info             status_info;
    char card_str[64] = {0};
    snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=%d", card, channel);

    int res = sv_openex(p_sv,
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
        fprintf(stderr, "card %d,%d (%s): %s\n", card, channel, card_str, sv_geterrortext(res));
        return res;
    }

    sv_status(*p_sv, &status_info);
    int video_width = status_info.xsize;
    int video_height = status_info.ysize;
    int video_size = video_width * video_height *2;

    printf("card[%d,%d] frame is %dx%d (%d bits) %d bytes\n", card, channel, video_width, video_height, status_info.nbit, video_size);
    return res;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: check_inputs [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c {0-3},{0,1}   single card and channel number pair to monitor (default all card,channel pairs)\n");
    fprintf(stderr, "    -d <microsecs>   delay in microseconds between updates (default 1000000)\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main (int argc, char ** argv)
{
    int opt_card = -1, opt_channel = -1;
    int delay_microsec = 1000*1000;

    // process command-line args
    int n;
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-c") == 0)
        {
            if (sscanf(argv[n+1], "%u,%u", &opt_card, &opt_channel) != 2)
            {
                fprintf(stderr, "-c requires card,channel pair\n");
                return 1;
            }
            n++;
        }
        else
            usage_exit();
    }

    if (opt_card != -1) {
        if (open_card_channel(&sv_array[0], opt_card, opt_channel) != SV_OK)
            return 1;
    }
    else {
        int channel_ok = 0;
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (open_card_channel(&sv_array[i], i/2, i%2) == SV_OK)
                channel_ok = 1;
        }
        if (!channel_ok)
            return 1;
    }

    // Loop forever monitoring input
loop:
    for (int i = 0; i < MAX_CHANNELS; i++) {
        sv = sv_array[i];
        if (sv == NULL)
            continue;

        int                 sdiA, videoin, audioin, timecode_status;
        sv_timecode_info    timecodes;
        char                tcstr[256];
        unsigned int        h_clock, l_clock;
        int                 current_tick;
    
        SV_CHECK( sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock) );
        SV_CHECK( sv_query(sv, SV_QUERY_INPUTRASTER_SDIA, 0, &sdiA) );
        SV_CHECK( sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin) );
        SV_CHECK( sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin) );
        SV_CHECK( sv_query(sv, SV_QUERY_VALIDTIMECODE, 0, &timecode_status) );
        SV_CHECK( sv_timecode_feedback(sv, &timecodes, NULL) );     // get input info, ignore output
        SV_CHECK( sv_tc2asc(sv, timecodes.altc_tc, tcstr, sizeof(tcstr)) );
    
        printf("%d,%d %8d %d,%d mode=%s V=%s A=%s tc=%s%s%s ALtc=%-12s\n",
                    i/2, i%2,
                    current_tick,
                    h_clock, l_clock,
                    dvs_modestring(sdiA),
                    videoin == SV_OK ? "OK " : "err",
                    audioin == SV_OK ? "OK " : "err",
                    (timecode_status & SV_VALIDTIMECODE_LTC) ? "LTC " : "    ",
                    (timecode_status & SV_VALIDTIMECODE_VITC_F1) ? "VITC/F1 " : "        ",
                    (timecode_status & SV_VALIDTIMECODE_VITC_F2) ? "VITC/F2 " : "        ",
                    tcstr
                    );

    }
    if (opt_card == -1)
        printf("--\n");         // when monitoring more than one channel, visually separate groups

    usleep(delay_microsec);
    goto loop;

    return 0;
}
