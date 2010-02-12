/*
 * $Id: test_ffmpeg_encoder_av.c,v 1.1 2010/02/12 17:18:20 john_f Exp $
 *
 * Test ffmpeg encoder av
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
#include <inttypes.h>

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/swscale.h>
#else
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#endif

#include "ffmpeg_encoder_av.h"

#define MAX_AUDIO_INPUTS        16


typedef enum
{
    YUV420_VIDEO_FORMAT,
    YUV422_VIDEO_FORMAT,
    UYVY_VIDEO_FORMAT,
} VideoFormat;



static void convert_audio_samples(unsigned char *input, int num_samples, int bytes_per_sample, int nch, short *output)
{
    int s, c;
    
    /* crude conversion */

    if (bytes_per_sample == 1) {
        for (s = 0; s < num_samples; s++) {
            for (c = 0; c < nch; c++)
                output[s * nch + c] = (short)(int16_t)(((uint16_t)input[s * nch + c]) << 8);
        }
    }
    else if (bytes_per_sample == 2) {
        for (s = 0; s < num_samples; s++) {
            for (c = 0; c < nch; c++)
                output[s * nch + c] = (short)(int16_t)(((uint16_t)input[s * nch * 2 + c * 2]) |
                                                      (((uint16_t)input[s * nch * 2 + c * 2 + 1]) << 8));
        }
    }
    else if (bytes_per_sample == 3) {
        for (s = 0; s < num_samples; s++) {
            for (c = 0; c < nch; c++)
                output[s * nch + c] = (short)(int16_t)(((uint16_t)input[s * nch * 3 + c * 3 + 1]) |
                                                      (((uint16_t)input[s * nch * 3 + c * 3 + 2]) << 8));
        }
    }
    else { /* bytes_per_sample == 4 */
        for (s = 0; s < num_samples; s++) {
            for (c = 0; c < nch; c++)
                output[s * nch + c] = (short)(int16_t)(((uint16_t)input[s * nch * 4 + c * 4 + 2]) |
                                                      (((uint16_t)input[s * nch * 4 + c * 4 + 3]) << 8));
        }
    }
}

static int require_conversion(VideoFormat input_video_format, int input_width, int input_height,
                              ffmpeg_encoder_av_resolution_t output_format)
{
    switch (output_format)
    {
        case FF_ENCODER_RESOLUTION_DVD:
            return input_video_format != YUV420_VIDEO_FORMAT || input_width != 720 || input_height != 576;
        case FF_ENCODER_RESOLUTION_MPEG4_MOV:
            return input_video_format != YUV420_VIDEO_FORMAT || input_width != 720 || input_height != 576;
        case FF_ENCODER_RESOLUTION_DV25_MOV:
            return input_video_format != YUV420_VIDEO_FORMAT || input_width != 720 || input_height != 576;
        case FF_ENCODER_RESOLUTION_DV50_MOV:
            return input_video_format != YUV422_VIDEO_FORMAT || input_width != 720 || input_height != 576;
        case FF_ENCODER_RESOLUTION_DV100_MOV:
            return input_video_format != YUV422_VIDEO_FORMAT || input_width != 1920 || input_height != 1080;
    }
    
    return 0; /* for the compiler */
}

static void convert_video(struct SwsContext **convert_context,
                          VideoFormat input_video_format, unsigned char *video_input_buffer,
                          int input_width, int input_height,
                          ffmpeg_encoder_av_resolution_t output_format, unsigned char *video_output_buffer,
                          int output_width, int output_height)
{
    AVPicture input_pict, output_pict;
    enum PixelFormat input_pix_fmt, output_pix_fmt;
    
    switch (input_video_format)
    {
        case YUV420_VIDEO_FORMAT:
            input_pix_fmt = PIX_FMT_YUV420P;
            break;
        case YUV422_VIDEO_FORMAT:
            input_pix_fmt = PIX_FMT_YUV422P;
            break;
        case UYVY_VIDEO_FORMAT:
            input_pix_fmt = PIX_FMT_UYVY422;
            break;
    }
    
    switch (output_format)
    {
        case FF_ENCODER_RESOLUTION_DVD:
            output_pix_fmt = PIX_FMT_YUV420P;
            break;
        case FF_ENCODER_RESOLUTION_MPEG4_MOV:
            output_pix_fmt = PIX_FMT_YUV420P;
            break;
        case FF_ENCODER_RESOLUTION_DV25_MOV:
            output_pix_fmt = PIX_FMT_YUV420P;
            break;
        case FF_ENCODER_RESOLUTION_DV50_MOV:
            output_pix_fmt = PIX_FMT_YUV422P;
            break;
        case FF_ENCODER_RESOLUTION_DV100_MOV:
            output_pix_fmt = PIX_FMT_YUV422P;
            break;
    }
    
    avpicture_fill(&input_pict, video_input_buffer, input_pix_fmt, input_width, input_height);
    avpicture_fill(&output_pict, video_output_buffer, output_pix_fmt, output_width, output_height);
    
    (*convert_context) = sws_getCachedContext((*convert_context),
                                              input_width, input_height, input_pix_fmt,
                                              output_width, output_height, output_pix_fmt,
                                              SWS_FAST_BILINEAR,
                                              NULL, NULL, NULL);

    sws_scale((*convert_context),
              input_pict.data, input_pict.linesize,
              0, input_height,
              output_pict.data, output_pict.linesize);
}


static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s <<options>> <<inputs>> <av output>\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --format <name>    Output format: DVD, MPEG4MOV, DV25MOV, DV50MOV, DV100MOV. Default is DV25MOV\n");
    fprintf(stderr, "  --start <tc>       Start timecode; frame count or hh:mm:ss:ff. Default is 0\n");
    fprintf(stderr, "  --size <WxH>       Picture dimensions. Default is 720x576, except DV100MOV which is 1920x1080\n");
    fprintf(stderr, "  --notwide          4:3 aspect ratio. Default is wide aspect ratio 16:9\n");
    fprintf(stderr, "  --uyvy             Video input is UYVY 4:2:2. Default is YUV 4:2:0 planar\n");
    fprintf(stderr, "  --yuv422           Video input is YUV 4:2:2 planar. Default is YUV 4:2:0 planar\n");
    fprintf(stderr, "  --bps <num>        Number of bits per audio sample. Default is 16\n");
    fprintf(stderr, "  --nch <num>        Number of channels per audio input. Default is 1\n");
    fprintf(stderr, "  --threads <num>    Number of FFmpeg threads. Default 4, except 0 for DVD and MPEG4MOV\n");
    fprintf(stderr, "Inputs:\n");
    fprintf(stderr, "  -v <filename>      Video input file\n");
    fprintf(stderr, "  [-a <filename>]*   Zero or more audio input files\n");
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv)
{
    ffmpeg_encoder_av_resolution_t output_format = FF_ENCODER_RESOLUTION_DV25_MOV;
    int start = 0;
    int hour, min, sec, frame;
    int notwide = 0;
    VideoFormat input_video_format = YUV420_VIDEO_FORMAT;
    int nch = 1;
    int bps = 16;
    int bytes_per_sample = 2;
    int num_ffmpeg_threads = -1;
    const char *video_filename = NULL;
    const char *audio_filenames[MAX_AUDIO_INPUTS];
    int num_audio_inputs = 0;
    const char *output_filename = NULL;
    int cmdln_index;
    FILE *video_file;
    FILE *audio_files[MAX_AUDIO_INPUTS];
    int i;
    ffmpeg_encoder_av_t *encoder;
    unsigned char *video_input_buffer;
    int video_input_frame_size = 0;
    unsigned char *video_conversion_buffer = NULL;
    int require_video_conversion = 0;
    unsigned char *video_input;
    unsigned char *audio_input_buffer;
    short *audio_16bit;
    struct SwsContext *convert_context = NULL;
    int frame_count;
    int input_width = 0, input_height = 0;
    int output_width = 0, output_height = 0;
    
    
    for (cmdln_index = 1; cmdln_index + 1 < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "--format") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (strcmp(argv[cmdln_index + 1], "DVD") == 0) {
                output_format = FF_ENCODER_RESOLUTION_DVD;
            } else if (strcmp(argv[cmdln_index + 1], "MPEG4MOV") == 0) {
                output_format = FF_ENCODER_RESOLUTION_MPEG4_MOV;
            } else if (strcmp(argv[cmdln_index + 1], "DV25MOV") == 0) {
                output_format = FF_ENCODER_RESOLUTION_DV25_MOV;
            } else if (strcmp(argv[cmdln_index + 1], "DV50MOV") == 0) {
                output_format = FF_ENCODER_RESOLUTION_DV50_MOV;
            } else if (strcmp(argv[cmdln_index + 1], "DV100MOV") == 0) {
                output_format = FF_ENCODER_RESOLUTION_DV100_MOV;
            } else {
                usage(argv[0]);
                fprintf(stderr, "Unknown format '%s'\n", argv[cmdln_index + 1]);
                return 1;
            }
            
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--start") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d:%d:%d:%d", &hour, &min, &sec, &frame) != 4) {
                start = hour * 60 * 60 * 25 + min * 60 * 25 + sec * 25 + frame;
            } else if (sscanf(argv[cmdln_index + 1], "%d", &start) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--size") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%dx%d", &input_width, &input_height) != 2) {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--notwide") == 0)
        {
            notwide = 1;
        }
        else if (strcmp(argv[cmdln_index], "--uyvy") == 0)
        {
            input_video_format = UYVY_VIDEO_FORMAT;
        }
        else if (strcmp(argv[cmdln_index], "--yuv422") == 0)
        {
            input_video_format = YUV422_VIDEO_FORMAT;
        }
        else if (strcmp(argv[cmdln_index], "--nch") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d", &nch) != 1 || nch < 0 || nch > 16) {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--bps") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d", &bps) != 1 || bps < 0 || bps > 32) {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            bytes_per_sample = (bps + 7) / 8;
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--threads") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d", &num_ffmpeg_threads) != 1) {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-v") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (video_filename) {
                usage(argv[0]);
                fprintf(stderr, "Multiple video inputs not supported\n");
                return 1;
            }
            video_filename = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-a") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (num_audio_inputs > MAX_AUDIO_INPUTS) {
                usage(argv[0]);
                fprintf(stderr, "Maximum audio inputs (%d) exceeded\n", MAX_AUDIO_INPUTS);
                return 1;
            }
            audio_filenames[num_audio_inputs] = argv[cmdln_index + 1];
            num_audio_inputs++;
            cmdln_index++;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdln_index]);
            return 1;
        }
    }
    
    if (cmdln_index >= argc) {
        usage(argv[0]);
        fprintf(stderr, "Missing output filename\n");
        return 1;
    }
    if (!video_filename) {
        usage(argv[0]);
        fprintf(stderr, "Missing video filename\n");
        return 1;
    }
    
    
    /* initializations */
    
    if (input_width == 0) {
        switch (output_format)
        {
            case FF_ENCODER_RESOLUTION_DVD:
            case FF_ENCODER_RESOLUTION_MPEG4_MOV:
            case FF_ENCODER_RESOLUTION_DV25_MOV:
            case FF_ENCODER_RESOLUTION_DV50_MOV:
                input_width = 720;
                input_height = 576;
                break;
            case FF_ENCODER_RESOLUTION_DV100_MOV:
                input_width = 1920;
                input_height = 1080;
                break;
        }
    }

    switch (input_video_format)
    {
        case YUV420_VIDEO_FORMAT:
            video_input_frame_size = input_width * input_height * 3 / 2;
            break;
        case YUV422_VIDEO_FORMAT:
        case UYVY_VIDEO_FORMAT:
            video_input_frame_size = input_width * input_height * 2;
            break;
    }

    switch (output_format)
    {
        case FF_ENCODER_RESOLUTION_DVD:
        case FF_ENCODER_RESOLUTION_MPEG4_MOV:
        case FF_ENCODER_RESOLUTION_DV25_MOV:
        case FF_ENCODER_RESOLUTION_DV50_MOV:
            output_width = 720;
            output_height = 576;
            break;
        case FF_ENCODER_RESOLUTION_DV100_MOV:
            output_width = 1920;
            output_height = 1080;
            break;
    }

    if (num_ffmpeg_threads < 0) {
        switch (output_format)
        {
            case FF_ENCODER_RESOLUTION_DVD:
            case FF_ENCODER_RESOLUTION_MPEG4_MOV:
                /* num_ffmpeg_threads > 1 was found to cause a seg fault for ffmpeg v0.5 */
                num_ffmpeg_threads = 0;
                break;
            case FF_ENCODER_RESOLUTION_DV25_MOV:
            case FF_ENCODER_RESOLUTION_DV50_MOV:
            case FF_ENCODER_RESOLUTION_DV100_MOV:
                num_ffmpeg_threads = 4;
                break;
        }
    }
    
    output_filename = argv[cmdln_index];
    
    
    /* open video and audio files */
    
    video_file = fopen(video_filename, "rb");
    if (!video_file) {
        fprintf(stderr, "Failed to open video input '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }
    for (i = 0; i < num_audio_inputs; i++) {
        audio_files[i] = fopen(audio_filenames[i], "rb");
        if (!audio_files[i]) {
            fprintf(stderr, "Failed to open audio input '%s': %s\n", argv[1], strerror(errno));
            return 1;
        }
    }

    
    /* allocate buffers */
    
    video_input_buffer = malloc(video_input_frame_size + FF_INPUT_BUFFER_PADDING_SIZE);
    audio_input_buffer = malloc(1920 * nch * bytes_per_sample);
    audio_16bit = (short*)malloc(1920 * nch * sizeof(short));
    
    require_video_conversion = require_conversion(input_video_format, input_width, input_height, output_format);
    if (require_video_conversion)
        video_conversion_buffer = malloc(output_width * output_height * 2 + FF_INPUT_BUFFER_PADDING_SIZE);

    
    /* init encoder */
    encoder = ffmpeg_encoder_av_init(output_filename, output_format, !notwide, start, num_ffmpeg_threads,
                                     num_audio_inputs, nch);
    
    frame_count = 0;
    while (1) {
        /* read and encode video */
        
        if (fread(video_input_buffer, video_input_frame_size, 1, video_file) != 1) {
            if (ferror(video_file))
                fprintf(stderr, "Failed to read from video input: %s\n", strerror(errno));
            break;
        }
        if (require_video_conversion) {
            convert_video(&convert_context, input_video_format, video_input_buffer, input_width, input_height,
                          output_format, video_conversion_buffer, output_width, output_height);
            video_input = video_conversion_buffer;
        } else {
            video_input = video_input_buffer;
        }
        ffmpeg_encoder_av_encode_video(encoder, video_input);
        
        
        /* read and encode audio */
        
        for (i = 0; i < num_audio_inputs; i++) {
            if (fread(audio_input_buffer, 1920 * bytes_per_sample * nch, 1, audio_files[i]) != 1) {
                if (ferror(audio_files[i]))
                    fprintf(stderr, "Failed to read from audio input %d: %s\n", i, strerror(errno));
                break;
            }
            convert_audio_samples(audio_input_buffer, 1920, bytes_per_sample, nch, audio_16bit);
            ffmpeg_encoder_av_encode_audio(encoder, 0, 1920, audio_16bit);
        }
        
        frame_count++;
        if (frame_count % 100 == 0)
            printf("Frame %d\n", frame_count);
    }
    
    printf("Total frames = %d\n", frame_count);
    
    
    /* close and free */
    
    ffmpeg_encoder_av_close(encoder);
    
    if (convert_context)
        sws_freeContext(convert_context);
    
    free(video_input_buffer);
    if (video_conversion_buffer)
        free(video_conversion_buffer);
    free(audio_input_buffer);
    
    fclose(video_file);
    for (i = 0; i <  num_audio_inputs; i++)
        fclose(audio_files[i]);
    
    return 0;
}

