/*
 * $Id: main.cpp,v 1.2 2009/04/16 17:52:49 john_f Exp $
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

#include "AvidClipWriter.h"



using namespace std;
using namespace mxfpp;


void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <filename>\n", cmd);
}

int main(int argc, const char** argv)
{
    string prefix;
    
    if (argc != 2)
    {
        usage(argv[0]);
        exit(1);
    }
    
    prefix = argv[1];

    mxfRational aspectRatio = {4, 3};
    
    AvidClipWriter writer(AVID_PAL_25I_PROJECT, aspectRatio, false, true);
    
    writer.setProjectName("test project");
    writer.setClipName("test clip");
    writer.setTape("test tape", 10 * 60 * 60 * 25);
    writer.addUserComment("Descript", "a test project");
    
    EssenceParams params;
    
    string videoFilename = prefix;
    videoFilename.append("_v1.mxf");
    writer.registerEssenceElement(1, 1, AVID_MJPEG201_ESSENCE, &params, videoFilename);

    params.quantizationBits = 16;
    string audio1Filename = prefix;
    audio1Filename.append("_a1.mxf");
    writer.registerEssenceElement(2, 3, AVID_PCM_ESSENCE, &params, audio1Filename);

    string audio2Filename = prefix;
    audio2Filename.append("_a2.mxf");
    writer.registerEssenceElement(3, 4, AVID_PCM_ESSENCE, &params, audio2Filename);
    
    writer.prepareToWrite();
    
    int i;
    unsigned char buffer[288000];
    memset(buffer, 0, sizeof(*buffer));
    for (i = 0; i < 50; i++)
    {
        writer.writeSamples(1, 1, buffer, 288000);
        writer.writeSamples(2, 1920, buffer, 1920 * 2);
        writer.writeSamples(3, 1920, buffer, 1920 * 2);
    }
    
    writer.completeWrite();
    
    return 0;
}


