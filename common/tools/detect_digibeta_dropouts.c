/*
 * $Id: detect_digibeta_dropouts.c,v 1.1 2010/01/12 17:08:16 john_f Exp $
 *
 * Utility to detect digibeta dropouts
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include <digibeta_dropout.h>
#include <video_conversion_10bits.h>
#include <YUV_frame.h>


static const int DEFAULT_WIDTH = 720;
static const int DEFAULT_HEIGHT = 576;
static const int DEFAULT_X_OFFSET = 0;
static const int DEFAULT_Y_OFFSET = 0;
static const int DEFAULT_THRESHOLD = 30;
static const long DEFAULT_FRAME_OFFSET = 0;


static char g_timecode_str[64];

static char* position_to_timecode(long position)
{
    sprintf(g_timecode_str, "%02ld:%02ld:%02ld:%02ld",
            position / (60 * 60 * 25),
            (position % (60 * 60 * 25)) / (60 * 25),
            ((position % (60 * 60 * 25)) % (60 * 25)) / 25,
            ((position % (60 * 60 * 25)) % (60 * 25)) % 25);
    
    return g_timecode_str;
}

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [options] <input>\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h|--help        Print this usage message and exit\n");
    fprintf(stderr, "  --10bit          Image is 10-bit UYVY (default 8-bit UYVY)\n");
    fprintf(stderr, "  --width          Image width (default %d)\n", DEFAULT_WIDTH);
    fprintf(stderr, "  --height         Image height (default %d)\n", DEFAULT_HEIGHT);
    fprintf(stderr, "  --f1xoff <val>   Field 1 X offset (default %d)\n", DEFAULT_X_OFFSET);
    fprintf(stderr, "  --f2xoff <val>   Field 2 X offset (default %d)\n", DEFAULT_X_OFFSET);
    fprintf(stderr, "  --f1yoff <val>   Field 1 Y offset (default %d)\n", DEFAULT_Y_OFFSET);
    fprintf(stderr, "  --f2yoff <val>   Field 2 Y offset (default %d)\n", DEFAULT_Y_OFFSET);
    fprintf(stderr, "  --thresh <val>   Dropout strength threshold (default %d)\n", DEFAULT_THRESHOLD);
    fprintf(stderr, "  --offset <val>   Frame offset (default %ld)\n", DEFAULT_FRAME_OFFSET);
    fprintf(stderr, "  --dur <val>      Analysis duration (default all frames)\n");
}

int main(int argc, const char **argv)
{
    int cmdln_index;
    int is10Bit = 0;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    int x_offset[2] = {DEFAULT_X_OFFSET, DEFAULT_X_OFFSET};
    int y_offset[2] = {DEFAULT_Y_OFFSET, DEFAULT_Y_OFFSET};
    int threshold = DEFAULT_THRESHOLD;
    long offset = DEFAULT_FRAME_OFFSET;
    long duration = -1;
    const char *input_filename = NULL;
    FILE *input;
    unsigned char *input_buffer;
    int input_buffer_size;
    unsigned char *input_buffer_8bit;
    int input_buffer_8bit_size;
    size_t num_read;
    int *workspace;
    long position;
    YUV_frame frame;
    
    for (cmdln_index = 1; cmdln_index < argc - 1; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--10bit") == 0)
        {
            is10Bit = 1;
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
        else if (strcmp(argv[cmdln_index], "--f1xoff") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &x_offset[0]) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse field 1 x offset '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--f2xoff") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &x_offset[1]) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse field 2 x offset '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--f1yoff") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &y_offset[0]) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse field 1 y offset '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--f2yoff") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &y_offset[1]) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse field 2 y offset '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--thresh") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%d", &threshold) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse threshold '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--offset") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%ld", &offset) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse offset '%s'\n", argv[cmdln_index]);
            }
        }
        else if (strcmp(argv[cmdln_index], "--dur") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%ld", &duration) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse duration '%s'\n", argv[cmdln_index]);
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
    input_filename = argv[cmdln_index];
    
    
    input = fopen(input_filename, "rb");
    if (!input) {
        fprintf(stderr, "Failed to open input '%s': %s\n", input_filename, strerror(errno));
        return 1;
    }

    if (is10Bit) {
        input_buffer_size = (width + 5) / 6 * 16 * height;
        input_buffer = malloc(input_buffer_size);
        input_buffer_8bit_size = width * 2 * height;
        input_buffer_8bit = malloc(input_buffer_8bit_size);
    } else {
        input_buffer_size = width * 2 * height;
        input_buffer = malloc(input_buffer_size);
        input_buffer_8bit_size = input_buffer_size;
        input_buffer_8bit = input_buffer;
    }
    
    YUV_frame_from_buffer(&frame, input_buffer_8bit, width, height, UYVY);
    
    workspace = malloc(sizeof(int) * width * height);
    
    if (offset > 0) {
        if (fseek(input, offset * input_buffer_size, SEEK_SET) != 0) {
            fprintf(stderr, "Failed to seek to offset %ld: %s\n", offset, strerror(errno));
            return 1;
        }
    }
        
    position = offset;
    while (duration < 0 || position - offset < duration)
    {
        num_read = fread(input_buffer, input_buffer_size, 1, input);
        if (num_read != 1) {
            if (ferror(input))
                fprintf(stderr, "Failed to read: %s\n", strerror(errno));
            break;
        }
        
        if (is10Bit)
            DitherFrame(input_buffer_8bit, input_buffer, width * 2, (width + 5) / 6 * 16, width, height);

        dropout_result result[2];
        result[0] = digibeta_dropout(&frame, x_offset[0], 0 + (y_offset[0] * 2), workspace);
        result[1] = digibeta_dropout(&frame, x_offset[0], 1 + (y_offset[1] * 2), workspace);
        
        if (result[0].strength >= threshold && result[0].strength > result[1].strength) {
            printf("pos=%s (%ld), strength=%d, field=1, sixth=%d, seq=%d\n",
                   position_to_timecode(position), position, result[0].strength, result[0].sixth, result[0].seq);
        }
        else if (result[1].strength >= threshold) {
            printf("pos=%s (%ld), strength=%d, field=2, sixth=%d, seq=%d\n",
                   position_to_timecode(position), position, result[1].strength, result[1].sixth, result[1].seq);
        }
        
        position++;
    }
    
    free(input_buffer);
    if (input_buffer_8bit != input_buffer)
        free(input_buffer_8bit);
    free(workspace);
    
    fclose(input);
    
    return 0;
}

