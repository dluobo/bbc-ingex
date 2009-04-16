/*
 * $Id: test_archivemxfwriter.cpp,v 1.2 2009/04/16 17:52:49 john_f Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#define __STDC_FORMAT_MACROS 1

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <memory>

#include "ArchiveMXFWriter.h"


using namespace std;
using namespace mxfpp;


#define VIDEO_FRAME_WIDTH       720
#define VIDEO_FRAME_HEIGHT      576
#define VIDEO_FRAME_SIZE        (VIDEO_FRAME_WIDTH * VIDEO_FRAME_HEIGHT * 2)
#define AUDIO_FRAME_SIZE        5760


// Represent the colour and position of a colour bar
typedef struct 
{
	double			position;
	unsigned char	colour[4];
} bar_colour_t;

// Generate a video buffer containing uncompressed UYVY video representing
// the familiar colour bars test signal (or YUY2 video if specified).
static void create_colour_bars(unsigned char *video_buffer, int width, int height, bool convert_to_YUY2)
{
	int				i,j,b;
	bar_colour_t	UYVY_table[] = {
				{52/720.0,	{0x80,0xEB,0x80,0xEB}},	// white
				{140/720.0,	{0x10,0xD2,0x92,0xD2}},	// yellow
				{228/720.0,	{0xA5,0xA9,0x10,0xA9}},	// cyan
				{316/720.0,	{0x35,0x90,0x22,0x90}},	// green
				{404/720.0,	{0xCA,0x6A,0xDD,0x6A}},	// magenta
				{492/720.0,	{0x5A,0x51,0xF0,0x51}},	// red
				{580/720.0,	{0xf0,0x29,0x6d,0x29}},	// blue
				{668/720.0,	{0x80,0x10,0x80,0x10}},	// black
				{720/720.0,	{0x80,0xEB,0x80,0xEB}}	// white
			};

	for (j = 0; j < height; j++)
	{
		for (i = 0; i < width; i+=2)
		{
			for (b = 0; b < 9; b++)
			{
				if ((i / ((double)width)) < UYVY_table[b].position)
				{
					if (convert_to_YUY2) {
						// YUY2 packing
						video_buffer[j*width*2 + i*2 + 1] = UYVY_table[b].colour[0];
						video_buffer[j*width*2 + i*2 + 0] = UYVY_table[b].colour[1];
						video_buffer[j*width*2 + i*2 + 3] = UYVY_table[b].colour[2];
						video_buffer[j*width*2 + i*2 + 2] = UYVY_table[b].colour[3];
					}
					else {
						// UYVY packing
						video_buffer[j*width*2 + i*2 + 0] = UYVY_table[b].colour[0];
						video_buffer[j*width*2 + i*2 + 1] = UYVY_table[b].colour[1];
						video_buffer[j*width*2 + i*2 + 2] = UYVY_table[b].colour[2];
						video_buffer[j*width*2 + i*2 + 3] = UYVY_table[b].colour[3];
					}
					break;
				}
			}
		}
	}
}

static void increment_timecode(Timecode* timecode)
{
    timecode->frame++;
    if (timecode->frame > 24)
    {
        timecode->frame = 0;
        timecode->sec++;
        if (timecode->sec > 59)
        {
            timecode->sec = 0;
            timecode->min++;
            if (timecode->min > 59)
            {
                timecode->min = 0;
                timecode->hour++;
                if (timecode->hour > 23)
                {
                    timecode->hour = 0;
                    timecode->frame++;
                }
            }
        }
    }
}



void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <filename>\n", cmd);
}

int main(int argc, const char** argv)
{
    const char* filename = 0;
    int numAudioTracks = 5;
    
    if (argc != 2)
    {
        usage(argv[0]);
        exit(1);
    }
    
    filename = argv[1];
    
    Timecode vitc(10, 0, 0, 0);
    Timecode ltc(10, 0, 0, 0);
    unsigned char uncData[VIDEO_FRAME_SIZE];
    unsigned char pcmData[AUDIO_FRAME_SIZE];
    int frame;
    int i;
    
    create_colour_bars(uncData, VIDEO_FRAME_WIDTH, VIDEO_FRAME_HEIGHT, false);
    memset(pcmData, 0, AUDIO_FRAME_SIZE);
    try
    {
        auto_ptr<ArchiveMXFWriter> writer(new ArchiveMXFWriter(filename, numAudioTracks, 0));
        
        for (frame = 0; frame < 100; frame++)
        {
            writer->writeTimecode(vitc, ltc);
            writer->writeVideoFrame(uncData, VIDEO_FRAME_SIZE);
            for (i = 0; i < numAudioTracks; i++)
            {
                writer->writeAudioFrame(pcmData, AUDIO_FRAME_SIZE);
            }
            
            increment_timecode(&vitc);
            increment_timecode(&ltc);
        }
        
        writer->complete();
    }
    catch (MXFException& ex)
    {
        fprintf(stderr, "Exception thrown: %s\n", ex.getMessage().c_str());
        exit(1);
    }
    
    return 0;
}
