/*
 * $Id: convert_10bit_video.c,v 1.1 2009/12/17 15:49:42 john_f Exp $
 *
 * Utility to convert 10-bit UYVY to 8-bit UYVY and vice versa
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

#include "video_conversion_10bits.h"


static const int DEFAULT_WIDTH = 720;
static const int DEFAULT_HEIGHT = 576;



static void pack12(unsigned short *pIn, unsigned char *pOut)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		*pOut++ = *pIn & 0xff;
		*pOut = (*pIn++ >> 8) & 0x03;
		*pOut++ += (*pIn & 0x3f) << 2;
		*pOut = (*pIn++ >> 6) & 0x0f;
		*pOut++ += (*pIn & 0x0f) << 4;
		*pOut++ = (*pIn++ >> 4) & 0x3f;
	}
}

static void inject_bits(unsigned char *buffer, int width, int height, unsigned char bits)
{
    unsigned char output_inject[16];
    unsigned short input_inject[12];
    unsigned char *output_ptr;
    int i, j;
    
    for (i = 0; i < 12; i++)
        input_inject[i] = bits;
    pack12(input_inject, output_inject);
    
    output_ptr = buffer;
    for (i = 0; i < width * height * 2; i += 12) {
        for (j = 0; j < 16; j++)
            *output_ptr++ |= output_inject[j];
    }
}

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [options] <input> <output>\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h|--help        Print this usage message and exit\n");
    fprintf(stderr, "  --nodither       Truncate 10-bit to 8-bit (default is to use dither)\n");
    fprintf(stderr, "  --to8bit         10-bit input is converted to 8-bit output (default is 8-bit to 10-bit conversion)\n");
    fprintf(stderr, "  --width          Image width (default %d)\n", DEFAULT_WIDTH);
    fprintf(stderr, "  --height         Image height (default %d)\n", DEFAULT_HEIGHT);
    fprintf(stderr, "  --inject <val>   Inject <val> into the 2 lsb when converting to 10-bit\n");
}

int main(int argc, const char **argv)
{
    int cmdln_index;
    int nodither = 0;
    int to8bit = 0;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    unsigned char inject = 0;
    const char *input_filename = NULL;
    const char *output_filename = NULL;
    FILE *input;
    FILE *output;
    unsigned char *input_buffer;
    int input_buffer_size;
    unsigned char *output_buffer;
    int output_buffer_size;
    size_t num_read, num_write;
    
    for (cmdln_index = 1; cmdln_index < argc - 2; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--nodither") == 0)
        {
            nodither = 1;
        }
        else if (strcmp(argv[cmdln_index], "--to8bit") == 0)
        {
            to8bit = 1;
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
        else if (strcmp(argv[cmdln_index], "--inject") == 0)
        {
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%c", &inject) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse inject '%s'\n", argv[cmdln_index]);
            }

            inject &= 0x3;
        }
        else
        {
            break;
        }
    }

    
    if (cmdln_index != argc - 2) {
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
    output_filename = argv[cmdln_index + 1];
    
    
    input = fopen(input_filename, "rb");
    if (!input) {
        fprintf(stderr, "Failed to open input '%s': %s\n", input_filename, strerror(errno));
        return 1;
    }
    output = fopen(output_filename, "wb");
    if (!output) {
        fprintf(stderr, "Failed to open output '%s': %s\n", output_filename, strerror(errno));
        return 1;
    }

    
    if (to8bit) {
        input_buffer_size = (width + 5) / 6 * 16 * height;
        input_buffer = malloc(input_buffer_size);
        output_buffer_size = width * 2 * height;
        output_buffer = malloc(output_buffer_size);
        
        while (1) {
            num_read = fread(input_buffer, input_buffer_size, 1, input);
            if (num_read != 1) {
                if (ferror(input))
                    fprintf(stderr, "Failed to read: %s\n", strerror(errno));
                break;
            }
            
            if (nodither)
                ConvertFrame10to8(output_buffer, input_buffer, width * 2, (width + 5) / 6 * 16, width, height);
            else
                DitherFrame(output_buffer, input_buffer, width * 2, (width + 5) / 6 * 16, width, height);
            
            num_write = fwrite(output_buffer, output_buffer_size, 1, output);
            if (num_write != 1) {
                if (ferror(output))
                    fprintf(stderr, "Failed to write: %s\n", strerror(errno));
                break;
            }
        }
        
    } else {
        input_buffer_size = width * 2 * height;
        input_buffer = malloc(input_buffer_size);
        output_buffer_size = (width + 5) / 6 * 16 * height;
        output_buffer = malloc(output_buffer_size);

        while (1) {
            num_read = fread(input_buffer, input_buffer_size, 1, input);
            if (num_read != 1) {
                if (ferror(input))
                    fprintf(stderr, "Failed to read: %s\n", strerror(errno));
                break;
            }
            
            ConvertFrame8to10(output_buffer, input_buffer, (width + 5) / 6 * 16, width * 2, width, height);
            if (inject > 0)
                inject_bits(output_buffer, width, height, inject);
            
            num_write = fwrite(output_buffer, output_buffer_size, 1, output);
            if (num_write != 1) {
                if (ferror(output))
                    fprintf(stderr, "Failed to write: %s\n", strerror(errno));
                break;
            }
        }
    }
    
    
    free(input_buffer);
    free(output_buffer);
    
    fclose(input);
    fclose(output);
    
    return 0;
}

