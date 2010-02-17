/*
 * $Id: test_d10mxfop1awriter.cpp,v 1.2 2010/02/17 16:04:24 philipn Exp $
 *
 * Test D10 MXF OP-1A writer
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#define __STDC_FORMAT_MACROS 1

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <inttypes.h>

#include <memory>

#include "D10MXFOP1AWriter.h"
#include "libMXF++/MXFException.h"

using namespace std;
using namespace mxfpp;


static const uint32_t DEFAULT_AUDIO_BPS = 24;
static const D10MXFOP1AWriter::D10BitRate DEFAULT_VIDEO_BIT_RATE = D10MXFOP1AWriter::D10_BIT_RATE_50;
static const uint32_t DEFAULT_VIDEO_FRAME_SIZE = 250000;


static bool parse_timecode(const char *tc_str, D10MXFOP1AWriter::D10SampleRate sample_rate,
                           int64_t *tc)
{
    uint16_t rounded_timecode_base;
    int hour, min, sec, frame;
    
    if (sample_rate == D10MXFOP1AWriter::D10_SAMPLE_RATE_625_50I)
        rounded_timecode_base = 25;
    else
        rounded_timecode_base = 30;
    
    if (sscanf(tc_str, "%d:%d:%d:%d", &hour, &min, &sec, &frame) != 4)
        return false;
    
    
    *tc = hour * 60 * 60 * rounded_timecode_base +
          min * 60 * rounded_timecode_base +
          sec * rounded_timecode_base +
          frame;

    return true;
}

static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <<options>> <output file>\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -h | --help           Show usage and exit\n");
    fprintf(stderr, " -l <rate>             Video sample rate. Either 25 or 30. Default 25\n");
    fprintf(stderr, " -q <bps>              Audio quantization bits per sample. Either 16 or 24. Default %d\n", DEFAULT_AUDIO_BPS);
    fprintf(stderr, " -b <size>             Video bit rate. Either 30, 40 or 50. Default 50\n");
    fprintf(stderr, " -s <size>             Video frame size. Default %d\n", DEFAULT_VIDEO_FRAME_SIZE);
    fprintf(stderr, " -t <hh:mm:ss:ff>      Start timecode. Default 00:00:00:00\n");
    fprintf(stderr, " -d                    Drop frame timecode. Default non-drop frame\n");
    fprintf(stderr, " -r <n:d>              Image aspect ratio. Either 4:3 or 16:9. Default 16:9\n");
    fprintf(stderr, " -v <filename>         Video filename\n");
    fprintf(stderr, " [-a <filename>]*      Zero or more audio filenames\n");
}

int main(int argc, const char** argv)
{
    const char *video_filename = 0;
    const char *audio_filename[8];
    int num_audio_files = 0;
    const char *out_filename = 0;
    D10MXFOP1AWriter::D10SampleRate sample_rate = D10MXFOP1AWriter::D10_SAMPLE_RATE_625_50I;
    uint32_t audio_bps = DEFAULT_AUDIO_BPS;
    uint32_t audio_bytes_ps = (audio_bps + 7) / 8;
    D10MXFOP1AWriter::D10BitRate video_bit_rate = DEFAULT_VIDEO_BIT_RATE;
    uint32_t video_frame_size = DEFAULT_VIDEO_FRAME_SIZE;
    int64_t start_timecode = 0;
    const char *start_timecode_str = 0;
    bool drop_frame = false;
    mxfRational aspect_ratio = {16, 9};
    int value, num, den;
    int cmdln_index;
    
    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++)
    {
        if (strcmp(argv[cmdln_index], "--help") == 0 ||
            strcmp(argv[cmdln_index], "-h") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "-l") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%u", &value) != 1 ||
                (value != 25 && value != 30))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            if (value == 25)
                sample_rate = D10MXFOP1AWriter::D10_SAMPLE_RATE_625_50I;
            else
                sample_rate = D10MXFOP1AWriter::D10_SAMPLE_RATE_525_60I;
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-q") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%u", &value) != 1 ||
                (value != 16 && value != 24))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            audio_bps = value;
            audio_bytes_ps = (audio_bps + 7) / 8;
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-b") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%u", &value) != 1 ||
                (value != 30 && value != 40 && value != 50))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            if (value == 30)
                video_bit_rate = D10MXFOP1AWriter::D10_BIT_RATE_30;
            else if (value == 40)
                video_bit_rate = D10MXFOP1AWriter::D10_BIT_RATE_40;
            else
                video_bit_rate = D10MXFOP1AWriter::D10_BIT_RATE_50;
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-s") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%u", &value) != 1 ||
                value <= 0 || value > 250000)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            video_frame_size = value;
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-t") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            start_timecode_str = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-d") == 0)
        {
            drop_frame = true;
        }
        else if (strcmp(argv[cmdln_index], "-r") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d:%d", &num, &den) != 2 ||
                ((num != 4 || den != 3) && (num != 16 || den != 9)))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            aspect_ratio.numerator = num;
            aspect_ratio.denominator = den;
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-v") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            video_filename = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-a") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (num_audio_files >= 8) {
                usage(argv[0]);
                fprintf(stderr, "Number of audio files exceeds maximum 8\n");
                return 1;
            }
            audio_filename[num_audio_files] = argv[cmdln_index + 1];
            num_audio_files++;
            cmdln_index++;
        }
        else
        {
            break;
        }
    }

    if (cmdln_index + 1 != argc) {
        usage(argv[0]);
        fprintf(stderr, "Unknown option '%s'\n", argv[cmdln_index]);
        return 1;
    }
    if (!video_filename) {
        usage(argv[0]);
        fprintf(stderr, "Missing video filename (-v)\n");
        return 1;
    }
    if (start_timecode_str && !parse_timecode(start_timecode_str, sample_rate, &start_timecode))
    {
        usage(argv[0]);
        fprintf(stderr, "Invalid value '%s' for option '-t'\n", start_timecode_str);
        return 1;
    }
    out_filename = argv[cmdln_index];
    
    
    FILE *video_file = fopen(video_filename, "rb");
    if (!video_file) {
        fprintf(stderr, "Failed to open video file '%s': %s\n", video_filename, strerror(errno));
        return 1;
    }
    
    FILE *audio_file[8];
    int i;
    for (i = 0; i < num_audio_files; i++) {
        audio_file[i] = fopen(audio_filename[i], "rb");
        if (!audio_file[i]) {
            fprintf(stderr, "Failed to open audio file '%s': %s\n", audio_filename[i], strerror(errno));
            return 1;
        }
    }
    
    try
    {
        auto_ptr<D10MXFOP1AWriter> writer(new D10MXFOP1AWriter());

        writer->SetSampleRate(sample_rate);
        writer->SetAudioChannelCount(num_audio_files);
        writer->SetAudioQuantizationBits(audio_bps);
        writer->SetAspectRatio(aspect_ratio);
        writer->SetStartTimecode(start_timecode, drop_frame);
        writer->SetBitRate(video_bit_rate, video_frame_size);
        if (!writer->CreateFile(out_filename))
            throw MXFException("Failed to create file %s", out_filename);
        
        unsigned char video[250000];
        unsigned char audio[1920*3];
        uint32_t audio_sample_count;
        
        int64_t frame_count = 0;
        while (true) {
            writer->SetUserTimecode(writer->GenerateUserTimecode());
            
            if (fread(video, video_frame_size, 1, video_file) != 1) {
                if (ferror(video_file))
                    fprintf(stderr, "Failed to read from video file: %s\n", strerror(errno));
                break;
            }
            writer->SetVideo(video, video_frame_size);
            
            audio_sample_count = writer->GetAudioSampleCount();
            
            for (i = 0; i < num_audio_files; i++) {
                if (fread(audio, audio_sample_count * audio_bytes_ps, 1, audio_file[i]) != 1) {
                    if (ferror(audio_file[i]))
                        fprintf(stderr, "Failed to read from audio file %d: %s\n", i, strerror(errno));
                    break;
                }
                writer->SetAudio(i, audio, audio_sample_count * audio_bytes_ps);
            }
            if (i < num_audio_files)
                break;

            writer->WriteContentPackage();
            
            frame_count++;
        }
        
        writer->CompleteFile();
        
        printf("Total frames written: %"PRId64"\n", frame_count);
    }
    catch (MXFException &ex)
    {
        fprintf(stderr, "Exception thrown: %s\n", ex.getMessage().c_str());
        return 1;
    }
    
    fclose(video_file);
    for (i = 0; i < num_audio_files; i++)
        fclose(audio_file[i]);

    
    return 0;
}

