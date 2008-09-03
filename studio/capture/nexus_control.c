/*
 * $Id: nexus_control.c,v 1.1 2008/09/03 14:13:26 john_f Exp $
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

