/*
 * $Id: source_event.c,v 1.3 2009/01/29 07:10:27 stuart_hc Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "source_event.h"
#include "logging.h"
#include "macros.h"


int svt_get_buffer_size(int numEvents)
{
    return sizeof(int) + numEvents * sizeof(SourceEvent);
}

void svt_write_num_events(unsigned char* buffer, int numEvents)
{
    memcpy(buffer, (const unsigned char*)&numEvents, sizeof(int));
}

int svt_read_num_events(const unsigned char* buffer)
{
    return *((const int*)buffer);
}

void svt_write_event(unsigned char* buffer, int index, const SourceEvent* event)
{
    memcpy(&buffer[sizeof(int) + index * sizeof(SourceEvent)], (const unsigned char*)event, sizeof(SourceEvent));
}

void svt_read_event(const unsigned char* buffer, int index, SourceEvent* event)
{
    memcpy((unsigned char*)event, &buffer[sizeof(int) + index * sizeof(SourceEvent)], sizeof(SourceEvent));
}

void svt_set_name_update_event(SourceEvent* event, char* name)
{
    memset(event, 0, sizeof(SourceEvent));

    event->type = SOURCE_NAME_UPDATE_EVENT_TYPE;
    strncpy(event->value.nameUpdate.name, name, sizeof(event->value.nameUpdate.name) - 1);
}

