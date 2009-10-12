/*
 * $Id: nexus_xv.c,v 1.5 2009/10/12 15:11:46 john_f Exp $
 *
 * Utility to display current video frame on X11 display.
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
#include <video_conversion.h>


#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

// little-endian specific
#  define VLC_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )

#define X11_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )
#define X112VLC_FOURCC( i ) \
        VLC_FOURCC( i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, \
                    (i >> 24) & 0xff )


int verbose = 1;

static int XVideoGetPort( Display *p_display,
                          int32_t i_chroma,
                          int i_requested_adaptor)      // -1 for autoscan
{
    XvAdaptorInfo *p_adaptor;
    unsigned int i;
    int i_adaptor;
    unsigned int i_num_adaptors;
    int i_selected_port;

    switch( XvQueryExtension( p_display, &i, &i, &i, &i, &i ) )
    {
        case Success:
            break;

        case XvBadExtension:
            fprintf(stderr, "XvBadExtension\n" );
            return -1;

        case XvBadAlloc:
            fprintf(stderr, "XvBadAlloc\n" );
            return -1;

        default:
            fprintf(stderr, "XvQueryExtension failed\n" );
            return -1;
    }

    switch( XvQueryAdaptors( p_display,
                             DefaultRootWindow( p_display ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            fprintf(stderr, "XvBadExtension for XvQueryAdaptors\n" );
            return -1;

        case XvBadAlloc:
            fprintf(stderr, "XvBadAlloc for XvQueryAdaptors\n" );
            return -1;

        default:
            fprintf(stderr, "XvQueryAdaptors failed\n" );
            return -1;
    }

    i_selected_port = -1;

    for( i_adaptor = 0; i_adaptor < (int)i_num_adaptors; ++i_adaptor )
    {
        XvImageFormatValues *p_formats;
        int i_format, i_num_formats;
        int i_port;

        /* If we requested an adaptor and it's not this one, we aren't
         * interested */
        if( i_requested_adaptor != -1 && i_adaptor != i_requested_adaptor )
        {
            continue;
        }

        /* If the adaptor doesn't have the required properties, skip it */
        if( !( p_adaptor[ i_adaptor ].type & XvInputMask ) ||
            !( p_adaptor[ i_adaptor ].type & XvImageMask ) )
        {
            continue;
        }

        /* Check that adaptor supports our requested format... */
        p_formats = XvListImageFormats( p_display,
                                        p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0;
             i_format < i_num_formats && ( i_selected_port == -1 );
             i_format++ )
        {
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;

            /* Matching chroma? */
            if( p_formats[ i_format ].id != i_chroma )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < (int)(p_adaptor[i_adaptor].base_id
                                   + p_adaptor[i_adaptor].num_ports) )
                   && ( i_selected_port == -1 );
                 i_port++ )
            {
                if( XvGrabPort( p_display, i_port, CurrentTime )
                     == Success )
                {
                    i_selected_port = i_port;
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            if (verbose)
                fprintf( stderr, "adaptor %i, port %i, format 0x%x (%4.4s) %s\n",
                     i_adaptor, i_selected_port, p_formats[ i_format ].id,
                     (char *)&p_formats[ i_format ].id,
                     ( p_formats[ i_format ].format == XvPacked ) ?
                         "packed" : "planar" );

            /* Make sure XV_AUTOPAINT_COLORKEY is set */
            p_attr = XvQueryPortAttributes( p_display,
                                            i_selected_port,
                                            &i_num_attributes );

            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                if( !strcmp( p_attr[i_attr].name, "XV_AUTOPAINT_COLORKEY" ) )
                {
                    const Atom autopaint =
                        XInternAtom( p_display,
                                     "XV_AUTOPAINT_COLORKEY", False );
                    XvSetPortAttribute( p_display,
                                        i_selected_port, autopaint, 1 );
                    break;
                }
            }

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
        }

        if( p_formats != NULL )
        {
            XFree( p_formats );
        }

    }

    if( i_num_adaptors > 0 )
    {
        XvFreeAdaptorInfo( p_adaptor );
    }

    if( i_selected_port == -1 )
    {
        int i_chroma_tmp = X112VLC_FOURCC( i_chroma );
        if( i_requested_adaptor == -1 )
        {
            fprintf(stderr, "no free XVideo port found for format\n"
                      "0x%.8x (%4.4s)", i_chroma_tmp, (char*)&i_chroma_tmp );
        }
        else
        {
            fprintf(stderr, "XVideo adaptor %i does not have a free\n"
                      "XVideo port for format 0x%.8x (%4.4s)",
                      i_requested_adaptor, i_chroma_tmp, (char*)&i_chroma_tmp );
        }
    }

    return i_selected_port;
}

static void XVideoReleasePort( Display *p_display, int i_port )
{
    XvUngrabPort( p_display, i_port, CurrentTime );
}

static char *framesToStr(int tc, char *s)
{
    int frames = tc % 25;
    int hours = (int)(tc / (60 * 60 * 25));
    int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
    int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

    if (tc < 0 || frames < 0 || hours < 0 || minutes < 0 || seconds < 0
        || hours > 59 || minutes > 59 || seconds > 59 || frames > 24)
        sprintf(s, "             ");
        //sprintf(s, "* INVALID *  ");
    else
        sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    return s;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: nexus_xv [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c channel view video on given channel [default 0]\n");
    fprintf(stderr, "    -p         view primary video [default secondary]\n");
    exit(1);
}

extern int main(int argc, char *argv[])
{
    int             shm_id, control_id;
    uint8_t         *ring[MAX_CHANNELS];
    NexusControl    *pctl = NULL;
    int             channelnum = 0, sec_video = 1;

    int n;
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-q") == 0)
        {
            verbose = 0;
        }
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (n+1 >= argc ||
                sscanf(argv[n+1], "%d", &channelnum) != 1 ||
                channelnum > 7 || channelnum < 0)
            {
                fprintf(stderr, "-c requires integer channel number {0...7}\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-p") == 0)
        {
            sec_video = 0;
        }
        else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
        {
            usage_exit();
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

    if (verbose)
        printf("  channels=%d elementsize=%d ringlen=%d\n",
                pctl->channels,
                pctl->elementsize,
                pctl->ringlen);

    if (channelnum+1 > pctl->channels)
    {
        printf("  channelnum not available\n");
        return 1;
    }

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
            printf("  attached to channel[%d]\n", i);
    }

    int yuv420_mode = 0;
    int format_convert = 0;
    int width, height;

    if (sec_video) {
        if (pctl->sec_video_format != Format420PlanarYUV &&
            pctl->sec_video_format != Format420PlanarYUVShifted) {
            format_convert = 1;
            printf("Using software convert to UYVY from secondary format %s\n", nexus_capture_format_name(pctl->sec_video_format));
        }
        switch (pctl->sec_video_format) {
            case Format420PlanarYUV:
            case Format420PlanarYUVShifted:
                yuv420_mode = 1;
                break;
            case FormatNone:
                printf("No secondary video\n");
                return 1;
                break;
            default:
                break;
        }
        width = pctl->sec_width;
        height = pctl->sec_height;
    }
    else {
        if (pctl->pri_video_format != Format422UYVY) {
            format_convert = 1;
            printf("Using software convert to UYVY from primary format %s\n", nexus_capture_format_name(pctl->pri_video_format));
        }
        yuv420_mode = 0;
        width = pctl->width;
        height = pctl->height;
    }
    printf("Picture is %dx%d\n", width, height);

    int                     xvport;
    Display                 *display;
    Window                  window;
    GC                      gc;
    XvImage                 *yuv_image = NULL;
    char                    title_string[256] = {0};

        if ((display = XOpenDisplay(NULL)) == NULL)
        {
            fprintf(stderr, "Cannot open Display.\n");
            exit(1);
        }
    
        /* Check that we have access to an XVideo port providing this chroma    */
        /* Commonly supported chromas: YV12, I420, UYVY, YUY2                   */
        xvport = XVideoGetPort( display,
                yuv420_mode ? X11_FOURCC('I','4','2','0') :
                            X11_FOURCC('U','Y','V','Y'),
                                    -1);
        if ( xvport < 0 )
        {
            fprintf(stderr, "Cannot find an xv port for requested chroma format\n");
            exit(1);
        }
    
        /* Setup X parameters */
        XSetWindowAttributes    x_attr;
        x_attr.background_pixel = 0;
        x_attr.backing_store = Always;
        x_attr.event_mask = ExposureMask | StructureNotifyMask;
    
        window = XCreateWindow(display, DefaultRootWindow(display),
                    0, 0, width, height,
                    0, 0, InputOutput, 0,
                    CWBackingStore | CWBackPixel | CWEventMask, &x_attr);
        snprintf(title_string, sizeof(title_string)-1, "Nexus Monitor Input %d", channelnum);
        XStoreName(display, window, title_string);
        XSelectInput(display, window, StructureNotifyMask);
        XMapWindow(display, window);
    
        XEvent                  event;
        do {
            XNextEvent(display, &event);
        } while (event.type != MapNotify || event.xmap.event != window);
    
        gc = XCreateGC(display, window, 0, 0);

        // Uncompressed video will be written into video_buffer[0] by libdv
        XShmSegmentInfo         yuv_shminfo;
        yuv_image = XvShmCreateImage(display, xvport,
                        yuv420_mode ? X11_FOURCC('I','4','2','0') : X11_FOURCC('U','Y','V','Y'),
                        0, width, height, &yuv_shminfo);
        yuv_shminfo.shmid = shmget(IPC_PRIVATE, yuv_image->data_size,
                                    IPC_CREAT | 0777);
        yuv_image->data = (char*)shmat(yuv_shminfo.shmid, 0, 0);
        yuv_shminfo.shmaddr = yuv_image->data;
        yuv_shminfo.readOnly = False;
    
        if (!XShmAttach(display, &yuv_shminfo))
        {
            fprintf(stderr, "XShmAttach failed\n");
            exit(1);
        }


    NexusBufCtl *pc;
    pc = &pctl->channel[channelnum];
    int tc, ltc;
    int last_saved = -1;
    int frame_size = width*height*2;
    int video_offset = 0;
    char tcstr[32], ltcstr[32];
    if (yuv420_mode) {
        frame_size = width*height*3/2;
        video_offset = pctl->sec_video_offset;
    }

    while (1)
    {
        if (last_saved == pc->lastframe)
        {
            usleep(20 * 1000);      // 0.020 seconds = 50 times a sec
            continue;
        }

        NexusFrameData * nfd = (NexusFrameData *)(ring[channelnum] + pctl->elementsize * (pc->lastframe % pctl->ringlen) + pctl->frame_data_offset);

        tc = nfd->vitc;
        ltc = nfd->ltc;
        /*
        tc = *(int*)(ring[channelnum] + pctl->elementsize *
                                    (pc->lastframe % pctl->ringlen)
                            + pctl->vitc_offset);
        ltc = *(int*)(ring[channelnum] + pctl->elementsize *
                                    (pc->lastframe % pctl->ringlen)
                            + pctl->ltc_offset);
        */

        uint8_t *in_video = ring[channelnum] + video_offset +
                                    pctl->elementsize *
                                    (pc->lastframe % pctl->ringlen);

        if (format_convert) {
            yuv422_to_uyvy(width, height, 0, in_video, (uint8_t*)yuv_image->data);
        }
        else {
            memcpy(yuv_image->data, in_video, frame_size);
        }

        XvShmPutImage(display, xvport, window, gc, yuv_image,
                        0, 0, yuv_image->width, yuv_image->height,
                        0, 0, yuv_image->width, yuv_image->height,
                        False);
        XSync(display, False);

        if (verbose) {
            printf("\rcam%d lastframe=%d  tc=%10d  %s   ltc=%11d  %s ",
                    channelnum, pc->lastframe,
                    tc, framesToStr(tc, tcstr), ltc, framesToStr(ltc, ltcstr));
            fflush(stdout);
        }

        last_saved = pc->lastframe;
    }

    XVideoReleasePort(display, xvport);

    return 0;
}

