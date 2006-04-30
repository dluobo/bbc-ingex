/*
 * $Id: nexus_control.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Shared memory interface between SDI capture threads and reader threads.
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

#ifndef NEXUS_CONTROL_H
#define NEXUS_CONTROL_H

#include <pthread.h>
#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");

typedef struct {
	pthread_mutex_t		m_lastframe;	// mutex for lastframe counter
	int		lastframe;		// last frame number written and now available
							// use lastframe % ringlen to get buffer index
	int		hwdrop;			// frame-drops recorded by sv interface

	int		key;			// shared memory key for this buffer
	int		width;			// width and height of video
	int		height;
	int		audiosize;		// size of all audio samples in buffer
} NexusBufCtl;

typedef struct {
	NexusBufCtl		card[4];		// array of buffer control information
									// for all 4 possible cards

	int				cards;			// number of cards in use
	int				elementsize;	// an element is video + audio + timecode
	int				audio12_offset;	// offset to start of audio ch 1,2 samples
	int				audio34_offset;	// offset to start of audio ch 3,4 samples
	int				audio_size;		// size in bytes of all audio data (4 chans)
									// including dvs internal padding
	int				tc_offset;		// offset to start of timecode data (int)
	int				ringlen;		// number of elements in ring buffer
} NexusControl;

#endif // NEXUS_CONTROL_H
