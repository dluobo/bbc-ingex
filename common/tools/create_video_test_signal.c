/*
 * $Id: create_video_test_signal.c,v 1.1 2009/12/17 15:50:42 john_f Exp $
 *
 * Create file with UYVY video test signal
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "video_test_signals.h"


static const int DEFAULT_NUM_FRAMES = 1;
static const int DEFAULT_WIDTH = 720;
static const int DEFAULT_HEIGHT = 576;


typedef enum
{
    COLOUR_BARS_SIGNAL,
    BLACK_SIGNAL,
    RANDOM_SIGNAL,
    NO_VIDEO_SIGNAL
} TestSignal;



static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [options] <output>\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h|--help        Print this usage message and exit\n");
    fprintf(stderr, "  --bars           Colour bars test signal (this is the default)\n");
    fprintf(stderr, "  --black          Black image test signal\n");
    fprintf(stderr, "  --random         Random image test signal\n");
    fprintf(stderr, "  --no-video       'No video' test signal\n");
    fprintf(stderr, "  --dur <frames>   Set the number of video frames (default %d)\n", DEFAULT_NUM_FRAMES);
    fprintf(stderr, "  --width          Image width (default %d)\n", DEFAULT_WIDTH);
    fprintf(stderr, "  --height         Image height (default %d)\n", DEFAULT_HEIGHT);
}

int main(int argc, const char **argv)
{
    int cmdln_index;
    TestSignal test_signal = COLOUR_BARS_SIGNAL;
    int dur = DEFAULT_NUM_FRAMES;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    const char *output_filename = NULL;
    FILE *output;
    unsigned char *output_buffer;
    int output_buffer_size;
    size_t num_write;
    int count;
    
    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--bars") == 0)
        {
            test_signal = COLOUR_BARS_SIGNAL;
        }
        else if (strcmp(argv[cmdln_index], "--black") == 0)
        {
            test_signal = BLACK_SIGNAL;
        }
        else if (strcmp(argv[cmdln_index], "--random") == 0)
        {
            test_signal = RANDOM_SIGNAL;
        }
        else if (strcmp(argv[cmdln_index], "--no-video") == 0)
        {
            test_signal = NO_VIDEO_SIGNAL;
        }
        else if (strcmp(argv[cmdln_index], "--dur") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &dur) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse dur '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--width") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &width) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse width '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--height") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &height) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse height '%s'\n", argv[cmdln_index]);
            }
        }
        else
        {
            break;
        }
    }
    
    if (cmdln_index != argc - 1) {
        if (cmdln_index < argc &&
            (strcmp(argv[cmdln_index], "-h") == 0 ||
                strcmp(argv[cmdln_index], "--help") == 0))
        {
            usage(argv[0]);
            return 0;
        }
        
        usage(argv[0]);
        return 1;
    }
    output_filename = argv[cmdln_index];
    
    
    output = fopen(output_filename, "wb");
    if (!output) {
        fprintf(stderr, "Failed to open output '%s': %s\n", output_filename, strerror(errno));
        return 1;
    }

    
    output_buffer_size = width * 2 * height;
    output_buffer = malloc(output_buffer_size);
    
    switch (test_signal)
    {
        case COLOUR_BARS_SIGNAL:
            uyvy_color_bars(width, height, output_buffer);
            break;
        case BLACK_SIGNAL:
            uyvy_black_frame(width, height, output_buffer);
            break;
        case RANDOM_SIGNAL:
            uyvy_random_frame(width, height, output_buffer);
            break;
        case NO_VIDEO_SIGNAL:
            uyvy_no_video_frame(width, height, output_buffer);
            break;
    }
    
    
    for (count = 0; count < dur; count++) {
        num_write = fwrite(output_buffer, output_buffer_size, 1, output);
        if (num_write != 1) {
            if (ferror(output))
                fprintf(stderr, "Failed to write: %s\n", strerror(errno));
            break;
        }
    }
    
    
    free(output_buffer);
    
    fclose(output);
    
    return 0;
}

