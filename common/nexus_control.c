/*
 * $Id: nexus_control.c,v 1.3 2009/10/12 14:39:04 john_f Exp $
 *
 * Module for creating and accessing nexus shared control memory
 *
 * Copyright (C) 2008  Stuart Cunningham, British Broadcasting Corporation
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

#include "nexus_control.h"

extern const char *nexus_capture_format_name(CaptureFormat fmt)
{
    const char *names[] = {
    "FormatNone",
    "Format422UYVY",
    "Format422PlanarYUV",
    "Format422PlanarYUVShifted",
    "Format420PlanarYUV",
    "Format420PlanarYUVShifted"
    };

    return names[fmt];
}

extern const char *nexus_timecode_type_name(NexusTimecode tc_type)
{
    const char * s;
    switch (tc_type)
    {
    case NexusTC_LTC:
        s = "LTC";
        break;
    case NexusTC_VITC:
        s = "VITC";
        break;
    case NexusTC_DLTC:
        s = "DLTC";
        break;
    case NexusTC_DVITC:
        s = "DVITC";
        break;
    case NexusTC_SYSTEM:
        s = "System";
        break;
    case NexusTC_DEFAULT:
        s = "Default";
        break;
    default:
        s = "Unknown";
        break;
    }

    return s;
}

extern int nexus_lastframe(NexusControl *pctl, int channel)
{
    int lastframe;
    
    PTHREAD_MUTEX_LOCK(&pctl->channel[channel].m_lastframe)
    lastframe = pctl->channel[channel].lastframe;
    PTHREAD_MUTEX_UNLOCK(&pctl->channel[channel].m_lastframe)
    
    return lastframe;
}

extern NexusFrameData* nexus_frame_data(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return (NexusFrameData *)(ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->frame_data_offset);
}

extern int nexus_num_aud_samp(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return nexus_frame_data(pctl, ring, channel, frame)->num_aud_samp;
}

extern int nexus_signal_ok(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return nexus_frame_data(pctl, ring, channel, frame)->signal_ok;
}

extern int nexus_frame_number(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return nexus_frame_data(pctl, ring, channel, frame)->frame_number;
}

extern int nexus_tc(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, NexusTimecode tctype)
{
    NexusFrameData * nfd = nexus_frame_data(pctl, ring, channel, frame);

    if (tctype == NexusTC_DEFAULT) {
        tctype = pctl->default_tc_type;
    }

    int tc;
    switch (tctype)
    {
    case NexusTC_VITC:
        tc = nfd->vitc;
        break;
    case NexusTC_LTC:
        tc = nfd->ltc;
        break;
    case NexusTC_DVITC:
        tc = nfd->dvitc;
        break;
    case NexusTC_DLTC:
        tc = nfd->dltc;
        break;
    case NexusTC_SYSTEM:
        tc = nfd->systc;
        break;
    default:
        tc = nfd->vitc;
        break;
    }

    return tc;
}

extern const uint8_t *nexus_primary_video(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return ring[channel] + pctl->elementsize * (frame % pctl->ringlen);
}

extern const uint8_t *nexus_secondary_video(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->sec_video_offset;
}

extern const uint8_t *nexus_audio12(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return (ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->audio12_offset);
}

extern const uint8_t *nexus_audio34(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return (ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->audio34_offset);
}

extern const uint8_t *nexus_audio56(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return (ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->audio56_offset);
}

extern const uint8_t *nexus_audio78(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return (ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->audio78_offset);
}

extern void nexus_set_source_name(NexusControl *pctl, int channel, const char *source_name)
{
    PTHREAD_MUTEX_LOCK(&pctl->m_source_name_update)
    strncpy(pctl->channel[channel].source_name, source_name, sizeof(pctl->channel[channel].source_name));
    pctl->channel[channel].source_name[sizeof(pctl->channel[channel].source_name) - 1] = '\0';   // guarantee null terminated string
    pctl->source_name_update++;
    PTHREAD_MUTEX_UNLOCK(&pctl->m_source_name_update)
}

extern void nexus_get_source_name(NexusControl *pctl, int channel, char *source_name, size_t source_name_size,
                                  int *update_count)
{
    PTHREAD_MUTEX_LOCK(&pctl->m_source_name_update)
    strncpy(source_name, pctl->channel[channel].source_name, source_name_size);
    source_name[source_name_size - 1] = '\0';   // guarantee null terminated string
    *update_count = pctl->source_name_update;
    PTHREAD_MUTEX_UNLOCK(&pctl->m_source_name_update)
}

extern int nexus_connect_to_shared_mem(int timeout_microsec, int read_only, int verbose, NexusConnection *p)
{
    int         shm_id, control_id;
    int         at_flags = 0;           // default 0 means read-write

    if (read_only)
        at_flags = SHM_RDONLY;

    if (timeout_microsec > 0 && verbose) {
        printf("Waiting for shared memory... ");
        fflush(stdout);
    }

    // If shared memory not found, sleep and try again
    int retry_time = 20 * 1000;     // 20ms
    int time_taken = 0;
    while (1)
    {
        control_id = shmget(9, sizeof(NexusControl), 0444);
        if (control_id != -1)
            break;

        if (timeout_microsec == 0)
            return 0;

        usleep(retry_time);
        time_taken += retry_time;
        if (time_taken >= timeout_microsec)
            return 0;
    }

    p->pctl = (NexusControl*)shmat(control_id, NULL, at_flags);
    if (verbose)
        printf("connected to pctl\n");

    int i;
    for (i = 0; i < p->pctl->channels; i++)
    {
        while (1)
        {
            shm_id = shmget(10 + i, p->pctl->elementsize, 0444);
            if (shm_id != -1)
                break;
            usleep(20 * 1000);
        }
        p->ring[i] = (uint8_t*)shmat(shm_id, NULL, at_flags);
        if (verbose)
            printf("  attached to channel[%d]: '%s'\n", i, p->pctl->channel[i].source_name);
    }

    return 1;
}

extern int nexus_disconnect_from_shared_mem(const NexusConnection *p)
{
    int i;
    for (i = 0; i < p->pctl->channels; i++)
    {
        if (shmdt(p->ring[i]) == -1) {
            fprintf(stderr, "p->ring[%d] (0x%p) ", i, p->ring[i]);
            perror("shmdt");
            return 0;
        }
    }

    if (shmdt(p->pctl) == -1) {
        fprintf(stderr, "p->pctl (0x%p) ", p->pctl);
        perror("shmdt");
        return 0;
    }
    return 1;
}

static int64_t tv_diff_microsecs(const struct timeval* a, const struct timeval* b)
{
    int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
    return diff;
}

// Returns 1 if connection ok, 0 otherwise and sets return flags
extern int nexus_connection_status(const NexusConnection *p, int *p_heartbeat_stopped, int *p_capture_dead)
{
    int heartbeat_stopped = 0;
    int capture_dead = 0;

    // p->pctl is NULL before first connection made
    if (p->pctl) {
        struct timeval now;
        gettimeofday(&now, NULL);
        int64_t diff = tv_diff_microsecs(&p->pctl->owner_heartbeat, &now);
        // 100ms or lower can give false positives for lost heartbeat
        if (diff > 160 * 1000) {
            heartbeat_stopped = 1;

            if (kill(p->pctl->owner_pid, 0) == -1) {
                // If dvs_sdi is alive and running as root,
                // kill() will fail with EPERM
                if (errno != EPERM)
                    capture_dead = 1;
            }
        }
    }
    else {
        heartbeat_stopped = 1;
        capture_dead = 1;
    }

    if (heartbeat_stopped == 0 && capture_dead == 0)    // i.e. connection OK
        return 1;

    if (p_heartbeat_stopped)
        *p_heartbeat_stopped = heartbeat_stopped;

    if (p_capture_dead)
        *p_capture_dead = capture_dead;

    return 0;
}
