// compile with:
// gcc -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -O3 -o save_mem save_mem.c
//
//
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

#include "nexus_control.h"


int verbose = 1;

static void usage_exit(void)
{
    fprintf(stderr, "Usage: nexus_set_source [options] source_name0 [source_name1, ...]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -v               increase verbosity\n");
    fprintf(stderr, "\n");
    exit(1);
}

extern int main(int argc, char *argv[])
{
    int             control_id;
    NexusControl    *pctl = NULL;
    int             num_source_names = 0;
    const char*     source_name[MAX_CHANNELS];

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
        else {
            source_name[num_source_names] = argv[n];
            num_source_names++;
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

    pctl = (NexusControl*)shmat(control_id, NULL, 0);   // attach with read-write (flags=0)
    if (verbose)
        printf("connected to pctl\n");

    int i;
    if (verbose) {
        printf("  channels=%d elementsize=%d ringlen=%d width,height=%dx%d fr=%d/%d\n",
                pctl->channels,
                pctl->elementsize,
                pctl->ringlen,
                pctl->width,
                pctl->height,
                pctl->frame_rate_numer,
                pctl->frame_rate_denom);
        printf("  pri_format=%d sec_format=%d a12_offset=%d a34_offset=%d audio_size=%d\n",
                pctl->pri_video_format,
                pctl->sec_video_format,
                pctl->audio12_offset,
                pctl->audio34_offset,
                pctl->audio_size);
        printf("  frame_data_offset=%d\n", pctl->frame_data_offset);
        /*
        printf("  signal_ok_offset=%d ltc_offset=%d vitc_offset=%d sec_video_offset=%d\n",
                pctl->signal_ok_offset,
                pctl->ltc_offset,
                pctl->vitc_offset,
                pctl->sec_video_offset);
        */

        for (i = 0; i < pctl->channels; i++) {
            printf("  channel[%d]: lastframe=%d sourcename='%s'\n", i, pctl->channel[i].lastframe, pctl->channel[i].source_name);
        }
    }


    for (i = 0; i < num_source_names; i++)
    {
        NexusBufCtl *pc;
        pc = &pctl->channel[i];

        if (i == pctl->channels) {
            printf("Too many source names specified for available channels = %d\n", pctl->channels);
            break;
        }
        printf("%d: changing source_name=\"%s\" to \"%s\"\n", i, pc->source_name, source_name[i]);
        PTHREAD_MUTEX_LOCK(&pctl->m_source_name_update);
        strncpy(pc->source_name, source_name[i], sizeof(pc->source_name)-1);
        pc->source_name[sizeof(pc->source_name)-1] = '\0';      // ensure string is terminated
        pctl->source_name_update++;
        PTHREAD_MUTEX_UNLOCK(&pctl->m_source_name_update);
    }

    return 0;
}
