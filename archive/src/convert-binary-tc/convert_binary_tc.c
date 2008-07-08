/*
 * $Id: convert_binary_tc.c,v 1.1 2008/07/08 16:47:12 philipn Exp $
 *
 * Converts legacy binary timecode files to text timecode files.
 *
 * Copyright (C) 2007 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
/*
    The timecode file lists the control, VITC and LTC timecode associated with
    each frame ingested. The Ingex Archive recorder was first using a binary 
    encoding (SMPTE 12M) for the timecode file. This was changed to a text 
    encoding and this utility allows the binary encoded files to be converted.
    
    This utility detects whether a timecode file has the binary or text format.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


typedef struct
{
    int hour;
    int min;
    int sec;
    int frame;
} Timecode;


static inline void parse_timecode(const char* t12m, Timecode* tc)
{
    // SMPTE 12M timecode format:
    //     upper nibble | lower nibble
    //     
    //     byte 0: frame tens | frame units
    //     byte 1: sec tens | sec units
    //     byte 2: min tens | min units
    //     byte 3: hour tens | hour units
    
    tc->frame = ((t12m[0] >> 4) & 0x03) * 10 + (t12m[0] & 0xf);
    tc->sec = ((t12m[1] >> 4) & 0x07) * 10 + (t12m[1] & 0xf);
    tc->min = ((t12m[2] >> 4) & 0x07) * 10 + (t12m[2] & 0xf);
    tc->hour = ((t12m[3] >> 4) & 0x03) * 10 + (t12m[3] & 0xf);
}

static inline void print_timecode(char prefix, Timecode* tc)
{
    printf("%c%02d:%02d:%02d:%02d", prefix, tc->hour, tc->min, tc->sec, tc->frame);
}

static inline int test_first_binary_timecode(const char* t12m)
{
    // the first control timecode is 0x00000000
    if (t12m[0] != 0x00 || t12m[1] != 0x00 || t12m[2] != 0x00 || t12m[3] != 0x00)
    {
        // the first text control timecode is C00:00:00:00 ...
        if (t12m[0] == 'C' && t12m[1] == '0' && t12m[2] == '0' && t12m[3] == ':')
        {
            fprintf(stderr, "    Not a binary tc file: file contains text timecode\n");
            return 0;
        }
        
        fprintf(stderr, "    *) Not a binary tc file: first 4 bytes are not all zero: 0x%02x%02x%02x%02x\n", 
            t12m[0], t12m[1], t12m[2], t12m[3]);
        return 0;
    }
    
    return 1;
}


int main(int argc, const char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <timecode file>\n", argv[0]);
        return 1;
    }
    
    FILE* tcFile;
    if ((tcFile = fopen(argv[1], "rb")) == 0)
    {
        fprintf(stderr, "Failed to open timecode file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    
    fprintf(stderr, "Converting file '%s'\n", argv[1]);

    int count = 0;
    char buf[4 * 3];
    Timecode tc;
    int isFirstSet = 1;
    while (fread(buf, 4 * 3, 1, tcFile) == 1)
    {
        if (isFirstSet)
        {
            if (!test_first_binary_timecode(&buf[0]))
            {
                break;
            }
            isFirstSet = 0;
        }
        // control timecode
        parse_timecode(&buf[0], &tc);
        print_timecode('C', &tc);
        printf(" ");
        
        // vitc
        parse_timecode(&buf[4], &tc);
        print_timecode('V', &tc);
        printf(" ");
        
        // ltc
        parse_timecode(&buf[8], &tc);
        print_timecode('L', &tc);
        printf(" ");
        
        printf("\n");
        
        count++;
    }
    
    if (!isFirstSet)
    {
        fprintf(stderr, "    Total = %d frames (%02d:%02d:%02d:%02d)\n", count,
            count / (60 * 60 * 25),
            (count % (60 * 60 * 25)) / (60 * 25),
            ((count % (60 * 60 * 25)) % (60 * 25)) / 25,
            ((count % (60 * 60 * 25)) % (60 * 25)) % 25);
    }
    
    
    fclose(tcFile);
    
    return isFirstSet;
}


