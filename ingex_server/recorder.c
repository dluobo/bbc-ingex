/*
 * $Id: recorder.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
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
#include <limits.h>
#include <stdarg.h>

#include "recorder.h"
#include "dvd_encoder.h"
#include "AudioMixer.h"
#include "audio_utils.h"
#include "quad_split_I420.h"
#include "utils.h"


#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))

// storage for static member variables
uint8_t					*Recorder::ring[4];
NexusControl			*Recorder::p_control;
int						Recorder::verbose = 0;
pthread_mutex_t 		Recorder::m_dvd_open_close;
std::list<Recorder *>	Recorder::m_recorder_list;

bool Recorder::recorder_init(void)
{
	int		shm_id, control_id;
	int		i;

	// Open logFile based on date and time
	openLogFileWithDate("recorder");

	// If shared memory not found, sleep and try again
	logTF("Waiting for shared memory... ");
	fflush(stdout);
	while (1)
	{
		control_id = shmget(9, sizeof(*p_control), 0666);
		if (control_id != -1)
			break;
		usleep(20 * 1000);
	}

	// Shared memory found for control data, attach to it
	p_control = (NexusControl*)shmat(control_id, NULL, 0);
	logFF("connected to p_control 0x%08x\n", (uint32_t)p_control);

	logFF("  cards=%d elementsize=%d ringlen=%d\n",
				p_control->cards,
				p_control->elementsize,
				p_control->ringlen);

	// Attach to each video ring buffer
	for (i = 0; i < p_control->cards; i++) {
		while (1) {
			shm_id = shmget(10 + i, p_control->elementsize, 0444);
			if (shm_id != -1)
				break;
			usleep(20 * 1000);
		}
		ring[i] = (uint8_t*)shmat(shm_id, NULL, SHM_RDONLY);
		logFF("  attached to card[%d]\n", i);
	}

	// init global mutex to prevent dvd_encoder conflicts
	int err;
	if ((err = pthread_mutex_init(&m_dvd_open_close, NULL)) != 0) {
		logTF("Failed to init m_dvd_open_close: %s\n", strerror(err));
		return false;
	}

	return true;
}

void Recorder::delete_all_finished()
{
	logTF("Searching for old objects to delete\n");
	// cleanup
	std::list<Recorder *>::iterator pos;
	for (pos = m_recorder_list.begin(); pos != m_recorder_list.end(); ) {
		if ((*pos)->m_finished) {
			logTF("  found finished object... deleting\n");
			delete *pos;
			pos = m_recorder_list.erase(pos);
		}
		else {
			logTF("  found not finished object... leaving it\n");
			++pos;
		}
	}
	logTF("  completed search\n");
}

Recorder::Recorder()
{
	delete_all_finished();

	for (int i = 0; i < MAX_CARDS; i++)
		memset(tapename, 0, sizeof(tapename));
	for (int i = 0; i < MAX_RECORD; i++) {
		memset(&record_opt[i], 0, sizeof(record_opt[0]));
		memset(&invoke_arg[i], 0, sizeof(invoke_arg[0]));
	}

	mDescription = NULL;
	manage_thread = 0;
	m_finished = false;

	m_recorder_list.push_back(this);
	logTF("Added a new recorder\n");
}

Recorder::~Recorder()
{
	// finished with these strings
	if (mDescription)
		free(mDescription);

	logTF("Destroyed a recorder\n");
}

bool Recorder::recorder_prepare_start(
				unsigned long opt_target_tc,
				bool a_enable[],
				const char *opt_tape_names[],
				const char *opt_description,
				const char *opt_video_path,
				const char *opt_dvd_path,
				int opt_bitrate,
				bool opt_crash)
{
	// Set which sources are enabled for record
	logTF("recorder_prepare_start: enabled=");
	for (int i = 0; i < MAX_RECORD; i++) {
		record_opt[i].enabled = a_enable[i];
		logFF("%d ", a_enable[i]);
	}
	logFF("\n");

	// Settings that never change after sdi daemon startup
	int ringlen = p_control->ringlen;
	int elementsize = p_control->elementsize;
	int tc_offset = p_control->tc_offset;

	// Set capture wide options
	bool crash_record = opt_crash;
	int		target_tc = (int)opt_target_tc;

	// If crash record, search across all card for the minimum current (lastframe) timecode
	if (crash_record) {
		int tc[MAX_CARDS];

		logTF("Crash record:\n");
		for (int card_i = 0; card_i < MAX_CARDS; card_i++) {
			if (! record_opt[card_i].enabled)
				continue;

			int lastframe;
			PTHREAD_MUTEX_LOCK( &(p_control->card[card_i].m_lastframe) );
			lastframe = p_control->card[card_i].lastframe;
			PTHREAD_MUTEX_UNLOCK( &(p_control->card[card_i].m_lastframe) );

			tc[card_i] = *(int*)(ring[card_i] + elementsize * (lastframe % ringlen) + tc_offset);
			char tmp[32];
			logTF("    tc[%d]=%d  %s\n", card_i, tc[card_i], framesToStr(tc[card_i], tmp));
		}

		int max_tc = 0;
		int min_tc = INT_MAX;
		for (int card_i = 0; card_i < MAX_CARDS; card_i++) {
			if (! record_opt[card_i].enabled)
				continue;
			max_tc = max(max_tc, tc[card_i]);
			min_tc = min(min_tc, tc[card_i]);
		}
		int max_diff = max_tc - min_tc;

		logTF("    crash record max diff=%d sleeping for %d frames...\n", max_diff, max_diff+2);
		usleep( (max_diff+2) * 40 * 1000 );		// sleep max_diff number of frames
		// store lowest enabled card's timecode
		for (int card_i = MAX_CARDS-1; card_i >= 0; card_i--)
			if (record_opt[card_i].enabled)
				target_tc = tc[card_i];
		logTF("    target_tc = %d\n", target_tc);
	}

	// local vars
	bool	found_all_target = true;
	char tcstr[32];

	// Search for desired timecode across all target sources
	logTF("Searching for timecode=%d %s\n", target_tc, framesToStr(target_tc, tcstr));
	for (int card_i = 0; card_i < MAX_CARDS; card_i++) {
		if (! record_opt[card_i].enabled)
			continue;

		int lastframe;
		PTHREAD_MUTEX_LOCK( &(p_control->card[card_i].m_lastframe) );
		lastframe = p_control->card[card_i].lastframe;
		PTHREAD_MUTEX_UNLOCK( &(p_control->card[card_i].m_lastframe) );

		int last_tc = -1;
		int i = 0;
		int search_limit = ringlen - 12;		// guard of 12 frames (0.48 sec)
		bool found_target = false;
		int first_tc_seen = -1, last_tc_seen = -1;

		// find target timecode
		while (1) {
			// read timecode value
			int tc = *(int*)(ring[card_i] + elementsize * ((lastframe-i) % ringlen) + tc_offset);
			if (first_tc_seen == -1)
				first_tc_seen = tc;

			if (tc == target_tc) {
				record_opt[card_i].cardnum = card_i;
				record_opt[card_i].start_tc = target_tc;
				record_opt[card_i].start_frame = lastframe - i;
				record_opt[card_i].dvd_bitrate = opt_bitrate;
				record_opt[card_i].bits_per_sample = 32;
				record_opt[card_i].video_path = opt_video_path;
				record_opt[card_i].dvd_path = opt_dvd_path;
				found_target = true;
				logTF("Found card%d lf[%d]=%6d lf-i=%8d tc=%d %s\n",
					card_i, card_i, lastframe, lastframe - i, tc, framesToStr(tc, tcstr));
				break;
			}
			last_tc = tc;
			i++;
			if (i > search_limit)
			{
				logTF("Target tc not found for card[%d].  Searched %d frames, %.2f sec (ringlen=%d)\n",
							card_i, search_limit, search_limit / 25.0, ringlen);
				char tcstr2[32];
				logTF("    first_tc_seen=%d %s  last_tc_seen=%d %s\n",
							first_tc_seen, framesToStr(first_tc_seen, tcstr),
							last_tc_seen, framesToStr(last_tc_seen, tcstr2));
				break;
			}
			last_tc_seen = tc;
		}

		if (! found_target)
			found_all_target = false;
	}

	if (!found_all_target) {
		logTF("Could not start record - not all target timecodes found\n");
		return false;
	}

	// copy strings used during each record session
	for (int card_i = 0; card_i < MAX_CARDS; card_i++)
		if (record_opt[card_i].enabled)
			strcpy(tapename[card_i], opt_tape_names[card_i]);
	strcpy(m_dvd_path, opt_dvd_path);
	strcpy(m_video_path, opt_video_path);
	mDescription = (char *)malloc(strlen(opt_description) + 1);
	strcpy(mDescription, opt_description);

	return true;
}

bool Recorder::recorder_start(void)
{
	int err;

	// Now start all record threads, including quad
	for (int card_i = 0; card_i < MAX_RECORD; card_i++) {
		if (! record_opt[card_i].enabled)
			continue;

		// init mutexes used by each thread
		if ((err = pthread_mutex_init(&record_opt[card_i].m_duration, NULL)) != 0) {
			logTF("Failed to init m_duration: %s\n", strerror(err));
			return false;
		}

		if (card_i == QUAD_SOURCE) {
			// Special case for quad thread
			// Copy source 0 then update for frame offsets into sources 1 and 2
			memcpy(&record_opt[card_i], &record_opt[ 0 ], sizeof(record_opt[ 0 ]));
			record_opt[card_i].quad1_frame_offset =
					record_opt[1].start_frame - record_opt[0].start_frame;
			record_opt[card_i].quad2_frame_offset =
					record_opt[2].start_frame - record_opt[0].start_frame;

			invoke_arg[card_i].invoke_card = card_i;
			invoke_arg[card_i].p_rec = this;
			if ((err = pthread_create(	&record_opt[card_i].record_thread, NULL,
									start_quad_thread, &invoke_arg[card_i])) != 0) {
				logTF("Failed to create record thread: %s\n", strerror(err));
				return false;
			}
		}
		else {
			invoke_arg[card_i].invoke_card = card_i;
			invoke_arg[card_i].p_rec = this;
			if ((err = pthread_create(	&record_opt[card_i].record_thread, NULL,
									start_record_thread, &invoke_arg[card_i])) != 0) {
				logTF("Failed to create record thread: %s\n", strerror(err));
				return false;
			}
		}
	}
	return true;
}

int Recorder::recorder_stop(unsigned long opt_capture_length)
{
	// check that there is at least one recording enabled & running
	bool thread_running = false;
	for (int card_i = 0; card_i < MAX_RECORD; card_i++)
		if (record_opt[card_i].enabled) {
			if (record_opt[card_i].record_thread != 0)
				thread_running = true;
		}

	if (! thread_running) {
		logTF("recorder_stop: no record threads running, returning 0 duration\n");
		return 0;
	}

	// copy option to member variable
	m_capture_length = (int)opt_capture_length;

	// guard against crazy capture lengths
	if (m_capture_length <= 0) {
		// force immediate-ish finish of recording
		// true duration will be updated from frames_written variable
		logTF("recorder_stop: crazy duration of %d passed - stopping recording immediately by\n"
				"              signalling duration of 1 - see metadata.txt for true durations\n", m_capture_length);
		m_capture_length = 1;
	}

	// Now stop record by signalling the duration in frames
	for (int card_i = 0; card_i < MAX_RECORD; card_i++) {
		if (! record_opt[card_i].enabled)
			continue;

		PTHREAD_MUTEX_LOCK( &record_opt[card_i].m_duration )
		record_opt[card_i].duration = m_capture_length;
		PTHREAD_MUTEX_UNLOCK( &record_opt[card_i].m_duration )
		logTF("card %d: Signalled duration of %d\n", card_i, m_capture_length);
	}

	// Create a new thread to manage termination of running threads
	int err;
	if ((err = pthread_create(	&manage_thread,
								NULL,
								manage_record_thread,
								this
								)) != 0) {
		logTF("Failed to create termination managing thread: %s\n", strerror(err));
		return 0;
	}

	return m_capture_length;
}

bool Recorder::recorder_wait_for_completion(void)
{
	// wait until manage_record_thread()
	// has completed
	if (manage_thread == 0) {
		logTF("recorder_wait_for_completion: manage_thread not running, returning\n");
		return false;
	}

	int err;
	if ((err = pthread_join( manage_thread, NULL)) != 0) {
		logTF("Failed to join manage thread: %s\n", strerror(err));
		return false;
	}
	return true;
}


void *manage_record_thread(void *p_arg)
{
	Recorder *p = (Recorder *)p_arg;

	logTF("manage_record_thread:\n");

	// Wait for all threads to finish
	for (int card_i = 0; card_i < MAX_RECORD; card_i++) {
		if (! p->record_opt[card_i].enabled)
			continue;

		logTF("Waiting for thread %d\n", card_i);
		if ( p->record_opt[card_i].record_thread == 0 ) {
			logTF("thread for card[%d] is 0.  skipping\n", card_i);
			continue;
		}
		int err;
		if ((err = pthread_join( p->record_opt[card_i].record_thread, NULL)) != 0) {
			logTF("Failed to join record thread: %s\n", strerror(err));
		}
		logTF("  thread %d finished\n", card_i);
	}

	// write "metadata.txt"
	char dvd_meta_name[FILENAME_MAX];
	char vid_meta_name[FILENAME_MAX];
	strcpy(dvd_meta_name, p->m_dvd_path);
	strcat(dvd_meta_name, "/metadata.txt");
	strcpy(vid_meta_name, p->m_video_path);
	strcat(vid_meta_name, "/metadata.txt");

	if (! write_metadata_file(p, dvd_meta_name)) {
		logTF("write_metadata_file failed for %s\n", dvd_meta_name);
		p->m_finished = true;
		return NULL;
	}
	// If dvd_path and video_path are different, save a copy of metadata.txt
	if (strcmp(p->m_dvd_path, p->m_video_path) != 0)	// paths differ?
		if (! write_metadata_file(p, vid_meta_name)) {
			logTF("write_metadata_file failed for %s\n", vid_meta_name);
			// fall through - OK for video metadata file to fail
		}

	logTF("Recording complete - metadata.txt written to %s\n", dvd_meta_name);

	// mark this object as ready for deletion
	p->m_finished = true;
	return NULL;
}

bool write_metadata_file(Recorder *p, const char *meta_name)
{
	FILE *fp_meta = NULL;
	char tmp_meta_name[FILENAME_MAX];
	strcpy(tmp_meta_name, meta_name);
	strcat(tmp_meta_name, ".tmp");
	char tmpstr[64];

	if ((fp_meta = fopen(tmp_meta_name, "wb")) == NULL)
	{
		logTF("Could not open for write: %s\n", tmp_meta_name);
		perror("fopen");
		return false;
	}
	fprintf(fp_meta, "path=%s\n", meta_name);
	fprintf(fp_meta, "a_tape=");
	for (int i = 0; i < MAX_CARDS; i++)
		fprintf(fp_meta, "%s%s", p->tapename[i], (i % MAX_CARDS) == MAX_CARDS - 1 ? "\n" : ",");
	fprintf(fp_meta, "enabled=");
	for (int i = 0; i < MAX_RECORD; i++) {
		fprintf(fp_meta, "%d%s", p->record_opt[i].enabled, (i % MAX_RECORD) == MAX_RECORD - 1 ? "\n" : ",");
	}
	fprintf(fp_meta, "start_tc=%d\n", p->record_opt[0].start_tc);
	fprintf(fp_meta, "start_tc_str=%s\n", framesToStr(p->record_opt[0].start_tc, tmpstr));
	fprintf(fp_meta, "capture_length=%d\n", p->m_capture_length);
	fprintf(fp_meta, "a_start_tc=");
	for (int i = 0; i < MAX_RECORD; i++) {
		fprintf(fp_meta, "%d%s", p->record_opt[i].start_tc, (i % MAX_RECORD) == MAX_RECORD - 1 ? "\n" : " ");
	}
	fprintf(fp_meta, "a_start_tc_str=");
	for (int i = 0; i < MAX_RECORD; i++) {
		fprintf(fp_meta, "%s%s", framesToStr(p->record_opt[i].start_tc, tmpstr), (i % MAX_RECORD) == MAX_RECORD - 1 ? "\n" : " ");
	}
	fprintf(fp_meta, "a_duration=");
	for (int i = 0; i < MAX_RECORD; i++) {
		fprintf(fp_meta, "%d%s", p->record_opt[i].duration, (i % MAX_RECORD) == MAX_RECORD - 1 ? "\n" : " ");
	}
	fprintf(fp_meta, "description=%s\n", p->mDescription);
	fclose(fp_meta);

	if (rename(tmp_meta_name, meta_name) != 0)
	{
		logTF( "Could not rename %s to %s\n", tmp_meta_name, meta_name);
		perror("rename");
		return false;
	}

	return true;
}

void *start_quad_thread(void *p_arg)
{
	Recorder *p = ((ThreadInvokeArg *)p_arg)->p_rec;
	int invoke_card = ((ThreadInvokeArg *)p_arg)->invoke_card;
	RecordOptions *p_opt = &(p->record_opt[invoke_card]);

	char			*path_stem_name = NULL;
	char			*mpg_name = NULL;

	int start_tc = p_opt->start_tc;
	int start_frame = p_opt->start_frame;
	int quad1_offset = p_opt->quad1_frame_offset;
	int quad2_offset = p_opt->quad2_frame_offset;
	int dvd_bitrate = p_opt->dvd_bitrate;
	const char *dvd_path = p_opt->dvd_path;

	logTF("Starting start_quad_thread(start_tc=%d start_frame=%d)\n"
			"                           quad1_frame_offset=%d quad2_frame_offset=%d\n", 
				start_tc, start_frame,
				quad1_offset, quad2_offset);

	// TODO: If main not present quad currently segv's
	if (! p->record_opt[0].enabled) {
		logTF("start_quad_thread: main not enabled - recording quad split not supported (returning)\n");
		return NULL;
	}

	// Convenience variables accessing class data
	NexusControl *p_control = p->p_control;
	uint8_t **ring = p->ring;

	// This makes the record start at the target timecode
	int last_saved = start_frame - 1;

	int tc;
	int last_tc = -1;
	int ringlen = p_control->ringlen;
	int elementsize = p_control->elementsize;
	int tc_offset = p_control->tc_offset;
 	int audio12_offset = p_control->audio12_offset;
 	int audio34_offset = p_control->audio34_offset;
 	int audio_size = p_control->audio_size;

	mpg_name = (char *)malloc(FILENAME_MAX);
	path_stem_name = (char *)malloc(FILENAME_MAX);
	strcpy(mpg_name, dvd_path);
	strcat(mpg_name, "/");

	strcat(mpg_name, "QUAD");
	strcat(mpg_name, ".mpg");

	dvd_encoder_t *dvd;
	// prevent simultaneous calls to dvd_encoder_init causing
	// "insufficient thread locking around avcodec_open/close()"
	// also
	// "dvd format not registered"
	bool dvd_init_failed = false;
	PTHREAD_MUTEX_LOCK( &p->m_dvd_open_close )
	if ((dvd = dvd_encoder_init(mpg_name, dvd_bitrate)) == 0) {
		logTF("quad: dvd_encoder_init() failed\n");
		dvd_init_failed = true;
	}
	PTHREAD_MUTEX_UNLOCK( &p->m_dvd_open_close )

	if (dvd_init_failed) {
		free(mpg_name);
		free(path_stem_name);
		return NULL;
	}

	AudioMixer main_mixer(AudioMixer::MAIN);

	bool finished_record = false;

	uint8_t			p_quadvideo[720*576*3/2];
	memset(p_quadvideo, 0x10, 720*576);			// Y black
	memset(p_quadvideo + 720*576, 0x80, 720*576/4);			// Y black
	memset(p_quadvideo + 720*576*5/4, 0x80, 720*576/4);			// Y black

	while (1)
	{
		// Workaround for future echos which can occur when video arrives
		// at slightly different times
		//
		// Give a guard of 2 extra frame before lastframe since the calculated
		// lastframe on the other (non key) sources might not be ready in practice
		//
		// There is currently no need for the encoded quad split to be
		// absolutely up-to-date in real time.
		int guard = 2;

		// Read lastframe counter from shared memory in a thread safe manner
		PTHREAD_MUTEX_LOCK( &p_control->card[ 0 ].m_lastframe )
		int lastframe0 = p_control->card[ 0 ].lastframe - guard;
		PTHREAD_MUTEX_UNLOCK( &p_control->card[ 0 ].m_lastframe )

		PTHREAD_MUTEX_LOCK( &p_control->card[ 1 ].m_lastframe )
		int lastframe1 = p_control->card[ 1 ].lastframe - guard;
		PTHREAD_MUTEX_UNLOCK( &p_control->card[ 1 ].m_lastframe )

		PTHREAD_MUTEX_LOCK( &p_control->card[ 2 ].m_lastframe )
		int lastframe2 = p_control->card[ 2 ].lastframe - guard;
		PTHREAD_MUTEX_UNLOCK( &p_control->card[ 2 ].m_lastframe )

		if (last_saved == lastframe0)
		{
			// Caught up to latest available frame
			// sleep for half a frame worth
			usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
			continue;
		}

		// read timecode value for first source
		tc = *(int*)(ring[ 0 ] + elementsize * (lastframe0 % ringlen) + tc_offset);

		// check for start timecode if any
		// we will record as soon as active tc is >= target timecode
		if (tc < start_tc)
		{
			usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
			continue;
		}

		int diff = lastframe0 - last_saved;
		if (diff >= ringlen)
		{
			logTF("dropped frames worth %d (last_saved=%d lastframe0=%d)\n",
						diff, last_saved, lastframe0);
			last_saved = lastframe0 - 1;
			diff = 1;
		}

		// Save all frames which have not been saved
		// All decisions taken from source 0
		//
		for (int fi = diff - 1; fi >= 0; fi--)
		{
			// lastframe adjusted for fi (frame back count index) and source offsets
			int lfa[MAX_CARDS];
			lfa[0] = lastframe0 - fi;
			lfa[1] = lastframe0 - fi + quad1_offset;
			lfa[2] = lastframe0 - fi + quad2_offset;

			uint8_t *p_yuv0 = ring[ 0 ] + elementsize *
								((lfa[0]) % ringlen)
								+ audio12_offset + audio_size;
			uint8_t *p_yuv1 = ring[ 1 ] + elementsize *
								((lfa[1]) % ringlen)
								+ audio12_offset + audio_size;
			uint8_t *p_yuv2 = ring[ 2 ] + elementsize *
								((lfa[2]) % ringlen)
								+ audio12_offset + audio_size;
			// audio is taken from source 0
			const int32_t	*p_audio12 = (int32_t*)
								(ring[ 0 ] + elementsize *
								((lfa[0]) % ringlen) +
								audio12_offset);
			const int32_t	*p_audio34 = (int32_t*)
								(ring[ 0 ] + elementsize *
								((lfa[0]) % ringlen) +
								audio34_offset);
			int16_t			mixed[1920*2];	// stereo pair output
			I420_frame		in, out;

			// mix audio
			main_mixer.Mix(p_audio12, p_audio34, mixed, 1920);

			// Make quarter size copy
			in.cbuff = p_yuv0;
			in.size = 720*576*3/2;
			in.Ybuff = in.cbuff;
			in.Ubuff = in.cbuff + 720*576;
			in.Vbuff = in.cbuff + 720*576 *5/4;
			in.w = 720;
			in.h = 576;
			out.cbuff = p_quadvideo;
			out.size = 720*576*3/2;
			out.Ybuff = out.cbuff;
			out.Ubuff = out.cbuff + 720*576;
			out.Vbuff = out.cbuff + 720*576 *5/4;
			out.w = 720;
			out.h = 576;

			// top left - MAIN
			quarter_frame_I420(&in, &out,
								0,		// x offset
								0,		// y offset
								1,		// interlace?
								1,		// horiz filter?
								1);		// vert filter?
			if (p->record_opt[1].enabled) {
				// bottom left - ISO 1
				in.cbuff = p_yuv1;
				in.Ybuff = in.cbuff;
				in.Ubuff = in.cbuff + 720*576;
				in.Vbuff = in.cbuff + 720*576 *5/4;
				quarter_frame_I420(&in, &out,
									0, 576/2,
									1, 1, 1);
			}
			if (p->record_opt[2].enabled) {
				// bottom right - ISO 2
				in.cbuff = p_yuv2;
				in.Ybuff = in.cbuff;
				in.Ubuff = in.cbuff + 720*576;
				in.Vbuff = in.cbuff + 720*576 *5/4;
				quarter_frame_I420(&in, &out,
									720/2, 576/2,
									1, 1, 1);
			}

			// read timecodes for each card
			int a_tc[MAX_CARDS];
			for (int i = 0; i < MAX_CARDS; i++)
				a_tc[i] = *(int*)(ring[i] + elementsize * (lfa[i] % ringlen) + tc_offset);
			// work out which timecode to burn in based on enable flags
			int tc_burn = a_tc[0];
			for (int i = MAX_CARDS-1; i >= 0; i--) {
				if (p->record_opt[i].enabled)
					tc_burn = a_tc[i];
			}

			// encode to dvd
			if (dvd_encoder_encode(dvd, p_quadvideo, mixed, tc_burn) != 0) {
				logTF("dvd_encoder_encode failed\n");
				free(mpg_name);
				free(path_stem_name);
				return NULL;
			}

			p_opt->frames_written++;

			if (p->verbose > 1)
				logF("quad fi=%2d lf0,lfa0=%6d,%6d lf1,lfa1=%6d,%6d lf2,lfa2=%6d,%6d tc0=%6d tc1=%6d tc2=%6d tc_burn=%d\n", fi,
					lastframe0, lfa[0], lastframe1, lfa[1], lastframe2, lfa[2],
					a_tc[0], a_tc[1], a_tc[2], tc_burn);

			// Finish when we've reached duration number of frames
			PTHREAD_MUTEX_LOCK( &p_opt->m_duration )
			int duration = p_opt->duration;
			PTHREAD_MUTEX_UNLOCK( &p_opt->m_duration )
			if (duration > 0 && p_opt->frames_written >= duration) {
				logTF("  quad   duration %d reached (frames_written=%d)\n",
							duration, p_opt->frames_written);
				finished_record = true;
				// update duration to true value from frames_written
				PTHREAD_MUTEX_LOCK( &p_opt->m_duration )
				p_opt->duration = p_opt->frames_written;
				PTHREAD_MUTEX_UNLOCK( &p_opt->m_duration )
				break;
			}

			// tiny delay to avoid getting future frames
			// didn't work
			//usleep(1 * 1000);
		}

		if (finished_record)
			break;

		last_tc = tc;
		last_saved = lastframe0;

		usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
	}

	// shutdown dvd encoder
	PTHREAD_MUTEX_LOCK( &p->m_dvd_open_close )
	if (dvd_encoder_close(dvd) != 0)
		logTF("quad  : dvd_encoder_close() failed\n");
	PTHREAD_MUTEX_UNLOCK( &p->m_dvd_open_close )

	// free malloc'd variable
	free(mpg_name);
	free(path_stem_name);
	return NULL;
}

void *start_record_thread(void *p_arg)
{
	Recorder *p = ((ThreadInvokeArg *)p_arg)->p_rec;
	int invoke_card = ((ThreadInvokeArg *)p_arg)->invoke_card;
	RecordOptions *p_opt = &(p->record_opt[invoke_card]);

	int cardnum = p_opt->cardnum;
	int start_tc = p_opt->start_tc;
	int start_frame = p_opt->start_frame;
	int dvd_bitrate = p_opt->dvd_bitrate;
	int audio_bits = p_opt->bits_per_sample;
	const char *video_path = p_opt->video_path;
	const char *dvd_path = p_opt->dvd_path;

	logTF("Starting start_record_thread(cardnum=%d, start_tc=%d start_frame=%d)\n",
				cardnum, start_tc, start_frame);

	// Convenience variables accessing class data
	NexusControl *p_control = p->p_control;
	uint8_t **ring = p->ring;

	// This makes the record start at the target timecode
	int last_saved = start_frame - 1;

	char			*path_stem_name = NULL, *video_name = NULL;
	char			*audio_name1 = NULL, *audio_name2 = NULL;
	char			*mpg_name = NULL;

	bool enable_dvd = true;
	int tc;
	int last_tc = -1;
 	int frame_size = 720*576*2;

	int ringlen = p_control->ringlen;
	int elementsize = p_control->elementsize;
	int tc_offset = p_control->tc_offset;
 	int audio12_offset = p_control->audio12_offset;
 	int audio34_offset = p_control->audio34_offset;
 	int audio_size = p_control->audio_size;

	video_name = (char *)malloc(FILENAME_MAX);
	audio_name1 = (char *)malloc(FILENAME_MAX);
	audio_name2 = (char *)malloc(FILENAME_MAX);
	mpg_name = (char *)malloc(FILENAME_MAX);
	path_stem_name = (char *)malloc(FILENAME_MAX);
	path_stem_name[0] = '\0';

	strcpy(video_name, video_path);		// e.g. "/video"
	strcpy(audio_name1, video_path);
	strcpy(audio_name2, video_path);
	strcpy(mpg_name, dvd_path);

	strcat(video_name, "/");
	strcat(audio_name1, "/");
	strcat(audio_name2, "/");
	strcat(mpg_name, "/");

	// depending upon card number append MAIN, ISO1, or ISO2
	if (cardnum == 0) {
		strcat(path_stem_name, "MAIN");
		strcat(mpg_name, "MAIN");
	}
	if (cardnum == 1) {
		strcat(path_stem_name, "ISO1");
		strcat(mpg_name, "ISO1");
	}
	if (cardnum == 2) {
		strcat(path_stem_name, "ISO2");
		strcat(mpg_name, "ISO2");
	}

	strcat(video_name, path_stem_name);		// e.g. "/a/b/c/take1/MAIN"
	strcat(audio_name1, path_stem_name);
	strcat(audio_name2, path_stem_name);

	strcat(video_name, ".uyvy");
	strcat(audio_name1, "_12.wav");
	strcat(audio_name2, "_34.wav");
	strcat(mpg_name, ".mpg");

	FILE		*fp_video;
	FILE		*fp_audio1;
	FILE		*fp_audio2;

	if ((fp_video = fopen(video_name, "wb")) == NULL)
	{
		logTF("Could not open %s\n", video_name);
		return NULL;
	}
	if ((fp_audio1 = fopen(audio_name1, "wb")) == NULL)
	{
		logTF("Could not open %s\n", audio_name1);
		return NULL;
	}
	if ((fp_audio2 = fopen(audio_name2, "wb")) == NULL)
	{
		logTF("Could not open %s\n", audio_name2);
		return NULL;
	}

	// Audio is 32-bit signed, little-endian, 48000, 2-channel
	writeWavHeader(fp_audio1, audio_bits);
	writeWavHeader(fp_audio2, audio_bits);

	dvd_encoder_t *dvd;
	if (enable_dvd) {
		// prevent simultaneous calls to dvd_encoder_init causing
		// "insufficient thread locking around avcodec_open/close()"
		// also
		// "dvd format not registered"
		PTHREAD_MUTEX_LOCK( &p->m_dvd_open_close )
		if ((dvd = dvd_encoder_init(mpg_name, dvd_bitrate)) == 0) {
			logTF("card %d: dvd_encoder_init() failed\n", cardnum);
			enable_dvd = false;			// give up on dvd
		}
		PTHREAD_MUTEX_UNLOCK( &p->m_dvd_open_close )
	}

	AudioMixer main_mixer(AudioMixer::MAIN);
	AudioMixer iso_mixer(AudioMixer::ISO);

	bool finished_record = false;

	while (1)
	{
		// Read lastframe counter from shared memory in a thread safe manner
		PTHREAD_MUTEX_LOCK( &p_control->card[cardnum].m_lastframe )
		int lastframe = p_control->card[cardnum].lastframe;
		PTHREAD_MUTEX_UNLOCK( &p_control->card[cardnum].m_lastframe )

		if (last_saved == lastframe)
		{
			// Caught up to latest available frame
			// sleep for half a frame worth
			usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
			continue;
		}

		// read timecode value
		tc = *(int*)(ring[cardnum]
				+ elementsize * (lastframe % ringlen)
				+ tc_offset);

		// check for start timecode if any
		// we will record as soon as active tc is >= target timecode
		if (tc < start_tc)
		{
			usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
			continue;
		}

		int diff = lastframe - last_saved;
		if (diff >= ringlen)
		{
			logTF("dropped frames worth %d (last_saved=%d lastframe=%d)\n",
						diff, last_saved, lastframe);
			last_saved = lastframe - 1;
			diff = 1;
		}

		// Save all frames which have not been saved
		for (int fi = diff - 1; fi >= 0; fi--)
		{
			//int		yuv_size = 720*576*3/2;
			uint8_t *p_yuv = ring[cardnum] + elementsize *
								((lastframe - fi) % ringlen)
								+ audio12_offset + audio_size;
			const int32_t	*p_audio12 = (int32_t*)
								(ring[cardnum] + elementsize *
								((lastframe - fi) % ringlen) +
								audio12_offset);
			const int32_t	*p_audio34 = (int32_t*)
								(ring[cardnum] + elementsize *
								((lastframe - fi) % ringlen) +
								audio34_offset);
			int tc_i = *(int*)(ring[cardnum] + elementsize *
								((lastframe - fi) % ringlen)
								+ tc_offset);
			int16_t			mixed[1920*2];	// stereo pair output

			// mix audio
			if (cardnum == 0)
				main_mixer.Mix(p_audio12, p_audio34, mixed, 1920);
			else
				iso_mixer.Mix(p_audio12, p_audio34, mixed, 1920);

			// encode to dvd
			if (enable_dvd)
				if (dvd_encoder_encode(dvd, p_yuv, mixed, tc_i) != 0)
					logTF("dvd_encoder_encode failed\n");

			// save uncompressed video and audio
			fwrite(ring[cardnum] + elementsize * ((lastframe - fi) % ringlen), 
						frame_size, 1, fp_video);

			// save uncompressed audio
			write_audio(fp_audio1, (uint8_t*)p_audio12, 1920*2, audio_bits);
			write_audio(fp_audio2, (uint8_t*)p_audio34, 1920*2, audio_bits);

			p_opt->frames_written++;
			//int tc_diff = tc - last_tc;
			//char tcstr[32];
			//logF("card%d lastframe=%6d diff(fi=%d) =%6d tc_i=%7d tc_diff=%3d %s\n", cardnum, lastframe, fi, diff, tc_i, tc_diff, framesToStr(tc, tcstr));

			// Finish when we've reached duration number of frames
			PTHREAD_MUTEX_LOCK( &p_opt->m_duration )
			int duration = p_opt->duration;
			PTHREAD_MUTEX_UNLOCK( &p_opt->m_duration )
			if (duration > 0 && p_opt->frames_written >= duration) {
				logTF("  card %d duration %d reached (frames_written=%d)\n",
							cardnum, duration, p_opt->frames_written);
				finished_record = true;
				// update duration to true value from frames_written
				PTHREAD_MUTEX_LOCK( &p_opt->m_duration )
				p_opt->duration = p_opt->frames_written;
				PTHREAD_MUTEX_UNLOCK( &p_opt->m_duration )
				break;
			}
		}

		if (finished_record)
			break;

		last_tc = tc;
		last_saved = lastframe;

		usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
	}

	// update and close files
	fclose(fp_video);
	update_WAV_header(fp_audio1);
	update_WAV_header(fp_audio2);

	// shutdown dvd encoder
	if (enable_dvd) {
		PTHREAD_MUTEX_LOCK( &p->m_dvd_open_close )
		if (dvd_encoder_close(dvd) != 0)
			logTF("card %d: dvd_encoder_close() failed\n", cardnum);
		PTHREAD_MUTEX_UNLOCK( &p->m_dvd_open_close )
	}

	// free malloc'd variable
	free(video_name);
	free(audio_name1);
	free(audio_name2);
	free(mpg_name);
	free(path_stem_name);

	return NULL;
}
