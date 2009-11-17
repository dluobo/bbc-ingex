/*
 * $Id: recover_mxf.cpp,v 1.1 2009/11/17 16:26:35 john_f Exp $
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

#define __STDC_FORMAT_MACROS    1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <string>

#include <IngexMXFInfo.h>
#include <MXFUtils.h>
#include <ProdAutoException.h>

#include "RecoverClip.h"

using namespace std;


const char * const DEFAULT_OUTPUT_PREFIX = "recover_";

const char * const DEFAULT_CREATING_DIR = "";
const char * const DEFAULT_DESTINATION_DIR = "";
const char * const DEFAULT_FAILURE_DIR = "";




static void print_usage(const char *cmd)
{
    fprintf(stderr, "%s [options] <filenames>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -h | --help            Show this usage message and exit\n");
    fprintf(stderr, "    --prefix               Prefix for output files (default '%s')\n", DEFAULT_OUTPUT_PREFIX);
    fprintf(stderr, "    --accept-incomplete    Accept incomplete set of files for a clip\n");
    fprintf(stderr, "    --del-on-error         Delete recovered files if an error occurs whilst recovering\n");
    fprintf(stderr, "    --simulate             Do everything except create and write recovered files\n");
    fprintf(stderr, "    --create <dir>         Recovered file creation directory (default '%s')\n", DEFAULT_CREATING_DIR);
    fprintf(stderr, "    --dest <dir>           Recovered file destination directory (default '%s')\n", DEFAULT_DESTINATION_DIR);
    fprintf(stderr, "    --fail <dir>           Recovered file failure directory (default '%s')\n", DEFAULT_FAILURE_DIR);
}

int main(int argc, const char **argv)
{
    int cmdln_index;
    vector<string> filenames;
    const char *output_filename_prefix = DEFAULT_OUTPUT_PREFIX;
    bool accept_incomplete = false;
    bool del_on_error = false;
    bool simulate = false;
    const char *create_dir = DEFAULT_CREATING_DIR;
    const char *dest_dir = DEFAULT_CREATING_DIR;
    const char *fail_dir = DEFAULT_CREATING_DIR;

    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--prefix") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                print_usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdln_index]);
                return 1;
            }
            output_filename_prefix = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--accept-incomplete") == 0)
        {
            accept_incomplete = true;
        }
        else if (strcmp(argv[cmdln_index], "--del-on-error") == 0)
        {
            del_on_error = true;
        }
        else if (strcmp(argv[cmdln_index], "--simulate") == 0)
        {
            simulate = true;
        }
        else if (strcmp(argv[cmdln_index], "--create") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                print_usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdln_index]);
                return 1;
            }
            create_dir = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--dest") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                print_usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdln_index]);
                return 1;
            }
            dest_dir = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--fail") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                print_usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdln_index]);
                return 1;
            }
            fail_dir = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else
        {
            break;
        }
    }
    
    if (cmdln_index == argc) {
        print_usage(argv[0]);
        fprintf(stderr, "Missing <filenames>\n");
        return 1;
    }

    for (; cmdln_index < argc; cmdln_index++)
        filenames.push_back(argv[cmdln_index]);
    
    
    // connect libMXF logging to prodauto logging
    prodauto::connectLibMXFLogging();
    
    
    size_t i, j, k;


    // read package info from each file

    vector<IngexMXFInfo*> mxf_infos;
    for (i = 0; i < filenames.size(); i++) {
        IngexMXFInfo *info;
        IngexMXFInfo::ReadResult result = IngexMXFInfo::read(filenames[i], &info);
        if (result != IngexMXFInfo::SUCCESS) {
            fprintf(stderr, "Failed to read package info from file '%s': %s\n",
                    filenames[i].c_str(), IngexMXFInfo::errorToString(result).c_str());
            return 1;
        }
        
        mxf_infos.push_back(info);
    }
    
    
    // group files into clips
    
    vector<RecoverClip*> clips;
    for (i = 0; i < mxf_infos.size(); i++) {
        for (j = 0; j < clips.size(); j++) {
            if (clips[j]->IsSameClip(mxf_infos[i])) {
                clips[j]->MergeIn(mxf_infos[i]);
                break;
            }
        }
        if (j == clips.size())
            clips.push_back(new RecoverClip(mxf_infos[i]));
    }
    
    
    // check clips are complete
    
    for (j = 0; j < clips.size(); j++) {
        if (!clips[j]->IsComplete()) {
            vector<uint32_t> missing_tracks = clips[j]->GetMissingTracks();
            fprintf(stderr, "Clip file set '%s' is incomplete: missing tracks ",
                    clips[j]->GetMaterialPackage()->name.c_str());
            for (k = 0; k < missing_tracks.size(); k++)
                fprintf(stderr, "%d ", missing_tracks[i]);
            fprintf(stderr, "\n");
            if (!accept_incomplete) {
                fprintf(stderr, "Failed because clip file set is incomplete\n");
                return 1;
            }
        }
    }
    
    
    // recover each clip
    
    for (j = 0; j < clips.size(); j++)
        clips[j]->Recover(create_dir, dest_dir, fail_dir, output_filename_prefix, del_on_error, simulate);
    
    
    // clean up
    
    for (i = 0; i < mxf_infos.size(); i++)
        delete mxf_infos[i];
    for (j = 0; j < clips.size(); j++)
        delete clips[j];
    
    
    return 0;
}

