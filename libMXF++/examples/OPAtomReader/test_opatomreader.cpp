/*
 * $Id: test_opatomreader.cpp,v 1.2 2009/10/23 09:05:21 philipn Exp $
 *
 * Test the OP-Atom reader
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#define __STDC_FORMAT_MACROS    1

#include <libMXF++/MXF.h>

#include "OPAtomClipReader.h"

using namespace std;
using namespace mxfpp;


int main(int argc, const char **argv)
{
    vector<string> filenames;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <mxf filename>\n", argv[0]);
        return 1;
    }
    
    filenames.push_back(argv[1]);
    
    OPAtomClipReader *clip_reader;
    pair<OPAtomOpenResult, string> result = OPAtomClipReader::Open(filenames, &clip_reader);
    if (result.first != OP_ATOM_SUCCESS) {
        fprintf(stderr, "Failed to open file '%s': %s\n", result.second.c_str(),
                OPAtomClipReader::ErrorToString(result.first).c_str());
        return 1;
    }
    
    printf("Duration = %"PRId64"\n", clip_reader->GetDuration());
    
    const OPAtomContentPackage *content_package;
    
    FILE *output = fopen("/tmp/test.raw", "wb");
    while (true) {
        content_package = clip_reader->Read();
        if (!content_package) {
            if (!clip_reader->IsEOF()) {
                fprintf(stderr, "Failed to read content package\n");
            }
            break;
        }
        
        printf("writing %d bytes from essence offset 0x%"PRIx64"\n", content_package->GetEssenceDataISize(0),
               content_package->GetEssenceDataI(0)->GetEssenceOffset());
        fwrite(content_package->GetEssenceDataIBytes(0), content_package->GetEssenceDataISize(0), 1, output);
    }

    printf("Duration = %"PRId64"\n", clip_reader->GetDuration());
    
    fclose(output);
    
    clip_reader->Seek(0);
    content_package = clip_reader->Read();
    if (!content_package) {
        if (!clip_reader->IsEOF()) {
            fprintf(stderr, "Failed to read content package\n");
        }
    }
    
    
    delete clip_reader;
    
    return 0;
}
