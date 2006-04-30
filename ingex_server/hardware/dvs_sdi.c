/*
 * $Id: dvs_sdi.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Record multiple SDI inputs to shared memory buffers.
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

#define _XOPEN_SOURCE 600		// for posix_memalign
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>	
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#ifdef __MMX__
#include <mmintrin.h>
#endif

#include "dvs_clib.h"
#include "dvs_fifo.h"


#define SV_CHECK(x) {int res = x; if (res != SV_OK) { fprintf(stderr, "sv call failed=%d  %s line %d\n", res, __FILE__, __LINE__); sv_errorprint(sv,res); cleanup_exit(1, sv); } }

#include <sys/shm.h>
#include "../nexus_control.h"
#include "../utils.h"

// Globals
pthread_t		sdi_thread[4];
sv_handle		*a_sv[4];
int				num_sdi_threads = 0;
NexusControl	*p_control = NULL;
uint8_t			*ring[4] = {0};
int				control_id, ring_id[4];
int				element_size = 0, audio_offset = 0, audio_size = 0;

static int verbose = 1;

static void cleanup_shared_mem(void)
{
	int				i, id;
	struct shmid_ds shm_desc;

	for (i = 0; i < (1 + num_sdi_threads); i++)
	{
		id = shmget(9 + i, sizeof(*p_control), 0444);
		if (id == -1)
		{
			fprintf(stderr, "No shmem id for key %d\n", 9 + i);
			continue;
		}
		if (shmctl(id, IPC_RMID, &shm_desc) == -1)
		{
			perror("shmctl(id, IPC_RMID):");
		}
	}
}

static void cleanup_exit(int res, sv_handle *sv)
{
    printf("cleaning up\n");
	// attempt to avoid lockup when stopping playback
	if (sv != NULL)
	{
		// sv_fifo_wait does nothing for record, only useful for playback
    	//sv_fifo_wait(sv, poutput);
		sv_close(sv);
	}

	// delete shared memory
	cleanup_shared_mem();

	usleep(100 * 1000);		// 0.1 seconds

	printf("done - exiting\n");
    exit(res);
}

static void catch_sigusr1(int sig_number)
{
	// toggle a flag
}

static void catch_sigint(int sig_number)
{
    printf("\nReceived signal %d - ", sig_number);
	cleanup_exit(0, NULL);
}

#ifdef __MMX__
static void uyvy_to_yuv420(int width, int height, uint8_t *input, uint8_t *output)
{
	__m64 chroma_mask = _mm_set_pi8(255, 0, 255, 0, 255, 0, 255, 0);
	__m64 luma_mask = _mm_set_pi8(0, 255, 0, 255, 0, 255, 0, 255);
	uint8_t *y_comp = output;
	uint8_t *u_comp = output + width * height;
	uint8_t *v_comp = u_comp + (int)((width * height)/4);
	int i, j;

	/* Do the y component */
	 uint8_t *tmp = input;
	for (j = 0; j < height; ++j)
	{
		for (i = 0; i < width*2;  i += 16)
		{
			//__m64 m1 = _mm_and_si64 (*(__m64 *)input, luma_mask);
			//__m64 m2 = _mm_and_si64 (*(__m64 *)(input+8), luma_mask);
			//__m64 m2 = _mm_set_pi8 (0, 0, 0, 0, 0, 0, 0, 0);
			//*(__m64 *)y_comp = _mm_packs_pu16 (m2, m1);
			__m64 m0 = *(__m64 *)input;
			__m64 m2 = _mm_srli_si64(m0, 8);
			__m64 m3 = _mm_slli_si64(m0, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m0 = m2;
			__m64 m1 = *(__m64 *)(input+8);
			m2 = _mm_srli_si64(m1, 8);
			m3 = _mm_slli_si64(m1, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m1 = m2;
			*(__m64 *)y_comp = _mm_packs_pu16 (m0, m1);

			y_comp += 8;
			input += 16;
		}
	}
	/* Do the chroma components */
	input = tmp;
	for (j = 0; j < height; ++j)
	{
		/* Skip every odd line to subsample to yuv 4:2:0 */
		if (j %2)
		{
			input += width*2;
			continue;
		}
		for (i = 0; i < width*2;  i += 16)
		{
			__m64 m1 = _mm_unpacklo_pi8 (*(__m64 *)input, *(__m64 *)(input+8));
			__m64 m2 = _mm_unpackhi_pi8 (*(__m64 *)input, *(__m64 *)(input+8));

			__m64 m3 = _mm_unpacklo_pi8 (m1, m2);
			__m64 m4 = _mm_unpackhi_pi8 (m1, m2);
			//*(__m64 *)u_comp = _mm_unpacklo_pi8 (m1, m2);
			//*(__m64 *)v_comp = _mm_unpackhi_pi8 (m1, m2);
			memcpy (u_comp, &m3, 4);
			memcpy (v_comp, &m4, 4);
			u_comp += 4;
			v_comp += 4;
			input += 16;
		}
	}
}
#else
// Convert UYVY -> Planar YUV 4:2:0 (fourcc=I420 or YV12)
// U0 Y0 V0 Y1   U2 Y2 V2 Y3   ->   Y0 Y1 Y2... U0 U2... V0 V2...
// but U and V are skipped every second line
static void uyvy_to_yuv420(int width, int height, uint8_t *input, uint8_t *output)
{
	int i;

	// Copy Y plane as is
	for (i = 0; i < width*height; i++)
	{
		output[i] = input[ i*2 + 1 ];
	}

	// Copy U & V planes, downsampling in vertical direction
	// by simply skipping every second line.
	// Each U or V plane is 1/4 the size of the Y plane.
	int i_macropixel = 0;
	for (i = 0; i < width*height / 4; i++)
	{
		output[width*height + i] = input[ i_macropixel*4 ];			// U
		output[width*height*5/4 + i] = input[ i_macropixel*4 + 2 ];	// V

		// skip every second line
		if (i_macropixel % (width) == (width - 1))
			i_macropixel += width/2;

		i_macropixel++;
	}
}
#endif

// Read available physical memory stats and
// allocate shared memory ring buffers accordingly
static int allocate_shared_buffers(int num_cards)
{
	long long	k_shmmax = 0;
	int			ring_len, i;
	FILE		*procfp;

	// Linux specific way to read shmmax value
	if ((procfp = fopen("/proc/sys/kernel/shmmax", "r")) == NULL)
	{
		perror("fopen(/proc/sys/kernel/shmmax)");
		return 0;
	}
	fscanf(procfp, "%lld", &k_shmmax);
	fclose(procfp);

	// calculate reasonable ring buffer length
	// reduce by small number 5 to leave a little room for other shared mem
	ring_len = k_shmmax / num_cards / element_size - 5;
	printf("shmmax=%lld (%.3fMiB) calculated ring_len=%d\n", k_shmmax, k_shmmax / (1024*1024.0), ring_len);

	printf("element_size=%d ring_len=%d (%.2f secs) (total=%lld)\n", element_size, ring_len, ring_len / 25.0, (long long)element_size * ring_len);
	if (ring_len < 10)
	{
		printf("ring_len=%d too small - try increasing shmmax:\n", ring_len);
		printf("  echo 1073741824 >> /proc/sys/kernel/shmmax\n");
		return 0;
	}

	// Allocate memory for control structure which is fixed size
	if ((control_id = shmget(9, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
	{
		if (errno == EEXIST)
		{
			fprintf(stderr, "shmget: shm segment exists, deleting all related segments\n");
			cleanup_shared_mem();
			if ((control_id = shmget(9, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
			{
				perror("shmget control key 9");
				return 0;
			}
		}
		else
		{
			perror("shmget control key 9");
			return 0;
		}
	}
	p_control = (NexusControl*)shmat(control_id, NULL, 0);

	p_control->cards = num_cards;
	p_control->elementsize = element_size;
	p_control->audio12_offset = audio_offset;
	p_control->audio34_offset = audio_offset + 0x4000;
	p_control->audio_size = audio_size;
	p_control->tc_offset = audio_offset + audio_size - sizeof(int);
	p_control->ringlen = ring_len;

	// Allocate multiple element ring buffers containing video + audio + tc
	//
	// by default 32MB is available (40 PAL frames), or change it with:
	//  root# echo  104857600 >> /proc/sys/kernel/shmmax  # 120 frms (100MB)
	//  root# echo  201326592 >> /proc/sys/kernel/shmmax  # 240 frms (192MB)
	//  root# echo 1073741824 >> /proc/sys/kernel/shmmax  # 1294 frms (1GB)

	// key for variable number of ring buffers can be 10, 11, 12, 13
	for (i = 0; i < num_cards; i++)
	{
		int key = i + 10;
		ring_id[i] = shmget(key, element_size * ring_len, IPC_CREAT | IPC_EXCL | 0666);
		if (ring_id[i] == -1)	/* shm error */
		{
			int save_errno = errno;
			fprintf(stderr, "Attemp to shmget for key %d: ", key);
			perror("shmget");
			if (save_errno == EEXIST)
				fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
			if (save_errno == EINVAL)
				fprintf(stderr, "You asked for too much?\n");
	
			return 0;
		}

		ring[i] = (uint8_t *)shmat(ring_id[i], NULL, 0);
		p_control->card[i].key = key;
		p_control->card[i].lastframe = -1;
		p_control->card[i].hwdrop = 0;

		// initialise mutex in shared memory
		if (pthread_mutex_init(&p_control->card[i].m_lastframe, NULL) != 0)
			fprintf(stderr, "Mutex init error\n");

		// Set ring frames to black - too slow!
		// memset(ring[i], 0x80, element_size * ring_len);
	}

	return 1;
}

// Table to quickly convert DVS style timecodes to integer timecodes
static int dvs_to_int[96] = {
 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0, 
10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0, 
20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0, 
30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0, 
40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0, 
50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0
};

static int dvs_tc_to_int(int tc)
{
	// E.g. 0x09595924
	// E.g. 0x10000000
	int hours = dvs_to_int[(tc & 0xff000000) >> 24];
	int mins  = dvs_to_int[(tc & 0x00ff0000) >> 16];
	int secs  = dvs_to_int[(tc & 0x0000ff00) >> 8];
	int frames= dvs_to_int[(tc & 0x000000ff)];

	int tc_as_total =	hours * (25*60*60) +
						mins * (25*60) +
						secs * (25) +
						frames;

	return tc_as_total;
}

//
// write_picture()
//
// Gets a frame from the SDI FIFO and stores it to memory ring buffer
// (also can write to disk as a debugging option)
static int write_picture(int card, sv_handle *sv, sv_fifo *poutput)
{
	sv_fifo_buffer		*pbuffer;
	int					res;
	int					ring_len = p_control->ringlen;
	NexusBufCtl			*pc = &(p_control->card[card]);

	// Get sv memmory buffer
	if ((res = sv_fifo_getbuffer(sv, poutput, &pbuffer, NULL, 0)) != SV_OK)
	{
		return res;
	}

	// dma buffer is structured as follows
	//
	// 0x0000 video frame					offset= 0
	// 0x0000 audio ch. 0, 1 interleaved	offset= pbuffer->audio[0].addr[0]
	// 0x0000 audio ch. 2, 3 interleaved	offset= pbuffer->audio[0].addr[1]
	//
	pbuffer->dma.addr = (char *)
				ring[card] + element_size * ((pc->lastframe+1) % ring_len);
	pbuffer->dma.size = element_size;			// video + audio + tc + yuv

	// read frame from DVS card
	// reception of a SIGUSR1 can sometimes cause this to fail
	// If it fails we should restart fifo.
	if (sv_fifo_putbuffer(sv, poutput, pbuffer, NULL) != SV_OK)
	{
		fprintf(stderr, "sv_fifo_putbuffer failed, restarting fifo\n");
		SV_CHECK( sv_fifo_reset(sv, poutput) );
		SV_CHECK( sv_fifo_start(sv, poutput) );
	}

	// Handle buggy field order
	// Bad field order causes nitc_tc and vitc2 to be swapped.
	// If the high bit is set on vitc use vitc2 instead.
	int vitc_to_use = pbuffer->timecode.vitc_tc;
	if ((unsigned)pbuffer->timecode.vitc_tc >= 0x80000000) {
		vitc_to_use = pbuffer->timecode.vitc_tc2;
	}

	// Copy timecode into last 4 bytes of element_size
	// This is a bit sneaky but saves maintaining a separate ring buffer
	int tc_as_int = dvs_tc_to_int(vitc_to_use);
	memcpy(ring[card] + element_size * ((pc->lastframe+1) % ring_len)
			+ p_control->tc_offset,
				&tc_as_int, sizeof(int));

	// Downconvert to 4:2:0 YUV buffer located just after audio
	uyvy_to_yuv420(720, 576,
					ring[card] + element_size * ((pc->lastframe+1) % ring_len),
					ring[card] + element_size * ((pc->lastframe+1) % ring_len)
					+ (audio_offset + audio_size));

	if (verbose > 1)
	{
		// timecode:
		//   vitc_tc=268441105 vitc_tc2=-1879042543   for "10:00:16:11"
		//
		int last_tc = *(int *)(ring[card] + element_size * ((pc->lastframe) % ring_len)
								+ p_control->tc_offset);
		int tc_diff = tc_as_int - last_tc;
		printf("card %d: lastframe=%6d vitc_tc=%8x vitc_tc2=%8x tc_int=%d last_tc=%d tc_diff=%d %s\n",
					card, pc->lastframe,
					pbuffer->timecode.vitc_tc,
					pbuffer->timecode.vitc_tc2,
					tc_as_int,
					last_tc,
					tc_diff,
					tc_diff != 1 ? "!" : "");
		fflush(stdout);
	}

	// signal frame is now ready
	PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
	pc->lastframe++;
	PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

	return SV_OK;
}

// card number passed as void * (using cast)
static void * sdi_monitor(void *arg)
{
	int					card = (int)arg;
	sv_handle			*sv = a_sv[card];
	sv_fifo				*poutput;
	sv_info				status_info;
	sv_storageinfo		storage_info;


	SV_CHECK( sv_status( sv, &status_info) );

	SV_CHECK( sv_storage_status(sv,
							0,
							NULL,
							&storage_info,
							sizeof(storage_info),
							0) );

	SV_CHECK( sv_fifo_init(	sv,
							&poutput,		// FIFO handle
							TRUE,			// bInput (FALSE for playback)
							TRUE,			// bShared (TRUE for input/output share memory)
							TRUE,			// bDMA
							FALSE,			// reserved
							0) );			// nFrames (0 means use maximum)

	SV_CHECK( sv_fifo_start(sv, poutput) );

	int last_res = -1;
	while (1)
    {
		int res;

		if ((res = write_picture(card, sv, poutput)) != SV_OK)
		{
			// Display error only when things change
			if (res != last_res) {
				logTF("card %d: failed to capture video: (%d) %s\n", card, res,
					res == SV_ERROR_INPUT_VIDEO_NOSIGNAL ? "INPUT_VIDEO_NOSIGNAL" : sv_geterrortext(res));
				// reset FIFO if error indicates FIFO problem
				if (res == SV_ERROR_FIFO_PUTBUFFER)
				{
					logTF("SV_ERROR_FIFO_PUTBUFFER: restarting fifo\n");
					SV_CHECK( sv_fifo_reset(sv, poutput) );
					SV_CHECK( sv_fifo_start(sv, poutput) );
				}
			}
			last_res = res;
			usleep( 2 * 1000 * 1000);		// 2 second poll
			continue;
		}
		if (res != last_res) {
			logTF("card %d: Video signal OK\n", card);
		}
		last_res = res;
		usleep( 20 * 1000);					// 0.02 second poll
    }

	return NULL;
}


static void usage_exit(void)
{
	fprintf(stderr, "Usage: nexus_sdi [-v] [-q] [-h] [-c max_cards]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "E.g.   nexus_sdi -c 2\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -q           Quiet operation\n");
	fprintf(stderr, "\n");
	exit(1);
}

int main (int argc, char ** argv)
{
	int					n, max_cards = 4;

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
		else if (strcmp(argv[n], "-h") == 0)
		{
			usage_exit();
		}
		else if (strcmp(argv[n], "-c") == 0)
		{
			if (sscanf(argv[n+1], "%d", &max_cards) != 1 ||
				max_cards > 4 || max_cards <= 0)
			{
				fprintf(stderr, "-c requires integer maximum card number <= 4\n");
				return 1;
			}
			n++;
		}
	}

	openLogFile("nexus_sdi.log");

	// Install signal handlers to do clean exit
	if (signal(SIGINT, catch_sigint) == SIG_ERR)
	{
		perror("signal(SIGINT)");
		return 1;
	}
	if (signal(SIGTERM, catch_sigint) == SIG_ERR)
	{
		perror("signal(SIGTERM)");
		return 1;
	}
	if (signal(SIGHUP, catch_sigint) == SIG_ERR)
	{
		perror("signal(SIGHUP)");
		return 1;
	}
	if (signal(SIGUSR1, catch_sigusr1) == SIG_ERR)
	{
		perror("signal(SIGUSR1)");
		return 1;
	}

	//////////////////////////////////////////////////////
	// Attempt to open all sv cards
	//
	// card specified by string of form "PCI,card=n" where n = 0,1,2,3
	//
	int card;
	for (card = 0; card < max_cards; card++)
	{
		char card_str[20] = {0};

		snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);

		int res = sv_openex(&a_sv[card],
							card_str,
							SV_OPENPROGRAM_DEMOPROGRAM,
							SV_OPENTYPE_DELAYEDOPEN|SV_OPENTYPE_MASK_DEFAULT,
							0,
							0);
		if (res != SV_OK)
		{
			// typical reasons include:
			//	SV_ERROR_DEVICENOTFOUND
			//	SV_ERROR_DEVICEINUSE
			logTF("card %d: %s\n", card, sv_geterrortext(res));
			continue;
		}
		num_sdi_threads++;
	}

	if (num_sdi_threads < 1)
	{
		logTF("Error: No SDI monitor threads created.  Exiting\n");
		return 1;
	}

	// FIXME - this could be calculated correctly by using a struct for example
	element_size =	720*576*2		// video frame
					+ 0x8000		// size of internal structure of dvs dma transfer
									// for 4 channels.  This is greater than 1920 * 4 * 4
									// so we use the last 4 bytes for timecode

					+ 720*576*3/2;	// YUV 4:2:0 video buffer

	audio_offset = 720*576*2;
	audio_size = 0x8000;

	if (! allocate_shared_buffers(num_sdi_threads))
	{
		return 1;
	}

	for (card = 0; card < num_sdi_threads; card++)
	{
		int err;
		logTF("card %d: starting capture thread\n", card);

		if ((err = pthread_create(&sdi_thread[card], NULL, sdi_monitor, (void *)card)) != 0)
        {
            logTF("Failed to create sdi_monitor thread: %s\n", strerror(err));
            return 1;
        }
	}

	// SDI monitor threads never terminate.
	// Loop forever monitoring status of threads for logging purposes
	while (1)
	{
		usleep( 2 * 1000 * 1000);
	}
}
