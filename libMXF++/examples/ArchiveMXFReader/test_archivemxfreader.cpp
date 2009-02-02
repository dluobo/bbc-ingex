/*
 * $Id: test_archivemxfreader.cpp,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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

#include "ArchiveMXFReader.h"



using namespace std;
using namespace mxfpp;


void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <filename>\n", cmd);
}

int main(int argc, const char** argv)
{
    const char* filename = 0;
    
    if (argc != 2)
    {
        usage(argv[0]);
        exit(1);
    }
    
    filename = argv[1];
    
    try
    {
        ArchiveMXFReader reader(filename);
        
        int count = 0;
        int i;
        while (!reader.isEOF() && count++ < 50)
        {
            const ArchiveMXFContentPackage* cp = reader.read();
            printf("video size = %d\n", cp->getVideo()->getSize());
            for (i = 0; i < cp->getNumAudioTracks(); i++)
            {
                printf("audio %d size = %d\n", i, cp->getAudio(i)->getSize());
            }
            printf("vitc = %02d:%02d:%02d:%02d\n", cp->getVITC().hour, 
                cp->getVITC().min, cp->getVITC().sec, cp->getVITC().frame);
        }
        
        reader.seekToPosition(1);
        count = 0;
        while (!reader.isEOF() && count++ < 10)
        {
            const ArchiveMXFContentPackage* cp = reader.read();
            printf("vitc = %02d:%02d:%02d:%02d\n", cp->getVITC().hour, 
                cp->getVITC().min, cp->getVITC().sec, cp->getVITC().frame);
        }

        reader.seekToPosition(0);
        
        Timecode vitc(10, 2, 5, 20);
        Timecode ltc(1);
        if (reader.seekToTimecode(vitc, ltc))
        {
            count = 0;
            while (!reader.isEOF() && count++ < 10)
            {
                const ArchiveMXFContentPackage* cp = reader.read();
                printf("vitc = %02d:%02d:%02d:%02d\n", cp->getVITC().hour, 
                    cp->getVITC().min, cp->getVITC().sec, cp->getVITC().frame);
            }
        }
        else
        {
            printf("Failed to seek to timecode\n");
        }

    }
    catch (MXFException& ex)
    {
        fprintf(stderr, "Exception thrown: %s\n", ex.getMessage().c_str());
        exit(1);
    }
    
    return 0;
}

