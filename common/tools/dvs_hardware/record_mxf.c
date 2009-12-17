/*
 * $Id: record_mxf.c,v 1.2 2009/12/17 15:53:43 john_f Exp $
 *
 * Record uncompressed SDI video and audio to disk.
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
#include <malloc.h>
#include <inttypes.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>	
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <pthread.h>

#include "dvs_clib.h"
#include "dvs_fifo.h"

#include "audio_utils.h"
#include "video_VITC.h"

#include <mxf/mxf.h>
#include <write_archive_mxf.h>

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
#define CHK(x) {if (! (x)) { fprintf(stderr, "failed at %s line %d\n", __FILE__, __LINE__); } }


// Globals
sv_handle		*a_sv[4];
int				element_size = 0, audio_offset = 0;
const char		*video_file = NULL, *audio12_file = NULL, *audio34_file = NULL;
FILE *fp_video = NULL, *fp_audio12 = NULL, *fp_audio34 = NULL;

static int verbose = 1;
static int no_audio = 0;
static int video_bit_format = 0;
static int frame_inpoint = -1;
static int frame_outpoint = -1;
static off_t frames_to_capture = 0xffffffff;
static int video_size = 0;
static int video_width = 0;
static int video_height = 0;
static int audio12_offset = 0;
static int audio34_offset = 0;

static int write_mxf = 0;
static ArchiveMXFWriter* mxfout;

#if 0
static inline void write_bcd(unsigned char* target, int val)
{
    *target = ((val / 10) << 4) + (val % 10);
}
static int int_to_dvs_tc(int tc)
{
	// e.g. turn 1020219 into hex 0x11200819
	int fr = tc % 25;
	int hr = (int)(tc / (60*60*25));
	int mi = (int)((tc - (hr * 60*60*25)) / (60 * 25));
	int se = (int)((tc - (hr * 60*60*25) - (mi * 60*25)) / 25);

	unsigned char raw_tc[4];
	write_bcd(raw_tc + 0, fr);
	write_bcd(raw_tc + 1, se);
	write_bcd(raw_tc + 2, mi);
	write_bcd(raw_tc + 3, hr);
	int result = *(int *)(raw_tc);
	return result;
}
#endif

static void cleanup_exit(int res, sv_handle *sv)
{
    printf("cleaning up\n");
	// attempt to avoid lockup when stopping playback
	if (sv != NULL)
	{
		// sv_fifo_wait does nothing for record, only useful for playback
		sv_close(sv);
	}
	if (write_mxf) {
		if (!complete_archive_mxf_file(&mxfout, NULL, NULL, 0, NULL, 0)) {
            fprintf(stderr, "Failed to complete writing MXF file\n");
        }
	}

	printf("done - exiting\n");
    exit(res);
}

typedef struct thread_arg_t {
	int			card;
	sv_handle	*sv;
	sv_fifo		*fifo;
} thread_arg_t;

static pthread_mutex_t m_last_frame_written;
static pthread_mutex_t m_last_frame_captured;
static int g_last_frame_written = -1;
static int g_last_frame_captured = -1;
static int g_finished_capture = 0;

#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");

static uint8_t *ring = NULL;
#define RING_SIZE 125

static uint8_t *ring_frame(int frame)
{
	return ring + (frame % RING_SIZE) * element_size;
}
static int ring_element_size(void)
{
	return element_size;
}

#define VIDEO_FRAME_WIDTH       720
#define VIDEO_FRAME_HEIGHT      576
#define VIDEO_FRAME_SIZE        (VIDEO_FRAME_WIDTH * VIDEO_FRAME_HEIGHT * 2)
#define AUDIO_FRAME_SIZE        1920 * 3		// 48000Hz sample rate at 24bit

void audio_32_to_24bit(uint8_t *buf32, uint8_t *buf24)
{
	int i;
	for (i = 0; i < 1920; i++) {
		buf24[i*3+0] = (buf32[i] & 0x0000ff00) >> 8;
		buf24[i*3+1] = (buf32[i] & 0x00ff0000) >> 16;
		buf24[i*3+2] = (buf32[i] & 0xff000000) >> 24;
	}
}

// Read from ring memory buffer, write to disk file
static void *write_to_disk(void *arg)
{
	while (1)
	{
	    int audio_size = 1920*2*4;
		int last_frame_written;
		int last_frame_captured;
	
		PTHREAD_MUTEX_LOCK(&m_last_frame_written)
		last_frame_written = g_last_frame_written;
		PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)
	
		PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
		last_frame_captured = g_last_frame_captured;
		PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)
	
		// -1 indicates nothing available yet (empty buffer) so nothing to save
		if (last_frame_captured == -1) {
			usleep(20 * 1000);
			continue;
		}

		// cannot write frames after the last safely captured frame
		if (last_frame_written >= last_frame_captured) {
			if (last_frame_written == last_frame_captured && g_finished_capture) {
				printf("exiting write_to_disk\n");
				pthread_exit(NULL);
			}

			usleep(20 * 1000);
			continue;
		}
	
		uint8_t *video_buf = ring_frame(last_frame_written + 1);

		if (write_mxf) {
			int stride = 720*2;
			uint8_t	*active_video = video_buf + stride * 16;
			ArchiveTimecode vitc;
			ArchiveTimecode ltc;

			// read ltc and vitc in ring buffer
			memcpy(&ltc, video_buf + element_size - sizeof(ArchiveTimecode)*2, sizeof(ArchiveTimecode));
			memcpy(&vitc, video_buf + element_size - sizeof(ArchiveTimecode)*1, sizeof(ArchiveTimecode));

			CHK(write_timecode(mxfout, vitc, ltc));
			CHK(write_video_frame(mxfout, active_video, VIDEO_FRAME_SIZE));

			// convert 32bit audio to 24bit (each frame has 1920 samples)
			uint8_t audioframe24bit[AUDIO_FRAME_SIZE*2];
			audio_32_to_24bit(video_buf + audio12_offset, audioframe24bit);
			CHK(write_audio_frame(mxfout, audioframe24bit, AUDIO_FRAME_SIZE));	// channel 1
			CHK(write_audio_frame(mxfout, audioframe24bit + AUDIO_FRAME_SIZE, AUDIO_FRAME_SIZE));	// channel 2

			audio_32_to_24bit(video_buf + audio34_offset, audioframe24bit);
			CHK(write_audio_frame(mxfout, audioframe24bit, AUDIO_FRAME_SIZE));	// channel 3
			CHK(write_audio_frame(mxfout, audioframe24bit + AUDIO_FRAME_SIZE, AUDIO_FRAME_SIZE));	// channel 4
		}
		else {
			if ( fwrite(video_buf, video_size, 1, fp_video) != 1 ) {
				perror("fwrite to video file");
				exit(1);
			}
	
			if (! no_audio) {
				if ( fwrite(video_buf + audio12_offset, audio_size, 1, fp_audio12) != 1 )
				{
					perror("fwrite to audio 12 file");
				}
				if ( fwrite(video_buf + audio34_offset, audio_size, 1, fp_audio34) != 1 )
				{
					perror("fwrite to audio 34 file");
				}
			}
}
	
		if (verbose > 1)
			printf("write_to_disk: last_frame_written=%d\n", last_frame_written + 1);
	
		PTHREAD_MUTEX_LOCK(&m_last_frame_written)
		g_last_frame_written = last_frame_written + 1;
		PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)
	}
}

static int capture_sv_fifo(sv_handle *sv, sv_fifo *pinput)
{
	int lastdropped = 0;

	while (1) {
		int last_frame_captured;
		int last_frame_written;

		PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
		last_frame_captured = g_last_frame_captured;
		PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)

		PTHREAD_MUTEX_LOCK(&m_last_frame_written)
		last_frame_written = g_last_frame_written;
		PTHREAD_MUTEX_UNLOCK(&m_last_frame_written)

		// Make sure we don't capture too quickly and wrap around the ring buffer
		if (last_frame_captured - last_frame_written >= RING_SIZE - 1) {
			usleep(40 * 1000);
			continue;
		}

		// write frame to sv fifo
		sv_fifo_buffer		*pbuffer;
		int					res;

		// Get sv memmory buffer
		if ((res = sv_fifo_getbuffer(sv, pinput, &pbuffer, NULL, 0)) != SV_OK)
		{
			fprintf(stderr, "failed to sv_fifo_getbuffer(): %d %s\n", res, sv_geterrortext(res));
			exit(1);
		}

		pbuffer->dma.addr = (char *)ring_frame(last_frame_captured + 1);
		pbuffer->dma.size = ring_element_size();
		if (audio12_offset == 0) {
			audio12_offset = (unsigned long)pbuffer->audio[0].addr[0];
			audio34_offset = (unsigned long)pbuffer->audio[0].addr[1];
		}

		// ask the hardware to capture
		SV_CHECK(sv_fifo_putbuffer(sv, pinput, pbuffer, NULL));

		// Read timecode from PALFF (fullframe)
		if (video_height == 592) {
			int stride = 720*2;
			unsigned hh, mm, ss, ff;
			ArchiveTimecode vitc = {0xff,0xff,0xff,0xff};	// initialise with invalid timecode
			ArchiveTimecode ltc = {0xff,0xff,0xff,0xff};
			uint8_t	*full_video = ring_frame(last_frame_captured + 1);

			// Read VITC (line 19 or line 8 here)
			if (readVITC(ring_frame(last_frame_captured + 1) + stride * 8, &hh, &mm, &ss, &ff)) {
				printf("readVITC: %02u:%02u:%02u:%02u", hh, mm, ss, ff);
				vitc.hour = hh;
				vitc.min = mm;
				vitc.sec = ss;
				vitc.frame = ff;
			}
			else
				printf("readVITC failed");

			// Read LTC (line 15 or line 0 here)
			if (readVITC(ring_frame(last_frame_captured + 1) + stride * 0, &hh, &mm, &ss, &ff)) {
				printf("  LTC: %02u:%02u:%02u:%02u\n", hh, mm, ss, ff);
				vitc.hour = hh;
				vitc.min = mm;
				vitc.sec = ss;
				vitc.frame = ff;
			}
			else
				printf("  LTC failed\n");

			// store ltc and vitc in ring buffer
			memcpy(full_video + element_size - sizeof(ArchiveTimecode)*2, &ltc, sizeof(ArchiveTimecode));
			memcpy(full_video + element_size - sizeof(ArchiveTimecode)*1, &vitc, sizeof(ArchiveTimecode));
		}


		// If scanning for start timecode, wait until timecode matches
		if (frame_inpoint != -1) {
			int current_tc_frames = dvs_tc_to_int(pbuffer->timecode.vitc_tc);
			if (frame_inpoint != current_tc_frames) {
				printf("\rwaiting for in point: fr=%6d current=%d (0x%08x)",
					frame_inpoint, current_tc_frames, pbuffer->timecode.vitc_tc);
				fflush(stdout);
				usleep(20 * 1000);			// wait for next frame
				continue;
			}
			// frame inpoint found so turn of frame in point scanning
			frame_inpoint = -1;
		}

		if (verbose && 0)
			printf("fr=%6d tc=0x%08x\n", last_frame_captured + 1,
						pbuffer->timecode.vitc_tc);

		sv_fifo_info info = {0};
		SV_CHECK(sv_fifo_status(sv, pinput, &info));

		if (verbose) {
			char str[64] = {0};
			printf("\rframe=%6d tc=%s (tc=0x%08x tc2=0x%08x) drop=%d",
						last_frame_captured + 1,
						framesToStr( dvs_tc_to_int(pbuffer->timecode.vitc_tc), str ),
						pbuffer->timecode.vitc_tc, pbuffer->timecode.vitc_tc2,
						info.dropped);
			fflush(stdout);
		}
		lastdropped = info.dropped;

		// update shared variable to signal to disk writing thread
		last_frame_captured++;
		PTHREAD_MUTEX_LOCK(&m_last_frame_captured)
		g_last_frame_captured = last_frame_captured;
		PTHREAD_MUTEX_UNLOCK(&m_last_frame_captured)

		// finished intended capture?
		// subtract 1 since last_frame_captured starts counting from 0
		if (last_frame_captured >= frames_to_capture - 1) {
			g_finished_capture = 1;
			return 1;
		}
	}
	return 1;
}

// card number passed as void * (using cast)
static void * sdi_monitor(void *arg)
{
	long				card = (long)arg;
	sv_handle			*sv = a_sv[card];
	sv_fifo				*pinput;
	sv_info				status_info;
	sv_storageinfo		storage_info;
	printf("card = %ld\n", card);

	SV_CHECK( sv_status( sv, &status_info) );

	SV_CHECK( sv_storage_status(sv,
							0,
							NULL,
							&storage_info,
							sizeof(storage_info),
							0) );

	SV_CHECK( sv_fifo_init(	sv,
							&pinput,		// FIFO handle
							TRUE,			// bInput (TRUE for record, FALSE for playback)
							TRUE,			// bShared (TRUE for input/output share memory)
							TRUE,			// bDMA
							FALSE,			// reserved
							0) );			// nFrames (0 means use maximum)

	// allocate ring buffer
	printf("Allocating memory %d (video+audio size) * %d frames = %"PFi64"\n", element_size, RING_SIZE, (int64_t)element_size * RING_SIZE);

	// sv internals need suitably aligned memory, so use valloc not malloc
	ring = (uint8_t*)valloc(element_size * RING_SIZE);
	if (ring == NULL) {
		fprintf(stderr, "valloc failed\n");
		exit(1);
	}

	// initialise mutexes
	pthread_mutex_init(&m_last_frame_written, NULL);
	pthread_mutex_init(&m_last_frame_captured, NULL);

	// Start disk writing thread
	pthread_t	write_thread;
	int err;
	if ((err = pthread_create(&write_thread, NULL, write_to_disk, NULL)) != 0) {
		fprintf(stderr, "Failed to create write thread: %s\n", strerror(err));
		exit(1);
	}

	// Start the fifo.  The dropped counter starts counting dropped frame from now.
	SV_CHECK( sv_fifo_start(sv, pinput) );

	// Loop capturing frames
	capture_sv_fifo(sv, pinput);

	printf("Finished capturing %"PFu64" frames.  Waiting for buffer to be written\n", frames_to_capture);
	if ((err = pthread_join(write_thread, NULL)) != 0) {
		fprintf(stderr, "Failed to join write thread: %s\n", strerror(err));
		exit(1);
	}

	if (write_mxf) {
		PSEFailure pseFailure = {0};
		pseFailure.luminanceFlash = 3000;
		VTRError vtrErrors[1] = { {{0xff,0,0,0},{0,0,0,0},1} };
		const char* d3InfaxDataString = 
            "D3|"
            "D3 preservation programme|"
            "|"
            "2006-02-02|"
            "|"
            "LME1306H|"
            "T|"
            "2006-01-01|"
            "PROGRAMME BACKING COPY|"
            "Bla bla bla|"
            "1732|"
            "DGN377505|"
            "DC193783|"
            "LONPROG";
		InfaxData d3InfaxData;
		parse_infax_data(d3InfaxDataString, &d3InfaxData, 1);
		if (!complete_archive_mxf_file(&mxfout, &d3InfaxData, &pseFailure, 1, vtrErrors, 0)) {
            fprintf(stderr, "Failed to complete writing MXF file\n");
        }

		//if (!update_mxf_file(mxfFilename, newMXFFilename, ltoInfaxDataString)) {
		//	fprintf(stderr, "Failed to update file with LTO Infax data and new filename\n");
		//}
	}

	printf("Finished.  last_frame_written=%d last_frame_captured=%d\n", g_last_frame_written, g_last_frame_captured);
	exit(0);

	return NULL;
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

static void usage_exit(void)
{
	fprintf(stderr, "Usage: record [options] video.uyvy [audio12.wav audio34.wav]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "E.g.   record video.uyvy audio12.wav audio34.wav\n");
	fprintf(stderr, "E.g.   record -n video.uyvy\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -f <int>     number of frames to capture\n");
	fprintf(stderr, "    -n           no audio\n");
	fprintf(stderr, "    --10b        record video in 10bit format\n");
	fprintf(stderr, "    --10bdvs     record video in 10bdvs format\n");
	fprintf(stderr, "    -in          in VITC timecode to capture from\n");
	fprintf(stderr, "    -out         out VITC timecode to capture up to (not including)\n");
	fprintf(stderr, "    -mxf         write MXF file instead of UYVY video and WAV files\n");
	fprintf(stderr, "    -q           quiet (no status messages)\n");
	fprintf(stderr, "    -v           increment verbosity\n");
	fprintf(stderr, "\n");
	exit(1);
}

int main (int argc, char ** argv)
{
	int n, max_cards = 1;

	time_t now;
	struct tm *tm_now;

	now = time ( NULL );
	tm_now = localtime ( &now );

	// process command-line args
	for (n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "-v") == 0)
		{
			verbose++;
		}
		else if (strcmp(argv[n], "-q") == 0)
		{
			verbose = 0;
		}
		else if (strcmp(argv[n], "-n") == 0)
		{
			no_audio = 1;
		}
		else if (strcmp(argv[n], "-mxf") == 0)
		{
			write_mxf = 1;
		}
		else if (strcmp(argv[n], "--10b") == 0)
		{
			video_bit_format = SV_MODE_NBIT_10B;
		}
		else if (strcmp(argv[n], "--10bdvs") == 0)
		{
			video_bit_format = SV_MODE_NBIT_10BDVS;
		}
		else if (strcmp(argv[n], "-f") == 0)
		{
			if (sscanf(argv[n+1], "%"PFu64"", &frames_to_capture) != 1)
			{
				fprintf(stderr, "-f requires integer number of frames\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-in") == 0)
		{
			if (! tcstr_to_frames(argv[n+1], &frame_inpoint) &&
				(sscanf(argv[n+1], "%d", &frame_inpoint) != 1))
			{
				fprintf(stderr, "-in requires timecode string or integer number of frames\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-out") == 0)
		{
			if (frame_inpoint == -1) {
				fprintf(stderr, "must specify -in first before -out\n");
				return 1;
			}
			if (! tcstr_to_frames(argv[n+1], &frame_outpoint) &&
				(sscanf(argv[n+1], "%d", &frame_outpoint) != 1))
			{
				fprintf(stderr, "-out requires timecode string or integer number of frames\n");
				return 1;
			}
			frames_to_capture = frame_outpoint - frame_inpoint + 1;
			n++;
		}
		else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
		{
			usage_exit();
		}
		// Set video and audio files
		else {
			if (! video_file)
				video_file = argv[n];
			else if (! audio12_file)
				audio12_file = argv[n];
			else if (! audio34_file)
				audio34_file = argv[n];
		}
	}

	if (no_audio && !video_file)
		usage_exit();

	if (!no_audio) {
		if (! write_mxf && (!audio12_file || !audio34_file))
			usage_exit();
	}

	//////////////////////////////////////////////////////
	// Attempt to open all sv cards
	//
	// card specified by string of form "PCI,card=n" where n = 0,1,2,3
	//
	int card;
    int componentDepth8Bit = 1;
	for (card = 0; card < max_cards; card++)
	{
		sv_info				status_info;
		char card_str[20] = {0};

		snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);

		int res = sv_openex(&a_sv[card],
							card_str,
							SV_OPENPROGRAM_DEMOPROGRAM,
							SV_OPENTYPE_INPUT,
							0,
							0);
		if (res != SV_OK)
		{
			// typical reasons include:
			//	SV_ERROR_DEVICENOTFOUND
			//	SV_ERROR_DEVICEINUSE
			fprintf(stderr, "card %d: %s\n", card, sv_geterrortext(res));
			continue;
		}

		sv_handle *sv = a_sv[card];

		sv_status( a_sv[card], &status_info);
		video_width = status_info.xsize;
		video_height = status_info.ysize;
		video_size = video_width * video_height *2;

		int videomode = status_info.config;
		int voptmode;
		// Switch to specified video mode
		SV_CHECK( sv_option_get(sv, SV_OPTION_VIDEOMODE, &voptmode) );
		//SV_CHECK( sv_option_set(sv, SV_OPTION_VIDEOMODE, (voptmode & (~SV_MODE_NBIT_MASK)) | video_bit_format) );
		SV_CHECK( sv_option_get(sv, SV_OPTION_VIDEOMODE, &voptmode) );
		printf("videomode = 0x%08x  voptmode=0x%08x\n", videomode, voptmode);

		componentDepth8Bit = 1;
		if ((videomode & SV_MODE_NBIT_MASK) != SV_MODE_NBIT_8B) {
			switch (videomode & SV_MODE_NBIT_MASK) {
			case SV_MODE_NBIT_10B:
				video_size = 4 * video_size / 3;
				componentDepth8Bit = 0;
				break;
			case SV_MODE_NBIT_10BDVS:
				if (write_mxf) {
					fprintf(stderr, "Video mode SV_MODE_NBIT_10BDVS not supported in MXF writer\n");
					return 1;
				}
				video_size = 4 * video_size / 3;
				componentDepth8Bit = 0;
				break;
			case SV_MODE_NBIT_10BDPX:
				if (write_mxf) {
					fprintf(stderr, "Video mode SV_MODE_NBIT_10BDPX not supported in MXF writer\n");
					return 1;
				}
				video_size = 4 * video_size / 3;
				componentDepth8Bit = 0;
				break;
			}
		}
		printf("card[%d] frame is %dx%d (%d bits) %d bytes, assuming input UYVY matches\n", card, video_width, video_height, status_info.nbit, video_size);
	}

	// FIXME - this could be calculated correctly by using a struct for example
	element_size =	video_size		// video frame
					+ 0x8000;		// size of internal structure of dvs dma transfer
									// for 4 channels.  This is greater than 1920 * 4 * 4
									// so we use the last 4 bytes for timecode


	audio_offset = video_size;

	// Setup MXF writer if specified
	if (write_mxf) {
		mxfRational aspect_ratio = {4, 3};
		if (!prepare_archive_mxf_file(video_file, componentDepth8Bit, &aspect_ratio, 4, 0, 1, &mxfout))
    	{
			fprintf(stderr, "Failed to prepare MXF writer\n");
			return 1;
    	}
	}
	else
	{
		// Open output video and audio files
		if ((fp_video = fopen(video_file, "wb")) == NULL)
		{
			perror("fopen output video file");
			return 0;
		}
		if (! no_audio) {
			if ((fp_audio12 = fopen(audio12_file, "wb")) == NULL)
			{
				perror("fopen output audio 1,2 file");
				return 0;
			}
			if ((fp_audio34 = fopen(audio34_file, "wb")) == NULL)
			{
				perror("fopen output audio 3,4 file");
				return 0;
			}
			writeWavHeader(fp_audio12, 32, 2);
			writeWavHeader(fp_audio34, 32, 2);
		}
	}

	// Loop forever capturing and writing to disk
	card = 0;
	sdi_monitor((void *)card);

	return 0;
}
