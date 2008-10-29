/*
 * $Id: video_switch_database.h,v 1.2 2008/10/29 17:47:42 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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

#ifndef __VIDEO_SWITCH_DATABASE_H__
#define __VIDEO_SWITCH_DATABASE_H__


#include "types.h"


#ifdef __cplusplus
extern "C" 
{
#endif

#define VIDEO_SWITCH_DB_ENTRY_SIZE      64


typedef struct VideoSwitchDatabase VideoSwitchDatabase;


int vsd_open_write(const char* filename, VideoSwitchDatabase** db);
int vsd_open_append(const char* filename, VideoSwitchDatabase** db);
int vsd_open_read(const char* filename, VideoSwitchDatabase** db);
void vsd_close(VideoSwitchDatabase** db);


/* writer */

int vsd_append_entry(VideoSwitchDatabase* db, int sourceIndex, const char* sourceId, const Timecode* tc);


/* reader */

typedef struct
{
    int sourceIndex;
    const char* sourceId;
    int year;
    int month;
    int day;
    Timecode tc;
} VideoSwitchDbEntry;

int vsd_get_num_entries(VideoSwitchDatabase* db);
int vsd_load(VideoSwitchDatabase* db, int startEntry, unsigned char* buffer, int bufferSize, int* numEntries);
int vsd_parse_entry(const unsigned char* entryBuffer, int entryBufferSize, VideoSwitchDbEntry* entry);


int vsd_dump(const char* filename, FILE* output);


#ifdef __cplusplus
}
#endif


#endif


