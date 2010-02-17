/*
 * $Id: create_audio_test_signal.c,v 1.1 2010/02/17 15:56:50 philipn Exp $
 *
 * Create PCM audio test signal
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
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
#include <math.h>

#define MAX_CHANNELS        32


typedef struct
{
    float dbfs;
    float freq;
    float fac1, fac2;
    unsigned int bytes_per_sample;
} Input;


static const float DEFAULT_DURATION = 1.0;
static const float DEFAULT_SAMPLING_RATE = 48000.0;
static const unsigned int DEFAULT_BITS_PER_SAMPLE = 16;



static inline int write_tone(FILE *file, Input *tone, int64_t position)
{
    int32_t sample = tone->fac1 * sin(position * tone->fac2);
    
    switch (tone->bytes_per_sample)
    {
        case 1:
            if (fputc((sample >> 24) & 0xff, file) == EOF)
                return 0;
            break;
        case 2:
            if (fputc((sample >> 16) & 0xff, file) == EOF)
                return 0;
            if (fputc((sample >> 24) & 0xff, file) == EOF)
                return 0;
            break;
        case 3:
            if (fputc((sample >> 8) & 0xff, file) == EOF)
                return 0;
            if (fputc((sample >> 16) & 0xff, file) == EOF)
                return 0;
            if (fputc((sample >> 24) & 0xff, file) == EOF)
                return 0;
            break;
        case 4:
            if (fputc(sample & 0xff, file) == EOF)
                return 0;
            if (fputc((sample >> 8) & 0xff, file) == EOF)
                return 0;
            if (fputc((sample >> 16) & 0xff, file) == EOF)
                return 0;
            if (fputc((sample >> 24) & 0xff, file) == EOF)
                return 0;
            break;
        default:
            break; // not reached
    }
    
    return 1;
}

static void init_tone(Input *tone, float sampling_rate, unsigned int bytes_per_sample)
{
    tone->fac1 = 2147483647 * pow(10, tone->dbfs / 20.0);
    tone->fac2 = 2 * M_PI * tone->freq / sampling_rate;
    tone->bytes_per_sample = bytes_per_sample;
}

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [options] <<inputs>> <output>\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h|--help                Print this usage message and exit\n");
    fprintf(stderr, "  --dur <sec>              Duration in seconds. Default %.1f\n", DEFAULT_DURATION);
    fprintf(stderr, "  --rate <hz>              Sampling rate. Default %.1f\n", DEFAULT_SAMPLING_RATE);
    fprintf(stderr, "  --bps <bits>             Bits per sample. Default %d\n", DEFAULT_BITS_PER_SAMPLE);
    fprintf(stderr, "Inputs:\n");
    fprintf(stderr, "  --tone <freq> <dbfs>     Tone signal with freq (Hz) and power (dbFS)\n");
}

int main(int argc, const char **argv)
{
    int cmdln_index;
    float dur = DEFAULT_DURATION;
    float sampling_rate = DEFAULT_SAMPLING_RATE;
    unsigned int bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
    unsigned int bytes_per_sample = (bits_per_sample + 7) / 8;
    const char *output_filename = NULL;
    FILE *output;
    Input input[MAX_CHANNELS];
    int num_inputs = 0;
    int i, s;
    int64_t sample_count;
    
    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--dur") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%f", &dur) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse dur '%s'\n", argv[cmdln_index]);
                return 1;
            }
        }
        else if (strcmp(argv[cmdln_index], "--rate") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%f", &sampling_rate) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse rate '%s'\n", argv[cmdln_index]);
                return 1;
            }
        }
        else if (strcmp(argv[cmdln_index], "--bps") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%u", &bits_per_sample) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse bps '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (bits_per_sample > 32) {
                fprintf(stderr, "> 32 bits per sample not supported\n");
                return 1;
            }
            bytes_per_sample = (bits_per_sample + 7) / 8;
        }
        else if (strcmp(argv[cmdln_index], "--tone") == 0)
        {
            if (cmdln_index + 2 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (num_inputs >= MAX_CHANNELS) {
                fprintf(stderr, "Maximum channels/inputs %d exceeded\n", MAX_CHANNELS);
                return 1;
            }
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%f", &input[num_inputs].freq) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse tone '%s'\n", argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
            if (sscanf(argv[cmdln_index], "%f", &input[num_inputs].dbfs) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Failed to parse tone '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (input[num_inputs].dbfs > 0.0) {
                fprintf(stderr, "dbFS > 0.0 not possible\n");
                return 1;
            }
            num_inputs++;
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
    if (num_inputs == 0) {
        usage(argv[0]);
        fprintf(stderr, "No inputs\n");
        return 0;
    }
    
    output_filename = argv[cmdln_index];
    
    for (i = 0; i < num_inputs; i++)
        init_tone(&input[i], sampling_rate, bytes_per_sample);
    
    sample_count = (int64_t)(sampling_rate * dur + 0.5);
    
    
    output = fopen(output_filename, "wb");
    if (!output) {
        fprintf(stderr, "Failed to open output '%s': %s\n", output_filename, strerror(errno));
        return 1;
    }
    
    
    for (s = 0; s < sample_count; s++) {
        for (i = 0; i < num_inputs; i++) {
            if (!write_tone(output, &input[i], s)) {
                fprintf(stderr, "Failed to write: %s\n", strerror(errno));
                return 1;
            }
        }
    }
    
    
    fclose(output);
    
    return 0;
}

