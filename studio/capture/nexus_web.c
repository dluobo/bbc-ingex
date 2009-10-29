/*
 * $Id: nexus_web.c,v 1.8 2009/10/29 09:23:54 philipn Exp $
 *
 * Stand-alone web server to monitor and control nexus applications
 *
 * Copyright (C) 2008  British Broadcasting Corporation
 * Author: Stuart Cunningham <stuart_hc@users.sourceforge.net>
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
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include <shttpd.h>

#include "nexus_control.h"

#include "audio_utils.h"


int verbose = 1;
int show_audio34 = 0;


struct NexusTC {
    int h;
    int m;
    int s;
    int f;
};

static void framesToTC(int tc, NexusTC *p)
{
    p->f = tc % 25;
    p->h = (int)(tc / (60 * 60 * 25));
    p->m = (int)((tc - (p->h * 60 * 60 * 25)) / (60 * 25));
    p->s = (int)((tc - (p->h * 60 * 60 * 25) - (p->m * 60 * 25)) / 25);
}

static int64_t tv_diff_microsecs(const struct timeval* a, const struct timeval* b)
{
    int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
    return diff;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: nexus_web [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    --a34            display 4 audio channels instead of 2 by default\n");
    fprintf(stderr, "    -v               increase verbosity\n");
    fprintf(stderr, "\n");
    exit(1);
}

// TimecodeInfo is used to record timecode snapshots so that stuck timecode can be determined
typedef struct {
    int             lasttc[MAX_CHANNELS];
    struct timeval  timestamp;
} TimecodeInfo ;

typedef struct {
    NexusConnection connection;
    TimecodeInfo    tcinfo;
} NexusWebInfo;

static void get_audio_peak_power(const NexusControl *pctl, uint8_t **ring, int input, int lastframe, double audio_peak_power[])
{
    int pair;
    for (pair = 0; pair < 2; pair++) {
        const uint8_t *audio_dvs;
        if (pair == 0)
            audio_dvs = nexus_audio12(pctl, ring, input, lastframe);
        else if (pair == 1)
            audio_dvs = nexus_audio34(pctl, ring, input, lastframe);

        int audio_chan;
        for (audio_chan = 0; audio_chan < 2; audio_chan++) {
            uint8_t audio_16bitmono[1920*2];
            dvsaudio32_to_16bitmono(audio_chan, audio_dvs, audio_16bitmono);
            audio_peak_power[pair*2 + audio_chan] = calc_audio_peak_power(audio_16bitmono, 1920, 2, -96.0);
        }
    }
}

static void nexus_state(struct shttpd_arg* arg)
{
    int i;
    NexusWebInfo        *p = (NexusWebInfo*)arg->user_data;

    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");

    // Use text/plain instead of application/json for ease of debugging in a browser
    shttpd_printf(arg, "Content-type: text/plain\r\n\r\n");

    // If nexus_web was started before dvs_sdi, p will be NULL
    if (p->connection.pctl == NULL) {
        // Connect to NexusControl structure in shared mem
        if (! nexus_connect_to_shared_mem(0, 0, 1, &p->connection)) {
            shttpd_printf(arg, "{\"requestError\":\"Shared memory unavailable. Is the capture process running?\",\"state\":\"error\"}\n");
            arg->flags |= SHTTPD_END_OF_OUTPUT;
            return;
        }
    }

    // Get health of dvs_sdi
    int heartbeat_stopped = 0;
    int capture_dead = 0;
    struct timeval now;
    gettimeofday(&now, NULL);
    int64_t diff = tv_diff_microsecs(&p->connection.pctl->owner_heartbeat, &now);
    if (diff > 200 * 1000) {
        heartbeat_stopped = 1;
        if (kill(p->connection.pctl->owner_pid, 0) == -1) {
            // If dvs_sdi is alive and running as root, kill() will fail with EPERM
            if (errno != EPERM)
                capture_dead = 1;
        }
    }

    // convenience variables to keep code clearer
    NexusControl        *pctl = p->connection.pctl;
    uint8_t             **ring = p->connection.ring;
    
    // Decide state of capture process
    char state[6] = "ok";
    if (heartbeat_stopped == 1 || capture_dead == 1) {
        strcpy(state,"error");
    }
    else {
        for (i = 0; i < pctl->channels; i++) {
            int lastframe = nexus_lastframe(pctl, i);
            if (nexus_signal_ok(pctl, ring, i, lastframe) == 0) {
                strcpy(state,"warn");
                break;
            }
        }
    }

    // Test for "stuck" timecode
    int tc_stuck[MAX_CHANNELS] = {0,0,0,0,0,0,0,0};         // initialise to good tc
    gettimeofday(&now, NULL);
    for (i = 0; i < pctl->channels; i++) {
        int lastframe = nexus_lastframe(pctl, i);
        int lasttc = nexus_tc(pctl, ring, i, lastframe, NexusTC_DEFAULT);
        if (p->tcinfo.lasttc[i] != lasttc) {
            // timecodes differ (good) so update lasttc and continue
            p->tcinfo.lasttc[i] = lasttc;
            continue;
        }
        // Has timecode been the same for at least three frames?
        if (tv_diff_microsecs(&p->tcinfo.timestamp, &now) > 3 * 40*1000) {
            tc_stuck[i] = 1;        // tc bad
        }
    }
    p->tcinfo.timestamp = now;

    // Display output as JSON
    shttpd_printf(arg, "{");
    shttpd_printf(arg, "\n\"state\":\"%s\",",state);
    shttpd_printf(arg, "\n\"monitorData\":{");
    shttpd_printf(arg, "\n\t\"capturestatus\":{");
    shttpd_printf(arg, "\n\t\t\"heartbeat_stopped\": \"%d\",", heartbeat_stopped);
    shttpd_printf(arg, "\n\t\t\"capture_dead\": \"%d\"", capture_dead);
    shttpd_printf(arg, "\n\t},");
    shttpd_printf(arg, "\n\t\"videoinfo\":{");
    shttpd_printf(arg, "\n\t\t\"videosize\": \"%dx%d\",", pctl->width, pctl->height);
    shttpd_printf(arg, "\n\t\t\"pri_video_format\": \"%s\",", nexus_capture_format_name(pctl->pri_video_format));
    shttpd_printf(arg, "\n\t\t\"sec_video_format\": \"%s\",", nexus_capture_format_name(pctl->sec_video_format));
    shttpd_printf(arg, "\n\t\t\"default_tc_type\": \"%s\",", nexus_timecode_type_name(pctl->default_tc_type));
    shttpd_printf(arg, "\n\t\t\"master_tc_channel\": %d", pctl->master_tc_channel);
    shttpd_printf(arg, "\n\t},");
    shttpd_printf(arg, "\n\t\"ringlen\": %d,", pctl->ringlen);
    shttpd_printf(arg, "\n\t\"ring\":{"); // start ring

    // Display info for all channels being captured
    for (i = 0; i < pctl->channels; i++) {
        NexusTC tc;
        const NexusBufCtl *pc = &pctl->channel[i];
        int lastframe = nexus_lastframe(pctl, i);
        int ltc = nexus_tc(pctl, ring, i, lastframe, NexusTC_DEFAULT);
        int sok = nexus_signal_ok(pctl, ring, i, lastframe);
        framesToTC(ltc, &tc);

        // Setup array of 4 channels of audio peak power data
        double audio_peak_power[4];
        get_audio_peak_power(pctl, ring, i, lastframe, audio_peak_power);

        shttpd_printf(arg, "\n\t\t%d:{",i); // start channel
        shttpd_printf(arg, "\n\t\t\t\"temperature\": %.1f,", pc->hwtemperature);
        shttpd_printf(arg, "\n\t\t\t\"source_name\": \"%s\",", pc->source_name);
        shttpd_printf(arg, "\n\t\t\t\"lastframe\": %d,", pc->lastframe);
        shttpd_printf(arg, "\n\t\t\t\"signal_ok\": %d,", sok);
        shttpd_printf(arg, "\n\t\t\t\"tc\": { \"h\": %d, \"m\": %d, \"s\": %d, \"f\": %d, \"stopped\": %d },", tc.h, tc.m, tc.s, tc.f, tc_stuck[i]);
        shttpd_printf(arg, "\n\t\t\t\"audio_power\": [ %3.0f, %3.0f", audio_peak_power[0], audio_peak_power[1]);
        if (show_audio34) {
            shttpd_printf(arg, ", %3.0f, %3.0f", audio_peak_power[2], audio_peak_power[3]);
        }
        shttpd_printf(arg, "]");
        shttpd_printf(arg, "\n\t\t}%s", (i == pctl->channels - 1) ? "" : ","); // end channel
    }
    shttpd_printf(arg, "\n\t},"); // end ring

    // Display Recorder info whether or not any Recorders are running
    shttpd_printf(arg, "\n\t\"recorders\":{");

    int recordersUsed = 0;
    for (int recidx = 0; recidx < MAX_RECORDERS; recidx++) {
        // Check if Recorder exists and is running
        if (pctl->record_info[recidx].pid == 0) {
            // Recorder never existed for this recidx
            continue;
        }
        if (kill(pctl->record_info[recidx].pid, 0) == -1 && errno != EPERM) {
            // Recorder was once running but is now dead
            continue;
        }
        
        recordersUsed++;

        // print comma if required
        if (recidx > 0)
            shttpd_printf(arg, "\n\t\t,");

        shttpd_printf(arg, "\n\t\t\"%s\":{", pctl->record_info[recidx].name); // start recorder block

        // Loop over channels
        for (i = 0; i < pctl->channels; i++) {
            shttpd_printf(arg, "\n\t\t\t%d:{",i); // start channel block
            int j;
            int encodersUsed = 0;
            int lastEncoderUsed = 0;
            // Iterate over each encoding per channel
            for (j = 0; j < MAX_ENCODES_PER_CHANNEL; j++) {
                const NexusRecordEncodingInfo *p_enc = &(pctl->record_info[recidx].channel[i][j]);

                if (! p_enc->enabled) {
                    lastEncoderUsed = 0;
                    // Encoding not enabled, skip it
                    // TODO - is it OK to just skip disabled encoding?
                    continue;
                }
                if(lastEncoderUsed == 1) {
                    shttpd_printf(arg, ",");
                }
                lastEncoderUsed = 1;
                encodersUsed ++;
                //TODO - possibly list as description:{details} rather than an arbitrary id
                shttpd_printf(arg, "\n\t\t\t\t%d:{", j); //start encode type block
                shttpd_printf(arg, "\n\t\t\t\t\t\"desc\": \"%s\",",p_enc->desc);
                shttpd_printf(arg, "\n\t\t\t\t\t\"enabled\": %d,", p_enc->enabled);
                shttpd_printf(arg, "\n\t\t\t\t\t\"recording\": %d,", p_enc->recording);
                shttpd_printf(arg, "\n\t\t\t\t\t\"record_error\": %d,", p_enc->record_error);
                shttpd_printf(arg, "\n\t\t\t\t\t\"error\": \"%s\",", p_enc->error);
                shttpd_printf(arg, "\n\t\t\t\t\t\"frames_written\": %d,", p_enc->frames_written);
                shttpd_printf(arg, "\n\t\t\t\t\t\"frames_dropped\": %d,", p_enc->frames_dropped);
                shttpd_printf(arg, "\n\t\t\t\t\t\"frames_in_backlog\": %d", p_enc->frames_in_backlog);
                shttpd_printf(arg, "\n\t\t\t\t}"); // end encode type block
            }
            if(encodersUsed == 0){
                shttpd_printf(arg, "\n\t\t\t\t\"noEncoders\":true");
            }
            shttpd_printf(arg, "\n\t\t\t}%s", (i == pctl->channels - 1) ? "" : ","); // end channel block
        }

        // The quad "channel" looks like a normal channel but it is virtual,
        // existing only in the Recorder as a composition of 4 normal channels.
        const NexusRecordEncodingInfo *p_enc = &(pctl->record_info[recidx].quad);
        if (p_enc->enabled) {
            shttpd_printf(arg, ","); // add comma
            shttpd_printf(arg, "\n\t\t\t\"quad\":{");
            // TODO single encode type hardboded = bad
            shttpd_printf(arg, "\n\t\t\t\t0:{"); // start encode type block
            shttpd_printf(arg, "\n\t\t\t\t\t\"desc\":\"%s\",", p_enc->desc);
            shttpd_printf(arg, "\n\t\t\t\t\t\"enabled\": %d,", p_enc->enabled);
            shttpd_printf(arg, "\n\t\t\t\t\t\"recording\": %d,", p_enc->recording);
            shttpd_printf(arg, "\n\t\t\t\t\t\"record_error\": %d,", p_enc->record_error);
            shttpd_printf(arg, "\n\t\t\t\t\t\"error\": \"%s\",", p_enc->error);
            shttpd_printf(arg, "\n\t\t\t\t\t\"frames_written\": %d,", p_enc->frames_written);
            shttpd_printf(arg, "\n\t\t\t\t\t\"frames_dropped\": %d,", p_enc->frames_dropped);
            shttpd_printf(arg, "\n\t\t\t\t\t\"frames_in_backlog\": %d", p_enc->frames_in_backlog);
            shttpd_printf(arg, "\n\t\t\t\t}"); // end encode type block
            shttpd_printf(arg, "\n\t\t\t}");
        }
        shttpd_printf(arg, "\n\t\t}"); // end recorder block
    }
    if(recordersUsed == 0){
        shttpd_printf(arg, "\n\t\t\"noRecorders\":true");
    }

    shttpd_printf(arg, "\n\t}"); // end recorders
    shttpd_printf(arg,"\n}"); // end monitor data
    shttpd_printf(arg,"\n}\n"); // end whole thing
    arg->flags |= SHTTPD_END_OF_OUTPUT;

    // If capture dead, try to reconnect for next status check
    if (capture_dead) {
        nexus_connect_to_shared_mem(0, 0, 1, &p->connection);
    }
}

static void timecode(struct shttpd_arg* arg)
{
    NexusWebInfo        *p = (NexusWebInfo*)arg->user_data;

    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");

    // Use text/plain instead of application/json for ease of debugging in a browser
    shttpd_printf(arg, "Content-type: text/plain\r\n\r\n");

    // If nexus_web was started before dvs_sdi, p will be NULL
    if (p->connection.pctl == NULL) {
        // Connect to NexusControl structure in shared mem
        if (! nexus_connect_to_shared_mem(0, 0, 1, &p->connection)) {
            shttpd_printf(arg, "{\"requestError\":\"Shared memory unavailable. Is the capture process running?\",\"state\":\"error\"}\n");
            arg->flags |= SHTTPD_END_OF_OUTPUT;
            return;
        }
    }
    
    // Get health of dvs_sdi
    int heartbeat_stopped = 0;
    int capture_dead = 0;
    struct timeval now;
    gettimeofday(&now, NULL);
    int64_t diff = tv_diff_microsecs(&p->connection.pctl->owner_heartbeat, &now);
    if (diff > 200 * 1000) {
        heartbeat_stopped = 1;
        if (kill(p->connection.pctl->owner_pid, 0) == -1) {
            // If dvs_sdi is alive and running as root, kill() will fail with EPERM
            if (errno != EPERM)
                capture_dead = 1;
        }
    }

    // convenience variables to keep code clearer
    NexusControl        *pctl = p->connection.pctl;
    uint8_t             **ring = p->connection.ring;

    // Test for "stuck" timecode
    int tc_stuck = 0;// initialise to good tc
    gettimeofday(&now, NULL);
    int lastframe = nexus_lastframe(pctl, 0);
    int lasttc = nexus_tc(pctl, ring, 0, lastframe, NexusTC_LTC);
    if (p->tcinfo.lasttc[0] != lasttc) {
        // timecodes differ (good) so update lasttc and continue
        p->tcinfo.lasttc[0] = lasttc;
    } else if (tv_diff_microsecs(&p->tcinfo.timestamp, &now) > 3 * 40*1000) {
        // Has timecode been the same for at least three frames?
        tc_stuck = 1;       // tc bad
    }
    p->tcinfo.timestamp = now;

    // Display output as JSON
    shttpd_printf(arg, "{");

    NexusTC tc;
    int ltc = nexus_tc(pctl, ring, 0, lastframe, NexusTC_LTC);
    framesToTC(ltc, &tc);
    shttpd_printf(arg, "\n\t\"tc\": { \"h\": %d, \"m\": %d, \"s\": %d, \"f\": %d, \"stopped\": %d },", tc.h, tc.m, tc.s, tc.f, tc_stuck);
    
    shttpd_printf(arg,"\n}\n"); // end whole thing
    arg->flags |= SHTTPD_END_OF_OUTPUT;

    // If capture dead, try to reconnect for next status check
    if (capture_dead) {
        nexus_connect_to_shared_mem(0, 0, 1, &p->connection);
    }
}

static void availability(struct shttpd_arg* arg)
{
    // VERY SIMPLE function returns {"availability":true}
    // Just indicates to the interface that its http request to me succeeded

    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");

    // Use text/plain instead of application/json for ease of debugging in a browser
    shttpd_printf(arg, "Content-type: text/plain\r\n\r\n");

    
    shttpd_printf(arg,"{\"availability\":true}\n"); // end whole thing
    arg->flags |= SHTTPD_END_OF_OUTPUT;
}


extern int main(int argc, char *argv[])
{
    NexusWebInfo    info;
    memset(&info, 0, sizeof(info));

    int n;
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-q") == 0)
        {
            verbose = 0;
        }
        else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
        {
            usage_exit();
        }
        else if (strcmp(argv[n], "--a34") == 0)
        {
            show_audio34 = 1;
        }
        else if (strcmp(argv[n], "-v") == 0)
        {
            verbose++;
        }
    }

    // Connect to NexusControl structure in shared mem
    nexus_connect_to_shared_mem(0, 0, 1, &info.connection);

    // Setup the shttpd object
    struct shttpd_ctx *ctx;
    ctx = shttpd_init(NULL,"document_root","/dev/null",NULL);

    shttpd_listen(ctx, 7000, 0);

    // register handlers for various pages
    shttpd_register_uri(ctx, "/availability", &availability, (void *) &info);
    shttpd_register_uri(ctx, "/tc", &timecode, (void *) &info);
    shttpd_register_uri(ctx, "/", &nexus_state, (void *) &info);

    printf("Web server ready...\n");

    // loop forever serving requests
    while (1)
    {
        shttpd_poll(ctx, 1000);
    }
    shttpd_fini(ctx);       // just for completeness but not reached

    return 0;
}
