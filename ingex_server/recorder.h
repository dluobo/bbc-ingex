/*
 * $Id: recorder.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Multi-threaded uncompressed video and MPEG-2 PS implementation.
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

#ifndef RECORDER_H
#define RECORDER_H

#include <stdio.h>
#include <inttypes.h>
#include <list>

#include "nexus_control.h"


#define MAX_CARDS 3			// real sources of SDI signals
#define MAX_RECORD 4		// real + virutal (quad split source)
#define QUAD_SOURCE 3		// id of quad source where ids are 0,1,2,3

// Store thread specific data
typedef struct {
	int			cardnum;
	bool		enabled;				// recording on this card?
	int			start_tc;
	int			start_frame;
	int			quad1_frame_offset;	// diff between source 0 and source 1
	int			quad2_frame_offset;	// diff between source 0 and source 2
	int			dvd_bitrate;
	int			bits_per_sample;
	const char	*video_path;
	const char	*dvd_path;
	pthread_t	record_thread;		// running thread
	int			frames_written;
	pthread_mutex_t		m_duration;	// lock acces to duration
	int			duration;			// signal when capture should stop
} RecordOptions;

class Recorder;

typedef struct {
	int			invoke_card;
	Recorder	*p_rec;
} ThreadInvokeArg;

class Recorder
{
public:
	// called once to attach to shared memory etc
	static bool recorder_init(void);

	// called every so often to deleted finished recorders
	static void delete_all_finished();

	// per record session functions
	// constructor clears all member data
	Recorder();
	~Recorder();

	bool recorder_prepare_start(
				unsigned long opt_target_tc,
				bool a_enable[],
				const char *tape_names[],
				const char *opt_description,
				const char *video_path,
				const char *dvd_path,
				int opt_bitrate,			// kbps e.g. 5000 = 5Mbps
				bool opt_crash);
	bool recorder_start(void);
	int recorder_stop(unsigned long opt_capture_length);
	bool recorder_wait_for_completion(void);

	//FIXME: Add
	// recorder_cancel(void)
	// recorder_status(void)

	// static data
	static uint8_t					*ring[4];
	static NexusControl				*p_control;
	static int						verbose;
	// mutex to ensure only one thread at a time can call dvd_encoder_init
	static pthread_mutex_t			m_dvd_open_close;
	static std::list<Recorder *>	m_recorder_list;

	// per session data (start to stop + wait)
	char			tapename[MAX_CARDS][FILENAME_MAX];
	char			m_dvd_path[FILENAME_MAX];
	char			m_video_path[FILENAME_MAX];
	char			*mDescription;
	int				cardnum;					// card for this recording
	int				m_capture_length;
	RecordOptions	record_opt[MAX_RECORD];
	ThreadInvokeArg	invoke_arg[MAX_RECORD];
	pthread_t		manage_thread;
	bool			m_finished;					// can the recorder object be deleted?
};

// called by threads routines
void *start_record_thread(void *p_arg);
void *start_quad_thread(void *p_arg);
void *manage_record_thread(void *p_arg);
bool write_metadata_file(Recorder *p, const char *meta_name);

#endif /* RECORDER_H */
