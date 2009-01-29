/*
 * $Id: source_event.h,v 1.3 2009/01/29 07:10:27 stuart_hc Exp $
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

#ifndef __SOURCE_EVENT_H__
#define __SOURCE_EVENT_H__



typedef enum
{
    NULL_EVENT_TYPE = 0,
    SOURCE_NAME_UPDATE_EVENT_TYPE
} SourceEventType;


typedef struct
{
    char name[64];
} SourceNameUpdateEvent;


typedef struct
{
    SourceEventType type;
    union
    {
        SourceNameUpdateEvent nameUpdate;
    } value;
} SourceEvent;


int svt_get_buffer_size(int numEvents);

void svt_write_num_events(unsigned char* buffer, int numEvents);
int svt_read_num_events(const unsigned char* buffer);

void svt_write_event(unsigned char* buffer, int index, const SourceEvent* event);
void svt_read_event(const unsigned char* buffer, int index, SourceEvent* event);


void svt_set_name_update_event(SourceEvent* event, char* name);


#endif


