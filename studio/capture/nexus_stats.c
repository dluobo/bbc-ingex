// compile with:
// gcc -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -O3 -o save_mem save_mem.c
//
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <signal.h>
#include <unistd.h>	
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#include <unistd.h>
#include <sys/times.h>

#include "nexus_control.h"

#include "audio_utils.h"


int verbose = 1;


static char *framesToStr(int tc, char *s)
{
	int frames = tc % 25;
	int hours = (int)(tc / (60 * 60 * 25));
	int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
	int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

	sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}

static void usage_exit(void)
{
	fprintf(stderr, "Usage: nexus_stats [options]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -t <ltc|vitc>    type of timecode to read [default ltc]\n");
	fprintf(stderr, "    -v               increase verbosity\n");
	fprintf(stderr, "\n");
	exit(1);
}

static int64_t tv_diff_microsecs(const struct timeval* a, const struct timeval* b)
{
    int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
    return diff;
}

extern int main(int argc, char *argv[])
{
	int				shm_id, control_id;
	uint8_t			*ring[MAX_CHANNELS];
	NexusControl	*pctl = NULL;
	NexusTimecode	tc_type = NexusTC_None;
	int				recorder_stats = 0;

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
		else if (strcmp(argv[n], "-v") == 0)
		{
			verbose++;
		}
		else if (strcmp(argv[n], "-r") == 0)
		{
			recorder_stats = 1;
		}
		else if (strcmp(argv[n], "-t") == 0)
		{
			if (strcmp(argv[n+1], "vitc") == 0) {
				tc_type = NexusTC_VITC;
			}
			n++;
		}
	}

	// If shared memory not found, sleep and try again
	if (verbose) {
		printf("Waiting for shared memory... ");
		fflush(stdout);
	}

	while (1)
	{
		control_id = shmget(9, sizeof(*pctl), 0444);
		if (control_id != -1)
			break;
		usleep(20 * 1000);
	}

	pctl = (NexusControl*)shmat(control_id, NULL, SHM_RDONLY);
	if (verbose)
		printf("connected to pctl\n");

	if (verbose) {
		printf("  channels=%d ringlen=%d elementsize=%d\n",
				pctl->channels,
				pctl->ringlen,
				pctl->elementsize);
		printf("  width,height=%dx%d fr=%d/%d\n",
				pctl->width,
				pctl->height,
				pctl->frame_rate_numer,
				pctl->frame_rate_denom);
		printf("  pri_format=%s sec_format=%s\n",
				nexus_capture_format_name(pctl->pri_video_format),
				nexus_capture_format_name(pctl->sec_video_format));
		printf("  sec_width,sec_height=%dx%d\n",
				pctl->sec_width, pctl->sec_height);
		printf("  master_tc_type=%s master_tc_channel=%d\n",
				nexus_timecode_type_name(pctl->master_tc_type),
				pctl->master_tc_channel);
		struct timeval now;
		gettimeofday(&now, NULL);
		printf("  owner_pid=%d owner_heartbeat=[%ld,%ld] (now=[%ld,%ld])\n",
				pctl->owner_pid,
				pctl->owner_heartbeat.tv_sec,
				pctl->owner_heartbeat.tv_usec,
				now.tv_sec,
				now.tv_usec);
		printf("  a12_offset=%d a34_offset=%d audio_size=%d\n",
				pctl->audio12_offset,
				pctl->audio34_offset,
				pctl->audio_size);
		printf("  signal_ok_offset=%d tick_offset=%d ltc_offset=%d vitc_offset=%d sec_video_offset=%d\n",
				pctl->signal_ok_offset,
				pctl->tick_offset,
				pctl->ltc_offset,
				pctl->vitc_offset,
				pctl->sec_video_offset);
	}

	if (tc_type == NexusTC_VITC)
		printf("Overriding master timecode type and displaying VITC\n");

	int i;
	for (i = 0; i < pctl->channels; i++)
	{
		while (1)
		{
			shm_id = shmget(10 + i, pctl->elementsize, 0444);
			if (shm_id != -1)
				break;
			usleep(20 * 1000);
		}
		ring[i] = (uint8_t*)shmat(shm_id, NULL, SHM_RDONLY);
		if (verbose)
			printf("  attached to channel[%d]: '%s'\n", i, pctl->channel[i].source_name);
	}

	int tc[MAX_CHANNELS];
	int signal_ok[MAX_CHANNELS] = {0,0,0,0,0,0,0,0};
	int num_aud_samp[MAX_CHANNELS] = {0,0,0,0,0,0,0,0};
	int last_saved[MAX_CHANNELS] = {-1, -1, -1, -1, -1, -1, -1, -1};
	double audio_peak_power[MAX_CHANNELS][2];
	int recording[MAX_CHANNELS][2] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
	int record_error[MAX_CHANNELS][2] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
	int frames_written[MAX_CHANNELS][2] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
	int frames_dropped[MAX_CHANNELS][2] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
	int frames_in_backlog[MAX_CHANNELS][2] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
	int first_display = 1;

	while (1)
	{
		// check heartbeat age
		struct timeval now;
		gettimeofday(&now, NULL);
		int64_t diff = tv_diff_microsecs(&pctl->owner_heartbeat, &now);
		if (diff > 200 * 1000) {
			int owner_dead = 0;
			if (kill(pctl->owner_pid, 0) == -1 && errno != EPERM)
				owner_dead = 1;
			printf("heartbeat stopped, owner_pid(%d) %s, heartbeat diff=%"PRIi64"microsecs\n", pctl->owner_pid, owner_dead ? "dead" : "alive", diff);
		}
		
		for (i = 0; i < pctl->channels; i++)
		{
			NexusBufCtl *pc;
			pc = &pctl->channel[i];
			if (i == 0 && last_saved[0] == pc->lastframe)
			{
				usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
				continue;
			}

			tc[i] = nexus_lastframe_tc(pctl, ring, i, tc_type);
			signal_ok[i] = nexus_lastframe_signal_ok(pctl, ring, i);
			num_aud_samp[i] = nexus_lastframe_num_aud_samp(pctl, ring, i);

			last_saved[i] = pc->lastframe;

			// reformat audio for calculating peak power
			uint8_t *audio_dvs = ring[i] + pctl->elementsize * (pc->lastframe % pctl->ringlen)
									+ pctl->audio12_offset;

			int audio_chan;
			for (audio_chan = 0; audio_chan < 2; audio_chan++) {
				uint8_t audio_16bitmono[1920*2];
				dvsaudio32_to_16bitmono(audio_chan, audio_dvs, audio_16bitmono);
				audio_peak_power[i][audio_chan] = calc_audio_peak_power(audio_16bitmono, 1920, 2, -96.0);
			}

			// Recorder stats
			int j;
			for (j = 0; j < 2; j++) {
				recording[i][j] = pctl->record_info[0].channel[i][j].recording;
				record_error[i][j] = pctl->record_info[0].channel[i][j].record_error;
				frames_written[i][j] = pctl->record_info[0].channel[i][j].frames_written;
				frames_dropped[i][j] = pctl->record_info[0].channel[i][j].frames_dropped;
				frames_in_backlog[i][j] = pctl->record_info[0].channel[i][j].frames_in_backlog;
			}
		}
		if (verbose < 2)
			printf("\r");

		if (recorder_stats) {
			printf("\"%s\": ", pctl->record_info[0].name);
			for (i = 0; i < pctl->channels; i++) {
				int j;
				for (j = 0; j < 2; j++) {
					printf("[%d,%d]%s%s %d d=%d b=%d ", i, j, recording[i][j]?"*":".", record_error[i][j]?"E":"-", frames_written[i][j], frames_dropped[i][j], frames_in_backlog[i][j]);
				}
			}
		}
		else {
			for (i = 0; i < pctl->channels; i++)
			{
				char tcstr[32];
				printf("%d,%d,%s,%4d,%3.0f,%3.0f,%s ",
						i, last_saved[i],
						signal_ok[i] ? "ok":"--",
						num_aud_samp[i],
						audio_peak_power[i][0], audio_peak_power[i][1],
						framesToStr(tc[i], tcstr));
			}
			if (pctl->channels == 3)
				printf("offset to 0:%3d,%3d", tc[1] - tc[0], tc[2] - tc[0]);
			else if (pctl->channels == 4)
				printf("offset to 0:%3d,%3d,%3d", tc[1] - tc[0], tc[2] - tc[0], tc[3] - tc[0]);
		}

		if (first_display) {
			printf("\n\n");
			first_display = 0;
		}
		if (verbose >= 2)
			printf("\n");
		else
			fflush(stdout);
	}

	return 0;
}
