/*
 * $Id: test_video_switch_database.c,v 1.2 2009/01/29 07:10:27 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#include "video_switch_database.h"


#define CHECK(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "Error: '%s' failed in %s:%d\n", #cmd, __FILE__, __LINE__); \
        exit(1); \
    }

#define CHECK_MALLOC(result) \
    if ((result) == NULL) \
    { \
        fprintf(stderr, "Error: Out of memory (%s:%d)\n", __FILE__, __LINE__); \
        exit(1); \
    }



static int file_exists(const char* filename)
{
    struct stat buf;

    return stat(filename, &buf) == 0;
}

static int parse_sources(const char* sourcesStr, char*** sources, int* numSources)
{
    int i;
    int len;
    int sourceIndex;
    int sourceLen;

    len = (int)strlen(sourcesStr);

    /* count number of sources */
    *numSources = 1;
    for (i = 0; i < len; i++)
    {
        if (sourcesStr[i] == ',')
        {
            (*numSources)++;
        }
    }

    /* allocate sources */
    *sources = (char**)malloc(sizeof(char*) * (*numSources));
    CHECK_MALLOC(*sources);

    /* parse source names */
    sourceIndex = 0;
    sourceLen = 0;
    for (i = 0; i < len; i++)
    {
        if (sourcesStr[i] == ',')
        {
            (*sources)[sourceIndex] = (char*)malloc(sourceLen + 1);
            CHECK_MALLOC((*sources)[sourceIndex]);

            strncpy((*sources)[sourceIndex], &sourcesStr[i - sourceLen], sourceLen);
            (*sources)[sourceIndex][sourceLen] = '\0';

            sourceIndex++;
            sourceLen = 0;
        }
        else
        {
            sourceLen++;
        }
    }

    (*sources)[sourceIndex] = (char*)malloc(sourceLen + 1);
    CHECK_MALLOC((*sources)[sourceIndex]);

    strncpy((*sources)[sourceIndex], &sourcesStr[i - sourceLen], sourceLen);
    (*sources)[sourceIndex][sourceLen] = '\0';


    return 1;
}

static int parse_start_date(const char* startStr, int* startYear, int* startMonth, int* startDay)
{
    return sscanf(startStr, "%d-%d-%d", startYear, startMonth, startDay) == 3;
}

static int parse_start(const char* startStr, Timecode* start)
{
    start->isDropFrame = 0;
    return sscanf(startStr, "%u:%u:%u:%u", &start->hour, &start->min, &start->sec, &start->frame) == 4;
}

static int parse_cut(const char* cutStr, int cutStrLen, char** sources, int numSources, Timecode* start,
    VideoSwitchDbEntry* cut)
{
    int i;
    int64_t position;

    /* parse position */
    if (sscanf(cutStr, "%"PRIi64"", &position) != 1)
    {
        fprintf(stderr, "Error: Failed to parse position in cut string\n");
        return 0;
    }

    position += start->hour * 60 * 60 * 25 +
        start->min * 60 * 25 +
        start->sec * 25 +
        start->frame;

    cut->tc.hour = (position) / (60 * 60 * 25);
    cut->tc.min = ((position) % (60 * 60 * 25)) / (60 * 25);
    cut->tc.sec = (((position) % (60 * 60 * 25)) % (60 * 25)) / 25;
    cut->tc.frame = (((position) % (60 * 60 * 25)) % (60 * 25)) % 25;

    /* parse source index and get source name */
    for (i = 0; i < cutStrLen; i++)
    {
        if (cutStr[i] == '=')
        {
            break;
        }
    }
    if (cutStr[i] != '=')
    {
        fprintf(stderr, "Error: Failed to find '=' in cut string\n");
        return 0;
    }
    i++;
    if (sscanf(&cutStr[i], "%d", &cut->sourceIndex) != 1)
    {
        fprintf(stderr, "Error: Failed to parse source index\n");
        return 0;
    }
    if (cut->sourceIndex < 0 || cut->sourceIndex >= numSources)
    {
        fprintf(stderr, "Error: Unknown source index %d in cuts\n", cut->sourceIndex);
    }
    cut->sourceId = sources[cut->sourceIndex];


    return 1;
}

static int parse_cuts(const char* cutsStr, char** sources, int numSources, Timecode* start,
    VideoSwitchDbEntry** cuts, int* numCuts)
{
    int i;
    int len;
    int cutIndex;
    int cutLen;

    len = (int)strlen(cutsStr);

    /* count number of cuts */
    *numCuts = 1;
    for (i = 0; i < len; i++)
    {
        if (cutsStr[i] == ',')
        {
            (*numCuts)++;
        }
    }

    /* allocate cuts */
    *cuts = (VideoSwitchDbEntry*)malloc(sizeof(VideoSwitchDbEntry) * (*numCuts));
    CHECK_MALLOC(*cuts);

    /* parse cuts */
    cutIndex = 0;
    cutLen = 0;
    for (i = 0; i < len; i++)
    {
        if (cutsStr[i] == ',')
        {
            if (!parse_cut(&cutsStr[i - cutLen], cutLen, sources, numSources, start, &((*cuts)[cutIndex])))
            {
                fprintf(stderr, "Error: Failed to parse cut\n");
                return 0;
            }

            cutIndex++;
            cutLen = 0;
        }
        else
        {
            cutLen++;
        }
    }

    if (!parse_cut(&cutsStr[i - cutLen], cutLen, sources, numSources, start, &((*cuts)[cutIndex])))
    {
        fprintf(stderr, "Error: Failed to parse cut\n");
        return 0;
    }


    return 1;
}


static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [-c|-a|-l] [options] <db filename>\n", cmd);
    fprintf(stderr, "   -c           Create new video switch database file\n");
    fprintf(stderr, "   -a           Append cuts to existing video switch database file\n");
    fprintf(stderr, "   -l           List contents of video switch database file (default)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, "   -h|--help    Print this usage and exit\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options for -c:\n");
    fprintf(stderr, "  --overwrite      Overwrite existing file\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options for -c and -a: (! means required)\n");
    fprintf(stderr, "! --sources <name>(,<name>)*               Comma separated list of source names\n");
    fprintf(stderr, "! --cuts <pos>=<source index>(,<pos>=<source index>)*  Comma separated list of cuts (pos 0 = start timecode)\n");
    fprintf(stderr, "! --start <hh:mm:ss:ff>                    Start timecode\n");
    fprintf(stderr, "  --start-date <yyyy-mm-dd>                Start date (default is today)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "   Create: ./test_video_switch_database -c --sources src1,src2,src3 --cuts 0=1,10=0,20=2 --start 10:00:00:00 test.db\n");
    fprintf(stderr, "   Append: ./test_video_switch_database -a --sources src1,src2 --cuts 0=0,20=1 --start 11:00:00:00 --start-date 2008-12-09 test.db\n");
    fprintf(stderr, "   List: ./test_video_switch_database -l test.db\n");
    fprintf(stderr, "\n");
}

int main (int argc, const char** argv)
{
    int cmdlnIndex = 1;
    VideoSwitchDatabase* db;
    int i;
    int doCreate = 0;
    int doAppend = 0;
    int doList = 1;
    const char* dbFilename = NULL;
    int overwrite = 0;
    const char* sourcesStr = NULL;
    const char* cutsStr =  NULL;
    const char* startStr = NULL;
    const char* startDateStr = NULL;
    Timecode startTimecode = {0, 0, 0, 0, 0};
    int startYear = -1;
    int startMonth = -1;
    int startDay = -1;
    char** sources = NULL;
    int numSources = 0;
    VideoSwitchDbEntry* cuts = NULL;
    int numCuts = 0;

    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "-c") == 0)
        {
            doCreate = 1;
            doAppend = 0;
            doList = 0;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "-a") == 0)
        {
            doAppend = 1;
            doCreate = 0;
            doList = 0;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "-l") == 0)
        {
            doList = 1;
            doCreate = 0;
            doAppend = 0;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "--overwrite") == 0)
        {
            overwrite = 1;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "--sources") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Error: Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            sourcesStr = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--cuts") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Error: Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            cutsStr = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--start") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Error: Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            startStr = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--start-date") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Error: Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            startDateStr = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else
        {
            break;
        }
    }

    if (cmdlnIndex + 1 < argc)
    {
        usage(argv[0]);
        fprintf(stderr, "Error: Unknown parameter '%s'\n", argv[cmdlnIndex]);
        return 1;
    }
    else if (cmdlnIndex + 1 > argc)
    {
        usage(argv[0]);
        fprintf(stderr, "Error: Missing <db filename>\n");
        return 1;
    }
    if (doCreate || doAppend)
    {
        if (sourcesStr == NULL)
        {
            usage(argv[0]);
            fprintf(stderr, "Error: Missing --sources\n");
            return 1;
        }
        if (cutsStr == NULL)
        {
            usage(argv[0]);
            fprintf(stderr, "Error: Missing --cuts\n");
            return 1;
        }
        if (startStr == NULL)
        {
            usage(argv[0]);
            fprintf(stderr, "Error: Missing --start\n");
            return 1;
        }

        if (!parse_sources(sourcesStr, &sources, &numSources))
        {
            usage(argv[0]);
            fprintf(stderr, "Error: Failed to parse --sources\n");
            return 1;
        }
        if (startDateStr != NULL && !parse_start_date(startDateStr, &startYear, &startMonth, &startDay))
        {
            usage(argv[0]);
            fprintf(stderr, "Error: Failed to parse --start-date\n");
            return 1;
        }
        if (!parse_start(startStr, &startTimecode))
        {
            usage(argv[0]);
            fprintf(stderr, "Error: Failed to parse --start\n");
            return 1;
        }
        if (!parse_cuts(cutsStr, sources, numSources, &startTimecode, &cuts, &numCuts))
        {
            usage(argv[0]);
            fprintf(stderr, "Error: Failed to parse --cuts\n");
            return 1;
        }
    }

    dbFilename = argv[cmdlnIndex];
    cmdlnIndex++;


    if (doCreate)
    {
        if (!overwrite && file_exists(dbFilename))
        {
            fprintf(stderr, "Error: No overwriting file '%s'. Use --overwrite to overwrite file\n", dbFilename);
            return 1;
        }

        CHECK(vsd_open_write(dbFilename, &db));

        for (i = 0; i < numCuts; i++)
        {
            if (startDateStr != NULL)
            {
                CHECK(vsd_append_entry_with_date(db, cuts[i].sourceIndex, cuts[i].sourceId, startYear, startMonth, startDay, &cuts[i].tc));
            }
            else
            {
                CHECK(vsd_append_entry(db, cuts[i].sourceIndex, cuts[i].sourceId, &cuts[i].tc));
            }
        }

        vsd_close(&db);
    }
    else if (doAppend)
    {
        if (!file_exists(dbFilename))
        {
            fprintf(stderr, "Error: Cannot append to non-existent file '%s'\n", dbFilename);
            return 1;
        }

        CHECK(vsd_open_append(dbFilename, &db));

        for (i = 0; i < numCuts; i++)
        {
            if (startDateStr != NULL)
            {
                CHECK(vsd_append_entry_with_date(db, cuts[i].sourceIndex, cuts[i].sourceId, startYear, startMonth, startDay, &cuts[i].tc));
            }
            else
            {
                CHECK(vsd_append_entry(db, cuts[i].sourceIndex, cuts[i].sourceId, &cuts[i].tc));
            }
        }

        vsd_close(&db);
    }
    else
    {
        CHECK(vsd_dump(dbFilename, stdout));
    }


    return 0;
}



