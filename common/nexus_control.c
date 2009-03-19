/*
 * $Id: nexus_control.c,v 1.1 2009/03/19 16:58:19 john_f Exp $
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

#define _XOPEN_SOURCE 600			// for posix_memalign

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
	const char *names[] = {
    "NexusTC_None",
    "NexusTC_LTC",
    "NexusTC_VITC"
	};

	return names[tc_type];
}

extern int nexus_lastframe_num_aud_samp(const NexusControl *pctl, uint8_t *ring[], int channel)
{
	const NexusBufCtl *pc = &pctl->channel[channel];

	return *(int*)(ring[channel] + pctl->elementsize * (pc->lastframe % pctl->ringlen)
								+ pctl->num_aud_samp_offset);
}

extern int nexus_lastframe_signal_ok(const NexusControl *pctl, uint8_t *ring[], int channel)
{
	const NexusBufCtl *pc = &pctl->channel[channel];

	return *(int*)(ring[channel] + pctl->elementsize * (pc->lastframe % pctl->ringlen)
								+ pctl->signal_ok_offset);
}

extern int nexus_lastframe_tc(const NexusControl *pctl, uint8_t *ring[], int channel, NexusTimecode tctype)
{
	const NexusBufCtl *pc = &pctl->channel[channel];

	// If no timecode type specified use the master timecode type
	// If that is not set, fallback to LTC
	int tc_offset = pctl->ltc_offset;
	if (tctype == NexusTC_None && pctl->master_tc_type != NexusTC_None) {
		if (pctl->master_tc_type == NexusTC_VITC)
			tc_offset = pctl->vitc_offset;
	}

	if (tctype == NexusTC_VITC)
		tc_offset = pctl->vitc_offset;

	return *(int*)(ring[channel] + pctl->elementsize * (pc->lastframe % pctl->ringlen) + tc_offset);
}

extern const uint8_t *nexus_lastframe_audio12(const NexusControl *pctl, uint8_t *ring[], int channel)
{
	const NexusBufCtl *pc = &pctl->channel[channel];

	return (ring[channel] + pctl->elementsize * (pc->lastframe % pctl->ringlen)
								+ pctl->audio12_offset);
}

extern const uint8_t *nexus_lastframe_audio34(const NexusControl *pctl, uint8_t *ring[], int channel)
{
	const NexusBufCtl *pc = &pctl->channel[channel];

	return (ring[channel] + pctl->elementsize * (pc->lastframe % pctl->ringlen)
								+ pctl->audio34_offset);
}

extern int nexus_connect_to_shared_mem(int timeout_microsec, int read_only, int verbose, NexusConnection *p)
{
	int			shm_id, control_id;
	int			at_flags = 0;			// default 0 means read-write

	if (read_only)
		at_flags = SHM_RDONLY;

	if (timeout_microsec > 0 && verbose) {
		printf("Waiting for shared memory... ");
		fflush(stdout);
	}

	// If shared memory not found, sleep and try again
	int retry_time = 20 * 1000;		// 20ms
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

	if (heartbeat_stopped == 0 && capture_dead == 0)	// i.e. connection OK
		return 1;

	if (p_heartbeat_stopped)
		*p_heartbeat_stopped = heartbeat_stopped;

	if (p_capture_dead)
		*p_capture_dead = capture_dead;

	return 0;
}
